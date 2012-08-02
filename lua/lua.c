#include <stdlib.h>
#include <stdio.h>
#include <math.h>
#include <lua.h>
#include <lauxlib.h>
#include <lualib.h>

/* Module */
int l_exp (lua_State *L) {
	double d = luaL_checknumber(L, 1); /* get argument */
	lua_pop(L, 1); /* remove argument */
	lua_pushnumber(L, exp(d)); /* push result */
	return 1; /* number of results */
}

int l_log (lua_State *L) {
	double d = luaL_checknumber(L, 1); /* get argument */
	lua_pop(L, 1); /* remove argument */
	lua_pushnumber(L, log(d)); /* push result */
	return 1; /* number of results */
}

static const struct luaL_Reg mylib [] = {
	{"exp", l_exp},
	{"log", l_log},
	{NULL, NULL} /* sentinel */
};

/* Assume that table is on the stack top */
double getfield(lua_State *L, const char *key) {
	double result;
	lua_getfield(L, -1, key);
	result = luaL_checknumber(L, -1);
	lua_pop(L, 1); /* remove number */
	return result;
}

int main() {
	lua_State *L = luaL_newstate();
	luaL_openlibs(L);
	luaL_register(L, "mylib", mylib);
	lua_settop(L, 0);

	if (luaL_loadfile(L, "testconf.lua") || lua_pcall(L, 0, 0, 0)) {
		printf("Cannot run config. file: %s\n", lua_tostring(L, -1));
		lua_close(L); exit(EXIT_FAILURE);
	}

	/* Globale valriables */
	lua_getglobal(L, "width");
	lua_getglobal(L, "height");
	if (!lua_isnumber(L, 1)) {
		printf("’width’ should be a number\n");
		lua_close(L); exit(EXIT_FAILURE);
	}
	if (!lua_isnumber(L, 2)) {
		printf("’height’ should be a number\n");
		lua_close(L); exit(EXIT_FAILURE);
	}

	printf("width = %i\n",  lua_tointeger(L, 1));
	printf("height = %i\n", lua_tointeger(L, 2));

	lua_settop(L, 0);

	/* Table fields */
	lua_getglobal(L, "background");
	if (!lua_istable(L, 1)) {
		printf("’background’ is not a table\n");
		lua_close(L); exit(EXIT_FAILURE);
	}

	printf("background.r = %f\n", getfield(L, "r"));
	printf("background.g = %f\n", getfield(L, "g"));
	printf("background.b = %f\n", getfield(L, "b"));

	lua_settop(L, 0);

	/* Call Lua function */
	lua_getglobal(L, "f"); /* function to be called */
	lua_pushnumber(L, 10.0); /* push 1st argument */
	lua_pushnumber(L, 0.0); /* push 2nd argument */
	if (lua_pcall(L, 2, 1, 0) != 0) { /* do the call (2 arguments, 1 result) */
		printf("error running function ’f’: %s\n", lua_tostring(L, -1));
		lua_close(L); exit(EXIT_FAILURE);
	}
	printf("f(10,0) = %f\n", luaL_checknumber(L, -1)); /* = -20 */

	lua_settop(L, 0); 

	exit(EXIT_SUCCESS);
}
