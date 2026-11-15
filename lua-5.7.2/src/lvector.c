#include <math.h>
#include <string.h>
#include <stdio.h>
#include <lprefix.h>
#include "lua.h"
#include "lauxlib.h"
#include "lvector.h"

#define VECTOR2_METATABLE "MaxCore.Vector2"
#define VECTOR3_METATABLE "MaxCore.Vector3"

typedef enum VectorOperation {
    VECTOR_ADD,
    VECTOR_SUB,
    VECTOR_MULT,
    VECTOR_DIV
} VectorOperation;

/* Internal helpers */

static MaxCoreVector *Vector_Check(lua_State *L, int Index, const char *Metatable) {
    return (MaxCoreVector *)luaL_checkudata(L, Index, Metatable);
}

static MaxCoreVector *Vector_Push(lua_State *L, const char *Metatable, lua_Number X, lua_Number Y, lua_Number Z) {
    MaxCoreVector *Result = (MaxCoreVector *)lua_newuserdatauv(L, sizeof(MaxCoreVector), 0);
    Result->x = X;
    Result->y = Y;
    Result->z = Z;
    luaL_setmetatable(L, Metatable);
    return Result;
}

/* Shared arithmetic */

static int Vector_Operate(lua_State *L, const char *Metatable, VectorOperation Operation, int Dimensions) {
    MaxCoreVector *A = Vector_Check(L, 1, Metatable);
    MaxCoreVector *B = Vector_Check(L, 2, Metatable);
    lua_Number X, Y, Z = 0;

    switch (Operation) {
        case VECTOR_ADD:
            X = A->x + B->x; Y = A->y + B->y;
            if (Dimensions == 3) Z = A->z + B->z;
            break;
        case VECTOR_SUB:
            X = A->x - B->x; Y = A->y - B->y;
            if (Dimensions == 3) Z = A->z - B->z;
            break;
        case VECTOR_MULT:
            X = A->x * B->x; Y = A->y * B->y;
            if (Dimensions == 3) Z = A->z * B->z;
            break;
        case VECTOR_DIV:
            X = A->x / B->x; Y = A->y / B->y;
            if (Dimensions == 3) Z = A->z / B->z;
            break;
        default:
            luaL_error(L, "unknown vector operation");
    }

    Vector_Push(L, Metatable, X, Y, Z);
    return 1;
}

static int Vector2_Add(lua_State *L) { return Vector_Operate(L, VECTOR2_METATABLE, VECTOR_ADD, 2); }
static int Vector2_Sub(lua_State *L) { return Vector_Operate(L, VECTOR2_METATABLE, VECTOR_SUB, 2); }
static int Vector2_Mult(lua_State *L) { return Vector_Operate(L, VECTOR2_METATABLE, VECTOR_MULT, 2); }
static int Vector2_Div(lua_State *L) { return Vector_Operate(L, VECTOR2_METATABLE, VECTOR_DIV, 2); }
static int Vector3_Add(lua_State *L) { return Vector_Operate(L, VECTOR3_METATABLE, VECTOR_ADD, 3); }
static int Vector3_Sub(lua_State *L) { return Vector_Operate(L, VECTOR3_METATABLE, VECTOR_SUB, 3); }
static int Vector3_Mult(lua_State *L) { return Vector_Operate(L, VECTOR3_METATABLE, VECTOR_MULT, 3); }
static int Vector3_Div(lua_State *L) { return Vector_Operate(L, VECTOR3_METATABLE, VECTOR_DIV, 3); }

/* Unary negation */

static int Vector2_Unm(lua_State *L) {
    MaxCoreVector *A = Vector_Check(L, 1, VECTOR2_METATABLE);
    Vector_Push(L, VECTOR2_METATABLE, -A->x, -A->y, 0);
    return 1;
}

static int Vector3_Unm(lua_State *L) {
    MaxCoreVector *A = Vector_Check(L, 1, VECTOR3_METATABLE);
    Vector_Push(L, VECTOR3_METATABLE, -A->x, -A->y, -A->z);
    return 1;
}

/* Equality */

static int Vector2_Eq(lua_State *L) {
    MaxCoreVector *A = Vector_Check(L, 1, VECTOR2_METATABLE);
    MaxCoreVector *B = Vector_Check(L, 2, VECTOR2_METATABLE);
    lua_pushboolean(L, A->x == B->x && A->y == B->y);
    return 1;
}

static int Vector3_Eq(lua_State *L) {
    MaxCoreVector *A = Vector_Check(L, 1, VECTOR3_METATABLE);
    MaxCoreVector *B = Vector_Check(L, 2, VECTOR3_METATABLE);
    lua_pushboolean(L, A->x == B->x && A->y == B->y && A->z == B->z);
    return 1;
}

