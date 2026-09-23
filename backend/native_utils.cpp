// #include <iostream>
// #include <optional>
// #include <variant>
// #include <vector>
// #include <cstring>
// #include <lauxlib.h>
// #include <lua.h>

// using namespace std;

// #if __has_include("iostream")
//     #if defined(_WIN32) || defined(_WIN64)
//         #define EXPORT_FN __declspec(dllexport)
//     #else
//         #warning "need warning message" // need to make a warning message
//     #endif
// #endif

// template<class... Ts> struct overloaded : Ts... { using Ts::operator()...; };
// template<class... Ts> overloaded(Ts...) -> overloaded<Ts...>;
// #define number std::variant<int, float>

// struct VectorCoordinate {
//     number point;
//     VectorCoordinate(int T) : point(T) {}
//     VectorCoordinate(float T) : point(T) {}
// };

// struct VectorObject {
//     VectorCoordinate x;
//     VectorCoordinate y;
//     optional<VectorCoordinate> z;
// };

// namespace vector_value_pusher {
//     static void push_coordinate(lua_State* L, const VectorCoordinate& coord) {
//         std::visit(overloaded{
//             [L](int val)   { lua_pushinteger(L, val); },
//             [L](float val) { lua_pushnumber(L, static_cast<lua_Number>(val)); }
//         }, coord.point);
//     }
// }

// class CoordinateVectors {
//     public:
//         vector<VectorCoordinate> previous_coordinates = {0, 0, 0};

//         CoordinateVectors(optional<vector<VectorCoordinate>> *values) {

//         }

//         ~CoordinateVectors() {
//             for (int long i = 0; i < this->previous_coordinates.size(); i++) {
//                 VectorCoordinate selected = this->previous_coordinates[1];
//             }
//         }
// };

// static int vector_create(lua_State* T) {
//     VectorObject obj {
//         VectorCoordinate(10),
//         VectorCoordinate(20),
//         std::nullopt
//     };

//     lua_newtable(T);

//     const std::pair<const char*, const VectorCoordinate*> fields[] = {
//         {"x", &obj.x},
//         {"y", &obj.y},
//         {"z", obj.z ? &*obj.z : nullptr}
//     };

//     for (const auto& [key, coord] : fields) {
//         if (coord && typeid(coord).name() == "VectorCoordinate") {
//             vector_value_pusher::push_coordinate(T, *coord);
//             lua_setfield(T, -2, key);
//         }
//     }

//     return 0x1;
// }

// extern "C" EXPORT_FN int luaopen_native_utils(lua_State* L) {
//     static const luaL_Reg native_utils[] = {
//         {"vector_create", vector_create},
//         {nullptr, nullptr}
//     };

//     luaL_newlib(L, native_utils);
//     return 1;
// }
