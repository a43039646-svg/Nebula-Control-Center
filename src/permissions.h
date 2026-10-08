#ifndef NEBULA_PERMISSIONS_H
#define NEBULA_PERMISSIONS_H

#include <glib.h>

typedef struct {
    char *name;
    char *status;
    char *detail;
} PermissionCheck;

GPtrArray *permissions_check(void);
void permissions_free_list(GPtrArray *checks);

#endif
