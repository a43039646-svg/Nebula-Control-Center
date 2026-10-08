#include "gaming.h"

#include <stdio.h>
#include <string.h>

static gboolean command_exists(const char *cmd)
{
    gchar *path = g_find_program_in_path(cmd);
    gboolean ok = path != NULL;
    g_free(path);
    return ok;
}

static gboolean run_simple(const char *command)
{
    gint status = 0;
    return g_spawn_command_line_sync(command, NULL, NULL, &status, NULL) && status == 0;
}

GamingStatus gaming_status(void)
{
    GamingStatus result = {0};
    result.gamemode = command_exists("gamemoded");
    result.mangohud = command_exists("mangohud");
    result.powerprofiles = command_exists("powerprofilesctl");

    if (result.powerprofiles) {
        gchar *out = NULL;
        gint status = 0;
        if (g_spawn_command_line_sync("powerprofilesctl get", &out, NULL, &status, NULL) && status == 0 && out) {
            g_strstrip(out);
            g_strlcpy(result.current_profile, out, sizeof result.current_profile);
        }
        g_free(out);
    }

    return result;
}

gboolean gaming_set_performance(void)
{
    return run_simple("powerprofilesctl set performance");
}

gboolean gaming_set_balanced(void)
{
    return run_simple("powerprofilesctl set balanced");
}
