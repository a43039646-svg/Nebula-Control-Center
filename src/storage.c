#include "storage.h"

#include <mntent.h>
#include <stdio.h>
#include <string.h>
#include <sys/statvfs.h>

static gboolean interesting_fs(const char *type)
{
    static const char *skip[] = {
        "proc", "sysfs", "devtmpfs", "devpts", "tmpfs", "cgroup", "cgroup2", "overlay",
        "squashfs", "pstore", "securityfs", "debugfs", "tracefs", "configfs", "fusectl", "mqueue"
    };
    for (gsize i = 0; i < G_N_ELEMENTS(skip); i++)
        if (strcmp(type, skip[i]) == 0) return FALSE;
    return TRUE;
}

GPtrArray *storage_list_mounts(void)
{
    GPtrArray *list = g_ptr_array_new_with_free_func(g_free);
    FILE *file = setmntent("/proc/mounts", "r");
    if (!file) return list;

    struct mntent *entry;
    while ((entry = getmntent(file))) {
        if (!interesting_fs(entry->mnt_type)) continue;
        struct statvfs fs;
        if (statvfs(entry->mnt_dir, &fs) != 0) continue;
        unsigned long long total = (unsigned long long)fs.f_blocks * fs.f_frsize;
        unsigned long long free = (unsigned long long)fs.f_bavail * fs.f_frsize;
        if (total == 0) continue;
        StorageInfo *info = g_new0(StorageInfo, 1);
        snprintf(info->mount, sizeof(info->mount), "%s", entry->mnt_dir);
        snprintf(info->filesystem, sizeof(info->filesystem), "%s", entry->mnt_type);
        info->total_bytes = total;
        info->used_bytes = total > free ? total - free : 0;
        info->usage = 100.0 * (double)info->used_bytes / (double)total;
        g_ptr_array_add(list, info);
    }
    endmntent(file);
    return list;
}

void storage_free_list(GPtrArray *list)
{
    if (list) g_ptr_array_free(list, TRUE);
}
