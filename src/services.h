#ifndef NEBULA_SERVICES_H
#define NEBULA_SERVICES_H

#include <glib.h>

typedef struct {
    char name[256];
    char state[64];
} ServiceInfo;

GPtrArray *services_list_running(void);
const char *services_manager_name(void);
void services_free_list(GPtrArray *list);

#endif
