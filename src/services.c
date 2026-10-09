#include "services.h"

#include <stdio.h>
#include <string.h>
#include <sys/wait.h>

static char active_service_manager[16] = "";

const char *services_manager_name(void)
{
    return active_service_manager[0] ? active_service_manager : NULL;
}

static gboolean command_exists(const char *command)
{
    gchar *path = g_find_program_in_path(command);
    gboolean found = path != NULL;
    g_free(path);
    return found;
}

static void add_service(GPtrArray *list, const char *name, const char *state)
{
    if (!name || !*name || list->len >= 80)
        return;
    ServiceInfo *info = g_new0(ServiceInfo, 1);
    g_strlcpy(info->name, name, sizeof(info->name));
    g_strlcpy(info->state, state && *state ? state : "unknown", sizeof(info->state));
    g_ptr_array_add(list, info);
}

static gboolean command_succeeded(int status)
{
    return WIFEXITED(status) && WEXITSTATUS(status) == 0;
}

static gboolean read_systemd_services(GPtrArray *list)
{
    FILE *pipe = popen("systemctl list-units --type=service --state=running --no-legend --no-pager 2>/dev/null", "r");
    if (!pipe)
        return FALSE;

    char line[512];
    while (fgets(line, sizeof(line), pipe) && list->len < 80) {
        char unit[256] = {0}, load[64] = {0}, active[64] = {0}, sub[64] = {0};
        if (sscanf(line, "%255s %63s %63s %63s", unit, load, active, sub) >= 4) {
            char state[64];
            (void)snprintf(state, sizeof(state), "%s / %s", active, sub);
            add_service(list, unit, state);
        }
    }
    int status = pclose(pipe);
    return command_succeeded(status) || list->len > 0;
}

static gboolean read_openrc_services(GPtrArray *list)
{
    FILE *pipe = popen("rc-status --all 2>/dev/null", "r");
    if (!pipe)
        return FALSE;

    char line[512];
    while (fgets(line, sizeof(line), pipe) && list->len < 80) {
        char *text = g_strstrip(line);
        if (!*text || g_str_has_prefix(text, "Runlevel:"))
            continue;

        char *open_bracket = strchr(text, '[');
        char *close_bracket = open_bracket ? strchr(open_bracket + 1, ']') : NULL;
        if (!open_bracket || !close_bracket)
            continue;

        *open_bracket = '\0';
        *close_bracket = '\0';
        char *name = g_strstrip(text);
        char *state = g_strstrip(open_bracket + 1);
        if (*name)
            add_service(list, name, state);
    }

    int status = pclose(pipe);
    return command_succeeded(status) || list->len > 0;
}

static gboolean read_runit_services(GPtrArray *list)
{
    /* Fixed command, no user input is interpolated into the shell expression. */
    const char *command =
        "for base in /var/service /etc/service /run/runit/service; do "
        "if [ -d \"$base\" ]; then "
        "for service in \"$base\"/*; do "
        "[ -e \"$service\" ] || continue; sv status \"$service\" 2>/dev/null; "
        "done; exit 0; fi; done; exit 1";
    FILE *pipe = popen(command, "r");
    if (!pipe)
        return FALSE;

    char line[512];
    while (fgets(line, sizeof(line), pipe) && list->len < 80) {
        char status_name[64] = {0}, service_name[256] = {0};
        if (sscanf(line, "%63[^:]: %255[^:]:", status_name, service_name) == 2) {
            char state[64];
            (void)snprintf(state, sizeof(state), "runit / %s", status_name);
            add_service(list, service_name, state);
        }
    }

    int status = pclose(pipe);
    return command_succeeded(status) || list->len > 0;
}

GPtrArray *services_list_running(void)
{
    GPtrArray *list = g_ptr_array_new_with_free_func(g_free);
    active_service_manager[0] = '\0';

    /* Prefer the native manager, but fall back if systemctl exists and is unusable. */
    if (command_exists("systemctl") && read_systemd_services(list)) {
        g_strlcpy(active_service_manager, "systemd", sizeof(active_service_manager));
        return list;
    }

    g_ptr_array_set_size(list, 0);
    if (command_exists("rc-status") && read_openrc_services(list)) {
        g_strlcpy(active_service_manager, "OpenRC", sizeof(active_service_manager));
        return list;
    }

    g_ptr_array_set_size(list, 0);
    if (command_exists("sv") && read_runit_services(list)) {
        g_strlcpy(active_service_manager, "runit", sizeof(active_service_manager));
        return list;
    }

    g_ptr_array_set_size(list, 0);
    return list;
}

void services_free_list(GPtrArray *list)
{
    if (list)
        g_ptr_array_free(list, TRUE);
}
