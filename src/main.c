#include <gtk/gtk.h>
#include <stdio.h>
#include <string.h>
#include <unistd.h>
#include <stdlib.h>
#include <locale.h>

#include "system_info.h"
#include "process_manager.h"
#include "storage.h"
#include "network.h"
#include "services.h"
#include "i18n.h"
#include "doctor.h"
#include "startup.h"
#include "gaming.h"
#include "permissions.h"

#define APP_VERSION "2.1.0"

typedef enum {
    PAGE_OVERVIEW,
    PAGE_PROCESSES,
    PAGE_HARDWARE,
    PAGE_SERVICES,
    PAGE_STORAGE,
    PAGE_NETWORK,
    PAGE_POWER,
    PAGE_DOCTOR,
    PAGE_STARTUP,
    PAGE_GAMING,
    PAGE_PERMISSIONS,
    PAGE_CONSOLE,
    PAGE_SETTINGS
} PageId;

typedef struct {
    GtkWidget *window;
    GtkWidget *stack;
    GtkWidget *status_label;

    GtkWidget *cpu_value;
    GtkWidget *cpu_meta;
    GtkWidget *cpu_bar;
    GtkWidget *memory_value;
    GtkWidget *memory_meta;
    GtkWidget *memory_bar;
    GtkWidget *gpu_value;
    GtkWidget *gpu_meta;
    GtkWidget *storage_value;
    GtkWidget *storage_meta;
    GtkWidget *storage_bar;
    GtkWidget *uptime_value;
    GtkWidget *network_value;
    GtkWidget *network_meta;
    GtkWidget *battery_value;

    GtkWidget *process_box;
    GtkWidget *hardware_box;
    GtkWidget *services_box;
    GtkWidget *storage_box;
    GtkWidget *network_box;
    GtkWidget *power_box;
    GtkWidget *doctor_box;
    GtkWidget *startup_box;
    GtkWidget *gaming_box;
    GtkWidget *permissions_box;
    GtkWidget *console_output;
    GtkWidget *console_dropdown;
    GtkWidget *refresh_dropdown;

    guint refresh_source;
    guint refresh_seconds;
    SystemInfo info;
} AppState;

typedef struct {
    GtkWindow *dialog;
    pid_t pid;
} KillContext;

typedef struct {
    char language[8];
    char theme[16];
    char shape[16];
} UiConfig;

static UiConfig ui_config = {"en", "auto", "square"};

static const char *tr(const char *key)
{
    return i18n_get(key);
}

static void detect_system_language(void)
{
    const char *lang = getenv("LANG");
    if (!lang) return;
    if (strncmp(lang, "C", 1) == 0 || strncmp(lang, "POSIX", 5) == 0) {
        g_strlcpy(ui_config.language, "en", sizeof(ui_config.language));
        return;
    }

    char code[8] = {0};
    size_t len = 0;
    while (lang[len] && lang[len] != '_' && lang[len] != '-' && lang[len] != '.' && len < sizeof(code) - 1)
        len++;

    if (len == 0) {
        g_strlcpy(ui_config.language, "en", sizeof(ui_config.language));
        return;
    }

    memcpy(code, lang, len);
    code[len] = '\0';
    g_strlcpy(ui_config.language, code, sizeof(ui_config.language));
}

static void load_ui_config(void)
{
    detect_system_language();
    GKeyFile *key = g_key_file_new();
    GError *error = NULL;
    char *path = g_build_filename(g_get_user_config_dir(), "nebula-control-center", "config.ini", NULL);
    if (g_key_file_load_from_file(key, path, G_KEY_FILE_NONE, &error)) {
        const char *lang = g_key_file_get_string(key, "ui", "language", NULL);
        const char *theme = g_key_file_get_string(key, "ui", "theme", NULL);
        const char *shape = g_key_file_get_string(key, "ui", "shape", NULL);
        if (lang && *lang) g_strlcpy(ui_config.language, lang, sizeof ui_config.language);
        if (theme && *theme) g_strlcpy(ui_config.theme, theme, sizeof ui_config.theme);
        if (shape && *shape) g_strlcpy(ui_config.shape, shape, sizeof ui_config.shape);
        g_free((gpointer)lang); g_free((gpointer)theme); g_free((gpointer)shape);
    }
    g_clear_error(&error);
    g_key_file_unref(key);
    g_free(path);

    i18n_init();
    if (!i18n_set_language(ui_config.language)) {
        g_strlcpy(ui_config.language, "en", sizeof(ui_config.language));
        i18n_set_language("en");
    }
}

static void save_ui_config(void)
{
    GKeyFile *key = g_key_file_new();
    g_key_file_set_string(key, "ui", "language", ui_config.language);
    g_key_file_set_string(key, "ui", "theme", ui_config.theme);
    g_key_file_set_string(key, "ui", "shape", ui_config.shape);
    gsize length = 0;
    GError *error = NULL;
    gchar *data = g_key_file_to_data(key, &length, &error);
    if (data) {
        gchar *dir = g_build_filename(g_get_user_config_dir(), "nebula-control-center", NULL);
        g_mkdir_with_parents(dir, 0700);
        gchar *path = g_build_filename(dir, "config.ini", NULL);
        g_file_set_contents(path, data, (gssize)length, NULL);
        g_free(path); g_free(dir); g_free(data);
    }
    g_clear_error(&error);
    g_key_file_unref(key);
}

static const char *effective_theme(void)
{
    if (strcmp(ui_config.theme, "auto") != 0) return ui_config.theme;
    FILE *f = fopen("/etc/os-release", "r");
    if (!f) return "nebula";
    char line[256];
    static char id[64] = "nebula";
    while (fgets(line, sizeof line, f)) {
        if (strncmp(line, "ID=", 3) == 0) {
            char *v = line + 3;
            v[strcspn(v, "\r\n\"")] = '\0';
            if (strstr(v, "mint")) strcpy(id, "mint");
            else if (strstr(v, "ubuntu")) strcpy(id, "ubuntu");
            else if (strstr(v, "arch")) strcpy(id, "arch");
            else if (strstr(v, "fedora")) strcpy(id, "fedora");
            else if (strstr(v, "debian")) strcpy(id, "debian");
            else if (strstr(v, "manjaro")) strcpy(id, "manjaro");
            else if (strstr(v, "opensuse") || strstr(v, "suse")) strcpy(id, "opensuse");
            else if (strstr(v, "pop")) strcpy(id, "pop");
            else if (strstr(v, "elementary")) strcpy(id, "elementary");
            else if (strstr(v, "zorin")) strcpy(id, "zorin");
            else if (strstr(v, "kali")) strcpy(id, "kali");
            else strcpy(id, "nebula");
            break;
        }
    }
    fclose(f);
    return id;
}

static const char *theme_primary(void)
{
    const char *theme = effective_theme();
    if (strcmp(theme, "mint") == 0) return "#8cc63f";
    if (strcmp(theme, "ubuntu") == 0) return "#e95420";
    if (strcmp(theme, "arch") == 0) return "#1793d1";
    if (strcmp(theme, "fedora") == 0) return "#51a2da";
    if (strcmp(theme, "debian") == 0) return "#d70a53";
    if (strcmp(theme, "manjaro") == 0) return "#35bf5c";
    if (strcmp(theme, "opensuse") == 0) return "#73ba25";
    if (strcmp(theme, "pop") == 0) return "#48b9c7";
    if (strcmp(theme, "elementary") == 0) return "#64b9ef";
    if (strcmp(theme, "zorin") == 0) return "#0b7fab";
    if (strcmp(theme, "kali") == 0) return "#557c94";
    if (strcmp(theme, "ocean") == 0) return "#2bb7d9";
    if (strcmp(theme, "rose") == 0) return "#e86ba5";
    if (strcmp(theme, "amber") == 0) return "#f0a202";
    return "#8f73ff";
}

