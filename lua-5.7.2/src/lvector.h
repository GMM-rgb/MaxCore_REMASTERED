#ifndef lvector_h
#define lvector_h

#include "lua.h"

/*
** MaxCore vector userdata.
**
** Vector2 uses X and Y.
** Vector3 uses X, Y and Z.
*/
typedef struct MaxCoreVector {
    lua_Number x;
    lua_Number y;
    lua_Number z;
} MaxCoreVector;

/* Register Vector2 and Vector3 into the Lua state. */
LUAMOD_API void Vector_Register(lua_State *L);

#endif
