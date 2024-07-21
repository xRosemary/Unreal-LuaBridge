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

	// 将一个在虚幻持有引用的UObject放到lua的注册表中
	// 此时lua不持有这个UObject的引用，所以会正常被虚幻释放
	// 可以理解为lua里有一个UObject的影子对象
	extern void RegisterObjectToLua(lua_State* L, UObject* Object);

	// 虚幻端主动GC时，删除lua的注册表里相应的UObject对象
	extern void UnRegisterObjectToLua(lua_State* L, UObject* Object);

	// 获取注册表里的表
	extern void GetRegistryTable(lua_State* L, const char* TableName);

	// 从"ObjectMap"里获取UObject对应的lua表实例
	extern bool GetObjectLuaInstance(lua_State* L, UObject* Object);
	// 获取lua表实例对应的UObject指针
	extern UObject* GetUObjectFromLuaInstance(lua_State* L, int Index);
	// 将UObject压入lua栈中
	extern void PushUObject(lua_State* L, UObject* Object);
	extern void PushObjectModule(lua_State* L, UObject* Object);

	// 注册UClass的原表
	// extern int Global_LoadClass(lua_State *L);
	// extern int Global_NewObject(lua_State *L);

	// Lua持有UObject的引用
	extern int Global_LuaRef(lua_State* L);
	// Lua释放UObject的引用
	extern int Global_LuaUnRef(lua_State* L);

	// UObject 的 Userdata 里自带的方法
	extern int UObject_Index(lua_State* L);
	extern int UObject_StaticClass(lua_State* L);
	extern int UObject_Cast(lua_State* L);
	extern int UObject_Identical(lua_State* L);
	extern int UObject_Delete(lua_State* L);

	// UObject 对应的 Lua Instance 里自带的方法
	extern int LuaInstance_Index(lua_State* L);
	extern int LuaInstance_NewIndex(lua_State* L);
}