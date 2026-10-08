#include "network.h"

#include <dirent.h>
#include <ifaddrs.h>
#include <net/if.h>
#include <netinet/in.h>
#include <stdio.h>
#include <string.h>
#include <arpa/inet.h>

static void read_iface_bytes(const char *name, unsigned long long *rx, unsigned long long *tx)
{
    FILE *file = fopen("/proc/net/dev", "r");
    if (!file) return;
    char line[512];
    while (fgets(line, sizeof(line), file)) {
        char iface[64];
        unsigned long long r = 0, t = 0;
        if (sscanf(line, " %63[^:]: %llu %*u %*u %*u %*u %*u %*u %*u %llu", iface, &r, &t) == 3 && strcmp(iface, name) == 0) {
            *rx = r; *tx = t; break;
        }
    }
    fclose(file);
}

GPtrArray *network_list(void)
{
    GPtrArray *list = g_ptr_array_new_with_free_func(g_free);
    DIR *dir = opendir("/sys/class/net");
    if (!dir) return list;
    struct dirent *entry;
    while ((entry = readdir(dir))) {
        if (entry->d_name[0] == '.') continue;
        NetworkInfo *info = g_new0(NetworkInfo, 1);
        snprintf(info->name, sizeof(info->name), "%s", entry->d_name);

        char path[256];
        snprintf(path, sizeof(path), "/sys/class/net/%s/operstate", entry->d_name);
        FILE *file = fopen(path, "r");
        if (file) { fgets(info->state, sizeof(info->state), file); fclose(file); info->state[strcspn(info->state, "\r\n")] = '\0'; }

        snprintf(path, sizeof(path), "/sys/class/net/%s/address", entry->d_name);
        file = fopen(path, "r");
        if (file) { fgets(info->mac, sizeof(info->mac), file); fclose(file); info->mac[strcspn(info->mac, "\r\n")] = '\0'; }

        read_iface_bytes(entry->d_name, &info->rx_bytes, &info->tx_bytes);
        g_ptr_array_add(list, info);
    }
    closedir(dir);

    struct ifaddrs *ifaddr = NULL;
    if (getifaddrs(&ifaddr) == 0) {
        for (struct ifaddrs *ifa = ifaddr; ifa; ifa = ifa->ifa_next) {
            if (!ifa->ifa_addr || ifa->ifa_addr->sa_family != AF_INET) continue;
            char address[INET_ADDRSTRLEN];
            struct sockaddr_in *addr = (struct sockaddr_in *)ifa->ifa_addr;
            if (!inet_ntop(AF_INET, &addr->sin_addr, address, sizeof(address))) continue;
            for (guint i = 0; i < list->len; i++) {
                NetworkInfo *info = g_ptr_array_index(list, i);
                if (strcmp(info->name, ifa->ifa_name) == 0) {
                    snprintf(info->address, sizeof(info->address), "%s", address);
                    break;
                }
            }
        }
        freeifaddrs(ifaddr);
    }
    return list;
}

void network_free_list(GPtrArray *list)
{
    if (list) g_ptr_array_free(list, TRUE);
}