static const char *shape_radius(void)
{
    if (strcmp(ui_config.shape, "square") == 0) return "2px";
    if (strcmp(ui_config.shape, "soft") == 0) return "8px";
    return "16px";
}

static GtkWidget *label_left(const char *text, const char *klass)
{
    GtkWidget *label = gtk_label_new(text ? text : "");
    gtk_label_set_xalign(GTK_LABEL(label), 0.0f);
    if (klass) gtk_widget_add_css_class(label, klass);
    return label;
}

static void apply_css(void)
{
    const char *primary = theme_primary();
    const char *radius = shape_radius();
    char css[8192];
    snprintf(css, sizeof css,
        "window { background: #070912; color: #f4f2ff; }"
        ".sidebar { background: #0d1020; border-right: 1px solid #252b46; }"
        ".brand { color: #f7f4ff; font-size: 22px; font-weight: 800; letter-spacing: 4px; }"
        ".brand-sub { color: #737b9d; font-size: 11px; letter-spacing: 2px; }"
        ".nav { background: transparent; color: #949dbc; border: 1px solid #252a40; border-radius: %s; padding: 9px 12px; }"
        ".nav:hover { background: #161b2e; color: #ffffff; border-color: %s; }"
        ".nav.active { background: #171c30; color: #ffffff; border-color: %s; box-shadow: inset 3px 0 0 %s; }"
        ".page-title { color: #f4f2ff; font-size: 26px; font-weight: 750; }"
        ".page-subtitle { color: #747e9e; font-size: 13px; }"
        ".card { background: #101424; border: 1px solid #28304e; border-radius: %s; }"
        ".card-title { color: #8e98bb; font-size: 11px; font-weight: 800; letter-spacing: 1.6px; }"
        ".card-value { color: #f7f5ff; font-size: 23px; font-weight: 800; }"
        ".card-meta { color: #717b9d; font-size: 12px; }"
        ".online { color: %s; font-size: 11px; }"
        ".muted { color: #727b9e; font-size: 11px; }"
        ".row { background: #0f1322; border: 1px solid #232a45; border-radius: %s; }"
        ".row-title { color: #eeeaff; font-weight: 700; }"
        ".row-meta { color: #7b85a7; font-size: 12px; }"
        ".danger { background: #3b1d2a; color: #ffb8c5; border-color: #713649; }"
        ".section-title { color: #eeeaff; font-size: 18px; font-weight: 800; }"
        ".setting { background: #101424; border: 1px solid #28304e; border-radius: %s; }""textview { background: #0b0f1b; color: #dce2ff; font-family: monospace; }"
        "progressbar trough { background: #1a2035; min-height: 8px; border-radius: 4px; }"
        "progressbar progress { background: %s; min-height: 8px; border-radius: 4px; }",
        radius, primary, primary, primary, radius, primary, radius, radius, primary);

    GtkCssProvider *provider = gtk_css_provider_new();
    gtk_css_provider_load_from_string(provider, css);
    gtk_style_context_add_provider_for_display(gdk_display_get_default(),
                                                GTK_STYLE_PROVIDER(provider),
                                                GTK_STYLE_PROVIDER_PRIORITY_APPLICATION);
    g_object_unref(provider);
}

static GtkWidget *card_new(const char *title, GtkWidget **value, GtkWidget **meta, GtkWidget **bar)
{
    GtkWidget *frame = gtk_frame_new(NULL);
    gtk_widget_add_css_class(frame, "card");

    GtkWidget *box = gtk_box_new(GTK_ORIENTATION_VERTICAL, 8);
    gtk_widget_set_margin_start(box, 18);
    gtk_widget_set_margin_end(box, 18);
    gtk_widget_set_margin_top(box, 18);
    gtk_widget_set_margin_bottom(box, 18);
    gtk_frame_set_child(GTK_FRAME(frame), box);

    gtk_box_append(GTK_BOX(box), label_left(title, "card-title"));

    GtkWidget *value_label = label_left("--", "card-value");
    GtkWidget *meta_label = label_left("--", "card-meta");
    gtk_label_set_ellipsize(GTK_LABEL(value_label), PANGO_ELLIPSIZE_END);
    gtk_label_set_ellipsize(GTK_LABEL(meta_label), PANGO_ELLIPSIZE_END);
    gtk_label_set_max_width_chars(GTK_LABEL(value_label), 28);
    gtk_label_set_max_width_chars(GTK_LABEL(meta_label), 34);
    gtk_widget_set_hexpand(value_label, TRUE);
    gtk_widget_set_hexpand(meta_label, TRUE);
    gtk_box_append(GTK_BOX(box), value_label);
    gtk_box_append(GTK_BOX(box), meta_label);

    if (bar) {
        GtkWidget *progress = gtk_progress_bar_new();
        gtk_progress_bar_set_fraction(GTK_PROGRESS_BAR(progress), 0.0);
        gtk_widget_set_margin_top(progress, 3);
        gtk_box_append(GTK_BOX(box), progress);
        *bar = progress;
    }

    if (value) *value = value_label;
    if (meta) *meta = meta_label;
    return frame;
}

static GtkWidget *page_box(void)
{
    GtkWidget *box = gtk_box_new(GTK_ORIENTATION_VERTICAL, 12);
    gtk_widget_set_margin_start(box, 30);
    gtk_widget_set_margin_end(box, 30);
    gtk_widget_set_margin_top(box, 26);
    gtk_widget_set_margin_bottom(box, 26);
    gtk_widget_set_hexpand(box, TRUE);
    gtk_widget_set_vexpand(box, TRUE);
    return box;
}

static void add_header(GtkWidget *box, const char *title, const char *subtitle)
{
    gtk_box_append(GTK_BOX(box), label_left(tr(title), "page-title"));
    gtk_box_append(GTK_BOX(box), label_left(tr(subtitle), "page-subtitle"));
}

static GtkWidget *scroll_page(GtkWidget **inner_out)
{
    GtkWidget *scroll = gtk_scrolled_window_new();
    gtk_scrolled_window_set_policy(GTK_SCROLLED_WINDOW(scroll), GTK_POLICY_NEVER, GTK_POLICY_AUTOMATIC);
    gtk_widget_set_vexpand(scroll, TRUE);

    GtkWidget *inner = gtk_box_new(GTK_ORIENTATION_VERTICAL, 10);
    gtk_scrolled_window_set_child(GTK_SCROLLED_WINDOW(scroll), inner);
    *inner_out = inner;
    return scroll;
}

static void clear_box(GtkWidget *box)
{
    GtkWidget *child = gtk_widget_get_first_child(box);
    while (child) {
        GtkWidget *next = gtk_widget_get_next_sibling(child);
        gtk_box_remove(GTK_BOX(box), child);
        child = next;
    }
}

static GtkWidget *info_row(const char *title, const char *meta)
{
    title = tr(title);
    GtkWidget *frame = gtk_frame_new(NULL);
    gtk_widget_add_css_class(frame, "row");

    GtkWidget *box = gtk_box_new(GTK_ORIENTATION_HORIZONTAL, 14);
    gtk_widget_set_margin_start(box, 14);
    gtk_widget_set_margin_end(box, 14);
    gtk_widget_set_margin_top(box, 12);
    gtk_widget_set_margin_bottom(box, 12);
    gtk_frame_set_child(GTK_FRAME(frame), box);

    GtkWidget *left = label_left(title, "row-title");
    GtkWidget *right = label_left(meta, "row-meta");
    gtk_widget_set_hexpand(left, TRUE);
    gtk_box_append(GTK_BOX(box), left);
    gtk_box_append(GTK_BOX(box), right);
    return frame;
}

