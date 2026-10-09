#include <errno.h>
#include <limits.h>
#include <signal.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/types.h>
#include <sys/syscall.h>
#include <unistd.h>

static int parse_pid(const char *text, pid_t *out)
{
    char *end = NULL;
    errno = 0;
    long value = strtol(text, &end, 10);

    if (errno || !text[0] || !end || *end ||
        value <= 0 || value > INT_MAX)
        return -1;

    *out = (pid_t)value;
    return 0;
}

static int parse_u64(const char *text, uint64_t *out)
{
    char *end = NULL;
    errno = 0;
    unsigned long long value = strtoull(text, &end, 10);

    if (errno || !text[0] || !end || *end || text[0] == '-')
        return -1;

    *out = (uint64_t)value;
    return 0;
}

#if defined(SYS_pidfd_open) && defined(SYS_pidfd_send_signal)
static int read_start_time(pid_t pid, uint64_t *out)
{
    char path[64];
    char line[8192];

    (void)snprintf(path, sizeof(path), "/proc/%ld/stat", (long)pid);

    FILE *file = fopen(path, "r");
    if (!file)
        return -1;

    if (!fgets(line, sizeof(line), file)) {
        int saved_errno = errno ? errno : EIO;
        fclose(file);
        errno = saved_errno;
        return -1;
    }
    fclose(file);

    char *close_paren = strrchr(line, ')');
    if (!close_paren || close_paren[1] != ' ') {
        errno = EINVAL;
        return -1;
    }

    char *save = NULL;
    char *token = strtok_r(close_paren + 2, " ", &save);
    int field = 3;

    while (token) {
        if (field == 22)
            return parse_u64(token, out);

        ++field;
        token = strtok_r(NULL, " ", &save);
    }

    errno = EINVAL;
    return -1;
}
#endif

int main(int argc, char **argv)
{
    if (argc != 5 || strcmp(argv[1], "--terminate") != 0) {
        fprintf(stderr,
                "Usage: %s --terminate PID EXPECTED_STARTTIME CALLER_PID\n",
                argv[0]);
        return 2;
    }

    pid_t pid = 0;
    pid_t caller_pid = 0;
    uint64_t expected_starttime = 0;

    if (parse_pid(argv[2], &pid) != 0 ||
        parse_u64(argv[3], &expected_starttime) != 0 ||
        parse_pid(argv[4], &caller_pid) != 0) {
        fprintf(stderr, "Invalid process identity.\n");
        return 2;
    }

    if (pid <= 1 || pid == caller_pid || pid == getpid()) {
        fprintf(stderr, "Refusing to terminate a protected process.\n");
        return 2;
    }

#if defined(SYS_pidfd_open) && defined(SYS_pidfd_send_signal)
    int pidfd = (int)syscall(SYS_pidfd_open, pid, 0U);
    if (pidfd < 0) {
        fprintf(stderr, "Could not safely open selected process: %s\n",
                strerror(errno));
        return 3;
    }

    uint64_t actual_starttime = 0;
    if (read_start_time(pid, &actual_starttime) != 0) {
        close(pidfd);
        fprintf(stderr, "The selected process is no longer available.\n");
        return 3;
    }

    if (actual_starttime != expected_starttime) {
        close(pidfd);
        fprintf(stderr,
                "The PID now belongs to a different process. Refresh the list and try again.\n");
        return 3;
    }

    int result = (int)syscall(
        SYS_pidfd_send_signal, pidfd, SIGTERM, NULL, 0U);
    int saved_errno = errno;
    close(pidfd);

    if (result != 0) {
        fprintf(stderr, "Could not send SIGTERM: %s\n",
                strerror(saved_errno));
        return 1;
    }

    return 0;
#else
    (void)expected_starttime;
    fprintf(stderr,
            "Safe administrator termination is unavailable: "
            "pidfd support is missing. No signal was sent.\n");
    return 1;
#endif
}
