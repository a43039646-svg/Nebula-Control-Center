#ifndef NEBULA_DOCTOR_H
#define NEBULA_DOCTOR_H

#include <glib.h>

typedef struct {
    char *name;
    char *status;
    char *detail;
} DoctorCheck;

GPtrArray *doctor_run(void);
void doctor_free_list(GPtrArray *checks);

#endif
