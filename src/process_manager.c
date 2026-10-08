#include "process_manager.h"

#include <dirent.h>
#include <errno.h>
#include <signal.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

static GHashTable *previous_ticks;
static uint64_t previous_total_ticks;

static gboolean is_pid_name(const char *name)
{
    if (!name || !*name) return FALSE;
    for (const char *p = name; *p; ++p)
        if (*p < '0' || *p > '9') return FALSE;
    return TRUE;
}

static uint64_t read_total_ticks(void)
{
    FILE *file = fopen("/proc/stat", "r");
    if (!file) return 0;

    char line[512];
    unsigned long long a, b, c, d, e, f, g, h;
    uint64_t total = 0;

    if (fgets(line, sizeof(line), file) &&
        sscanf(line, "cpu %llu %llu %llu %llu %llu %llu %llu %llu",
               &a, &b, &c, &d, &e, &f, &g, &h) == 8) {
        total = a + b + c + d + e + f + g + h;
    }

    fclose(file);
    return total;
}

static gboolean read_process(pid_t pid, ProcessInfo *out, uint64_t total_ticks_now)
{
    char path[256];
    snprintf(path, sizeof(path), "/proc/%d/stat", pid);

    FILE *file = fopen(path, "r");
    if (!file) return FALSE;

    char line[4096];
    if (!fgets(line, sizeof(line), file)) {
        fclose(file);
        return FALSE;
    }
    fclose(file);

    char *open = strchr(line, '(');
    char *close = strrchr(line, ')');
    if (!open || !close || close < open) return FALSE;

    size_t name_len = (size_t)(close - open - 1);
    if (name_len >= sizeof(out->name)) name_len = sizeof(out->name) - 1;
    memcpy(out->name, open + 1, name_len);
    out->name[name_len] = '\0';

    char state = 0;
    unsigned long long utime = 0;
    unsigned long long stime = 0;
    long rss_pages = 0;
    char *fields = close + 2; /* skip ") " -> field 3 starts here */

    /* Fields 4..13 are skipped, then 14=utime, 15=stime, 16..23 skipped, 24=rss. */
    if (sscanf(fields,
               "%c %*d %*d %*d %*d %*d %*u %*u %*u %*u %*u %llu %llu %*d %*d %*d %*d %*d %*d %*d %*d %ld",
               &state, &utime, &stime, &rss_pages) != 4) {
        return FALSE;
    }

    out->pid = pid;
    out->state = state;
    out->memory_mb = (uint64_t)((rss_pages > 0 ? rss_pages : 0) * (long)getpagesize()) / (1024ULL * 1024ULL);

    uint64_t ticks = (uint64_t)utime + (uint64_t)stime;
    uint64_t old_ticks = ticks;

    if (!previous_ticks) {
        previous_ticks = g_hash_table_new_full(g_direct_hash, g_direct_equal, NULL, g_free);
    }

    guint64 *old_ptr = g_hash_table_lookup(previous_ticks, GINT_TO_POINTER((gint)pid));
    if (old_ptr) old_ticks = *old_ptr;

    uint64_t proc_delta = ticks >= old_ticks ? ticks - old_ticks : 0;
    uint64_t total_delta = previous_total_ticks && total_ticks_now >= previous_total_ticks
                               ? total_ticks_now - previous_total_ticks
                               : 0;

    long cpu_count = sysconf(_SC_NPROCESSORS_ONLN);
    if (cpu_count < 1) cpu_count = 1;

    out->cpu_percent = total_delta
        ? 100.0 * (double)proc_delta / (double)total_delta * (double)cpu_count
        : 0.0;

    if (out->cpu_percent < 0.0) out->cpu_percent = 0.0;
    if (out->cpu_percent > 100.0 * (double)cpu_count)
        out->cpu_percent = 100.0 * (double)cpu_count;

    guint64 *saved = g_new(guint64, 1);
    *saved = ticks;
    g_hash_table_replace(previous_ticks, GINT_TO_POINTER((gint)pid), saved);

    return TRUE;
}

static gint compare_cpu(gconstpointer a, gconstpointer b)
{
    const ProcessInfo *pa = *(const ProcessInfo * const *)a;
    const ProcessInfo *pb = *(const ProcessInfo * const *)b;

    if (pa->cpu_percent < pb->cpu_percent) return 1;
    if (pa->cpu_percent > pb->cpu_percent) return -1;
    return (pa->pid > pb->pid) - (pa->pid < pb->pid);
}

GPtrArray *process_manager_list(void)
{
    GPtrArray *list = g_ptr_array_new_with_free_func(g_free);
    DIR *dir = opendir("/proc");
    if (!dir) return list;

    uint64_t total_ticks = read_total_ticks();
    struct dirent *entry;

    while ((entry = readdir(dir))) {
        if (!is_pid_name(entry->d_name)) continue;

        pid_t pid = (pid_t)strtol(entry->d_name, NULL, 10);
        if (pid <= 0) continue;

        ProcessInfo *info = g_new0(ProcessInfo, 1);
        if (read_process(pid, info, total_ticks))
            g_ptr_array_add(list, info);
        else
            g_free(info);
    }

    closedir(dir);
    previous_total_ticks = total_ticks;
    g_ptr_array_sort(list, compare_cpu);
    return list;
}

int process_manager_terminate(pid_t pid)
{
    if (pid <= 1 || pid == getpid()) {
        errno = EPERM;
        return -1;
    }
    return kill(pid, SIGTERM);
}

void process_manager_free_list(GPtrArray *list)
{
    if (list) g_ptr_array_free(list, TRUE);
}
