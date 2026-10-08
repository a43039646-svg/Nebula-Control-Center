#include "system_info.h"

#include <arpa/inet.h>
#include <dirent.h>
#include <ifaddrs.h>
#include <netinet/in.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/statvfs.h>
#include <sys/types.h>
#include <sys/utsname.h>
#include <time.h>
#include <unistd.h>

static uint64_t previous_idle = 0;
static uint64_t previous_total = 0;
static uint64_t previous_rx = 0;
static uint64_t previous_tx = 0;
static struct timespec previous_net_time = {0};

static int read_os_release(char *out, size_t out_size)
{
    FILE *file = fopen("/etc/os-release", "r");
    if (!file) return 0;

    char line[512];
    while (fgets(line, sizeof(line), file)) {
        if (strncmp(line, "PRETTY_NAME=", 12) == 0) {
            char *value = line + 12;
            value[strcspn(value, "\r\n")] = '\0';
            if (value[0] == '"') {
                size_t len = strlen(value);
                if (len >= 2 && value[len - 1] == '"') {
                    value[len - 1] = '\0';
                    memmove(value, value + 1, len - 1);
                }
            }
            snprintf(out, out_size, "%s", value);
            fclose(file);
            return 1;
        }
    }
    fclose(file);
    return 0;
}

static int read_cpu(SystemInfo *info)
{
    FILE *file = fopen("/proc/stat", "r");
    if (!file) return 0;

    char line[512];
    uint64_t user, nice, system, idle, iowait, irq, softirq, steal;
    if (!fgets(line, sizeof(line), file)) {
        fclose(file);
        return 0;
    }
    fclose(file);

    if (sscanf(line, "cpu %llu %llu %llu %llu %llu %llu %llu %llu",
               (unsigned long long *)&user,
               (unsigned long long *)&nice,
               (unsigned long long *)&system,
               (unsigned long long *)&idle,
               (unsigned long long *)&iowait,
               (unsigned long long *)&irq,
               (unsigned long long *)&softirq,
               (unsigned long long *)&steal) != 8)
        return 0;

    uint64_t idle_all = idle + iowait;
    uint64_t total = user + nice + system + idle + iowait + irq + softirq + steal;

    if (previous_total == 0) {
        info->cpu_usage = 0.0;
    } else {
        uint64_t total_delta = total - previous_total;
        uint64_t idle_delta = idle_all - previous_idle;
        info->cpu_usage = total_delta == 0 ? 0.0 : 100.0 * (1.0 - (double)idle_delta / (double)total_delta);
        if (info->cpu_usage < 0.0) info->cpu_usage = 0.0;
        if (info->cpu_usage > 100.0) info->cpu_usage = 100.0;
    }

    previous_total = total;
    previous_idle = idle_all;

    file = fopen("/proc/cpuinfo", "r");
    if (file) {
        info->cpu_cores = 0;
        while (fgets(line, sizeof(line), file)) {
            if (strncmp(line, "model name", 10) == 0 && info->cpu_model[0] == '\0') {
                char *colon = strchr(line, ':');
                if (colon) {
                    snprintf(info->cpu_model, sizeof(info->cpu_model), "%s", colon + 2);
                    info->cpu_model[strcspn(info->cpu_model, "\r\n")] = '\0';
                }
            }
            if (strncmp(line, "processor", 9) == 0)
                info->cpu_cores++;
        }
        fclose(file);
    }
    return 1;
}

static int read_memory(SystemInfo *info)
{
    FILE *file = fopen("/proc/meminfo", "r");
    if (!file) return 0;
    char key[64], unit[16];
    unsigned long long value;
    uint64_t total_kb = 0, available_kb = 0;

    while (fscanf(file, "%63s %llu %15s", key, &value, unit) == 3) {
        if (strcmp(key, "MemTotal:") == 0) total_kb = value;
        else if (strcmp(key, "MemAvailable:") == 0) available_kb = value;
    }
    fclose(file);

    if (total_kb == 0) return 0;
    uint64_t used_kb = total_kb > available_kb ? total_kb - available_kb : 0;
    info->memory_total_mb = total_kb / 1024;
    info->memory_used_mb = used_kb / 1024;
    info->memory_usage = 100.0 * (double)used_kb / (double)total_kb;
    return 1;
}

