// =========================================================================
// scheduler_native.cpp
// =========================================================================
// Lua binding for task_scheduler.hpp. Module name: "scheduler_native".
//
//   local sched = require("scheduler_native")
//   local s = sched.new(workers?)            -- workers default: cores - 1
//
//   s:now()                                   -> seconds on the scheduler clock
//   s:timer(seconds)                          -> handle (integer)
//   s:read_file(path)                         -> handle   (background; result = string or nil,err)
//   s:cancel(handle)                          -> boolean
//   s:pending(handle)                         -> boolean
//   s:drain(budgetSeconds?, maxItems?)        -> array of events, see below
//   s:wait(maxSeconds)                        -> blocks until work completes OR maxSeconds
//   s:until_next_timer(fallback)              -> seconds
//   s:shutdown()
//
// An event from drain() is a table:
//   { handle = <int>, kind = "timer" | "work", ok = <bool>, value = <string|nil>, error = <string|nil> }
//
// THREADING RULE: Lua code runs ONLY on the thread that calls into this
// module. Background jobs are native built-ins whose arguments were copied
// out of Lua before they were queued, so a worker never touches a lua_State.
// =========================================================================
#include <lua.hpp>
#include "task_scheduler.hpp"

#include <iostream>
#include <fstream>
#include <memory>
#include <string>

#if defined(_WIN32) || defined(_WIN64)
    #define EXPORT_FN __declspec(dllexport)
#else
    #define EXPORT_FN
#endif