static char *format_bytes(uint64_t bytes)
{
    double value = (double)bytes;
    if (value >= 1024.0 * 1024.0 * 1024.0)
        return g_strdup_printf("%.1f GB", value / (1024.0 * 1024.0 * 1024.0));
    if (value >= 1024.0 * 1024.0)
        return g_strdup_printf("%.1f MB", value / (1024.0 * 1024.0));
    return g_strdup_printf("%.0f KB", value / 1024.0);
}

static char *format_uptime(uint64_t seconds)
{
    uint64_t days = seconds / 86400;
    seconds %= 86400;
    uint64_t hours = seconds / 3600;
    seconds %= 3600;
    uint64_t minutes = seconds / 60;

    if (days > 0)
        return g_strdup_printf("%llud %lluh %llum",
                               (unsigned long long)days,
                               (unsigned long long)hours,
                               (unsigned long long)minutes);
    return g_strdup_printf("%lluh %llum",
                           (unsigned long long)hours,
                           (unsigned long long)minutes);
}

static void update_overview(AppState *state)
{
    if (!system_info_read(&state->info)) {
        gtk_label_set_text(GTK_LABEL(state->status_label), "SYSTEM DATA ERROR");
        return;
    }

    char *cpu = g_strdup_printf("%.1f%%", state->info.cpu_usage);
    char *cpu_meta = g_strdup_printf("%u logical CPUs", state->info.cpu_cores);
    char *mem = g_strdup_printf("%.1f%%", state->info.memory_usage);
    char *mem_meta = g_strdup_printf("%llu / %llu MB",
        (unsigned long long)state->info.memory_used_mb,
        (unsigned long long)state->info.memory_total_mb);
    char *gpu = g_strdup(state->info.gpu_name[0] ? state->info.gpu_name : "Unavailable");
    char *gpu_meta = g_strdup_printf("Driver: %s",
        state->info.gpu_driver[0] ? state->info.gpu_driver : "Unknown");
    char *storage = g_strdup_printf("%.1f%%", state->info.root_usage);
    char *used = format_bytes(state->info.root_used_bytes);
    char *total = format_bytes(state->info.root_total_bytes);
    char *storage_meta = g_strdup_printf("%s / %s", used, total);
    char *uptime = format_uptime(state->info.uptime_seconds);
    char *network = g_strdup(state->info.network_interface[0] ? state->info.network_interface : "Offline");
    char *network_meta = g_strdup_printf("%s  RX %.1f KB/s  TX %.1f KB/s",
        state->info.network_ip[0] ? state->info.network_ip : "No IPv4",
        state->info.rx_kbps,
        state->info.tx_kbps);
    char *battery = state->info.battery_available
        ? g_strdup_printf("%d%%", state->info.battery_percent)
        : g_strdup("AC / desktop");

    gtk_label_set_text(GTK_LABEL(state->cpu_value), cpu);
    gtk_label_set_text(GTK_LABEL(state->cpu_meta), cpu_meta);
    gtk_progress_bar_set_fraction(GTK_PROGRESS_BAR(state->cpu_bar), state->info.cpu_usage / 100.0);

    gtk_label_set_text(GTK_LABEL(state->memory_value), mem);
    gtk_label_set_text(GTK_LABEL(state->memory_meta), mem_meta);
    gtk_progress_bar_set_fraction(GTK_PROGRESS_BAR(state->memory_bar), state->info.memory_usage / 100.0);

    gtk_label_set_text(GTK_LABEL(state->gpu_value), gpu);
    gtk_label_set_text(GTK_LABEL(state->gpu_meta), gpu_meta);

    gtk_label_set_text(GTK_LABEL(state->storage_value), storage);
    gtk_label_set_text(GTK_LABEL(state->storage_meta), storage_meta);
    gtk_progress_bar_set_fraction(GTK_PROGRESS_BAR(state->storage_bar), state->info.root_usage / 100.0);

    gtk_label_set_text(GTK_LABEL(state->uptime_value), uptime);
    gtk_label_set_text(GTK_LABEL(state->network_value), network);
    gtk_label_set_text(GTK_LABEL(state->network_meta), network_meta);
    gtk_label_set_text(GTK_LABEL(state->battery_value), battery);
    gtk_label_set_text(GTK_LABEL(state->status_label), "SYSTEM ONLINE");

    g_free(cpu); g_free(cpu_meta); g_free(mem); g_free(mem_meta);
    g_free(gpu); g_free(gpu_meta); g_free(storage); g_free(used); g_free(total);
    g_free(storage_meta); g_free(uptime); g_free(network); g_free(network_meta); g_free(battery);
}

static void render_hardware(AppState *state)
{
    clear_box(state->hardware_box);

    char temp[64];
    if (state->info.cpu_temp_c > 0.0)
        snprintf(temp, sizeof(temp), "%.1f C", state->info.cpu_temp_c);
    else
        snprintf(temp, sizeof(temp), "Unavailable");

    char cores[64];
    snprintf(cores, sizeof(cores), "%u logical cores", state->info.cpu_cores);

    gtk_box_append(GTK_BOX(state->hardware_box), info_row("CPU", state->info.cpu_model[0] ? state->info.cpu_model : "Unknown"));
    gtk_box_append(GTK_BOX(state->hardware_box), info_row("Cores", cores));
    gtk_box_append(GTK_BOX(state->hardware_box), info_row("GPU", state->info.gpu_name));
    gtk_box_append(GTK_BOX(state->hardware_box), info_row("GPU driver", state->info.gpu_driver));
    gtk_box_append(GTK_BOX(state->hardware_box), info_row("CPU temperature", temp));
    gtk_box_append(GTK_BOX(state->hardware_box), info_row("Kernel", state->info.kernel));
    gtk_box_append(GTK_BOX(state->hardware_box), info_row("Distribution", state->info.distro));
    gtk_box_append(GTK_BOX(state->hardware_box), info_row("Hostname", state->info.hostname));
}

static void render_storage(AppState *state)
{
    clear_box(state->storage_box);

    GPtrArray *mounts = storage_list_mounts();
    guint shown = 0;

    for (guint i = 0; i < mounts->len && shown < 40; i++) {
        StorageInfo *mount = g_ptr_array_index(mounts, i);
        char *used = format_bytes(mount->used_bytes);
        char *total = format_bytes(mount->total_bytes);
        char *meta = g_strdup_printf("%s  |  %s / %s  |  %.1f%%",
                                     mount->filesystem, used, total, mount->usage);
        gtk_box_append(GTK_BOX(state->storage_box), info_row(mount->mount, meta));
        g_free(used); g_free(total); g_free(meta);
        shown++;
    }

    if (shown == 0)
        gtk_box_append(GTK_BOX(state->storage_box), info_row("No filesystems found", ""));

    storage_free_list(mounts);
}

static void render_network(AppState *state)
{
    clear_box(state->network_box);

    GPtrArray *interfaces = network_list();
    for (guint i = 0; i < interfaces->len; i++) {
        NetworkInfo *network = g_ptr_array_index(interfaces, i);
        char *meta = g_strdup_printf("%s  |  IP %s  |  RX %llu B  |  TX %llu B",
                                     network->state[0] ? network->state : "unknown",
                                     network->address[0] ? network->address : "-",
                                     network->rx_bytes,
                                     network->tx_bytes);
        gtk_box_append(GTK_BOX(state->network_box), info_row(network->name, meta));
        g_free(meta);
    }
    network_free_list(interfaces);

    if (state->info.network_interface[0]) {
        char *meta = g_strdup_printf("%s  |  %s  |  RX %.1f KB/s  |  TX %.1f KB/s",
                                     state->info.network_ip[0] ? state->info.network_ip : "No IPv4",
                                     state->info.network_interface,
                                     state->info.rx_kbps,
                                     state->info.tx_kbps);
        gtk_box_append(GTK_BOX(state->network_box), info_row("Current traffic", meta));
        g_free(meta);
    }
}

