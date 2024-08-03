#pragma once

#include "CoreMinimal.h"

#ifdef __cplusplus
extern "C" {
#endif
#include "ThirdParty/lua.h"
#include "ThirdParty/lualib.h"
#include "ThirdParty/lauxlib.h"
#ifdef __cplusplus
}
#endif

namespace LuaBridge
{
	// UProperty 与 lua 交互
	extern void PushBytesToLua(lua_State* L, FProperty* Property, BYTE* Params);
	extern void PullBytesFromLua(lua_State* L, FProperty* Property, BYTE* OutParams, int Index);
	extern void PushUPropertyToLua(lua_State* L, FProperty* Property, UObject* Obj);
	extern void PullUPropertyFromLua(lua_State* L, FProperty* Property, UObject* Obj);

	extern void PushUFunctionToLua(lua_State* L, UFunction* Func, UObject* Obj);

	// 将栈顶的lua表放到全局表里的UE表中
	extern void SetTableForClass(lua_State* L, const char* Name);

	// 创建UObject对应的Lua Instance的代理表
	// 代理表触发GC时会删除相应的Lua Instance，以及GObjectReferencer中的强引用
	// 这样使得两边都能释放干净
	extern void PushInstanceProxy(lua_State* L, UObject* Object);

	// 虚幻端主动GC时，删除lua的注册表里相应的UObject对象
	extern void UnRegisterObjectToLua(lua_State* L, UObject* Object);

	// 获取注册表里的表
	extern void GetRegistryTable(lua_State* L, const char* TableName, bool IsWeakTable = false);

	// 从"ObjectMap"里获取UObject对应的lua表实例
	extern bool GetObjectLuaInstance(lua_State* L, UObject* Object);
	// 获取lua代理对象对应的UObject指针
	extern UObject* GetUObjectFromLuaProxy(lua_State* L, int Index);
	// 将UObject压入lua栈中
	extern void PushUObject(lua_State* L, UObject* Object);
	extern void PushObjectModule(lua_State* L, UObject* Object);

	// 注册UClass的原表
	// extern int Global_LoadClass(lua_State *L);
	// extern int Global_NewObject(lua_State *L);

	// Lua释放UObject的引用
	extern int Global_LuaUnRef(lua_State* L);

	// UObject 的 Userdata 里自带的方法
	extern int UObject_Index(lua_State* L);
	extern int UObject_StaticClass(lua_State* L);

	// UObject 对应的 Lua Instance 里自带的方法
	extern int LuaInstance_Index(lua_State* L);
	extern int LuaInstance_NewIndex(lua_State* L);

	// Lua Instance 对应的 代理对象 里自带的方法
	extern int LuaProxy_Identical(lua_State* L);
	extern int LuaProxy_Delete(lua_State* L);
}