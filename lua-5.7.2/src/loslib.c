/*
** $Id: loslib.c $
** Standard Operating System library
** See Copyright Notice in lua.h
*/

#define loslib_c
#define LUA_LIB

#include "lprefix.h"


#include <errno.h>
#include <locale.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>
#include <limits.h>
#include <stdint.h>
#if defined(_WIN32)
#define WIN32_LEAN_AND_MEAN
#include <windows.h>
#else
#include <spawn.h>
#include <sys/wait.h>
#include <unistd.h>
#include <fcntl.h>
#include <pthread.h>
#endif

#include "lua.h"

#include "lauxlib.h"
#include "lualib.h"
#include "llimits.h"


/*
** {==================================================================
** List of valid conversion specifiers for the 'strftime' function;
** options are grouped by length; group of length 2 start with '||'.
** ===================================================================
*/
#if !defined(LUA_STRFTIMEOPTIONS)	/* { */

#if defined(LUA_USE_WINDOWS)
#define LUA_STRFTIMEOPTIONS  "aAbBcdHIjmMpSUwWxXyYzZ%" \
    "||" "#c#x#d#H#I#j#m#M#S#U#w#W#y#Y"  /* two-char options */
#elif defined(LUA_USE_C89)  /* C89 (only 1-char options) */
#define LUA_STRFTIMEOPTIONS  "aAbBcdHIjmMpSUwWxXyYZ%"
#else  /* C99 specification */
#define LUA_STRFTIMEOPTIONS  "aAbBcCdDeFgGhHIjmMnprRStTuUVwWxXyYzZ%" \
    "||" "EcECExEXEyEY" "OdOeOHOIOmOMOSOuOUOVOwOWOy"  /* two-char options */
#endif

#endif					/* } */
/* }================================================================== */


/*
** {==================================================================
** Configuration for time-related stuff
** ===================================================================
*/

/*
** type to represent time_t in Lua
*/
#if !defined(LUA_NUMTIME)	/* { */

#define l_timet			lua_Integer
#define l_pushtime(L,t)		lua_pushinteger(L,(lua_Integer)(t))
#define l_gettime(L,arg)	luaL_checkinteger(L, arg)

#else				/* }{ */

#define l_timet			lua_Number
#define l_pushtime(L,t)		lua_pushnumber(L,(lua_Number)(t))
#define l_gettime(L,arg)	luaL_checknumber(L, arg)

#endif				/* } */


#if !defined(l_gmtime)		/* { */
/*
** By default, Lua uses gmtime/localtime, except when POSIX is available,
** where it uses gmtime_r/localtime_r
*/

#if defined(LUA_USE_POSIX)	/* { */

#define l_gmtime(t,r)		gmtime_r(t,r)
#define l_localtime(t,r)	localtime_r(t,r)

#else				/* }{ */

/* ISO C definitions */
#define l_gmtime(t,r)		((void)(r)->tm_sec, gmtime(t))
#define l_localtime(t,r)	((void)(r)->tm_sec, localtime(t))

#endif				/* } */

#endif				/* } */

/* }================================================================== */


/*
** {==================================================================
** Configuration for 'tmpnam':
** By default, Lua uses tmpnam except when POSIX is available, where
** it uses mkstemp.
** ===================================================================
*/
#if !defined(lua_tmpnam)	/* { */

#if defined(LUA_USE_POSIX)	/* { */

#include <unistd.h>

#define LUA_TMPNAMBUFSIZE	32

#if !defined(LUA_TMPNAMTEMPLATE)
#define LUA_TMPNAMTEMPLATE	"/tmp/lua_XXXXXX"
#endif

#define lua_tmpnam(b,e) { \
        strcpy(b, LUA_TMPNAMTEMPLATE); \
        e = mkstemp(b); \
        if (e != -1) close(e); \
        e = (e == -1); }

#else				/* }{ */

/* ISO C definitions */
#define LUA_TMPNAMBUFSIZE	L_tmpnam
#define lua_tmpnam(b,e)		{ e = (tmpnam(b) == NULL); }

