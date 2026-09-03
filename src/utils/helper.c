#include "schaufel.h"
#include <stdlib.h>
#include <string.h>

#include "utils/array.h"
#include "utils/helper.h"


size_t number_length(long number)
{
    size_t count = 0;
    if( number == 0 || number < 0 )
        ++count;
    while( number != 0 )
    {
        number /= 10;
        ++count;
    }
    return count;
}

int
parse_connstring(const char *conninfo, char **hostname, int *port, char **socket_dir)
{
    const char *delim = ":";
    char *save,
         *port_str,
         *socket_str,
         *dup;
    int res = 0;

    if (socket_dir != NULL)
        *socket_dir = NULL;

    dup = strdup(conninfo);
    *hostname = strdup(strtok_r(dup, delim, &save));
    if (*hostname == NULL)
        res = -1;
    else if ((port_str = strtok_r(NULL, delim, &save)) == NULL)
        res = 1;
    else if ((*port = atoi(port_str)) == 0)
        res = -1;
    else if (socket_dir != NULL &&
             (socket_str = strtok_r(NULL, delim, &save)) != NULL)
        *socket_dir = strdup(socket_str);

    free(dup);
    return res;
}

/*
 * Replaces every literal \u0000 JSON escape in the null-terminated buf
 * with "??????", in place. Postgres's JSON parser rejects that escape
 * (it decodes to a NUL codepoint, which it can never store), so this
 * must run before the payload reaches Postgres. Returns 1 if anything
 * was replaced, 0 otherwise.
 */
int
repair_null_escape(char *buf)
{
    bool  dirty = false;
    char *p = buf;

    while ((p = strstr(p, "\\u0000")) != NULL)
    {
        memset(p, '?', 6);
        dirty = true;
        p += 6;
    }

    return dirty ? 1 : 0;
}

/*
 * Validates buf[0..len) as UTF-8 in place, replacing any invalid byte
 * with '?' so a single bad field doesn't sink the whole record.
 * Returns -1 if an embedded NUL is found (Postgres can never store one,
 * valid UTF-8 or not, so there is nothing to repair), 0 if buf was
 * already valid, 1 if one or more bytes were replaced.
 */
int
sanitize_utf8(char *buf, size_t len)
{
    size_t i = 0;
    bool   dirty = false;

    while (i < len)
    {
        unsigned char c0 = (unsigned char) buf[i];
        unsigned char c1, c2, c3;
        size_t        seqlen;
        bool          valid;

        if (c0 == 0x00)
            return -1;

        if (c0 < 0x80)
        {
            i += 1;
            continue;
        }
        else if ((c0 & 0xE0) == 0xC0)
            seqlen = 2;
        else if ((c0 & 0xF0) == 0xE0)
            seqlen = 3;
        else if ((c0 & 0xF8) == 0xF0)
            seqlen = 4;
        else
            seqlen = 0;

        valid = (seqlen >= 2) && (i + seqlen <= len);

        if (valid)
        {
            c1 = (unsigned char) buf[i + 1];
            valid = (c1 & 0xC0) == 0x80;
        }

        if (valid && seqlen == 2)
        {
            valid = c0 >= 0xC2; /* reject overlong 2-byte encodings */
        }
        else if (valid && seqlen == 3)
        {
            c2 = (unsigned char) buf[i + 2];
            valid = ((c2 & 0xC0) == 0x80) &&
                    !(c0 == 0xE0 && c1 < 0xA0) && /* overlong */
                    !(c0 == 0xED && c1 > 0x9F);   /* UTF-16 surrogate half */
        }
        else if (valid && seqlen == 4)
        {
            c2 = (unsigned char) buf[i + 2];
            c3 = (unsigned char) buf[i + 3];
            valid = ((c2 & 0xC0) == 0x80) &&
                    ((c3 & 0xC0) == 0x80) &&
                    (c0 <= 0xF4) &&
                    !(c0 == 0xF0 && c1 < 0x90) && /* overlong */
                    !(c0 == 0xF4 && c1 > 0x8F);   /* > U+10FFFF */
        }

        if (valid)
        {
            i += seqlen;
        }
        else
        {
            buf[i] = '?';
            dirty = true;
            i += 1;
        }
    }

    return dirty ? 1 : 0;
}

bool get_state(const volatile atomic_bool *state)
{
    #ifdef __clang__ // accomodate clangs opinionated stance on the spec
    return atomic_load((volatile atomic_bool *) state);
    #else
    return atomic_load(state);
    #endif
}

bool set_state(volatile atomic_bool *state, bool value)
{
    bool expected = !value;

    return atomic_compare_exchange_strong(state, &expected, value);
}

Array
_delimit_by(char *str, char* delim)
{
    if (str == NULL)
        return NULL;
    Array a = array_init(1);
    char *match,
         *save,
         *dup,
         *dup_old;

    dup = strdup(str);
    dup_old = dup;

    while ((match = strtok_r(dup, delim, &save)) != NULL)
    {
        dup = NULL;
        array_insert(a, match);
    }

    if (dup != NULL)
        return NULL;

    free(dup_old);
    return a;
}

Array
parse_hostinfo_master(char *hostinfo)
{
    char *delim1 = ";";
    Array a = _delimit_by(hostinfo, delim1);
    char *delim = ",";
    Array b = _delimit_by(array_get(a, 0), delim);
    array_free(&a);
    return b;
}

Array
parse_hostinfo_replica(char *hostinfo)
{
    char *delim1 = ";";
    Array a = _delimit_by(hostinfo, delim1);
    char *delim = ",";
    Array b = _delimit_by(array_get(a, 1), delim);
    array_free(&a);
    return b;
}