static void render_power(AppState *state)
{
    clear_box(state->power_box);

    if (state->info.battery_available) {
        char *meta = g_strdup_printf("%s", state->info.battery_charging ? "Charging / AC connected" : "Discharging");
        char *value = g_strdup_printf("%d%%", state->info.battery_percent);
        gtk_box_append(GTK_BOX(state->power_box), info_row("Battery", value));
        gtk_box_append(GTK_BOX(state->power_box), info_row("Status", meta));
        g_free(value);
        g_free(meta);
    } else {
        gtk_box_append(GTK_BOX(state->power_box), info_row("Battery", "No battery detected"));
    }

    char *load = g_strdup_printf("1 min %.2f  |  5 min %.2f  |  %u CPUs",
                                 state->info.load1, state->info.load5, state->info.cpu_cores);
    gtk_box_append(GTK_BOX(state->power_box), info_row("Load average", load));
    g_free(load);

    char *uptime = format_uptime(state->info.uptime_seconds);
    gtk_box_append(GTK_BOX(state->power_box), info_row("Uptime", uptime));
    g_free(uptime);
}

static void render_services(AppState *state)
{
    clear_box(state->services_box);

    GPtrArray *services = services_list_running();
    for (guint i = 0; i < services->len; i++) {
        ServiceInfo *service = g_ptr_array_index(services, i);
        gtk_box_append(GTK_BOX(state->services_box), info_row(service->name, service->state));
    }

    if (services->len == 0)
        gtk_box_append(GTK_BOX(state->services_box), info_row("No running services found", "systemctl unavailable or none detected"));

    services_free_list(services);
}

static void kill_dialog_response(GtkButton *button, gpointer user_data)
{
    (void)button;
    KillContext *ctx = user_data;
    process_manager_terminate(ctx->pid);
    gtk_window_close(ctx->dialog);
    g_free(ctx);
}

static void kill_dialog_cancel(GtkButton *button, gpointer user_data)
{
    (void)button;
    GtkWindow *dialog = user_data;
    gtk_window_close(dialog);
}

static void on_kill_clicked(GtkButton *button, gpointer user_data)
{
    AppState *state = user_data;
    pid_t pid = (pid_t)GPOINTER_TO_INT(g_object_get_data(G_OBJECT(button), "pid"));

    char path[256];
    snprintf(path, sizeof(path), "/proc/%d/comm", pid);
    char name[256] = "this process";
    FILE *file = fopen(path, "r");
    if (file) {
        if (fgets(name, sizeof(name), file))
            name[strcspn(name, "\r\n")] = '\0';
        fclose(file);
    }

    GtkWidget *dialog = gtk_window_new();
    gtk_window_set_transient_for(GTK_WINDOW(dialog), GTK_WINDOW(state->window));
    gtk_window_set_modal(GTK_WINDOW(dialog), TRUE);
    gtk_window_set_title(GTK_WINDOW(dialog), tr("Terminate process"));
    gtk_window_set_default_size(GTK_WINDOW(dialog), 420, 170);

    GtkWidget *box = gtk_box_new(GTK_ORIENTATION_VERTICAL, 14);
    gtk_widget_set_margin_start(box, 22);
    gtk_widget_set_margin_end(box, 22);
    gtk_widget_set_margin_top(box, 22);
    gtk_widget_set_margin_bottom(box, 22);
    gtk_window_set_child(GTK_WINDOW(dialog), box);

    char *message = g_strdup_printf("Terminate %s (PID %d)?\nThe process will receive SIGTERM.", name, pid);
    gtk_box_append(GTK_BOX(box), label_left(message, "section-title"));
    g_free(message);

    GtkWidget *buttons = gtk_box_new(GTK_ORIENTATION_HORIZONTAL, 10);
    gtk_widget_set_halign(buttons, GTK_ALIGN_END);

    GtkWidget *cancel = gtk_button_new_with_label(tr("Cancel"));
    GtkWidget *terminate = gtk_button_new_with_label(tr("Terminate"));
    gtk_widget_add_css_class(terminate, "danger");

    gtk_box_append(GTK_BOX(buttons), cancel);
    gtk_box_append(GTK_BOX(buttons), terminate);
    gtk_box_append(GTK_BOX(box), buttons);

    g_signal_connect(cancel, "clicked", G_CALLBACK(kill_dialog_cancel), dialog);

    KillContext *ctx = g_new0(KillContext, 1);
    ctx->dialog = GTK_WINDOW(dialog);
    ctx->pid = pid;
    g_signal_connect(terminate, "clicked", G_CALLBACK(kill_dialog_response), ctx);

    gtk_window_present(GTK_WINDOW(dialog));
}

static void render_processes(AppState *state)
{
    clear_box(state->process_box);

    GtkWidget *header = gtk_box_new(GTK_ORIENTATION_HORIZONTAL, 12);
    GtkWidget *hname = label_left(tr("Process"), "row-title");
    GtkWidget *hpid = label_left(tr("PID"), "row-meta");
    GtkWidget *hcpu = label_left(tr("CPU"), "row-meta");
    GtkWidget *hram = label_left(tr("RAM"), "row-meta");
    gtk_widget_set_hexpand(hname, TRUE);
    gtk_widget_set_size_request(hpid, 70, -1);
    gtk_widget_set_size_request(hcpu, 80, -1);
    gtk_widget_set_size_request(hram, 90, -1);
    gtk_box_append(GTK_BOX(header), hname);
    gtk_box_append(GTK_BOX(header), hpid);
    gtk_box_append(GTK_BOX(header), hcpu);
    gtk_box_append(GTK_BOX(header), hram);
    gtk_box_append(GTK_BOX(state->process_box), header);

    GPtrArray *processes = process_manager_list();
    guint limit = processes->len < 80 ? processes->len : 80;

    for (guint i = 0; i < limit; i++) {
        ProcessInfo *process = g_ptr_array_index(processes, i);
        GtkWidget *frame = gtk_frame_new(NULL);
        gtk_widget_add_css_class(frame, "row");

        GtkWidget *row = gtk_box_new(GTK_ORIENTATION_HORIZONTAL, 12);
        gtk_widget_set_margin_start(row, 12);
        gtk_widget_set_margin_end(row, 12);
        gtk_widget_set_margin_top(row, 8);
        gtk_widget_set_margin_bottom(row, 8);
        gtk_frame_set_child(GTK_FRAME(frame), row);

        char pid_text[32];
        char cpu_text[32];
        char ram_text[32];
        snprintf(pid_text, sizeof(pid_text), "%d", process->pid);
        snprintf(cpu_text, sizeof(cpu_text), "%.1f%%", process->cpu_percent);
        snprintf(ram_text, sizeof(ram_text), "%llu MB", (unsigned long long)process->memory_mb);

        GtkWidget *name = label_left(process->name, "row-title");
        GtkWidget *pid = label_left(pid_text, "row-meta");
        GtkWidget *cpu = label_left(cpu_text, "row-meta");
        GtkWidget *ram = label_left(ram_text, "row-meta");
        gtk_widget_set_hexpand(name, TRUE);
        gtk_widget_set_size_request(pid, 70, -1);
        gtk_widget_set_size_request(cpu, 80, -1);
        gtk_widget_set_size_request(ram, 90, -1);

        gtk_box_append(GTK_BOX(row), name);
        gtk_box_append(GTK_BOX(row), pid);
        gtk_box_append(GTK_BOX(row), cpu);
        gtk_box_append(GTK_BOX(row), ram);

        GtkWidget *end = gtk_button_new_with_label(tr("End"));
        gtk_widget_add_css_class(end, "danger");
        gtk_widget_set_sensitive(end, process->pid > 1 && process->pid != getpid());
        g_object_set_data(G_OBJECT(end), "pid", GINT_TO_POINTER(process->pid));
        g_signal_connect(end, "clicked", G_CALLBACK(on_kill_clicked), state);
        gtk_box_append(GTK_BOX(row), end);

        gtk_box_append(GTK_BOX(state->process_box), frame);
    }

    process_manager_free_list(processes);
}