#endif				/* } */

#endif				/* } */
/* }================================================================== */


#if !defined(l_system)
#if defined(LUA_USE_IOS)
/* Despite claiming to be ISO C, iOS does not implement 'system'. */
#define l_system(cmd) ((cmd) == NULL ? 0 : -1)
#else
#define l_system(cmd)	system(cmd)  /* default definition */
#endif
#endif


static int os_execute (lua_State *L) {
  const char *cmd = luaL_optstring(L, 1, NULL);
  int stat;
  errno = 0;
  stat = l_system(cmd);
  if (cmd != NULL)
    return luaL_execresult(L, stat);
  else {
    lua_pushboolean(L, stat);  /* true if there is a shell */
    return 1;
  }
}


/*
** Execute a native process without passing it through a shell.
** os.native_execute(command [, options])
** The historical os.native_execute(command, waitBoolean) form remains valid.
*/
typedef struct NativeExecuteOptions {
  int wait;
  int inherit_stdio;
  int detached;
  int return_pid;
  const char *cwd;
  size_t cwd_len;
  int cwd_set;
  int window;
  int window_set;
  int priority;
  int priority_set;
} NativeExecuteOptions;

static const char *const native_windows[] = {
  "hidden", "inherit", "normal", "minimized", "maximized", "new_console", NULL
};
static const char *const native_priorities[] = {
  "idle", "below_normal", "normal", "above_normal", "high", NULL
};

static void native_default_options (NativeExecuteOptions *o) {
  memset(o, 0, sizeof(*o));
  o->wait = 1;
#if defined(_WIN32)
  o->inherit_stdio = 0;
  o->window = 0;  /* hidden */
#else
  o->inherit_stdio = 1;
  o->window = 1;  /* inherited/default; Windows-only option otherwise */
#endif
  o->priority = 2;  /* normal */
}

static void native_read_options (lua_State *L, NativeExecuteOptions *o) {
  native_default_options(o);
  if (lua_isnoneornil(L, 2))
    return;
  if (lua_isboolean(L, 2)) {  /* compatibility with the previous API */
    o->wait = lua_toboolean(L, 2);
    return;
  }
  luaL_checktype(L, 2, LUA_TTABLE);

  lua_getfield(L, 2, "wait");
  if (!lua_isnil(L, -1)) {
    luaL_checktype(L, -1, LUA_TBOOLEAN);
    o->wait = lua_toboolean(L, -1);
  }
  lua_pop(L, 1);
  lua_getfield(L, 2, "inherit_stdio");
  if (!lua_isnil(L, -1)) {
    luaL_checktype(L, -1, LUA_TBOOLEAN);
    o->inherit_stdio = lua_toboolean(L, -1);
  }
  lua_pop(L, 1);
  lua_getfield(L, 2, "detached");
  if (!lua_isnil(L, -1)) {
    luaL_checktype(L, -1, LUA_TBOOLEAN);
    o->detached = lua_toboolean(L, -1);
  }
  lua_pop(L, 1);
  lua_getfield(L, 2, "return_pid");
  if (!lua_isnil(L, -1)) {
    luaL_checktype(L, -1, LUA_TBOOLEAN);
    o->return_pid = lua_toboolean(L, -1);
  }
  lua_pop(L, 1);

  lua_getfield(L, 2, "cwd");
  if (!lua_isnil(L, -1)) {
    o->cwd = luaL_checklstring(L, -1, &o->cwd_len);
    if (memchr(o->cwd, '\0', o->cwd_len) != NULL)
      luaL_argerror(L, 2, "cwd contains an embedded NUL byte");
    o->cwd_set = 1;
  }
  lua_pop(L, 1);

  lua_getfield(L, 2, "window");
  if (!lua_isnil(L, -1)) {
    o->window = luaL_checkoption(L, -1, NULL, native_windows);
    o->window_set = 1;
  }
  lua_pop(L, 1);

  lua_getfield(L, 2, "priority");
  if (!lua_isnil(L, -1)) {
    o->priority = luaL_checkoption(L, -1, NULL, native_priorities);
    o->priority_set = 1;
  }
  lua_pop(L, 1);

  if (o->return_pid && o->wait)
    luaL_error(L, "return_pid requires wait = false");
}

