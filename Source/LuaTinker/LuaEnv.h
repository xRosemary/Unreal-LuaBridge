#ifndef _LUA_ENV_H_
#define _LUA_ENV_H_

#include "../Core.h"
#include "../UnObjBas.h"

// #ifndef CORE_API
// #define CORE_API DLL_IMPORT
// #endif

namespace LuaBridge
{
	class ILuaEnv
	{
	public:
		virtual void DynamicBind(class UObject* ObjectBase, const TCHAR* ModuleName) = 0;
		virtual void NotifyUObjectCreated(class UObject* ObjectBase) = 0;
		virtual void NotifyUObjectDeleted(class UObject* ObjectBase) = 0;
		virtual void ExecCallLua(UObject* Context, UFunction* NativeFunc, FFrame& TheStack, RESULT_DECL) = 0;
		virtual bool IsBind(UObject* Context) = 0;
		virtual void DoCleanUp() = 0;
		virtual void LuaEngineTick(FLOAT DeltaSeconds) = 0;
		virtual void LuaMain() = 0;
	};

	class CORE_API LuaEnv : public ILuaEnv
	{
	private:
		ILuaEnv* m_EnvImpl;

	public:
		static LuaEnv* GetInstance();
		virtual void DynamicBind(class UObject* ObjectBase, const TCHAR* ModuleName);
		virtual void NotifyUObjectCreated(class UObject* ObjectBase);
		virtual void NotifyUObjectDeleted(class UObject* ObjectBase);
		virtual void ExecCallLua(UObject* Context, UFunction* NativeFunc, FFrame& TheStack, RESULT_DECL);

		virtual bool IsBind(UObject* Context);
		virtual void DoCleanUp();
		virtual void LuaEngineTick(FLOAT DeltaSeconds);
		virtual void LuaMain();
	};
}
#endif