static int read_uptime(SystemInfo *info)
{
    FILE *file = fopen("/proc/uptime", "r");
    if (!file) return 0;
    double uptime = 0.0;
    int ok = fscanf(file, "%lf", &uptime) == 1;
    fclose(file);
    if (!ok) return 0;
    info->uptime_seconds = (uint64_t)uptime;

    FILE *load = fopen("/proc/loadavg", "r");
    if (load) {
        if (fscanf(load, "%lf %lf", &info->load1, &info->load5) != 2) {
            info->load1 = 0.0;
            info->load5 = 0.0;
        }
        fclose(load);
    }
    return 1;
}

static void read_kernel_hostname(SystemInfo *info)
{
    struct utsname uts;
    if (uname(&uts) == 0) {
        snprintf(info->kernel, sizeof(info->kernel), "%s %s", uts.sysname, uts.release);
        snprintf(info->hostname, sizeof(info->hostname), "%s", uts.nodename);
    }
    read_os_release(info->distro, sizeof(info->distro));
}

static void read_root_storage(SystemInfo *info)
{
    struct statvfs fs;
    if (statvfs("/", &fs) != 0) return;
    uint64_t total = (uint64_t)fs.f_blocks * fs.f_frsize;
    uint64_t free = (uint64_t)fs.f_bavail * fs.f_frsize;
    uint64_t used = total > free ? total - free : 0;
    info->root_total_bytes = total;
    info->root_used_bytes = used;
    info->root_usage = total ? 100.0 * (double)used / (double)total : 0.0;
}

static void read_gpu(SystemInfo *info)
{
    FILE *pipe = popen("command -v lspci >/dev/null 2>&1 && lspci -nnk 2>/dev/null | grep -E -A1 'VGA compatible controller|3D controller|Display controller' | head -n 2", "r");
    if (!pipe) {
        snprintf(info->gpu_name, sizeof(info->gpu_name), "GPU information unavailable");
        return;
    }

    char line[512];
    info->gpu_name[0] = '\0';
    info->gpu_driver[0] = '\0';
    int lines = 0;
    while (fgets(line, sizeof(line), pipe) && lines < 2) {
        line[strcspn(line, "\r\n")] = '\0';
        if (lines == 0) {
            const char *name = strstr(line, ": ");
            name = name ? name + 2 : line;
            while (*name == ' ')
                name++;

            snprintf(info->gpu_name, sizeof(info->gpu_name), "%s", name);

            char *rev = strstr(info->gpu_name, " (rev ");
            if (rev)
                *rev = '\0';

            char *class_tag = strstr(info->gpu_name, " [");
            if (class_tag)
                *class_tag = '\0';
        } else if (strstr(line, "Kernel driver in use:")) {
            const char *p = strstr(line, "Kernel driver in use:");
            p += strlen("Kernel driver in use:");
            while (*p == ' ') p++;
            snprintf(info->gpu_driver, sizeof(info->gpu_driver), "%s", p);
        }
        lines++;
    }
    pclose(pipe);
    if (info->gpu_name[0] == '\0') snprintf(info->gpu_name, sizeof(info->gpu_name), "GPU information unavailable");
    if (info->gpu_driver[0] == '\0') snprintf(info->gpu_driver, sizeof(info->gpu_driver), "Unknown");
}

static void read_cpu_temp(SystemInfo *info)
{
    DIR *dir = opendir("/sys/class/thermal");
    if (!dir) return;
    struct dirent *entry;
    double best = -1.0;
    while ((entry = readdir(dir))) {
        if (strncmp(entry->d_name, "thermal_zone", 11) != 0) continue;
        char path[256];
        snprintf(path, sizeof(path), "/sys/class/thermal/%s/temp", entry->d_name);
        FILE *file = fopen(path, "r");
        if (!file) continue;
        long value = 0;
        if (fscanf(file, "%ld", &value) == 1) {
            double temp = value > 1000 ? value / 1000.0 : (double)value;
            if (temp > best && temp < 120.0) best = temp;
        }
        fclose(file);
    }
    closedir(dir);
    info->cpu_temp_c = best;
}