#if defined(_WIN32)

static wchar_t *native_utf8_to_wide (lua_State *L, const char *s, size_t len,
                                     DWORD *error) {
  int count;
  wchar_t *out;
  if (len > INT_MAX) {
    *error = ERROR_FILENAME_EXCED_RANGE;
    return NULL;
  }
  count = MultiByteToWideChar(CP_UTF8, MB_ERR_INVALID_CHARS, s, (int)len,
                              NULL, 0);
  if (count == 0) {
    *error = GetLastError();
    return NULL;
  }
  out = (wchar_t *)malloc(((size_t)count + 1) * sizeof(wchar_t));
  if (out == NULL)
    luaL_error(L, "not enough memory");
  if (MultiByteToWideChar(CP_UTF8, MB_ERR_INVALID_CHARS, s, (int)len,
                          out, count) == 0) {
    *error = GetLastError();
    free(out);
    return NULL;
  }
  out[count] = L'\0';
  return out;
}

static HANDLE native_duplicate_std_handle (DWORD which, DWORD *error) {
  HANDLE source = GetStdHandle(which);
  HANDLE copy = NULL;
  if (source == NULL || source == INVALID_HANDLE_VALUE)
    return NULL;
  if (!DuplicateHandle(GetCurrentProcess(), source, GetCurrentProcess(),
                       &copy, 0, TRUE, DUPLICATE_SAME_ACCESS)) {
    *error = GetLastError();
    return INVALID_HANDLE_VALUE;
  }
  return copy;
}

