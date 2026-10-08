#ifndef NEBULA_GAMING_H
#define NEBULA_GAMING_H

#include <glib.h>

typedef struct {
    gboolean gamemode;
    gboolean mangohud;
    gboolean powerprofiles;
    char current_profile[64];
} GamingStatus;

GamingStatus gaming_status(void);
gboolean gaming_set_performance(void);
gboolean gaming_set_balanced(void);

#endif
