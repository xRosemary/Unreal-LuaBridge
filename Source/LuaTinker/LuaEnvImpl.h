#ifndef _LUA_ENV_IMPL_H_
#define _LUA_ENV_IMPL_H_

#include "LuaCore.h"
#include "LuaEnv.h"

namespace LuaBridge
{
	class LuaEnvImpl : public ILuaEnv
	{
	protected:
		struct lua_State* L;

	public:
		LuaEnvImpl();
		~LuaEnvImpl();

		virtual void DynamicBind(class UObject* ObjectBase, const TCHAR* ModuleName);
		virtual void NotifyUObjectCreated(class UObject* ObjectBase);
		virtual void NotifyUObjectDeleted(class UObject* ObjectBase);
		virtual void ExecCallLua(UObject* Context, UFunction* NativeFunc, FFrame& TheStack, RESULT_DECL);
		virtual bool IsBind(UObject* Context);
		virtual void DoCleanUp();
		virtual void LuaEngineTick(FLOAT DeltaSeconds);
		virtual void LuaMain();

	private:
		bool TryToBind(UObject* Object);
		bool BindInternal(UObject* Object, UClass* Class, const TCHAR* InModuleName);
		bool LoadTableForObject(UObject* Object, const TCHAR* InModuleName);
		void RunStepGC();

	private:
        double stepGCTimeLimit;
        int stepGCCountLimit;
        double fullGCInterval;
        double lastFullGCSeconds;
	};

	// 空服务的实现，为了方便用启动命令行停用所有lua服务
	class LuaEnvImplNull : public ILuaEnv
	{
	public:
		virtual void DynamicBind(class UObject* ObjectBase, const TCHAR* ModuleName) {}
		virtual void NotifyUObjectCreated(class UObject* ObjectBase) {}
		virtual void NotifyUObjectDeleted(class UObject* ObjectBase) {}
		virtual void ExecCallLua(UObject* Context, UFunction* NativeFunc, FFrame& TheStack, RESULT_DECL) {}
		virtual bool IsBind(UObject* Context) { return false; }
		virtual void DoCleanUp() {}
		virtual void LuaEngineTick(FLOAT DeltaSeconds) {}
		virtual void LuaMain() {}
	};
}

#endif