static int os_native_execute (lua_State *L) {
  size_t len;
  const char *cmd = luaL_checklstring(L, 1, &len);
  NativeExecuteOptions options;
  int wlen;
  wchar_t *wcmd, *wcwd = NULL;
  STARTUPINFOW si;
  PROCESS_INFORMATION pi;
  DWORD error = ERROR_SUCCESS, flags = 0, result;
  HANDLE hin = NULL, hout = NULL, herr = NULL;
  BOOL inherit_handles = FALSE;

  native_read_options(L, &options);
  if (len == 0 || memchr(cmd, '\0', len) != NULL)
    return luaL_argerror(L, 1, "invalid command line");
  if (len > INT_MAX)
    return luaL_argerror(L, 1, "command line too long");

  wcmd = native_utf8_to_wide(L, cmd, len, &error);
  if (wcmd == NULL) {
    lua_pushnil(L);
    lua_pushinteger(L, (lua_Integer)error);
    return 2;
  }
  if (options.cwd_set) {
    wcwd = native_utf8_to_wide(L, options.cwd, options.cwd_len, &error);
    if (wcwd == NULL) {
      free(wcmd);
      lua_pushnil(L);
      lua_pushinteger(L, (lua_Integer)error);
      return 2;
    }
  }

  memset(&si, 0, sizeof(si));
  memset(&pi, 0, sizeof(pi));
  si.cb = sizeof(si);

  if (options.window_set || options.window == 0) {
    si.dwFlags |= STARTF_USESHOWWINDOW;
    switch (options.window) {
      case 0: si.wShowWindow = SW_HIDE; flags |= CREATE_NO_WINDOW; break;
      case 1: si.dwFlags &= ~STARTF_USESHOWWINDOW; break;
      case 2: si.wShowWindow = SW_SHOWNORMAL; break;
      case 3: si.wShowWindow = SW_SHOWMINIMIZED; break;
      case 4: si.wShowWindow = SW_SHOWMAXIMIZED; break;
      case 5: si.dwFlags &= ~STARTF_USESHOWWINDOW; flags |= CREATE_NEW_CONSOLE; break;
    }
  }
  if (options.detached) {
    if (options.window == 5) {
      free(wcmd); free(wcwd);
      return luaL_error(L, "window = 'new_console' cannot be combined with detached = true");
    }
    flags &= ~CREATE_NO_WINDOW;
    flags |= DETACHED_PROCESS | CREATE_NEW_PROCESS_GROUP;
  }
  {
    static const DWORD priority_flags[] = {
      IDLE_PRIORITY_CLASS, BELOW_NORMAL_PRIORITY_CLASS, NORMAL_PRIORITY_CLASS,
      ABOVE_NORMAL_PRIORITY_CLASS, HIGH_PRIORITY_CLASS
    };
    flags |= priority_flags[options.priority];
  }
  if (options.inherit_stdio) {
    hin = native_duplicate_std_handle(STD_INPUT_HANDLE, &error);
    if (hin == INVALID_HANDLE_VALUE) goto launch_error;
    hout = native_duplicate_std_handle(STD_OUTPUT_HANDLE, &error);
    if (hout == INVALID_HANDLE_VALUE) goto launch_error;
    herr = native_duplicate_std_handle(STD_ERROR_HANDLE, &error);
    if (herr == INVALID_HANDLE_VALUE) goto launch_error;
    si.dwFlags |= STARTF_USESTDHANDLES;
    si.hStdInput = hin;
    si.hStdOutput = hout;
    si.hStdError = herr;
    inherit_handles = TRUE;
  }

  if (!CreateProcessW(NULL, wcmd, NULL, NULL, inherit_handles, flags,
                      NULL, wcwd, &si, &pi)) {
    error = GetLastError();
    goto launch_error;
  }
  free(wcmd); free(wcwd);
  if (hin != NULL) CloseHandle(hin);
  if (hout != NULL) CloseHandle(hout);
  if (herr != NULL) CloseHandle(herr);
  CloseHandle(pi.hThread);

  if (!options.wait) {
    CloseHandle(pi.hProcess);
    if (options.return_pid)
      lua_pushinteger(L, (lua_Integer)pi.dwProcessId);
    else
      lua_pushboolean(L, 1);
    return 1;
  }
  result = WaitForSingleObject(pi.hProcess, INFINITE);
  if (result != WAIT_OBJECT_0) {
    error = GetLastError();
    CloseHandle(pi.hProcess);
    lua_pushnil(L); lua_pushinteger(L, (lua_Integer)error); return 2;
  }
  if (!GetExitCodeProcess(pi.hProcess, &result)) {
    error = GetLastError();
    CloseHandle(pi.hProcess);
    lua_pushnil(L); lua_pushinteger(L, (lua_Integer)error); return 2;
  }
  CloseHandle(pi.hProcess);
  lua_pushinteger(L, (lua_Integer)result);
  return 1;

launch_error:
  free(wcmd); free(wcwd);
  if (hin != NULL && hin != INVALID_HANDLE_VALUE) CloseHandle(hin);
  if (hout != NULL && hout != INVALID_HANDLE_VALUE) CloseHandle(hout);
  if (herr != NULL && herr != INVALID_HANDLE_VALUE) CloseHandle(herr);
  lua_pushnil(L);
  lua_pushinteger(L, (lua_Integer)(error == ERROR_SUCCESS ? GetLastError() : error));
  return 2;
}

#else  /* POSIX platforms, including macOS */

extern char **environ;

static void *native_reap_child (void *arg) {
  pid_t pid = *(pid_t *)arg;
  int status;
  free(arg);
  while (waitpid(pid, &status, 0) == -1 && errno == EINTR)
    ;
  return NULL;
}

