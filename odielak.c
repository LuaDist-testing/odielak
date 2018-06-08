#include "lua.h"
#include "lualib.h"
#include "lauxlib.h"

#include "stdlib.h"
#include "string.h"
#include "limits.h"

#define LAK_VERSION 103

#define LAK_DICT_SIZE (UCHAR_MAX + 1)
#define LAK_BUF_STACK_TOTALSIZE 2048

#define LAK_BUF_OVERSIZE (sizeof(unsigned char *) - sizeof(unsigned char))
#define LAK_BUF_STACK_SIZE (LAK_BUF_STACK_TOTALSIZE - LAK_BUF_OVERSIZE/sizeof(unsigned char))

#define LAK_DSTRING 0x1
#define LAK_DFUNCTION 0x2

static inline int replace_tostring(lua_State *l, int obj)
{
	if (luaL_callmeta(l, obj, "__tostring")) {
		lua_replace(l, obj < 0 ? (obj - 1) : obj);
		return 1;
	}

	return 0;
}

static inline const char *tolstring(lua_State *l, int obj, size_t *len)
{
	const char *str = lua_tolstring(l, obj, len);

	if (!str && replace_tostring(l, obj)) {
		str = lua_tolstring(l, obj, len);
	}

	return str;
}

static int error_no_dict(lua_State *l)
{
	luaL_typerror(l, 1, "a table with a proper odielak dictionary [-1]");
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
	luaL_error(l, "the dictionary [-1] is malformed: missing value for %u!", c);
	return 0;
}

static int replace(lua_State *l)
{
	int argc = lua_gettop(l);
	int ar;

	if (!lua_istable(l, 1)) {
		return error_no_dict(l);
	}

	lua_rawgeti(l, 1, -1);

	size_t dlen;
	const unsigned char *udict = (const unsigned char *) lua_tolstring(l, -1, &dlen);

	if (!udict) {
		return error_no_dict(l);
	}

	if (!dlen) {
		lua_pop(l, 1); // remove last value (dict)
		return argc - 1;
	}

	const unsigned char *str_rep[LAK_DICT_SIZE];
	unsigned char str_inited[LAK_DICT_SIZE] = {};
	unsigned char dict[LAK_DICT_SIZE];

	size_t i;
	size_t matched;
	size_t oversize;
	size_t str_rep_len[LAK_DICT_SIZE];

	size_t alloc_size = 0;

	unsigned char sbuf[LAK_BUF_STACK_TOTALSIZE];
	unsigned char *new, *push, *alloc = NULL;

	size_t len;
	const unsigned char *str;

	memcpy(dict, udict, dlen * sizeof(unsigned char));

	if (dlen < LAK_DICT_SIZE) {
		memset(&dict[dlen], 0, (LAK_DICT_SIZE - dlen) * sizeof(unsigned char));
	}

	for (ar = 2; ar <= argc; ++ar) {

		str = (const unsigned char *) tolstring(l, ar, &len);

		if (!str) {
			lua_pushnil(l);
			continue;
		}

		matched = 0;
		oversize = 0;

		for (i = 0; i < len; ++i) {
			if (dict[str[i]]) {
				if (!str_inited[str[i]]) {

					str_inited[str[i]] = 1;

					lua_rawgeti(l, 1, str[i]);

					if (dict[str[i]] == LAK_DFUNCTION) {
						lua_pushvalue(l, 1);
						lua_pushvalue(l, ar);
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

		if (!matched) { // skip, just push
			lua_pushvalue(l, ar);
			continue;
		}

		oversize+= len - matched;

		if (oversize > LAK_BUF_STACK_SIZE) {
			size_t total_oversize = (oversize * sizeof(unsigned char) + LAK_BUF_OVERSIZE);

			if (alloc_size < total_oversize) {
				alloc_size = total_oversize;
				alloc = (unsigned char *) (!alloc ? malloc(alloc_size) : realloc(alloc, alloc_size));
			}

			if (!alloc) {
				return error_bad_malloc(l);
			}

			new = alloc;
		} else {
			new = sbuf;
		}

		push = new;

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
	}

	if (alloc) {
		free(alloc);
	}

	return argc - 1;
}

static int new(lua_State *l)
{
	int argc = lua_gettop(l);
	int i;

	for (i = 1; i <= argc; ++i) {
		luaL_checktype(l, i, LUA_TTABLE);
	}

	lua_newtable(l);
	lua_pushvalue(l, lua_upvalueindex(1));

	if (!lua_setmetatable(l, -2)) {
		return error_meta_set(l);
	}

	unsigned char dict[LAK_DICT_SIZE] = {};

	const unsigned char *key;
	long long int keyi;

	size_t max_value = 0;
	size_t lnk;

	for (i = 1; i <= argc; ++i) {

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

	lua_pushnumber(l, LAK_VERSION);
	lua_setfield(l, -2, "_VERSION");

	lua_createtable(l, 0, 1);
	lua_pushcfunction(l, replace);
	lua_setfield(l, -2, "__call");

	lua_pushvalue(l, -1);
	lua_pushcclosure(l, new, 1);
	lua_setfield(l, -3, "new");

	lua_setfield(l, -2, "_meta");

	return 1;
}