static void render_gaming(AppState *state);

static void render_doctor(AppState *state)
{
    clear_box(state->doctor_box);
    GPtrArray *checks = doctor_run();
    for (guint i = 0; i < checks->len; i++) {
        DoctorCheck *c = g_ptr_array_index(checks, i);
        char *meta = g_strdup_printf("%s  |  %s", c->status, c->detail);
        gtk_box_append(GTK_BOX(state->doctor_box), info_row(c->name, meta));
        g_free(meta);
    }
    doctor_free_list(checks);
}

static void render_startup(AppState *state)
{
    clear_box(state->startup_box);
    GPtrArray *entries = startup_list();

    if (entries->len == 0) {
        gtk_box_append(GTK_BOX(state->startup_box),
                       info_row(tr("Startup"), tr("No .desktop autostart entries detected.")));
    }

    for (guint i = 0; i < entries->len; i++) {
        StartupEntry *entry = g_ptr_array_index(entries, i);
        gtk_box_append(GTK_BOX(state->startup_box), info_row(entry->name, entry->path));
    }

    startup_free_list(entries);
}

static void gaming_performance_clicked(GtkButton *button, gpointer user_data)
{
    (void)button;
    AppState *state = user_data;
    if (gaming_set_performance())
        gtk_label_set_text(GTK_LABEL(state->status_label), tr("Performance profile selected."));
    else
        gtk_label_set_text(GTK_LABEL(state->status_label), tr("Could not switch power profile."));
    render_gaming(state);
}

static void gaming_balanced_clicked(GtkButton *button, gpointer user_data)
{
    (void)button;
    AppState *state = user_data;
    if (gaming_set_balanced())
        gtk_label_set_text(GTK_LABEL(state->status_label), tr("Balanced profile selected."));
    else
        gtk_label_set_text(GTK_LABEL(state->status_label), tr("Could not switch power profile."));
    render_gaming(state);
}

static void render_gaming(AppState *state)
{
    clear_box(state->gaming_box);

    GamingStatus gaming = gaming_status();
    gtk_box_append(GTK_BOX(state->gaming_box),
                   info_row(tr("GameMode"), gaming.gamemode ? tr("Detected") : tr("Not installed")));
    gtk_box_append(GTK_BOX(state->gaming_box),
                   info_row(tr("MangoHud"), gaming.mangohud ? tr("Detected") : tr("Not installed")));

    char profile[128];
    snprintf(profile, sizeof profile, "%s",
             gaming.powerprofiles ? gaming.current_profile[0] ? gaming.current_profile : "Unknown" : tr("powerprofilesctl unavailable"));
    gtk_box_append(GTK_BOX(state->gaming_box), info_row(tr("Power profile"), profile));

    GtkWidget *buttons = gtk_box_new(GTK_ORIENTATION_HORIZONTAL, 8);
    gtk_widget_set_margin_top(buttons, 10);

    GtkWidget *performance = gtk_button_new_with_label(tr("Performance"));
    GtkWidget *balanced = gtk_button_new_with_label(tr("Balanced"));

    g_signal_connect(performance, "clicked", G_CALLBACK(gaming_performance_clicked), state);

    g_signal_connect(balanced, "clicked", G_CALLBACK(gaming_balanced_clicked), state);

    gtk_box_append(GTK_BOX(buttons), performance);
    gtk_box_append(GTK_BOX(buttons), balanced);
    gtk_box_append(GTK_BOX(state->gaming_box), buttons);
}

static void render_permissions(AppState *state)
{
    clear_box(state->permissions_box);
    GPtrArray *checks = permissions_check();

    for (guint i = 0; i < checks->len; i++) {
        PermissionCheck *c = g_ptr_array_index(checks, i);
        char *meta = g_strdup_printf("%s  |  %s", c->status, c->detail);
        gtk_box_append(GTK_BOX(state->permissions_box), info_row(c->name, meta));
        g_free(meta);
    }

    permissions_free_list(checks);
}

static void console_run_selected(GtkButton *button, gpointer user_data)
{
    (void)button;
    AppState *state = user_data;

    const char *commands[] = {
        "uname -a",
        "df -h /",
        "ip -brief address",
        "systemctl --failed --no-legend --plain"
    };

    guint selected = gtk_drop_down_get_selected(GTK_DROP_DOWN(state->console_dropdown));
    if (selected >= G_N_ELEMENTS(commands))
        return;

    gchar *stdout_data = NULL;
    gchar *stderr_data = NULL;
    gint status = 0;

    gboolean ok = g_spawn_command_line_sync(
        commands[selected], &stdout_data, &stderr_data, &status, NULL);

    GtkTextBuffer *buffer = gtk_text_view_get_buffer(GTK_TEXT_VIEW(state->console_output));

    if (!ok) {
        gtk_text_buffer_set_text(buffer, tr("Failed to execute command."), -1);
    } else {
        GString *result = g_string_new(stdout_data ? stdout_data : "");
        if (stderr_data && *stderr_data) {
            g_string_append(result, "\n--- stderr ---\n");
            g_string_append(result, stderr_data);
        }
        gtk_text_buffer_set_text(buffer, result->str, -1);
        g_string_free(result, TRUE);
    }

    g_free(stdout_data);
    g_free(stderr_data);
    gtk_label_set_text(GTK_LABEL(state->status_label), tr("Command completed."));
}

static GtkWidget *build_console_page(AppState *state)
{
    GtkWidget *page = page_box();
    add_header(page, tr("Command Console"), tr("Run a small set of safe diagnostic commands."));

    const char *commands[] = {
        tr("Kernel / OS"),
        tr("Root disk"),
        tr("Network addresses"),
        tr("Failed services"),
        NULL
    };

    GtkWidget *controls = gtk_box_new(GTK_ORIENTATION_HORIZONTAL, 8);
    state->console_dropdown = gtk_drop_down_new_from_strings(commands);
    GtkWidget *run = gtk_button_new_with_label(tr("Run"));
    gtk_box_append(GTK_BOX(controls), state->console_dropdown);
    gtk_box_append(GTK_BOX(controls), run);
    gtk_box_append(GTK_BOX(page), controls);

    GtkWidget *frame = gtk_frame_new(NULL);
    gtk_widget_add_css_class(frame, "setting");
    GtkWidget *view = gtk_text_view_new();
    state->console_output = view;
    gtk_text_view_set_editable(GTK_TEXT_VIEW(view), FALSE);
    gtk_text_view_set_monospace(GTK_TEXT_VIEW(view), TRUE);
    gtk_widget_set_vexpand(view, TRUE);
    gtk_frame_set_child(GTK_FRAME(frame), view);
    gtk_box_append(GTK_BOX(page), frame);

    g_signal_connect(run, "clicked", G_CALLBACK(console_run_selected), state);
    return page;
}

