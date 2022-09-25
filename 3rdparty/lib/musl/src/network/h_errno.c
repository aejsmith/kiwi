#include <netdb.h>
#ifdef __Kiwi__
static __thread int __h_errno = 0;
#else
#include "pthread_impl.h"

#undef h_errno
int h_errno;
#endif

int *__h_errno_location(void)
{
#ifdef __Kiwi__
    return &__h_errno;
#else
	if (!__pthread_self()->stack) return &h_errno;
	return &__pthread_self()->h_errno_val;
#endif
}
