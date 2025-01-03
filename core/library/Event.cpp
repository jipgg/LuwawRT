#include "common/Namecall_atom.hpp"
#include <lualib.h>
#include "common.hpp"
#include "type_utils.hpp"
#include "library.hpp"
#include "common/metamethod.hpp"
using library::Event;
static constexpr auto type = "Event";
static constexpr auto connection_type = "EventConnection";
static bool already_registered = false;

Event::Event(lua_State* L): L(L), refs() {
}
Event::~Event() {
    for (auto& [id, ref] : refs) {
        lua_unref(L, ref);
    }
}
Event::Connection Event::connect(int idx) {
    if (!lua_isfunction(L, idx)) {
        printerr("connect() error: Value is not a function\n");
        lua_pop(L, 1); // Pop table from stack
        return Connection{nullid};
    }
    refs.push_back(std::make_pair(++curr_id, lua_ref(L, idx)));
    return Connection{curr_id};
}
void Event::disconnect(Event::Connection connection) {
    using pair_t = std::pair<size_t, int>;
    const pair_t dummy{connection.id, 0};
    auto it = std::lower_bound(refs.begin(), refs.end(), dummy, [](const pair_t& e, const pair_t& v) {
        return e.first < v.first;
    });
    if (it != refs.end() and it->first == connection.id) {
        refs.erase(it);
    } else luaL_error(L, "invalid id");
}
void Event::fire(int arg_count) {
    for (auto& [id, ref] : refs) {
        lua_getref(L, ref);
        if (lua_isfunction(L, -1)) {
            lua_pushvalue(L, -1); // Push a copy of the function
            for (int i = 0; i < arg_count; ++i) {
                lua_pushvalue(L, -(arg_count + 2));
            }
            if (lua_pcall(L, arg_count, 0, 0) != LUA_OK) {
                printerr("Error:", lua_tostring(L, -1));
                lua_pop(L, 1);
            }
        }
        lua_pop(L, 1);
    }
    lua_pop(L, arg_count);
}
static int ctor(lua_State* L) {
    halia::create<Event>(L, L);
    return 1;
}
static int namecall(lua_State* L) {
    int atom;
    auto& r = halia::check<Event>(L, 1); 
    lua_namecallatom(L, &atom);
    using A = NamecallAtom;
    switch (static_cast<A>(atom)) {
        case A::connect: {
            if (not lua_isfunction(L, 2)) return 0;
            Event::Connection connection = r.connect(2);
            halia::create<Event::Connection>(L, std::move(connection));
            return 1;
        }
        case A::disconnect: {
            r.disconnect(halia::check<Event::Connection>(L, 2));
            return 0;
        }
        case A::fire: {
            lua_remove(L, -lua_gettop(L));//removes event from stack
            //print("top is", lua_gettop(L));
            r.fire(lua_gettop(L));
            return 0;
        }
        default:
        return 0;
    }
}
int connection_id_tostring(lua_State* L) {
    auto& connection = halia::check<Event::Connection>(L, 1);
    lua_pushstring(L, std::to_string(connection.id).c_str());
    return 1;
}

namespace library {
int event_ctor(lua_State* L) {
    if (not already_registered) {
        if (luaL_newmetatable(L, halia::metatable_name<Event::Connection>())) {
            lua_pushcfunction(L, connection_id_tostring, "event_connection_id_tostring");
            lua_setfield(L, -2, metamethod::tostring);
            lua_pushstring(L, connection_type);
            lua_setfield(L, -2, metamethod::type);
        }
        lua_pop(L, 1);
        if (luaL_newmetatable(L, halia::metatable_name<Event>())) {
            const luaL_Reg lib[] = {
                {metamethod::namecall, namecall},
                {nullptr, nullptr}
            };
            luaL_register(L, nullptr, lib);
            lua_pushstring(L, type);
            lua_setfield(L, -2, metamethod::type);
        }
        lua_pop(L, 1);
        already_registered = true;
    }
    //lua_pushcfunction(L, ctor, type);
    return ctor(L);
}
void register_event_type(lua_State* L) {
    if (luaL_newmetatable(L, halia::metatable_name<Event::Connection>())) {
        lua_pushcfunction(L, connection_id_tostring, "event_connection_id_tostring");
        lua_setfield(L, -2, metamethod::tostring);
        lua_pushstring(L, connection_type);
        lua_setfield(L, -2, metamethod::type);
    }
    lua_pop(L, 1);
    if (luaL_newmetatable(L, halia::metatable_name<Event>())) {
        const luaL_Reg lib[] = {
            {"__namecall", namecall},
            {nullptr, nullptr}
        };
        luaL_register(L, nullptr, lib);
        lua_pushstring(L, type);
        lua_setfield(L, -2, metamethod::type);
    }
    lua_pop(L, 1);
    lua_pushcfunction(L, ctor, type);
    lua_setglobal(L, type);
}
}