static void read_battery(SystemInfo *info)
{
    DIR *dir = opendir("/sys/class/power_supply");
    if (!dir) return;
    struct dirent *entry;
    while ((entry = readdir(dir))) {
        if (strncmp(entry->d_name, "BAT", 3) != 0) continue;
        char path[256];
        snprintf(path, sizeof(path), "/sys/class/power_supply/%s/capacity", entry->d_name);
        FILE *file = fopen(path, "r");
        if (file) {
            int percent;
            if (fscanf(file, "%d", &percent) == 1) {
                info->battery_available = 1;
                info->battery_percent = percent;
            }
            fclose(file);
        }
        snprintf(path, sizeof(path), "/sys/class/power_supply/%s/status", entry->d_name);
        file = fopen(path, "r");
        if (file) {
            char status[64];
            if (fgets(status, sizeof(status), file)) {
                status[strcspn(status, "\r\n")] = '\0';
                info->battery_charging = strcmp(status, "Charging") == 0 || strcmp(status, "Full") == 0;
            }
            fclose(file);
        }
        break;
    }
    closedir(dir);
}

static void read_network(SystemInfo *info)
{
    FILE *file = fopen("/proc/net/dev", "r");
    if (!file) return;
    char line[512];
    uint64_t best_rx = 0, best_tx = 0;
    char best_if[64] = "";
    while (fgets(line, sizeof(line), file)) {
        char iface[64];
        unsigned long long rx = 0, tx = 0;
        if (sscanf(line, " %63[^:]: %llu %*u %*u %*u %*u %*u %*u %*u %llu", iface, &rx, &tx) == 3) {
            if (strcmp(iface, "lo") == 0) continue;
            char state_path[256];
            snprintf(state_path, sizeof(state_path), "/sys/class/net/%s/operstate", iface);
            FILE *state_file = fopen(state_path, "r");
            char state[32] = "";
            if (state_file) { fgets(state, sizeof(state), state_file); fclose(state_file); }
            if (strncmp(state, "up", 2) != 0) continue;
            snprintf(best_if, sizeof(best_if), "%s", iface);
            best_rx = rx;
            best_tx = tx;
            break;
        }
    }
    fclose(file);

    if (best_if[0] == '\0') return;
    snprintf(info->network_interface, sizeof(info->network_interface), "%s", best_if);

    struct ifaddrs *ifaddr = NULL;
    if (getifaddrs(&ifaddr) == 0) {
        for (struct ifaddrs *ifa = ifaddr; ifa; ifa = ifa->ifa_next) {
            if (!ifa->ifa_addr || strcmp(ifa->ifa_name, best_if) != 0) continue;
            if (ifa->ifa_addr->sa_family == AF_INET) {
                struct sockaddr_in *addr = (struct sockaddr_in *)ifa->ifa_addr;
                if (inet_ntop(AF_INET, &addr->sin_addr, info->network_ip, sizeof(info->network_ip))) break;
            }
        }
        freeifaddrs(ifaddr);
    }

    struct timespec now;
    clock_gettime(CLOCK_MONOTONIC, &now);
    double elapsed = 0.0;
    if (previous_net_time.tv_sec != 0)
        elapsed = (now.tv_sec - previous_net_time.tv_sec) + (now.tv_nsec - previous_net_time.tv_nsec) / 1e9;
    if (elapsed > 0.05) {
        info->rx_kbps = ((double)(best_rx - previous_rx) / 1024.0) / elapsed;
        info->tx_kbps = ((double)(best_tx - previous_tx) / 1024.0) / elapsed;
    }
    previous_rx = best_rx;
    previous_tx = best_tx;
    previous_net_time = now;
}

int system_info_read(SystemInfo *info)
{
    if (!info) return 0;
    memset(info, 0, sizeof(*info));
    int ok = 1;
    if (!read_cpu(info)) ok = 0;
    if (!read_memory(info)) ok = 0;
    if (!read_uptime(info)) ok = 0;
    read_kernel_hostname(info);
    read_root_storage(info);
    read_gpu(info);
    read_cpu_temp(info);
    read_battery(info);
    read_network(info);
    return ok;
}
