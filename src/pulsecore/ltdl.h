#ifndef LTDL_H
#define LTDL_H

#include <dlfcn.h>
#include <stdlib.h>

typedef void * lt_dlhandle;
typedef void * lt_ptr;
typedef struct { const char *name; void *address; } lt_dlsymlist;

#define lt_dlsym dlsym
#define lt_dlclose dlclose
#define lt_dlerror dlerror
#define lt_dlinit() 0
#define lt_dlexit() 0

#define LTDL_SET_PRELOADED_SYMBOLS()

static char *
lt_dlgetsearchpath(void)
{
    return getenv("LD_LIBRARY_PATH");
}

static void *
lt_dlopenext(char *name)
{
    char buf[256];
    sprintf(buf, "/system/lib/pulse/%s.so", name);
    return dlopen(buf, RTLD_NOW);
}

#endif /* LTDL_H */