namespace {

constexpr const char* kMeta = "MaxCore.SchedulerNative";

struct Instance {
    std::unique_ptr<Sched::Scheduler> scheduler;
};

Instance* check(lua_State* L, int idx) {
    auto** p = static_cast<Instance**>(luaL_checkudata(L, idx, kMeta));
    if (!*p) luaL_error(L, "scheduler has been shut down");
    return *p;
}

int s_gc(lua_State* L) {
    auto** p = static_cast<Instance**>(luaL_checkudata(L, 1, kMeta));
    if (*p) {
        (*p)->scheduler.reset();   // joins every thread
        delete *p;
        *p = nullptr;
    }
    return 0;
}

int s_shutdown(lua_State* L) {
    std::cout << "[Engine Debug] INFO: shutting down runtime... " << L << std::endl;
    return s_gc(L);
}

int s_now(lua_State* L) {
    lua_pushnumber(L, check(L, 1)->scheduler->Now());
    return 1;
}

int s_timer(lua_State* L) {
    Instance* in = check(L, 1);
    double secs = luaL_checknumber(L, 2);
    Sched::Handle h = in->scheduler->AddTimer(secs);
    if (!h.Valid()) { lua_pushnil(L); return 1; }
    lua_pushinteger(L, (lua_Integer)h.Pack());
    return 1;
}

// Built-in background operation: read a whole file. The path is copied into
// the closure here, on the Lua thread; the worker never sees Lua. The file's
// bytes come back inside the Completion, so nothing is stored anywhere else.
int s_read_file(lua_State* L) {
    Instance* in = check(L, 1);
    size_t len = 0;
    const char* raw = luaL_checklstring(L, 2, &len);
    std::string path(raw, len);
    // Refuse sizes that would be a memory bomb. 256 MiB is far beyond any asset.
    const uint64_t kMaxBytes = 256ull * 1024 * 1024;

    Sched::Handle h = in->scheduler->AddWork([path, kMaxBytes]() -> Sched::JobResult {
        Sched::JobResult r;
        std::ifstream f(path, std::ios::binary | std::ios::ate);
        if (!f) { r.ok = false; r.error = "cannot open file: " + path; return r; }
        std::streamoff size = f.tellg();
        if (size < 0) { r.ok = false; r.error = "cannot determine file size: " + path; return r; }
        if ((uint64_t)size > kMaxBytes) { r.ok = false; r.error = "file too large: " + path; return r; }
        r.payload.resize((size_t)size);
        f.seekg(0);
        if (size > 0 && !f.read(&r.payload[0], size)) {
            r.ok = false; r.payload.clear(); r.error = "read failed: " + path;
        }
        return r;
    });
    if (!h.Valid()) { lua_pushnil(L); return 1; }
    lua_pushinteger(L, (lua_Integer)h.Pack());
    return 1;
}

int s_cancel(lua_State* L) {
    Instance* in = check(L, 1);
    Sched::Handle h = Sched::Handle::Unpack((uint64_t)luaL_checkinteger(L, 2));
    lua_pushboolean(L, in->scheduler->Cancel(h));
    return 1;
}

int s_pending(lua_State* L) {
    Instance* in = check(L, 1);
    Sched::Handle h = Sched::Handle::Unpack((uint64_t)luaL_checkinteger(L, 2));
    lua_pushboolean(L, in->scheduler->IsPending(h));
    return 1;
}

int s_drain(lua_State* L) {
    Instance* in = check(L, 1);
    double budget = luaL_optnumber(L, 2, 0.002);
    lua_Integer maxItems = luaL_optinteger(L, 3, 4096);
    if (maxItems < 1) maxItems = 1;

    std::vector<Sched::Completion> got;
    in->scheduler->Drain(got, budget, (size_t)maxItems);

    lua_createtable(L, (int)got.size(), 0);
    int i = 1;
    for (auto& c : got) {
        lua_createtable(L, 0, 5);
        lua_pushinteger(L, (lua_Integer)c.handle.Pack()); lua_setfield(L, -2, "handle");
        lua_pushstring(L, c.kind == Sched::Completion::Kind::Timer ? "timer" : "work"); lua_setfield(L, -2, "kind");

        if (c.kind == Sched::Completion::Kind::Timer) {
            lua_pushboolean(L, 1); lua_setfield(L, -2, "ok");
        } else {
            lua_pushboolean(L, c.ok); lua_setfield(L, -2, "ok");
            const std::string& text = c.ok ? c.payload : c.error;
            lua_pushlstring(L, text.data(), text.size());
            lua_setfield(L, -2, c.ok ? "value" : "error");
        }
        lua_rawseti(L, -2, i++);
    }
    return 1;
}

int s_wait(lua_State* L) {
    Instance* in = check(L, 1);
    double secs = luaL_checknumber(L, 2);
    if (!(secs > 0)) return 0;
    if (secs > 60.0) secs = 60.0;
    // Do NOT hold the Lua state's attention hostage: we hold no Lua locks,
    // and the call only blocks this one OS thread.
    in->scheduler->WaitForCompletion(secs);
    return 0;
}

int s_until_next_timer(lua_State* L) {
    Instance* in = check(L, 1);
    lua_pushnumber(L, in->scheduler->SecondsUntilNextTimer(luaL_optnumber(L, 2, 1.0)));
    return 1;
}

int m_new(lua_State* L) {
    lua_Integer workers = luaL_optinteger(L, 1, 0);
    if (workers < 0) workers = 0;
    if (workers > 64) workers = 64; // sanity cap
    auto** p = static_cast<Instance**>(lua_newuserdatauv(L, sizeof(Instance*), 0));
    *p = nullptr;
    luaL_setmetatable(L, kMeta);
    *p = new Instance();
    (*p)->scheduler = std::make_unique<Sched::Scheduler>((unsigned)workers);
    return 1;
}

} // namespace

extern "C" EXPORT_FN int luaopen_scheduler_native(lua_State* L) {
    if (luaL_newmetatable(L, kMeta)) {
        static const luaL_Reg methods[] = {
            {"now", s_now}, {"timer", s_timer}, {"read_file", s_read_file},
            {"cancel", s_cancel}, {"pending", s_pending}, {"drain", s_drain},
            {"wait", s_wait}, {"until_next_timer", s_until_next_timer},
            {"shutdown", s_shutdown}, {nullptr, nullptr}
        };
        luaL_setfuncs(L, methods, 0);
        lua_pushvalue(L, -1);
        lua_setfield(L, -2, "__index");
        lua_pushcfunction(L, s_gc);
        lua_setfield(L, -2, "__gc");
    }
    lua_pop(L, 1);

    lua_createtable(L, 0, 1);
    lua_pushcfunction(L, m_new);
    lua_setfield(L, -2, "new");
    return 1;
}
