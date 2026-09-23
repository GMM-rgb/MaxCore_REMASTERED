#include <iostream>
#include <optional>
#include <variant>
#include <vector>
#include <cstring>
#include <lauxlib.h>
#include <lua.h>

using namespace std;

#if __has_include("iostream")
#ifdef _WIN32 || _WIN64
#define EXPORT_FN __declspec(dllexport)
#else
// #warning "need warning message" // need to make a warning message
#endif
#endif

struct VectorCoordinate {
    std::variant<int, float> point;
    VectorCoordinate(int T) : point(T) {}
    VectorCoordinate(float T) : point(T) {}
};

struct VectorObject {
    VectorCoordinate x;
    VectorCoordinate y;
    optional<VectorCoordinate> z;
};

class CoordinateVectors {
public:
    CoordinateVectors() {

    }

    ~CoordinateVectors() {
        free(nullptr);
        delete this;
    }

private:
    vector<VectorCoordinate> previous_coordinates = {0, 0, 0};
};

static VectorObject vector_create() {
    CoordinateVectors vector = CoordinateVectors();
}

extern "C" EXPORT_FN int luaopen_native_utils(lua_State *) {
    static const luaL_Reg native_utils[] = {
        {nullptr, nullptr},
    };
}