static void refresh_now_for_visible_page(AppState *state)
{
    update_overview(state);

    const char *name = gtk_stack_get_visible_child_name(GTK_STACK(state->stack));
    if (!name) return;

    if (strcmp(name, "processes") == 0) render_processes(state);
    else if (strcmp(name, "hardware") == 0) render_hardware(state);
    else if (strcmp(name, "services") == 0) render_services(state);
    else if (strcmp(name, "storage") == 0) render_storage(state);
    else if (strcmp(name, "network") == 0) render_network(state);
    else if (strcmp(name, "power") == 0) render_power(state);
    else if (strcmp(name, "doctor") == 0) render_doctor(state);
    else if (strcmp(name, "startup") == 0) render_startup(state);
    else if (strcmp(name, "gaming") == 0) render_gaming(state);
    else if (strcmp(name, "permissions") == 0) render_permissions(state);
}

static gboolean refresh_timer(gpointer user_data)
{
    refresh_now_for_visible_page(user_data);
    return G_SOURCE_CONTINUE;
}

static void restart_refresh_timer(AppState *state)
{
    if (state->refresh_source)
        g_source_remove(state->refresh_source);
    state->refresh_source = g_timeout_add_seconds(state->refresh_seconds, refresh_timer, state);
}

static void refresh_setting_changed(GObject *object, GParamSpec *pspec, gpointer user_data)
{
    (void)pspec;
    AppState *state = user_data;
    guint selected = gtk_drop_down_get_selected(GTK_DROP_DOWN(object));
    state->refresh_seconds = selected == 0 ? 1 : (selected == 1 ? 2 : 5);
    restart_refresh_timer(state);
}

static void nav_clicked(GtkButton *button, gpointer user_data)
{
    AppState *state = user_data;
    PageId page = (PageId)GPOINTER_TO_INT(g_object_get_data(G_OBJECT(button), "page-id"));
    static const char *page_names[] = {
        "overview", "processes", "hardware", "services",
        "storage", "network", "power", "doctor",
        "startup", "gaming", "permissions", "console", "settings"
    };

    gtk_stack_set_visible_child_name(GTK_STACK(state->stack), page_names[page]);

    GtkWidget *sidebar = gtk_widget_get_parent(GTK_WIDGET(button));
    if (sidebar) {
        GtkWidget *child = gtk_widget_get_first_child(sidebar);
        while (child) {
            if (GTK_IS_BUTTON(child)) gtk_widget_remove_css_class(child, "active");
            child = gtk_widget_get_next_sibling(child);
        }
    }
    gtk_widget_add_css_class(GTK_WIDGET(button), "active");
    refresh_now_for_visible_page(state);
}

static void add_data_page(AppState *state, const char *name, const char *title,
                          const char *subtitle, GtkWidget **inner_out)
{
    GtkWidget *page = page_box();
    add_header(page, title, subtitle);
    GtkWidget *scroll = scroll_page(inner_out);
    gtk_box_append(GTK_BOX(page), scroll);
    gtk_stack_add_named(GTK_STACK(state->stack), page, name);
}

static void theme_changed(GObject *object, GParamSpec *pspec, gpointer user_data)
{
    (void)pspec;
    AppState *state = user_data;
    guint selected = gtk_drop_down_get_selected(GTK_DROP_DOWN(object));
    const char *themes[] = {"auto", "nebula", "mint", "ubuntu", "arch", "fedora", "debian", "manjaro", "opensuse", "pop", "elementary", "zorin", "kali", "ocean", "rose", "amber"};
    if (selected < G_N_ELEMENTS(themes)) {
        g_strlcpy(ui_config.theme, themes[selected], sizeof ui_config.theme);
        save_ui_config();
        apply_css();
        (void)state;
    }
}

static void shape_changed(GObject *object, GParamSpec *pspec, gpointer user_data)
{
    (void)pspec;
    AppState *state = user_data;
    guint selected = gtk_drop_down_get_selected(GTK_DROP_DOWN(object));
    const char *shapes[] = {"rounded", "soft", "square"};
    if (selected < G_N_ELEMENTS(shapes)) {
        g_strlcpy(ui_config.shape, shapes[selected], sizeof ui_config.shape);
        save_ui_config();
        apply_css();
        (void)state;
    }
}

static const char *language_codes[] = {"en", "uk", "ru", "da", "de", "fr", "es", "it", "pt", "nl", "pl", "cs", "sk", "sl", "hr", "sr", "bg", "ro", "hu", "el", "tr", "sv", "no", "fi", "et", "lv", "lt", "is", "ga", "mt", "sq", "mk", "bs", "ca", "eu", "gl", "cy", "eo", "hi", "bn", "ur", "pa", "gu", "mr", "ta", "te", "kn", "ml", "si", "ne", "th", "vi", "id", "ms", "fil", "sw", "am", "af", "zu", "xh", "yo", "ig", "ha", "ar", "he", "fa", "ps", "kk", "uz", "ky", "tg", "tk", "az", "hy", "ka", "mn", "ja", "ko", "zh", "la", "jv", "so", "mg", "ny", "rw", "ht", "lb", "mi", "sm", "to", "fj", "ceb", "co", "haw", "su", "fy", "gd", "oc", "br", "ku", "ba", "tt", "ug", "be"};
static const char *language_names[] = {
    "English",
    "Українська",
    "Русский",
    "Dansk",
    "Deutsch",
    "Français",
    "Español",
    "Italiano",
    "Português",
    "Nederlands",
    "Polski",
    "Čeština",
    "Slovenčina",
    "Slovenščina",
    "Hrvatski",
    "Српски",
    "Български",
    "Română",
    "Magyar",
    "Ελληνικά",
    "Türkçe",
    "Svenska",
    "Norsk",
    "Suomi",
    "Eesti",
    "Latviešu",
    "Lietuvių",
    "Íslenska",
    "Gaeilge",
    "Malti",
    "Shqip",
    "Македонски",
    "Bosanski",
    "Català",
    "Euskara",
    "Galego",
    "Cymraeg",
    "Esperanto",
    "हिन्दी",
    "বাংলা",
    "اردو",
    "ਪੰਜਾਬੀ",
    "ગુજરાતી",
    "मराठी",
    "தமிழ்",
    "తెలుగు",
    "ಕನ್ನಡ",
    "മലയാളം",
    "සිංහල",
    "नेपाली",
    "ไทย",
    "Tiếng Việt",
    "Bahasa Indonesia",
    "Bahasa Melayu",
    "Filipino",
    "Kiswahili",
    "አማርኛ",
    "Afrikaans",
    "isiZulu",
    "isiXhosa",
    "Yorùbá",
    "Igbo",
    "Hausa",
    "العربية",
    "עברית",
    "فارسی",
    "پښتو",
    "Қазақша",
    "O‘zbek",
    "Кыргызча",
    "Тоҷикӣ",
    "Türkmençe",
    "Azərbaycanca",
    "Հայերեն",
    "ქართული",
    "Монгол",
    "日本語",
    "한국어",
    "简体中文",
    "Latina",
    "Basa Jawa",
    "Soomaali",
    "Malagasy",
    "Chichewa",
    "Kinyarwanda",
    "Kreyòl ayisyen",
    "Lëtzebuergesch",
    "Māori",
    "Gagana Samoa",
    "Tongan",
    "Fijian",
    "Cebuano",
    "Corsu",
    "ʻŌlelo Hawaiʻi",
    "Basa Sunda",
    "Frysk",
    "Gàidhlig",
    "Occitan",
    "Brezhoneg",
    "Kurdî",
    "Башҡортса",
    "Татарча",
    "ئۇيغۇرچە",
    "Беларуская",
    NULL
};

