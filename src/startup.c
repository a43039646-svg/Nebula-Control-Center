#include "startup.h"

#include <stdio.h>
#include <string.h>

static void scan_dir(GPtrArray *entries, const char *dirpath)
{
    GDir *dir = g_dir_open(dirpath, 0, NULL);
    if (!dir) return;

    const gchar *name;
    while ((name = g_dir_read_name(dir))) {
        if (!g_str_has_suffix(name, ".desktop"))
            continue;

        gchar *path = g_build_filename(dirpath, name, NULL);
        gchar *contents = NULL;
        gsize length = 0;
        if (!g_file_get_contents(path, &contents, &length, NULL)) {
            g_free(path);
            continue;
        }

        const char *display = name;
        gchar **lines = g_strsplit(contents, "\n", -1);
        for (guint i = 0; lines[i]; i++) {
            if (g_str_has_prefix(lines[i], "Name=")) {
                display = lines[i] + 5;
                break;
            }
        }

        StartupEntry *entry = g_new0(StartupEntry, 1);
        entry->name = g_strdup(display);
        entry->path = path;

        g_ptr_array_add(entries, entry);
        g_strfreev(lines);
        g_free(contents);
    }

    g_dir_close(dir);
}

GPtrArray *startup_list(void)
{
    GPtrArray *entries = g_ptr_array_new();
    gchar *user_dir = g_build_filename(g_get_user_config_dir(), "autostart", NULL);

    scan_dir(entries, user_dir);
    scan_dir(entries, "/etc/xdg/autostart");

    g_free(user_dir);
    return entries;
}

void startup_free_list(GPtrArray *entries)
{
    if (!entries) return;
    for (guint i = 0; i < entries->len; i++) {
        StartupEntry *e = g_ptr_array_index(entries, i);
        g_free(e->name);
        g_free(e->path);
        g_free(e);
    }
    g_ptr_array_free(entries, TRUE);
}
