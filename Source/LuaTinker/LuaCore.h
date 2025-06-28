#ifndef _LUA_CORE_H_
#define _LUA_CORE_H_

#include "../Core.h"
#include "../UnObjBas.h"

#ifdef __cplusplus
extern "C" {
#endif
#include "./Lua/lua.h"
#include "./Lua/lualib.h"
#include "./Lua/lauxlib.h"
#ifdef __cplusplus
}
#endif

namespace LuaBridge
{
	typedef UProperty FProperty;

	extern int REGISTRY_KEY; // UObject的注册表
	extern int REFLECTION_CACHE_REGISTRY_KEY; // 缓存反射信息
	extern int PROXY_REGISTRY_KEY; // 缓存代理对象，避免每次创建
	extern int OBJECT_MODULE_REGISTRY_KEY; // UObject对应的模块表
	extern int LOADED_MODULE_REGISTRY_KEY; // 已经加载的模块表
	extern int WEAK_LOADED_MODULE_REGISTRY_KEY; // 弱引用的已经加载的模块表

	typedef void (*LuaErrorMessage)(FString& ReportContent);
	extern CORE_API LuaErrorMessage GLuaErrorMessageDelegate;

	struct AutoLuaStack {
        AutoLuaStack(lua_State* l) {
            this->L = l;
            this->top = lua_gettop(L);
        }
        ~AutoLuaStack() {
            lua_settop(this->L,this->top);
        }
        lua_State* L;
        int top;
    };

	
	struct AutoLuaZoneCheck {
        AutoLuaZoneCheck(lua_State* l, const char* name) {
            this->L = l;
            this->top = lua_gettop(L);
			ScopeName = FString(ANSI_TO_TCHAR(name));
			debugf(TEXT("AutoLuaZoneCheck Zone Begin[%s]: %d"), *ScopeName, this->top);
        }
        ~AutoLuaZoneCheck() {
			debugf(TEXT("AutoLuaZoneCheck Zone End[%s]: %d"), *ScopeName, lua_gettop(L));
        }
        lua_State* L;
        int top;
		FString ScopeName;
    };


	// UProperty 与 lua 交互
	extern void PushUPropertyToLua(lua_State* L, FProperty* Property, UObject* Obj);
	extern void PullUPropertyFromLua(lua_State* L, FProperty* Property, UObject* Obj);

	extern void PushUFunctionToLua(lua_State* L, UFunction* Func);

	// 压入错误处理函数
	extern void PushPCallErrorHandler(lua_State* L);
	// 根据UFunction执行lua函数
	extern void InternalCallLua(lua_State* L, UFunction* Function, BYTE* Params, BYTE* Result);
	// 虚幻的参数压入lua栈，并且给出返回值类型
    extern FProperty* PrepareParmsForLua(lua_State* L, UFunction* Func, BYTE* Params);

	// 将栈顶的lua表放到全局表里的UE表中
	extern void SetTableForClass(lua_State* L, const char* Name);

	// 创建UObject对应的Lua Instance，Lua栈元素+1
	// 通过Lua Instance可以访问和修改UObject
	extern void PushObjectInstance(lua_State* L, UObject* Object);

	// 创建UObject对应的Lua Instance的代理表
	// 代理表触发GC时会删除相应的Lua Instance，以及GObjectReferencer中的强引用
	// 这样使得两边都能释放干净
	extern void PushInstanceProxy(lua_State* L, UObject* Object);

	// 虚幻端主动GC时，删除lua的注册表里相应的UObject对象
	extern void UnRegisterObjectToLua(lua_State* L, UObject* Object);

	// 初始化注册表
	extern void InitRegistryTable(lua_State* L);

	// 注册常用的数据结构和库
	extern void RegisterCommonLib(lua_State* L);

	// 注册扩展用（例如socket，pb之类的）的数据结构和库
	extern void RegisterExtLib(lua_State* L);

	// 从"LuaModule"里是否存在UObject对应的lua表实例，可用来判定UObject对象是否绑定了lua
	extern bool HasObjectLuaModule(lua_State* L, UObject* Object);
	// 获取lua代理对象对应的UObject指针
	extern UObject* GetUObjectFromLuaProxy(lua_State* L, int Index);
	// 将UObject压入lua栈中
	extern void PushUObject(lua_State* L, UObject* Object);
	extern void PushObjectModule(lua_State* L, UObject* Object);

	// 注册UClass的原表
	extern int Global_LoadClass(lua_State *L);
	extern int Global_NewObject(lua_State *L);

	// 检查是否是有效的UObject
	bool CheckObjIsValid(UObject* pObj);

	// Lua释放UObject的引用
	extern int Global_LuaUnRef(lua_State* L);

	// UObject 的 Userdata 里自带的方法
	extern int UObject_Index(lua_State* L);
	extern int UObject_PushUpValueObject(lua_State* L);

	// UObject 对应的 Lua Instance 里自带的方法
	extern int LuaInstance_Index(lua_State* L);
	extern int LuaInstance_NewIndex(lua_State* L);

	// Lua Instance 对应的 代理对象 里自带的方法
	extern int LuaProxy_Identical(lua_State* L);
	extern int LuaProxy_Delete(lua_State* L);

	extern TCHAR* FastConvertToTCHAR(const char* str);
	extern const char* FastConvertToANSI(const TCHAR* str);
}

#endif