static void language_changed(GObject *object, GParamSpec *pspec, gpointer user_data)
{
    (void)pspec;
    AppState *state = user_data;
    guint selected = gtk_drop_down_get_selected(GTK_DROP_DOWN(object));
    if (selected < G_N_ELEMENTS(language_codes)) {
        g_strlcpy(ui_config.language, language_codes[selected], sizeof ui_config.language);
        save_ui_config();
        i18n_set_language(ui_config.language);
        gtk_label_set_text(GTK_LABEL(state->status_label), tr("Restart required to apply language."));
    }
}

static guint theme_index(void)
{
    const char *themes[] = {"auto", "nebula", "mint", "ubuntu", "arch", "fedora", "debian", "manjaro", "opensuse", "pop", "elementary", "zorin", "kali", "ocean", "rose", "amber"};
    for (guint i = 0; i < G_N_ELEMENTS(themes); i++) if (strcmp(ui_config.theme, themes[i]) == 0) return i;
    return 0;
}

static guint shape_index(void)
{
    const char *shapes[] = {"rounded", "soft", "square"};
    for (guint i = 0; i < G_N_ELEMENTS(shapes); i++) if (strcmp(ui_config.shape, shapes[i]) == 0) return i;
    return 0;
}

static guint language_index(void)
{
    for (guint i = 0; i < G_N_ELEMENTS(language_codes); i++)
        if (strcmp(ui_config.language, language_codes[i]) == 0)
            return i;
    return 0;
}

static void build_settings_page(AppState *state)
{
    GtkWidget *page = page_box();
    add_header(page, tr("Settings"), tr("Nebula Control Center preferences."));

    GtkWidget *setting = gtk_frame_new(NULL);
    gtk_widget_add_css_class(setting, "setting");
    GtkWidget *row = gtk_box_new(GTK_ORIENTATION_HORIZONTAL, 15);
    gtk_widget_set_margin_start(row, 16);
    gtk_widget_set_margin_end(row, 16);
    gtk_widget_set_margin_top(row, 14);
    gtk_widget_set_margin_bottom(row, 14);
    gtk_frame_set_child(GTK_FRAME(setting), row);

    GtkWidget *label = label_left(tr("Refresh interval"), "row-title");
    gtk_widget_set_hexpand(label, TRUE);
    state->refresh_dropdown = gtk_drop_down_new_from_strings((const char *[]) {tr("1 second"), tr("2 seconds"), tr("5 seconds"), NULL});
    gtk_drop_down_set_selected(GTK_DROP_DOWN(state->refresh_dropdown), 0);
    gtk_box_append(GTK_BOX(row), label);
    gtk_box_append(GTK_BOX(row), state->refresh_dropdown);
    gtk_box_append(GTK_BOX(page), setting);

    GtkWidget *theme_row = gtk_box_new(GTK_ORIENTATION_HORIZONTAL, 15);
    GtkWidget *theme_setting = gtk_frame_new(NULL);
    gtk_widget_add_css_class(theme_setting, "setting");
    gtk_widget_set_margin_start(theme_row, 16);
    gtk_widget_set_margin_end(theme_row, 16);
    gtk_widget_set_margin_top(theme_row, 14);
    gtk_widget_set_margin_bottom(theme_row, 14);
    gtk_frame_set_child(GTK_FRAME(theme_setting), theme_row);
    GtkWidget *theme_label = label_left(tr("Theme"), "row-title");
    gtk_widget_set_hexpand(theme_label, TRUE);
    GtkWidget *theme_dropdown = gtk_drop_down_new_from_strings((const char *[]) {tr("Auto"), tr("Nebula"), tr("Mint"), tr("Ubuntu"), tr("Arch"), tr("Fedora"), tr("Debian"), tr("Manjaro"), tr("openSUSE"), tr("Pop!_OS"), tr("elementary"), tr("Zorin"), tr("Kali"), tr("Ocean"), tr("Rose"), tr("Amber"), NULL});
    gtk_drop_down_set_selected(GTK_DROP_DOWN(theme_dropdown), theme_index());
    gtk_box_append(GTK_BOX(theme_row), theme_label);
    gtk_box_append(GTK_BOX(theme_row), theme_dropdown);
    gtk_box_append(GTK_BOX(page), theme_setting);
    g_signal_connect(theme_dropdown, "notify::selected", G_CALLBACK(theme_changed), state);

    GtkWidget *shape_row = gtk_box_new(GTK_ORIENTATION_HORIZONTAL, 15);
    GtkWidget *shape_setting = gtk_frame_new(NULL);
    gtk_widget_add_css_class(shape_setting, "setting");
    gtk_widget_set_margin_start(shape_row, 16);
    gtk_widget_set_margin_end(shape_row, 16);
    gtk_widget_set_margin_top(shape_row, 14);
    gtk_widget_set_margin_bottom(shape_row, 14);
    gtk_frame_set_child(GTK_FRAME(shape_setting), shape_row);
    GtkWidget *shape_label = label_left(tr("Shape"), "row-title");
    gtk_widget_set_hexpand(shape_label, TRUE);
    GtkWidget *shape_dropdown = gtk_drop_down_new_from_strings((const char *[]) {tr("Rounded"), tr("Soft"), tr("Square"), NULL});
    gtk_drop_down_set_selected(GTK_DROP_DOWN(shape_dropdown), shape_index());
    gtk_box_append(GTK_BOX(shape_row), shape_label);
    gtk_box_append(GTK_BOX(shape_row), shape_dropdown);
    gtk_box_append(GTK_BOX(page), shape_setting);
    g_signal_connect(shape_dropdown, "notify::selected", G_CALLBACK(shape_changed), state);

    GtkWidget *lang_row = gtk_box_new(GTK_ORIENTATION_HORIZONTAL, 15);
    GtkWidget *lang_setting = gtk_frame_new(NULL);
    gtk_widget_add_css_class(lang_setting, "setting");
    gtk_widget_set_margin_start(lang_row, 16);
    gtk_widget_set_margin_end(lang_row, 16);
    gtk_widget_set_margin_top(lang_row, 14);
    gtk_widget_set_margin_bottom(lang_row, 14);
    gtk_frame_set_child(GTK_FRAME(lang_setting), lang_row);
    GtkWidget *lang_label = label_left(tr("Language"), "row-title");
    gtk_widget_set_hexpand(lang_label, TRUE);
    GtkWidget *lang_dropdown = gtk_drop_down_new_from_strings(language_names);
    gtk_drop_down_set_selected(GTK_DROP_DOWN(lang_dropdown), language_index());
    gtk_box_append(GTK_BOX(lang_row), lang_label);
    gtk_box_append(GTK_BOX(lang_row), lang_dropdown);
    gtk_box_append(GTK_BOX(page), lang_setting);
    g_signal_connect(lang_dropdown, "notify::selected", G_CALLBACK(language_changed), state);

    gtk_box_append(GTK_BOX(page), label_left(tr("Restart required to apply language."), "muted"));

    char about_text[256];
    snprintf(about_text, sizeof(about_text), "%s %s | C + GTK4 | %s", tr("Version"), APP_VERSION, tr("Native Linux application"));
    gtk_box_append(GTK_BOX(page), info_row(tr("Nebula Control Center"), about_text));

    g_signal_connect(state->refresh_dropdown, "notify::selected", G_CALLBACK(refresh_setting_changed), state);
    gtk_stack_add_named(GTK_STACK(state->stack), page, "settings");
}

