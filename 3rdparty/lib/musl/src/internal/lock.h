#ifndef LOCK_H
#define LOCK_H

#ifdef __Kiwi__

#include <core/mutex.h>

_Static_assert(sizeof(core_mutex_t) == sizeof(int), "core_mutex is incompatible with musl lock");
_Static_assert(CORE_MUTEX_INITIALIZER == 0, "core_mutex is incompatible with musl lock");

#define LOCK(x) core_mutex_lock((core_mutex_t *)(x), -1)
#define UNLOCK(x) core_mutex_unlock((core_mutex_t *)(x))

#else
hidden void __lock(volatile int *);
hidden void __unlock(volatile int *);
#define LOCK(x) __lock(x)
#define UNLOCK(x) __unlock(x)
#endif

#endif
