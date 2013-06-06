#include <sys/time.h>
#include <sys/resource.h>

#include "timer.h"

double
get_timer(void)
{
    struct rusage ru;

    getrusage(RUSAGE_SELF, &ru);

    return (double)ru.ru_utime.tv_sec + ru.ru_utime.tv_usec * 1e-6;
}
