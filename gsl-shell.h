#ifndef GSL_SHELL_INCLUDED
#define GSL_SHELL_INCLUDED

#include "defs.h"
#include <pthread.h>
#include <lua.h>

__BEGIN_DECLS

extern lua_State *globalL;
extern pthread_mutex_t gsl_shell_mutex[1];
extern pthread_mutex_t gsl_shell_shutdown_mutex[1];
extern volatile int gsl_shell_shutting_down;

__END_DECLS

#define GSL_SHELL_LOCK() pthread_mutex_lock (gsl_shell_mutex)
#define GSL_SHELL_UNLOCK() pthread_mutex_unlock (gsl_shell_mutex)

#endif
