#include <unistd.h>
#include <stdint.h>
#include <stdlib.h>
#include <string.h>
#include "codec.h"

ssize_t trim_lf(char *string)
{
    char *tmp;
    char *p, *q;
    char c;

    tmp = strdup(string);
    if (tmp == NULL )
    {
        return -1;
    }

    p = tmp;
    q = string;
    while ((c = *p++) != 0)
    {
        if (c == '\n')
        {
            continue;
        }
        else
        {
            *q++ = c;
        }
    }

    *q = 0;
    free(tmp);

    return 0;
}
