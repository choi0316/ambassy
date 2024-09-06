#ifndef PGTIMVER_H
#define PGTIMVER_H

long pg_get_current_time(struct timespec *ts);
uint64_t pg_timespec_diff_ns(struct timespec *start, struct timespec *end);

#endif /*PGTIMVER_H*/