/* Magnitude and Unit */

static lua_Number Vector2_Magnitude(MaxCoreVector *V) { return sqrt(V->x * V->x + V->y * V->y); }
static lua_Number Vector3_Magnitude(MaxCoreVector *V) { return sqrt(V->x * V->x + V->y * V->y + V->z * V->z); }

static int Vector2_Unit(lua_State *L) {
    MaxCoreVector *V = Vector_Check(L, 1, VECTOR2_METATABLE);
    lua_Number Magnitude = Vector2_Magnitude(V);
    if (Magnitude == 0) Vector_Push(L, VECTOR2_METATABLE, 0, 0, 0);
    else Vector_Push(L, VECTOR2_METATABLE, V->x / Magnitude, V->y / Magnitude, 0);
    return 1;
}

static int Vector3_Unit(lua_State *L) {
    MaxCoreVector *V = Vector_Check(L, 1, VECTOR3_METATABLE);
    lua_Number Magnitude = Vector3_Magnitude(V);
    if (Magnitude == 0) Vector_Push(L, VECTOR3_METATABLE, 0, 0, 0);
    else Vector_Push(L, VECTOR3_METATABLE, V->x / Magnitude, V->y / Magnitude, V->z / Magnitude);
    return 1;
}

/* Dot product */

static int Vector2_Dot(lua_State *L) {
    MaxCoreVector *A = Vector_Check(L, 1, VECTOR2_METATABLE);
    MaxCoreVector *B = Vector_Check(L, 2, VECTOR2_METATABLE);
    lua_pushnumber(L, A->x * B->x + A->y * B->y);
    return 1;
}

static int Vector3_Dot(lua_State *L) {
    MaxCoreVector *A = Vector_Check(L, 1, VECTOR3_METATABLE);
    MaxCoreVector *B = Vector_Check(L, 2, VECTOR3_METATABLE);
    lua_pushnumber(L, A->x * B->x + A->y * B->y + A->z * B->z);
    return 1;
}

/* Cross product */

static int Vector3_Cross(lua_State *L) {
    MaxCoreVector *A = Vector_Check(L, 1, VECTOR3_METATABLE);
    MaxCoreVector *B = Vector_Check(L, 2, VECTOR3_METATABLE);
    Vector_Push(L, VECTOR3_METATABLE, A->y * B->z - A->z * B->y, A->z * B->x - A->x * B->z, A->x * B->y - A->y * B->x);
    return 1;
}

/* Property access */

static int Vector2_Index(lua_State *L) {
    MaxCoreVector *V = Vector_Check(L, 1, VECTOR2_METATABLE);
    const char *Key = luaL_checkstring(L, 2);

    if (strcmp(Key, "X") == 0 || strcmp(Key, "x") == 0) lua_pushnumber(L, V->x);
    else if (strcmp(Key, "Y") == 0 || strcmp(Key, "y") == 0) lua_pushnumber(L, V->y);
    else if (strcmp(Key, "Magnitude") == 0) lua_pushnumber(L, Vector2_Magnitude(V));
    else if (strcmp(Key, "Unit") == 0) return Vector2_Unit(L);
    else if (strcmp(Key, "Dot") == 0) lua_pushcfunction(L, Vector2_Dot);
    else lua_pushnil(L);

    return 1;
}

static int Vector3_Index(lua_State *L) {
    MaxCoreVector *V = Vector_Check(L, 1, VECTOR3_METATABLE);
    const char *Key = luaL_checkstring(L, 2);

    if (strcmp(Key, "X") == 0 || strcmp(Key, "x") == 0) lua_pushnumber(L, V->x);
    else if (strcmp(Key, "Y") == 0 || strcmp(Key, "y") == 0) lua_pushnumber(L, V->y);
    else if (strcmp(Key, "Z") == 0 || strcmp(Key, "z") == 0) lua_pushnumber(L, V->z);
    else if (strcmp(Key, "Magnitude") == 0) lua_pushnumber(L, Vector3_Magnitude(V));
    else if (strcmp(Key, "Unit") == 0) return Vector3_Unit(L);
    else if (strcmp(Key, "Dot") == 0) lua_pushcfunction(L, Vector3_Dot);
    else if (strcmp(Key, "Cross") == 0) lua_pushcfunction(L, Vector3_Cross);
    else lua_pushnil(L);

    return 1;
}

/* Mutable component assignment */

static int Vector2_NewIndex(lua_State *L) {
    MaxCoreVector *V = Vector_Check(L, 1, VECTOR2_METATABLE);
    const char *Key = luaL_checkstring(L, 2);

    if (strcmp(Key, "X") == 0 || strcmp(Key, "x") == 0) V->x = luaL_checknumber(L, 3);
    else if (strcmp(Key, "Y") == 0 || strcmp(Key, "y") == 0) V->y = luaL_checknumber(L, 3);
    else return luaL_error(L, "Vector2 has no writable property '%s'", Key);

    return 0;
}

