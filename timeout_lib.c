#include "timeout_lib.h"

void get_current_time(struct timespec *curr_t)
{
	if (clock_gettime(CLOCK_REALTIME, curr_t)==-1)
		error_msg("error in gettime");	
}

void timespec_sum(struct timespec *t, unsigned int off_ns, struct timespec *result)
{
	result->tv_sec = t->tv_sec + ((t->tv_nsec + off_ns) / TIME_PRECISION);
	result->tv_nsec = (t->tv_nsec + off_ns) % TIME_PRECISION;	
}

void timespec_diff(struct timespec *start, struct timespec *stop, struct timespec *result)
{
    if ((stop->tv_nsec - start->tv_nsec) < 0) 
    {
        result->tv_sec = stop->tv_sec - start->tv_sec - 1;
        result->tv_nsec = stop->tv_nsec - start->tv_nsec + TIME_PRECISION;
    }
    else 
    {
        result->tv_sec = stop->tv_sec - start->tv_sec;
        result->tv_nsec = stop->tv_nsec - start->tv_nsec;
    }
}

void my_profiling(int interval)
{
	struct timespec curr_t, diff_t;
	static struct timespec old_t = {.tv_nsec = 0, .tv_sec = 0};
	if (clock_gettime(CLOCK_REALTIME, &curr_t)==-1)
		error_msg("error in gettime");
	if(!TIMESPEC_IS_ZERO(old_t))
	{
		timespec_diff(&old_t, &curr_t, &diff_t);
		if(interval < diff_t.tv_nsec)
			printf("diff_t sec: %lu nsec %lu\n", diff_t.tv_sec, diff_t.tv_nsec);
	}	
	old_t = curr_t;	
}
