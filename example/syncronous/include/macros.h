#ifndef MACROS_H
#define MACROS_H

#ifndef CLEAR
#define CLEAR(array) memset(array, 0, sizeof(array));
#endif

#ifndef CLEARSTRUCT
#define CLEARSTRUCT(str) memset(&str, 0, sizeof(str));
#endif

#ifndef FREE
#define FREE(ptr) if(ptr != NULL) {free(ptr); ptr = NULL;}
#endif

#ifndef COPYSTRING
#define COPYSTRING(dest, src) snprintf(dest, sizeof(dest) - 1, "%s", src);
#endif

#ifndef ASSIGN
#define ASSIGN(dest, src) dest = (typeof(dest)) src;
#endif


#endif