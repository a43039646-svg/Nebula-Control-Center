#ifndef NEBULA_NETWORK_H
#define NEBULA_NETWORK_H

#include <glib.h>

typedef struct {
    char name[64];
    char state[32];
    char address[64];
    char mac[64];
    unsigned long long rx_bytes;
    unsigned long long tx_bytes;
} NetworkInfo;

GPtrArray *network_list(void);
void network_free_list(GPtrArray *list);

#endif