/* Split a command line into argv without shell expansion. */
static int native_parse_argv (const char *cmd, size_t len,
                              char **storage, char ***argv_out) {
  char *buf;
  char **argv;
  size_t r = 0, w = 0, argc = 0;
  int quote = 0;
  if (len > (SIZE_MAX / sizeof(char *)) - 2) return -1;
  buf = (char *)malloc(len + 1);
  argv = (char **)malloc((len + 2) * sizeof(char *));
  if (buf == NULL || argv == NULL) { free(buf); free(argv); return -1; }
  memcpy(buf, cmd, len); buf[len] = '\0';
  while (r < len) {
    while (r < len && (buf[r] == ' ' || buf[r] == '\t' || buf[r] == '\n' || buf[r] == '\r')) r++;
    if (r >= len) break;
    argv[argc++] = buf + w; quote = 0;
    while (r < len) {
      char c = buf[r++];
      if (quote == 0 && (c == ' ' || c == '\t' || c == '\n' || c == '\r')) break;
      if (quote != '\'' && c == '\\') {
        if (r >= len) { free(argv); free(buf); return -1; }
        buf[w++] = buf[r++];
      }
      else if (c == '\'' && quote != '"') quote = (quote == '\'') ? 0 : '\'';
      else if (c == '"' && quote != '\'') quote = (quote == '"') ? 0 : '"';
      else buf[w++] = c;
    }
    if (quote != 0) { free(argv); free(buf); return -1; }
    buf[w++] = '\0';
  }
  if (argc == 0) { free(argv); free(buf); return -1; }
  argv[argc] = NULL; *storage = buf; *argv_out = argv; return 0;
}

static int os_native_execute (lua_State *L) {
  size_t len;
  const char *cmd = luaL_checklstring(L, 1, &len);
  NativeExecuteOptions options;
  char *storage = NULL;
  char **argv = NULL;
  pid_t pid;
  int err;
  posix_spawn_file_actions_t actions;
  posix_spawn_file_actions_t *actionsp = NULL;
  posix_spawnattr_t attr;
  posix_spawnattr_t *attrp = NULL;
  int actions_ready = 0, attr_ready = 0;
  short attrflags = 0;

  native_read_options(L, &options);
  if (options.window_set || options.priority_set)
    return luaL_error(L, "window and priority options are only supported on Windows");
  if (len == 0 || memchr(cmd, '\0', len) != NULL)
    return luaL_argerror(L, 1, "invalid command line");
  if (native_parse_argv(cmd, len, &storage, &argv) != 0)
    return luaL_argerror(L, 1, "invalid command line (check quoting and arguments)");

  if (options.cwd_set || !options.inherit_stdio) {
    err = posix_spawn_file_actions_init(&actions);
    if (err != 0) goto spawn_error;
    actions_ready = 1; actionsp = &actions;
    if (!options.inherit_stdio) {
      if ((err = posix_spawn_file_actions_addopen(&actions, STDIN_FILENO, "/dev/null", O_RDONLY, 0)) != 0 ||
          (err = posix_spawn_file_actions_addopen(&actions, STDOUT_FILENO, "/dev/null", O_WRONLY, 0)) != 0 ||
          (err = posix_spawn_file_actions_addopen(&actions, STDERR_FILENO, "/dev/null", O_WRONLY, 0)) != 0)
        goto spawn_error;
    }
    if (options.cwd_set) {
#if defined(__APPLE__)
      /* macOS exposes the non-suffixed API in <spawn.h>. */
      err = posix_spawn_file_actions_addchdir(&actions, options.cwd);
      if (err != 0) goto spawn_error;
#elif defined(__GLIBC__)
      /* glibc exposes this extension with the _np suffix. */
      err = posix_spawn_file_actions_addchdir_np(&actions, options.cwd);
      if (err != 0) goto spawn_error;
#else
      err = ENOTSUP;
      goto spawn_error;
#endif
    }
  }
  if (options.detached) {
    err = posix_spawnattr_init(&attr);
    if (err != 0) goto spawn_error;
    attr_ready = 1; attrp = &attr;
    err = posix_spawnattr_setpgroup(&attr, 0);
    if (err != 0) goto spawn_error;
    attrflags = POSIX_SPAWN_SETPGROUP;
    err = posix_spawnattr_setflags(&attr, attrflags);
    if (err != 0) goto spawn_error;
  }

  err = posix_spawnp(&pid, argv[0], actionsp, attrp, argv, environ);
  if (actions_ready) posix_spawn_file_actions_destroy(&actions);
  if (attr_ready) posix_spawnattr_destroy(&attr);
  free(argv); free(storage);
  if (err != 0) { lua_pushnil(L); lua_pushinteger(L, (lua_Integer)err); return 2; }

  if (!options.wait) {
    pid_t *child = (pid_t *)malloc(sizeof(pid_t));
    pthread_t reaper;
    int thread_error;
    if (child == NULL) {
      do { err = waitpid(pid, NULL, 0) == -1 ? errno : 0; } while (err == EINTR);
      if (options.return_pid && err == 0) { lua_pushinteger(L, (lua_Integer)pid); return 1; }
      lua_pushboolean(L, 1); return 1;
    }
    *child = pid;
    thread_error = pthread_create(&reaper, NULL, native_reap_child, child);
    if (thread_error == 0) {
      (void)pthread_detach(reaper);
      if (options.return_pid) lua_pushinteger(L, (lua_Integer)pid);
      else lua_pushboolean(L, 1);
      return 1;
    }
    free(child);
    do { err = waitpid(pid, NULL, 0) == -1 ? errno : 0; } while (err == EINTR);
    if (err != 0) { lua_pushnil(L); lua_pushinteger(L, (lua_Integer)err); return 2; }
    if (options.return_pid) lua_pushinteger(L, (lua_Integer)pid);
    else lua_pushboolean(L, 1);
    return 1;
  }
  {
    int status;
    pid_t waited;
    do { waited = waitpid(pid, &status, 0); } while (waited == -1 && errno == EINTR);
    if (waited == -1) { lua_pushnil(L); lua_pushinteger(L, (lua_Integer)errno); return 2; }
    if (WIFEXITED(status)) lua_pushinteger(L, (lua_Integer)WEXITSTATUS(status));
    else if (WIFSIGNALED(status)) lua_pushinteger(L, (lua_Integer)(128 + WTERMSIG(status)));
    else lua_pushinteger(L, (lua_Integer)status);
    return 1;
  }

spawn_error:
  if (actions_ready) posix_spawn_file_actions_destroy(&actions);
  if (attr_ready) posix_spawnattr_destroy(&attr);
  free(argv); free(storage);
  lua_pushnil(L); lua_pushinteger(L, (lua_Integer)err); return 2;
}

