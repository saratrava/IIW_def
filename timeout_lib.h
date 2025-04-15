#ifndef TIMEOUT_LIB_H_   /* Include guard */
#define TIMEOUT_LIB_H_

#include <time.h>
#include <stdio.h>
#include <stdlib.h>
#include <sys/types.h>
#include <sys/time.h>
#include <sys/stat.h>
#include <errno.h>

#include "mylib.h"

#define CONN_TIMEOUT 5
#define FIN_TIMEOUT 1

#define TIME_PRECISION 1000000000

#define TIMESPEC_IS_POSITIVE(timespec) (timespec.tv_sec >= 0 && timespec.tv_nsec > 0)
#define TIMESPEC_IS_ZERO(timespec) (timespec.tv_sec == 0 && timespec.tv_nsec == 0)
#define TIMESPEC_EXP(t1,t2) ((t1.tv_sec > t2.tv_sec) || ((t1.tv_sec == t2.tv_sec) && (t1.tv_nsec >= t2.tv_nsec)) )

#define GET_TIMEOUT(nano) (struct timespec) {.tv_sec = nano / TIME_PRECISION, .tv_nsec = nano % TIME_PRECISION }
#define GET_TIMEOUT_US(micro) (struct timeval) {.tv_sec =  micro / 1000000, .tv_usec = micro % 1000000}

#define GET_MAX_TENTATIVE(timeout_nsec) ( CONN_TIMEOUT * (TIME_PRECISION / timeout_nsec)) //see data type

void get_current_time(struct timespec *curr_t);

void timespec_sum(struct timespec *t, unsigned int off_us, struct timespec *result);
void timespec_diff(struct timespec *start, struct timespec *stop, struct timespec *result);

void my_profiling(int interval);

#endif // TIMEOUT_LIB_H_
