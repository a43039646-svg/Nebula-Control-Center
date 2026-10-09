#include "doctor.h"
#include "i18n.h"

#include <stdio.h>
#include <string.h>
#include <sys/statvfs.h>
#include <unistd.h>

static DoctorCheck *check_new(const char *name, const char *status, const char *detail)
{
    DoctorCheck *c = g_new0(DoctorCheck, 1);
    c->name = g_strdup(name);
    c->status = g_strdup(status);
    c->detail = g_strdup(detail);
    return c;
}

static gboolean command_exists(const char *cmd)
{
    gchar *path = g_find_program_in_path(cmd);
    gboolean ok = path != NULL;
    g_free(path);
    return ok;
}

static int failed_service_count(void)
{
    if (!command_exists("systemctl"))
        return -1;

    gchar *stdout_data = NULL;
    gchar *stderr_data = NULL;
    gint status = 0;
    if (!g_spawn_command_line_sync(
            "systemctl --failed --no-legend --plain",
            &stdout_data, &stderr_data, &status, NULL) || status != 0) {
        g_free(stdout_data);
        g_free(stderr_data);
        return -1;
    }

    int count = 0;
    if (stdout_data) {
        gchar **lines = g_strsplit(stdout_data, "\n", -1);
        for (guint i = 0; lines[i]; i++)
            if (lines[i][0] != '\0')
                count++;
        g_strfreev(lines);
    }

    g_free(stdout_data);
    g_free(stderr_data);
    return count;
}

GPtrArray *doctor_run(void)
{
    GPtrArray *checks = g_ptr_array_new();

    g_ptr_array_add(checks, check_new(
        i18n_get("Proc filesystem"),
        g_file_test("/proc", G_FILE_TEST_IS_DIR) ? i18n_get("OK") : i18n_get("FAIL"),
        g_file_test("/proc", G_FILE_TEST_IS_DIR) ? i18n_get("/proc is available.") : i18n_get("/proc is missing.")
    ));

    g_ptr_array_add(checks, check_new(
        i18n_get("Sysfs"),
        g_file_test("/sys", G_FILE_TEST_IS_DIR) ? i18n_get("OK") : i18n_get("FAIL"),
        g_file_test("/sys", G_FILE_TEST_IS_DIR) ? i18n_get("/sys is available.") : i18n_get("/sys is missing.")
    ));

    struct statvfs fs;
    if (statvfs("/", &fs) == 0) {
        guint64 total = (guint64)fs.f_blocks * fs.f_frsize;
        guint64 free = (guint64)fs.f_bavail * fs.f_frsize;
        double usage = total ? 100.0 * (double)(total - free) / (double)total : 0.0;
        const char *status = usage >= 95.0 ? i18n_get("FAIL") : (usage >= 85.0 ? "WARN" : i18n_get("OK"));
        char detail[128];
        snprintf(detail, sizeof detail, i18n_get("Root filesystem usage: %.1f%%."), usage);
        g_ptr_array_add(checks, check_new(i18n_get("Root disk"), status, detail));
    }

    const char *pkg =
        command_exists("apt") ? "apt" :
        command_exists("pacman") ? "pacman" :
        command_exists("dnf") ? "dnf" :
        command_exists("yum") ? "yum" :
        command_exists("zypper") ? "zypper" :
        command_exists("xbps-install") ? "xbps-install" :
        command_exists("emerge") ? "emerge" :
        command_exists("apk") ? "apk" :
        NULL;

    g_ptr_array_add(checks, check_new(
        i18n_get("Package manager"),
        pkg ? i18n_get("OK") : "WARN",
        pkg ? pkg : i18n_get("No supported package manager detected.")
    ));

    int failed = failed_service_count();
    if (failed >= 0) {
        char detail[128];
        snprintf(detail, sizeof detail, i18n_get("%d failed service(s) reported by systemd."), failed);
        g_ptr_array_add(checks, check_new(
            i18n_get("systemd services"),
            failed == 0 ? i18n_get("OK") : "WARN",
            detail
        ));
    } else if (command_exists("rc-status")) {
        gchar *out = NULL;
        gint exit_status = 0;
        gboolean ran = g_spawn_command_line_sync("rc-status --all", &out, NULL, &exit_status, NULL);
        gboolean ok = ran && exit_status == 0;
        g_ptr_array_add(checks, check_new(
            i18n_get("OpenRC services"),
            ok ? i18n_get("OK") : "WARN",
            ok ? "OpenRC status is available; see Services for individual entries." :
                 "OpenRC was detected, but rc-status could not report service status."
        ));
        g_free(out);
    } else if (command_exists("sv")) {
        g_ptr_array_add(checks, check_new(
            i18n_get("runit services"),
            i18n_get("OK"),
            "runit was detected; see Services for individual service status."
        ));
    } else {
        g_ptr_array_add(checks, check_new(
            i18n_get("Service manager"),
            "WARN",
            "No supported service manager (systemd, OpenRC or runit) was detected."
        ));
    }

    g_ptr_array_add(checks, check_new(
        i18n_get("Internet interface"),
        command_exists("ip") ? i18n_get("OK") : "WARN",
        command_exists("ip") ? i18n_get("ip command is available.") : i18n_get("ip command is not installed.")
    ));

    g_ptr_array_add(checks, check_new(
        i18n_get("Administrator process authentication"),
        command_exists("pkexec") ? i18n_get("OK") : "WARN",
        command_exists("pkexec") ? "pkexec is available." :
            "pkexec is required for authenticated termination of protected processes."
    ));

    return checks;
}

void doctor_free_list(GPtrArray *checks)
{
    if (!checks) return;
    for (guint i = 0; i < checks->len; i++) {
        DoctorCheck *c = g_ptr_array_index(checks, i);
        g_free(c->name);
        g_free(c->status);
        g_free(c->detail);
        g_free(c);
    }
    g_ptr_array_free(checks, TRUE);
}
