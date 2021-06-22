#ifndef _SCHAUFEL_UTILS_STRLWR_H
#define _SCHAUFEL_UTILS_STRLWR_H
// win32 check
#ifndef strlwr
#include <ctype.h>
#include <stddef.h>
/* strlwr converts upper case string to lower case
 * returns pointer to start of string
 *
 * !do not pass const char!
 */
char *
strlwr(char *s1)
{
    char *ret = s1;

    for (; *s1; s1++) *s1 = tolower(*s1);
    return ret;
}

#endif
#endif
