#define _GNU_SOURCE
#include <netdb.h>
#ifdef __Kiwi__
// TODO
#define LCTRANS_CUR(s) s
#else
#include "locale_impl.h"
#endif

static const char msgs[] =
	"Host not found\0"
	"Try again\0"
	"Non-recoverable error\0"
	"Address not available\0"
	"\0Unknown error";

const char *hstrerror(int ecode)
{
	const char *s;
	for (s=msgs, ecode--; ecode && *s; ecode--, s++) for (; *s; s++);
	if (!*s) s++;
	return LCTRANS_CUR(s);
}
