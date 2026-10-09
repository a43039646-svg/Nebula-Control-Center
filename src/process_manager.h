#ifndef NEBULA_PROCESS_MANAGER_H
#define NEBULA_PROCESS_MANAGER_H

#include <glib.h>
#include <stdint.h>
#include <sys/types.h>

typedef struct {
    pid_t pid;
    char name[256];
    double cpu_percent;
    uint64_t memory_mb;
    char state;
    uint64_t start_time_ticks;
} ProcessInfo;

GPtrArray *process_manager_list(void);
int process_manager_terminate(pid_t pid, uint64_t expected_start_time_ticks);
void process_manager_free_list(GPtrArray *list);

#endif
