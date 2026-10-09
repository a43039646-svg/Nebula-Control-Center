#include "permissions.h"
#include "i18n.h"

#include <unistd.h>

static PermissionCheck *new_check(const char *name, const char *status, const char *detail)
{
    PermissionCheck *c = g_new0(PermissionCheck, 1);
    c->name = g_strdup(name);
    c->status = g_strdup(status);
    c->detail = g_strdup(detail);
    return c;
}

static gboolean exists(const char *path)
{
    return g_file_test(path, G_FILE_TEST_EXISTS);
}

GPtrArray *permissions_check(void)
{
    GPtrArray *checks = g_ptr_array_new();

    g_ptr_array_add(checks, new_check(
        i18n_get("Administrator"),
        geteuid() == 0 ? i18n_get("YES") : i18n_get("NO"),
        geteuid() == 0 ? i18n_get("Running as root.") : i18n_get("Running as the normal user.")
    ));

    g_ptr_array_add(checks, new_check(
        i18n_get("/proc"),
        exists(i18n_get("/proc")) ? "OK" : "FAIL",
        i18n_get("Process information access.")
    ));

    g_ptr_array_add(checks, new_check(
        i18n_get("/sys"),
        exists(i18n_get("/sys")) ? "OK" : "FAIL",
        i18n_get("Hardware and kernel interface access.")
    ));

    gchar *pkexec = g_find_program_in_path("pkexec");
    gchar *sudo = g_find_program_in_path("sudo");
    g_ptr_array_add(checks, new_check(
        i18n_get("Admin helper"),
        pkexec ? "pkexec" : (sudo ? "sudo only" : i18n_get("NONE")),
        pkexec ? i18n_get("PolicyKit authentication is available.") :
        (sudo ? "sudo is installed, but protected-process authentication requires pkexec." :
                "pkexec is not installed; protected-process authentication is unavailable.")
    ));
    g_free(pkexec);
    g_free(sudo);

    gchar *systemctl = g_find_program_in_path("systemctl");
    gchar *rc_status = g_find_program_in_path("rc-status");
    gchar *sv = g_find_program_in_path("sv");
    const char *service_manager = systemctl ? "systemd" :
        (rc_status ? "OpenRC" : (sv ? "runit" : NULL));
    g_ptr_array_add(checks, new_check(
        i18n_get("Service manager"),
        service_manager ? service_manager : i18n_get("NONE"),
        service_manager ? i18n_get("A supported service manager was detected.") :
                          i18n_get("systemd, OpenRC or runit tools were not detected.")
    ));
    g_free(systemctl);
    g_free(rc_status);
    g_free(sv);

    return checks;
}

void permissions_free_list(GPtrArray *checks)
{
    if (!checks) return;
    for (guint i = 0; i < checks->len; i++) {
        PermissionCheck *c = g_ptr_array_index(checks, i);
        g_free(c->name);
        g_free(c->status);
        g_free(c->detail);
        g_free(c);
    }
    g_ptr_array_free(checks, TRUE);
}
