#include "services.h"

#include <stdio.h>
#include <string.h>

GPtrArray *services_list_running(void)
{
    GPtrArray *list = g_ptr_array_new_with_free_func(g_free);
    FILE *pipe = popen("command -v systemctl >/dev/null 2>&1 && systemctl list-units --type=service --state=running --no-legend --no-pager 2>/dev/null", "r");
    if (!pipe) return list;

    char line[512];
    while (fgets(line, sizeof(line), pipe) && list->len < 80) {
        char unit[256] = {0}, load[64] = {0}, active[64] = {0}, sub[64] = {0};
        if (sscanf(line, "%255s %63s %63s %63s", unit, load, active, sub) >= 4) {
            ServiceInfo *info = g_new0(ServiceInfo, 1);
            snprintf(info->name, sizeof(info->name), "%s", unit);
            snprintf(info->state, sizeof(info->state), "%s / %s", active, sub);
            g_ptr_array_add(list, info);
        }
    }
    pclose(pipe);
    return list;
}

void services_free_list(GPtrArray *list)
{
    if (list) g_ptr_array_free(list, TRUE);
}
