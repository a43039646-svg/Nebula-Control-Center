#ifndef NEBULA_SYSTEM_INFO_H
#define NEBULA_SYSTEM_INFO_H

#include <stdint.h>

typedef struct {
    double cpu_usage;
    double memory_usage;
    uint64_t memory_used_mb;
    uint64_t memory_total_mb;
    uint64_t uptime_seconds;
    double load1;
    double load5;
    unsigned cpu_cores;

    char cpu_model[256];
    char gpu_name[256];
    char gpu_driver[128];
    double cpu_temp_c;

    char distro[128];
    char kernel[128];
    char hostname[128];

    uint64_t root_total_bytes;
    uint64_t root_used_bytes;
    double root_usage;

    char network_interface[64];
    char network_ip[64];
    double rx_kbps;
    double tx_kbps;

    int battery_percent;
    int battery_available;
    int battery_charging;
} SystemInfo;

int system_info_read(SystemInfo *info);

#endif
