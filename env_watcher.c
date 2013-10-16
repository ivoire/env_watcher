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
#include <stdbool.h>        /* true */
#include <unistd.h>         /* getpid */

#define __USE_GNU
#include <dlfcn.h>

#include <uthash.h>


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
typedef struct {
  unsigned int create :1;
  unsigned int read   :1;
  unsigned int update :1;
  unsigned int delete :1;
} Actions;


struct variable {
  char *name;         /* key for the hash table */
  char *value;        /* environment variable value */
  Actions actions;
  UT_hash_handle hh;  /* make the structure hashable for uthash */
};

LOCAL struct {
  int initialized;
  unsigned verbosity;

  struct variable *vars;

  /* Pointers to the original functions */
  struct {
    int   (*clearenv)(void);
    char* (*getenv)(const char *);
    int   (*putenv)(char *);
    int   (*setenv)(const char *, const char *, int);
    int   (*unsetenv)(const char *);
  } funcs;

} enw_config = { .initialized = 0,
                 .verbosity = 1,
                 .vars = NULL };


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
    enw_init();

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
 * Destructor function that dump the results
 */
LOCAL void enw_fini(void)
{
  enw_log(DEBUG, "Dumping the result file for %d", getpid());
  /* Get the name of the logfile from the environment */
  const char *psz_logfile = enw_config.funcs.getenv("ENW_RESULTS");
  if(!psz_logfile) psz_logfile = "results.yaml";

  FILE *fd = fopen(psz_logfile, "a");
  if(fd < 0)
    return;

  fprintf(fd, "- result:\n");
  fprintf(fd, "  pid: %d\n", getpid());
  fprintf(fd, "  variables:\n");

  struct variable *var;
  for(var = enw_config.vars; var != NULL; var = var->hh.next)
  {
    fprintf(fd, "  - name: \"%s\"\n", var->name);
    fprintf(fd, "    value: \"%s\"\n", var->value);
    fprintf(fd, "    actions: %d%d%d%d\n", var->actions.create,
            var->actions.read, var->actions.update, var->actions.delete);
  }
  fclose(fd);
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

  /* Register the atexit function */
  atexit(enw_fini);

  /* Print some information about the configuration */
  enw_log(DEBUG, "");
  enw_log(DEBUG, "================================================");
  enw_log(DEBUG, "env watcher v%d.%d initialization finished with:", ENW_VERSION_MAJOR, ENW_VERSION_MINOR);
  enw_log(DEBUG, " * verbosity=%d", enw_config.verbosity);
  enw_log(DEBUG, "================================================");
}


/**
 * The clearenv function
 */
GLOBAL int clearenv(void)
{
  PROLOGUE();
  enw_log(DEBUG, "clearenv()");

  return enw_config.funcs.clearenv();
}


/**
 * The getenv function
 */
GLOBAL char *getenv(const char *name)
{
  PROLOGUE();
  enw_log(DEBUG, "getenv(%s)", name);

  char *value = enw_config.funcs.getenv(name);
  enw_log(DEBUG, " -> %s", value);

  if(!value)
    return NULL;

  /* Add the key to the hash table */
  struct variable *var;
  HASH_FIND_STR(enw_config.vars, name, var);
  if(!var)
  {
    var = calloc(1, sizeof(*var));
    var->name = strdup(name);
    var->value = strdup(value);
    var->actions.read = true;
    HASH_ADD_KEYPTR(hh, enw_config.vars, var->name, strlen(var->name), var);
  }
  else
  {
    var->actions.read = true;
  }

  return value;
}


/**
 * The putenv function
 */
GLOBAL int putenv(char *string)
{
  PROLOGUE();
  enw_log(DEBUG, "putenv(%s)", string);

  int result = enw_config.funcs.putenv(string);

  if(result)
    return result;

  /* Copy the string to manipulate it */
  char *str = strdup(string);
  char* end = strchr(str, '=');
  if(end)
  {
    *end = '\0';
    struct variable *var;
    enw_log(DEBUG, " -> %s", str);
    enw_log(DEBUG, " -> %s", end + 1);
    HASH_FIND_STR(enw_config.vars, str, var);
    if(!var)
    {
      var = calloc(1, sizeof(*var));
      var->name = strdup(str);
      var->value = strdup(end + 1);
      var->actions.create = true;
      HASH_ADD_KEYPTR(hh, enw_config.vars, var->name, strlen(var->name), var);
    }
    else
    {
      free(var->value);
      var->value = strdup(end + 1);
      var->actions.update = true;
    }
  }
  free(str);
  return result;
}


/**
 * the setenv function
 */
GLOBAL int setenv(const char *name, const char *value, int overwrite)
{
  PROLOGUE();
  enw_log(DEBUG, "setenv(%s, %s, %d)", name, value, overwrite);

  /* Add the key to the hash table */
  struct variable *var;
  HASH_FIND_STR(enw_config.vars, name, var);
  if(!var)
  {
    var = calloc(1, sizeof(*var));
    var->name = strdup(name);
    var->value = strdup(value);
    var->actions.create = true;
    HASH_ADD_KEYPTR(hh, enw_config.vars, var->name, strlen(var->name), var);
  }
  /* Only overwrite the value if required */
  else if(overwrite)
  {
    free(var->value);
    var->value = strdup(value);
    var->actions.update = true;
  }

  return enw_config.funcs.setenv(name, value, overwrite);
}


/**
 * The unsetenv function
 */
GLOBAL int unsetenv(const char *name)
{
  PROLOGUE();

  enw_log(DEBUG, "unsetenv(%s)", name);

  struct variable *var;
  HASH_FIND_STR(enw_config.vars, name, var);
  if(var)
  {
    var->actions.delete = true;
  }

 return enw_config.funcs.unsetenv(name);
}
