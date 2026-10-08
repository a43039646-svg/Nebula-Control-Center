#ifndef NEBULA_STARTUP_H
#define NEBULA_STARTUP_H

#include <glib.h>

typedef struct {
    char *name;
    char *path;
} StartupEntry;

GPtrArray *startup_list(void);
void startup_free_list(GPtrArray *entries);

#endif