static int Vector3_NewIndex(lua_State *L) {
    MaxCoreVector *V = Vector_Check(L, 1, VECTOR3_METATABLE);
    const char *Key = luaL_checkstring(L, 2);

    if (strcmp(Key, "X") == 0 || strcmp(Key, "x") == 0) V->x = luaL_checknumber(L, 3);
    else if (strcmp(Key, "Y") == 0 || strcmp(Key, "y") == 0) V->y = luaL_checknumber(L, 3);
    else if (strcmp(Key, "Z") == 0 || strcmp(Key, "z") == 0) V->z = luaL_checknumber(L, 3);
    else return luaL_error(L, "Vector3 has no writable property '%s'", Key);

    return 0;
}

/* String conversion */

static int Vector2_ToString(lua_State *L) {
    MaxCoreVector *V = Vector_Check(L, 1, VECTOR2_METATABLE);
    char Buffer[128];
    snprintf(Buffer, sizeof(Buffer), "Vector2(%g, %g)", (double)V->x, (double)V->y);
    lua_pushstring(L, Buffer);
    return 1;
}

static int Vector3_ToString(lua_State *L) {
    MaxCoreVector *V = Vector_Check(L, 1, VECTOR3_METATABLE);
    char Buffer[128];
    snprintf(Buffer, sizeof(Buffer), "Vector3(%g, %g, %g)", (double)V->x, (double)V->y, (double)V->z);
    lua_pushstring(L, Buffer);
    return 1;
}

/* Constructors */

static int Vector2_New(lua_State *L) {
    lua_Number X = luaL_checknumber(L, 1);
    lua_Number Y = luaL_checknumber(L, 2);
    Vector_Push(L, VECTOR2_METATABLE, X, Y, 0);
    return 1;
}

static int Vector3_New(lua_State *L) {
    lua_Number X = luaL_checknumber(L, 1);
    lua_Number Y = luaL_checknumber(L, 2);
    lua_Number Z = luaL_checknumber(L, 3);
    Vector_Push(L, VECTOR3_METATABLE, X, Y, Z);
    return 1;
}

/* Registration */

static void Vector_RegisterMetatable(
    lua_State *L,
    const char *Metatable,
    lua_CFunction Index,
    lua_CFunction NewIndex,
    lua_CFunction Add,
    lua_CFunction Mult,
    lua_CFunction Div,
    lua_CFunction Sub,
    lua_CFunction Unm,
    lua_CFunction Eq,
    lua_CFunction ToString
) {
    luaL_newmetatable(L, Metatable);

    lua_pushcfunction(L, Index); lua_setfield(L, -2, "__index");
    lua_pushcfunction(L, NewIndex); lua_setfield(L, -2, "__newindex");
    lua_pushcfunction(L, Add); lua_setfield(L, -2, "__add");
    lua_pushcfunction(L, Sub); lua_setfield(L, -2, "__sub");
    lua_pushcfunction(L, Mult); lua_setfield(L, -2, "__mul");
    lua_pushcfunction(L, Div); lua_setfield(L, -2, "__div");
    lua_pushcfunction(L, Unm); lua_setfield(L, -2, "__unm");
    lua_pushcfunction(L, Eq); lua_setfield(L, -2, "__eq");
    lua_pushcfunction(L, ToString); lua_setfield(L, -2, "__tostring");

    lua_pop(L, 1);
}

static void Vector_RegisterLibrary(lua_State *L, const char *Name, lua_CFunction Constructor) {
    lua_newtable(L);
    lua_pushcfunction(L, Constructor);
    lua_setfield(L, -2, "new");
    lua_setglobal(L, Name);
}

/* Public registration */

LUAMOD_API void Vector_Register(lua_State *L) {
    Vector_RegisterMetatable(L, VECTOR2_METATABLE, Vector2_Index, Vector2_NewIndex, Vector2_Add, Vector2_Mult, Vector2_Div, Vector2_Sub, Vector2_Unm, Vector2_Eq, Vector2_ToString);
    Vector_RegisterMetatable(L, VECTOR3_METATABLE, Vector3_Index, Vector3_NewIndex, Vector3_Add, Vector3_Mult, Vector3_Div, Vector3_Sub, Vector3_Unm, Vector3_Eq, Vector3_ToString);
    Vector_RegisterLibrary(L, "Vector2", Vector2_New);
    Vector_RegisterLibrary(L, "Vector3", Vector3_New);
}
