#ifndef NEBULA_STORAGE_H
#define NEBULA_STORAGE_H

#include <glib.h>

typedef struct {
    char mount[256];
    char filesystem[128];
    unsigned long long total_bytes;
    unsigned long long used_bytes;
    double usage;
} StorageInfo;

GPtrArray *storage_list_mounts(void);
void storage_free_list(GPtrArray *list);

#endif
