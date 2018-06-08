#include "lua.h"
#include "lualib.h"
#include "lauxlib.h"

#include "stdlib.h"
#include "string.h"
#include "limits.h"

#define LAK_DICT_SIZE (UCHAR_MAX + 1)
#define LAK_BUF_STACK_SIZE 2048

#define LAK_DSTRING 0x1
#define LAK_DFUNCTION 0x2

static int replace_ut_tostring(lua_State *l, int obj)
{
	if (luaL_callmeta(l, obj, "__tostring")) {
		if (lua_isstring(l, -1)) {
			lua_replace(l, obj - 1);
			return 1;
		}

		lua_pop(l, 1);
	}

	return 0;
}

static int nil_value(lua_State *l)
{
	lua_pushnil(l);
	return 1;
}

static int error_no_dict(lua_State *l)
{
	luaL_typerror(l, 1, "table (with a proper odielak dictionary [-1])");
	return 0;
}

static int error_meta_set(lua_State *l)
{
	luaL_error(l, "unable to setmetatable()");
	return 0;
}

static int error_bad_malloc(lua_State *l)
{
	luaL_error(l, "bad malloc()");
	return 0;
}

static int error_no_str_in_dict(lua_State *l, const unsigned char c)
{
	luaL_error(l, "the odielak dictionary [-1] is malformed: missing value for %u!", c);
	return 0;
}

static int replace(lua_State *l)
{
	if (lua_gettop(l) > 2) {
		lua_pop(l, lua_gettop(l) - 2);
	}

	if (!lua_istable(l, 1)) {
		return error_no_dict(l);
	}

	lua_rawgeti(l, 1, -1);

	size_t dlen;
	const unsigned char *udict = (const unsigned char *) lua_tolstring(l, -1, &dlen);

	if (!udict) {
		return error_no_dict(l);
	}

	switch (lua_type(l, 2)) {
		case LUA_TTABLE:
		case LUA_TUSERDATA:

			if (!replace_ut_tostring(l, -2))
				return nil_value(l);

		case LUA_TSTRING:
		case LUA_TNUMBER: break;
		default:

			return nil_value(l);
	}

	size_t len;
	const unsigned char *str = (const unsigned char *) lua_tolstring(l, 2, &len);

	if (!dlen) {
		lua_pop(l, 1); // remove last value (dict)
		return 1;
	}

	const unsigned char *str_rep[LAK_DICT_SIZE];
	unsigned char str_inited[LAK_DICT_SIZE] = {};
	unsigned char dict[LAK_DICT_SIZE];

	size_t i;
	size_t matched = 0;
	size_t oversize = 0;
	size_t str_rep_len[LAK_DICT_SIZE];

	memcpy(dict, udict, dlen * sizeof(unsigned char));

	if (dlen < LAK_DICT_SIZE) {
		memset(&dict[dlen], 0, (LAK_DICT_SIZE - dlen) * sizeof(unsigned char));
	}

	for (i = 0; i < len; ++i) {
		if (dict[str[i]]) {
			if (!str_inited[str[i]]) {

				str_inited[str[i]] = 1;

				lua_rawgeti(l, 1, str[i]);

				if (dict[str[i]] == LAK_DFUNCTION) {
					lua_pushvalue(l, 1);
					lua_pushvalue(l, 2);
					lua_pushnumber(l, str[i]);
					lua_call(l, 3, 1);
				}

				str_rep[str[i]] = (const unsigned char *) lua_tolstring(l, -1, &str_rep_len[str[i]]);

				if (!str_rep[str[i]]) {
					return error_no_str_in_dict(l, str[i]);
				}
			}

			oversize+= str_rep_len[str[i]];
			++matched;
		}
	}

	if (!matched) {
		lua_pop(l, 1); // remove last value (dict)
		return 1;
	}

	oversize+= len - matched;

	unsigned char sbuf[LAK_BUF_STACK_SIZE];
	unsigned char *new;

	if (oversize > LAK_BUF_STACK_SIZE) {
		new = (unsigned char *) malloc(oversize);

		if (!new) {
			return error_bad_malloc(l);
		}
	} else {
		new = sbuf;
	}

	unsigned char *push = new;

	for (i = 0; i < len; ++i) {
		if (dict[str[i]]) {
			if (str_rep_len[str[i]] > 1) {
				memcpy(new, str_rep[str[i]], str_rep_len[str[i]] * sizeof(unsigned char));
				new+= str_rep_len[str[i]];
			} else if (str_rep_len[str[i]]) {
				*(new++) = *str_rep[str[i]];
			}
		} else {
			*(new++) = str[i];
		}
	}

	lua_pushlstring(l, (const char *) push, oversize);

	if (oversize > LAK_BUF_STACK_SIZE) {
		free(push);
	}

	return 1;
}

static int make_new(lua_State *l)
{
	if (lua_gettop(l) > 2) {
		lua_pop(l, lua_gettop(l) - 2);
	}

	lua_newtable(l);

	if (!lua_istable(l, 1)) {
		return error_meta_set(l);
	}

	lua_getfield(l, 1, "_meta");

	if (!lua_istable(l, -1)) {
		return error_meta_set(l);
	}

	lua_setmetatable(l, 3);

	unsigned char dict[LAK_DICT_SIZE] = {};

	const unsigned char *key;
	long long int keyi;

	size_t max_value = 0;
	size_t lnk;

	if (lua_istable(l, 2)) {
		lua_pushnil(l);

		while(lua_next(l, 2)) {

			if (lua_isnumber(l, -2)) {
				keyi = lua_tonumber(l, -2);

				if (keyi >= LAK_DICT_SIZE || keyi < 0) {
					lua_pop(l, 1);
					continue;
				}

			} else {
				key = (const unsigned char *) lua_tolstring(l, -2, &lnk);

				if (lnk != 1) {
					lua_pop(l, 1);
					continue;
				}

				keyi = key[0];
			}

			switch (lua_type(l, -1)) {
				case LUA_TTABLE:
				case LUA_TUSERDATA:
					if (!replace_ut_tostring(l, -1)) {
						lua_pop(l, 1);
						continue;
					}

				case LUA_TSTRING:
				case LUA_TNUMBER:
					if (!lua_tostring(l, -1)) { // force convert to string
						lua_pop(l, 1);
						continue;
					}

					dict[keyi] = LAK_DSTRING;
					break;

				case LUA_TFUNCTION:
					dict[keyi] = LAK_DFUNCTION;
					break;

				default:
					lua_pop(l, 1);
					continue;
			}

			lua_rawseti(l, 3, keyi);

			if (max_value <= keyi) {
				max_value = (size_t) (keyi + 1);
			}
		}
	}

	lua_pushlstring(l, (const char *) dict, max_value);
	lua_rawseti(l, 3, -1);

	return 1;
}

int luaopen_odielak(lua_State *l)
{
	lua_createtable(l, 0, 3);

	lua_pushcfunction(l, make_new);
	lua_setfield(l, -2, "New");

	lua_pushnumber(l, 101);
	lua_setfield(l, -2, "_VERSION");

	lua_createtable(l, 0, 1);
	lua_pushcfunction(l, replace);
	lua_setfield(l, -2, "__call");
	lua_setfield(l, -2, "_meta");

	return 1;
}