static void activate(GtkApplication *app, gpointer user_data)
{
    (void)user_data;
    load_ui_config();
    apply_css();

    AppState *state = g_new0(AppState, 1);
    state->refresh_seconds = 1;

    GtkWidget *window = gtk_application_window_new(app);
    state->window = window;
    gtk_window_set_title(GTK_WINDOW(window), "Nebula Control Center");
    gtk_window_set_default_size(GTK_WINDOW(window), 1000, 600);

    GtkWidget *root = gtk_box_new(GTK_ORIENTATION_HORIZONTAL, 0);
    gtk_window_set_child(GTK_WINDOW(window), root);

    GtkWidget *sidebar = gtk_box_new(GTK_ORIENTATION_VERTICAL, 8);
    gtk_widget_add_css_class(sidebar, "sidebar");
    gtk_widget_set_size_request(sidebar, 225, -1);
    gtk_widget_set_margin_start(sidebar, 12);
    gtk_widget_set_margin_end(sidebar, 12);
    gtk_widget_set_margin_top(sidebar, 12);
    gtk_widget_set_margin_bottom(sidebar, 12);
    gtk_box_append(GTK_BOX(root), sidebar);

    gtk_box_append(GTK_BOX(sidebar), label_left("NEBULA", "brand"));
    gtk_box_append(GTK_BOX(sidebar), label_left("CONTROL CENTER", "brand-sub"));
    gtk_box_append(GTK_BOX(sidebar), gtk_separator_new(GTK_ORIENTATION_HORIZONTAL));

    GtkWidget *nav_scroll = gtk_scrolled_window_new();
    gtk_scrolled_window_set_policy(GTK_SCROLLED_WINDOW(nav_scroll), GTK_POLICY_NEVER, GTK_POLICY_AUTOMATIC);
    gtk_widget_set_vexpand(nav_scroll, TRUE);
    GtkWidget *nav_box = gtk_box_new(GTK_ORIENTATION_VERTICAL, 7);
    gtk_scrolled_window_set_child(GTK_SCROLLED_WINDOW(nav_scroll), nav_box);
    gtk_box_append(GTK_BOX(sidebar), nav_scroll);

    const char *nav_labels[13];
    nav_labels[0] = tr("Overview");
    nav_labels[1] = tr("Processes");
    nav_labels[2] = tr("Hardware");
    nav_labels[3] = tr("Services");
    nav_labels[4] = tr("Storage");
    nav_labels[5] = tr("Network");
    nav_labels[6] = tr("Power");
    nav_labels[7] = tr("System Doctor");
    nav_labels[8] = tr("Startup");
    nav_labels[9] = tr("Gaming Mode");
    nav_labels[10] = tr("Permissions");
    nav_labels[11] = tr("Command Console");
    nav_labels[12] = tr("Settings");

    for (int i = 0; i < 13; i++) {
        GtkWidget *button = gtk_button_new_with_label(nav_labels[i]);
        gtk_widget_add_css_class(button, "nav");
        gtk_widget_set_hexpand(button, TRUE);
        if (i == 0) gtk_widget_add_css_class(button, "active");
        g_object_set_data(G_OBJECT(button), "page-id", GINT_TO_POINTER(i));
        g_signal_connect(button, "clicked", G_CALLBACK(nav_clicked), state);
        gtk_box_append(GTK_BOX(nav_box), button);
    }

    state->status_label = label_left(tr("SYSTEM ONLINE"), "online");
    gtk_box_append(GTK_BOX(sidebar), state->status_label);
    gtk_box_append(GTK_BOX(sidebar), label_left("v" APP_VERSION, "muted"));

    state->stack = gtk_stack_new();
    gtk_stack_set_transition_type(GTK_STACK(state->stack), GTK_STACK_TRANSITION_TYPE_CROSSFADE);
    gtk_widget_set_hexpand(state->stack, TRUE);
    gtk_widget_set_vexpand(state->stack, TRUE);
    gtk_box_append(GTK_BOX(root), state->stack);

    /* Overview page */
    GtkWidget *overview = page_box();
    add_header(overview, tr("System Overview"), tr("Monitor your Linux system in one place."));

    GtkWidget *grid = gtk_grid_new();
    gtk_grid_set_row_spacing(GTK_GRID(grid), 16);
    gtk_grid_set_column_spacing(GTK_GRID(grid), 16);
    gtk_widget_set_vexpand(grid, TRUE);

    gtk_grid_attach(GTK_GRID(grid), card_new(tr("CPU"), &state->cpu_value, &state->cpu_meta, &state->cpu_bar), 0, 0, 1, 1);
    gtk_grid_attach(GTK_GRID(grid), card_new(tr("MEMORY"), &state->memory_value, &state->memory_meta, &state->memory_bar), 1, 0, 1, 1);
    gtk_grid_attach(GTK_GRID(grid), card_new(tr("GPU"), &state->gpu_value, &state->gpu_meta, NULL), 2, 0, 1, 1);
    gtk_grid_attach(GTK_GRID(grid), card_new(tr("STORAGE"), &state->storage_value, &state->storage_meta, &state->storage_bar), 0, 1, 2, 1);
    gtk_grid_attach(GTK_GRID(grid), card_new(tr("UPTIME"), &state->uptime_value, NULL, NULL), 2, 1, 1, 1);
    gtk_grid_attach(GTK_GRID(grid), card_new(tr("NETWORK"), &state->network_value, &state->network_meta, NULL), 0, 2, 2, 1);
    gtk_grid_attach(GTK_GRID(grid), card_new(tr("POWER"), &state->battery_value, NULL, NULL), 2, 2, 1, 1);

    gtk_box_append(GTK_BOX(overview), grid);
    gtk_stack_add_named(GTK_STACK(state->stack), overview, "overview");

    /* Other pages */
    {
        GtkWidget *page = page_box();
        add_header(page, tr("Processes"), tr("Running programs and resource usage."));
        GtkWidget *scroll = scroll_page(&state->process_box);
        gtk_box_append(GTK_BOX(page), scroll);
        gtk_stack_add_named(GTK_STACK(state->stack), page, "processes");
    }
    add_data_page(state, "hardware", tr("Hardware"), tr("Detected CPU, GPU, temperature and system identity."), &state->hardware_box);
    add_data_page(state, "services", tr("Services"), tr("Currently running systemd services."), &state->services_box);
    add_data_page(state, "storage", tr("Storage"), tr("Mounted filesystems and their usage."), &state->storage_box);
    add_data_page(state, "network", tr("Network"), tr("Interfaces, addresses and transfer counters."), &state->network_box);
    add_data_page(state, "power", tr("Power"), tr("Battery and system load information."), &state->power_box);
    add_data_page(state, "doctor", tr("System Doctor"), tr("Quick health checks for common Linux subsystems."), &state->doctor_box);
    add_data_page(state, "startup", tr("Startup"), tr("Programs configured to launch with your desktop."), &state->startup_box);
    add_data_page(state, "gaming", tr("Gaming Mode"), tr("GameMode, MangoHud and power-profile controls."), &state->gaming_box);
    add_data_page(state, "permissions", tr("Permissions"), tr("What system interfaces and admin helpers are available."), &state->permissions_box);
    gtk_stack_add_named(GTK_STACK(state->stack), build_console_page(state), "console");
    build_settings_page(state);

    update_overview(state);
    render_processes(state);
    render_hardware(state);
    render_services(state);
    render_storage(state);
    render_network(state);
    render_power(state);
    render_doctor(state);
    render_startup(state);
    render_gaming(state);
    render_permissions(state);

    state->refresh_source = g_timeout_add_seconds(state->refresh_seconds, refresh_timer, state);
    gtk_window_present(GTK_WINDOW(window));
}

int main(int argc, char **argv)
{
    GtkApplication *app = gtk_application_new("org.nebula.ControlCenter", G_APPLICATION_DEFAULT_FLAGS);
    g_signal_connect(app, "activate", G_CALLBACK(activate), NULL);

    int status = g_application_run(G_APPLICATION(app), argc, argv);
    g_object_unref(app);
    return status;
}
