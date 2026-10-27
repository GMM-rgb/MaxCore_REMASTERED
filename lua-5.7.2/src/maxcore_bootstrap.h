#ifndef MAXCORE_BOOTSTRAP_H
#define MAXCORE_BOOTSTRAP_H

#include "lua.h"

/*
** Initializes MaxCore and exposes the resulting core table as EngineCore.
** Also generates LuaLS metadata for the discovered runtime API.
*/
void MaxCore_Initialize(lua_State *L);

#endif