#endif  /* platform-specific native execution */



static int os_remove (lua_State *L) {
  const char *filename = luaL_checkstring(L, 1);
  errno = 0;
  return luaL_fileresult(L, remove(filename) == 0, filename);
}


static int os_rename (lua_State *L) {
  const char *fromname = luaL_checkstring(L, 1);
  const char *toname = luaL_checkstring(L, 2);
  errno = 0;
  return luaL_fileresult(L, rename(fromname, toname) == 0, NULL);
}


static int os_tmpname (lua_State *L) {
  char buff[LUA_TMPNAMBUFSIZE];
  int err;
  lua_tmpnam(buff, err);
  if (l_unlikely(err))
    return luaL_error(L, "unable to generate a unique filename");
  lua_pushstring(L, buff);
  return 1;
}


static int os_getenv (lua_State *L) {
  lua_pushstring(L, getenv(luaL_checkstring(L, 1)));  /* if NULL push nil */
  return 1;
}


static int os_clock (lua_State *L) {
  lua_pushnumber(L, ((lua_Number)clock())/(lua_Number)CLOCKS_PER_SEC);
  return 1;
}


/*
** {======================================================
** Time/Date operations
** { year=%Y, month=%m, day=%d, hour=%H, min=%M, sec=%S,
**   wday=%w+1, yday=%j, isdst=? }
** =======================================================
*/

