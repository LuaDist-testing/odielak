#include "lua.h"
#include "lualib.h"
#include "lauxlib.h"

#include "stdlib.h"
#include "string.h"
#include "limits.h"

#define LAK_VERSION 102

#define LAK_DICT_SIZE (UCHAR_MAX + 1)
#define LAK_BUF_STACK_TOTALSIZE 2048

#define LAK_BUF_OVERSIZE (sizeof(unsigned char *) - sizeof(unsigned char))
#define LAK_BUF_STACK_SIZE (LAK_BUF_STACK_TOTALSIZE - LAK_BUF_OVERSIZE/sizeof(unsigned char))

#define LAK_DSTRING 0x1
#define LAK_DFUNCTION 0x2

static int replace_tostring(lua_State *l, int obj)
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

static __inline__ const char *tolstring(lua_State *l, int obj, size_t *len)
{
	const char *str = lua_tolstring(l, obj, len);

	if (!str && replace_tostring(l, obj)) {
		return lua_tolstring(l, obj, len);
	}

	return str;
}

static int nil_value(lua_State *l)
{
	lua_pushnil(l);
	return 1;
}

static int error_no_dict(lua_State *l)
{
	luaL_typerror(l, 1, "table with a proper odielak dictionary [-1]");
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

	size_t len;
	const unsigned char *str = (const unsigned char *) tolstring(l, -2, &len);

	if (!str) {
		return nil_value(l);
	}

	if (!len || !dlen) {
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

				if (str_rep_len[str[i]] <= sizeof(unsigned char *)) { // avoid future memcpy if got a short string
					memcpy(&str_rep[str[i]], str_rep[str[i]], str_rep_len[str[i]] * sizeof(unsigned char));
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

	unsigned char sbuf[LAK_BUF_STACK_TOTALSIZE];
	unsigned char *new;

	if (oversize > LAK_BUF_STACK_SIZE) {
		new = (unsigned char *) malloc(oversize * sizeof(unsigned char) + LAK_BUF_OVERSIZE);

		if (!new) {
			return error_bad_malloc(l);
		}
	} else {
		new = sbuf;
	}

	unsigned char *push = new;

	for (i = 0; i < len; ++i) {
		if (dict[str[i]]) {
			if (str_rep_len[str[i]]) {
				if (str_rep_len[str[i]] > sizeof(unsigned char *)) {
					memcpy(new, str_rep[str[i]], str_rep_len[str[i]] * sizeof(unsigned char));
				} else {
					*((const unsigned char **)new) = str_rep[str[i]]; //lol
				}

				new+= str_rep_len[str[i]];
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

static int new(lua_State *l)
{
	short argc = (short)lua_gettop(l);
	short i;

	for (i = 1; i <= argc; ++i) {
		luaL_checktype(l, i, LUA_TTABLE);
	}

	lua_newtable(l);
	lua_getfield(l, 1, "_meta");

	if (!lua_istable(l, -1)) {
		return error_meta_set(l);
	}

	lua_setmetatable(l, -2);

	unsigned char dict[LAK_DICT_SIZE] = {};

	const unsigned char *key;
	long long int keyi;

	size_t max_value = 0;
	size_t lnk;

	for (i = 2; i <= argc; ++i) {

		lua_pushnil(l);

		while(lua_next(l, i)) {

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

			if (tolstring(l, -1, NULL)) { // force convert to string
				dict[keyi] = LAK_DSTRING;
			} else if (lua_type(l, -1) == LUA_TFUNCTION) {
				dict[keyi] = LAK_DFUNCTION;
			} else {
				lua_pop(l, 1);
				continue;
			}

			lua_rawseti(l, -3, keyi);

			if (max_value <= keyi) {
				max_value = (size_t) (keyi + 1);
			}
		}
	}

	lua_pushlstring(l, (const char *) dict, max_value);
	lua_rawseti(l, -2, -1);

	return 1;
}

int luaopen_odielak(lua_State *l)
{
	lua_createtable(l, 0, 3);

	lua_pushcfunction(l, new);
	lua_setfield(l, -2, "New");

	lua_pushnumber(l, LAK_VERSION);
	lua_setfield(l, -2, "_VERSION");

	lua_createtable(l, 0, 1);
	lua_pushcfunction(l, replace);
	lua_setfield(l, -2, "__call");
	lua_setfield(l, -2, "_meta");

	return 1;
}
