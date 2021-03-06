#include "stdafx.h"

int GCHookEventObjParam(void *obj, WPARAM wParam, LPARAM lParam, LPARAM param)
{
	lua_State *L = (lua_State*)obj;

	int ref = param;
	lua_rawgeti(L, LUA_REGISTRYINDEX, ref);

	lua_pushnumber(L, wParam);

	GCEVENT *gce = (GCEVENT*)lParam;

	lua_newtable(L);
	lua_pushliteral(L, "Module");
	lua_pushstring(L, gce->pDest->pszModule);
	lua_settable(L, -3);
	lua_pushliteral(L, "Id");
	lua_pushstring(L, ptrA(mir_utf8encodeT(gce->pDest->ptszID)));
	lua_settable(L, -3);
	lua_pushliteral(L, "Type");
	lua_pushinteger(L, gce->pDest->iType);
	lua_settable(L, -3);
	lua_pushliteral(L, "Timestamp");
	lua_pushnumber(L, gce->time);
	lua_settable(L, -3);
	lua_pushliteral(L, "IsMe");
	lua_pushboolean(L, gce->bIsMe);
	lua_settable(L, -3);
	lua_pushliteral(L, "Flags");
	lua_pushinteger(L, gce->dwFlags);
	lua_settable(L, -3);
	lua_pushliteral(L, "Uid");
	lua_pushstring(L, ptrA(mir_utf8encodeT(gce->pDest->ptszID)));
	lua_settable(L, -3);
	lua_pushliteral(L, "Nick");
	lua_pushstring(L, ptrA(mir_utf8encodeT(gce->pDest->ptszID)));
	lua_settable(L, -3);
	lua_pushliteral(L, "Status");
	lua_pushstring(L, ptrA(mir_utf8encodeT(gce->pDest->ptszID)));
	lua_settable(L, -3);
	lua_pushliteral(L, "Text");
	lua_pushstring(L, ptrA(mir_utf8encodeT(gce->pDest->ptszID)));
	lua_settable(L, -3);

	if (lua_pcall(L, 2, 1, 0))
		printf("%s\n", lua_tostring(L, -1));

	int res = (int)lua_tointeger(L, 1);

	return res;
}

static int lua_OnReceiveEvent(lua_State *L)
{
	if (!lua_isfunction(L, 1))
	{
		lua_pushlightuserdata(L, NULL);
		return 1;
	}

	lua_pushvalue(L, 1);
	int ref = luaL_ref(L, LUA_REGISTRYINDEX);

	HANDLE res = ::HookEventObjParam(ME_GC_HOOK_EVENT, GCHookEventObjParam, L, ref);
	lua_pushlightuserdata(L, res);

	CMLua::Hooks.insert(res);
	CMLua::HookRefs.insert(new HandleRefParam(L, res, ref));

	return 1;
}

static luaL_Reg chatApi[] =
{
	{ "OnReceiveEvent", lua_OnReceiveEvent },

	{ NULL, NULL }
};

LUAMOD_API int luaopen_m_chat(lua_State *L)
{
	luaL_newlib(L, chatApi);

	return 1;
}