/*
** About the overflow check: an overflow cannot occur when time
** is represented by a lua_Integer, because either lua_Integer is
** large enough to represent all int fields or it is not large enough
** to represent a time that cause a field to overflow.  However, if
** times are represented as doubles and lua_Integer is int, then the
** time 0x1.e1853b0d184f6p+55 would cause an overflow when adding 1900
** to compute the year.
*/
static void setfield (lua_State *L, const char *key, int value, int delta) {
  #if (defined(LUA_NUMTIME) && LUA_MAXINTEGER <= INT_MAX)
    if (l_unlikely(value > LUA_MAXINTEGER - delta))
      luaL_error(L, "field '%s' is out-of-bound", key);
  #endif
  lua_pushinteger(L, (lua_Integer)value + delta);
  lua_setfield(L, -2, key);
}


static void setboolfield (lua_State *L, const char *key, int value) {
  if (value < 0)  /* undefined? */
    return;  /* does not set field */
  lua_pushboolean(L, value);
  lua_setfield(L, -2, key);
}


/*
** Set all fields from structure 'tm' in the table on top of the stack
*/
static void setallfields (lua_State *L, struct tm *stm) {
  setfield(L, "year", stm->tm_year, 1900);
  setfield(L, "month", stm->tm_mon, 1);
  setfield(L, "day", stm->tm_mday, 0);
  setfield(L, "hour", stm->tm_hour, 0);
  setfield(L, "min", stm->tm_min, 0);
  setfield(L, "sec", stm->tm_sec, 0);
  setfield(L, "yday", stm->tm_yday, 1);
  setfield(L, "wday", stm->tm_wday, 1);
  setboolfield(L, "isdst", stm->tm_isdst);
}


static int getboolfield (lua_State *L, const char *key) {
  int res;
  res = (lua_getfield(L, -1, key) == LUA_TNIL) ? -1 : lua_toboolean(L, -1);
  lua_pop(L, 1);
  return res;
}


static int getfield (lua_State *L, const char *key, int d, int delta) {
  int isnum;
  int t = lua_getfield(L, -1, key);  /* get field and its type */
  lua_Integer res = lua_tointegerx(L, -1, &isnum);
  if (!isnum) {  /* field is not an integer? */
    if (l_unlikely(t != LUA_TNIL))  /* some other value? */
      return luaL_error(L, "field '%s' is not an integer", key);
    else if (l_unlikely(d < 0))  /* absent field; no default? */
      return luaL_error(L, "field '%s' missing in date table", key);
    res = d;
  }
  else {
    if (!(res >= 0 ? res - delta <= INT_MAX : INT_MIN + delta <= res))
      return luaL_error(L, "field '%s' is out-of-bound", key);
    res -= delta;
  }
  lua_pop(L, 1);
  return (int)res;
}


static const char *checkoption (lua_State *L, const char *conv,
                                size_t convlen, char *buff) {
  const char *option = LUA_STRFTIMEOPTIONS;
  unsigned oplen = 1;  /* length of options being checked */
  for (; *option != '\0' && oplen <= convlen; option += oplen) {
    if (*option == '|')  /* next block? */
      oplen++;  /* will check options with next length (+1) */
    else if (memcmp(conv, option, oplen) == 0) {  /* match? */
      memcpy(buff, conv, oplen);  /* copy valid option to buffer */
      buff[oplen] = '\0';
      return conv + oplen;  /* return next item */
    }
  }
  luaL_argerror(L, 1,
    lua_pushfstring(L, "invalid conversion specifier '%%%s'", conv));
  return conv;  /* to avoid warnings */
}


static time_t l_checktime (lua_State *L, int arg) {
  l_timet t = l_gettime(L, arg);
  luaL_argcheck(L, (time_t)t == t, arg, "time out-of-bounds");
  return (time_t)t;
}


/* maximum size for an individual 'strftime' item */
#define SIZETIMEFMT	250


