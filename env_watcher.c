/*****************************************************************************
 * Copyright (C) 2013 Rémi Duraffort
 *
 * This program is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation, either version 2 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program. If not, see <http://www.gnu.org/licenses/>.
 *****************************************************************************/

#include <stdarg.h>         /* va_list, va_args, */
#include <stdio.h>          /* fprintf, */
#include <stdlib.h>         /* atoi, getenv */

#define __USE_GNU
#include <dlfcn.h>

/**
 * Hide most symboles by default and export only the hooks
 */
#if __GNUC__ >= 4
# define LOCAL  __attribute__ ((visibility ("hidden")))
# define GLOBAL __attribute__ ((visibility ("default")))
#else
# define GLOBAL
# define LOCAL
#endif

/**
 * The unlikely hint for the compiler as initialized check are unlikely to fail
 */
#ifdef __GNUC__
# define unlikely(p) __builtin_expect(!!(p), 0)
#else
# define unlikely(p) (!!(p))
#endif

/** Current version */
#define ENW_VERSION_MAJOR 0
#define ENW_VERSION_MINOR 1


/**
 * Global configuration
 */
LOCAL struct {
  int initialized;
  unsigned verbosity;

  /* Pointers to the original functions */
  struct {
    int   (*clearenv)(void);
    char* (*getenv)(const char *);
    int   (*putenv)(char *);
    int   (*setenv)(const char *, const char *, int);
    int   (*unsetenv)(const char *);
  } funcs;

} enw_config = { .initialized = 0,
                 .verbosity = 1 };

/**
 * Global function pointers
 */


/**
 * The logging levels from error to debug
 */
typedef enum
{
  ERROR = 1,
  WARNING = 2,
  DEBUG = 3
} log_level;

static const char *psz_log_level[] =
{
  "ERROR",
  "WARNING",
  "DEBUG"
};


#define PROLOGUE()                                  \
  if(unlikely(!enw_config.initialized))             \
    enw_init();                                     \
  enw_log(DEBUG, "Calling '%s'", __func__);

/**
 * Logging function for the env_watcher library
 * @param level: the level of the message
 * @param psz_fmt: the message to print
 * @return nothing
 */
static inline void enw_log(log_level level, const char *psz_fmt, ...)
{
  if(unlikely(level <= enw_config.verbosity))
  {
    if(level > 3) level = 3;

    va_list args;
    va_start(args, psz_fmt);
    fprintf(stderr, "[%s] ", psz_log_level[level - 1]);
    vfprintf(stderr, psz_fmt, args);
    fprintf(stderr, "\n");
    va_end(args);
  }
}


/**
 * Constructor function that read the environment variables
 */
LOCAL void __attribute__ ((constructor)) enw_init(void)
{
  /*
    The constructor function is not always the first function to be called.
    Indeed one of the hooks can be called by the constructor of a binary
    intiailialized before the env_watcher library.
    In this case the hook should call the constructor and then continue to
    execute the right code.
  */
  if(enw_config.initialized)
    return;
  enw_config.initialized = 1;

  /* Resolve the symboles that we will need afterward */
#define HOOK(name) enw_config.funcs.name = dlsym(RTLD_NEXT, #name)
  HOOK(clearenv);
  HOOK(getenv);
  HOOK(putenv);
  HOOK(setenv);
  HOOK(unsetenv);
#undef HOOK

  /* Fetch the configuration from the environment variables */
  const char *psz_verbosity = enw_config.funcs.getenv("ENW_VERBOSITY");
  if(psz_verbosity)
    enw_config.verbosity = atoi(psz_verbosity);

  /* Print some information about the configuration */
  enw_log(DEBUG, "env watcher v%d.%d initialization finished with:", ENW_VERSION_MAJOR, ENW_VERSION_MINOR);
  enw_log(DEBUG, " * verbosity=%d", enw_config.verbosity);
}


/**
 * The clearenv function
 */
GLOBAL int clearenv(void)
{
  PROLOGUE();

  return enw_config.funcs.clearenv();
}


/**
 * The getenv function
 */
GLOBAL char *getenv(const char *name)
{
  PROLOGUE();

  char *value = enw_config.funcs.getenv(name);

  enw_log(DEBUG, "\t%s => %s", name, value);

  return value;
}


/**
 * The putenv function
 */
GLOBAL int putenv(char *string)
{
  PROLOGUE();

  enw_log(DEBUG, "\t%s", string);

  return enw_config.funcs.putenv(string);
}


/**
 * the setenv function
 */
GLOBAL int setenv(const char *name, const char *value, int overwrite)
{
  PROLOGUE();

  enw_log(DEBUG, "\t%s => %s (%d)", name, value, overwrite);

  return enw_config.funcs.setenv(name, value, overwrite);
}


/**
 * The unsetenv function
 */
GLOBAL int unsetenv(const char *name)
{
  PROLOGUE();

  enw_log(DEBUG, "\t%s", name);

 return enw_config.funcs.unsetenv(name);
}