static int os_date (lua_State *L) {
  size_t slen;
  const char *s = luaL_optlstring(L, 1, "%c", &slen);
  time_t t = luaL_opt(L, l_checktime, 2, time(NULL));
  const char *se = s + slen;  /* 's' end */
  struct tm tmr, *stm;
  if (*s == '!') {  /* UTC? */
    stm = l_gmtime(&t, &tmr);
    s++;  /* skip '!' */
  }
  else
    stm = l_localtime(&t, &tmr);
  if (stm == NULL)  /* invalid date? */
    return luaL_error(L,
                 "date result cannot be represented in this installation");
  if (strcmp(s, "*t") == 0) {
    lua_createtable(L, 0, 9);  /* 9 = number of fields */
    setallfields(L, stm);
  }
  else {
    char cc[4];  /* buffer for individual conversion specifiers */
    luaL_Buffer b;
    cc[0] = '%';
    luaL_buffinit(L, &b);
    while (s < se) {
      if (*s != '%')  /* not a conversion specifier? */
        luaL_addchar(&b, *s++);
      else {
        size_t reslen;
        char *buff = luaL_prepbuffsize(&b, SIZETIMEFMT);
        s++;  /* skip '%' */
        /* copy specifier to 'cc' */
        s = checkoption(L, s, ct_diff2sz(se - s), cc + 1);
        reslen = strftime(buff, SIZETIMEFMT, cc, stm);
        luaL_addsize(&b, reslen);
      }
    }
    luaL_pushresult(&b);
  }
  return 1;
}


static int os_time (lua_State *L) {
  time_t t;
  if (lua_isnoneornil(L, 1))  /* called without args? */
    t = time(NULL);  /* get current time */
  else {
    struct tm ts;
    luaL_checktype(L, 1, LUA_TTABLE);
    lua_settop(L, 1);  /* make sure table is at the top */
    ts.tm_year = getfield(L, "year", -1, 1900);
    ts.tm_mon = getfield(L, "month", -1, 1);
    ts.tm_mday = getfield(L, "day", -1, 0);
    ts.tm_hour = getfield(L, "hour", 12, 0);
    ts.tm_min = getfield(L, "min", 0, 0);
    ts.tm_sec = getfield(L, "sec", 0, 0);
    ts.tm_isdst = getboolfield(L, "isdst");
    t = mktime(&ts);
    setallfields(L, &ts);  /* update fields with normalized values */
  }
  if (t != (time_t)(l_timet)t || t == (time_t)(-1))
    return luaL_error(L,
                  "time result cannot be represented in this installation");
  l_pushtime(L, t);
  return 1;
}


static int os_difftime (lua_State *L) {
  time_t t1 = l_checktime(L, 1);
  time_t t2 = l_checktime(L, 2);
  lua_pushnumber(L, (lua_Number)difftime(t1, t2));
  return 1;
}

/* }====================================================== */


static int os_setlocale (lua_State *L) {
  static const int cat[] = {LC_ALL, LC_COLLATE, LC_CTYPE, LC_MONETARY,
                      LC_NUMERIC, LC_TIME};
  static const char *const catnames[] = {"all", "collate", "ctype", "monetary",
     "numeric", "time", NULL};
  const char *l = luaL_optstring(L, 1, NULL);
  int op = luaL_checkoption(L, 2, "all", catnames);
  lua_pushstring(L, setlocale(cat[op], l));
  return 1;
}


static int os_exit (lua_State *L) {
  int status;
  if (lua_isboolean(L, 1))
    status = (lua_toboolean(L, 1) ? EXIT_SUCCESS : EXIT_FAILURE);
  else
    status = (int)luaL_optinteger(L, 1, EXIT_SUCCESS);
  if (lua_toboolean(L, 2))
    lua_close(L);
  if (L) exit(status);  /* 'if' to avoid warnings for unreachable 'return' */
  return 0;
}


static const luaL_Reg syslib[] = {
  {"clock",     os_clock},
  {"date",      os_date},
  {"difftime",  os_difftime},
  {"execute",   os_execute},
  {"native_execute", os_native_execute},
  {"exit",      os_exit},
  {"getenv",    os_getenv},
  {"remove",    os_remove},
  {"rename",    os_rename},
  {"setlocale", os_setlocale},
  {"time",      os_time},
  {"tmpname",   os_tmpname},
  {NULL, NULL}
};

/* }====================================================== */



LUAMOD_API int luaopen_os (lua_State *L) {
  luaL_newlib(L, syslib);
  return 1;
}

