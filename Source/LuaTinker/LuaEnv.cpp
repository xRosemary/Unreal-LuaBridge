#include "CorePrivate.h"
#include "../Inc/LuaBridge/LuaEnv.h"
#include "../Inc/LuaBridge/LuaEnvImpl.h"
#include "../Inc/LuaBridge/UEObjectReferencer.h"

namespace LuaBridge
{
    LuaEnv* LuaEnv::GetInstance()
    {
        guard(LuaEnv::GetInstance, AG4WINCMDRwtYyR4fHCC9IVI307jS6qN);
        static LuaEnv* Env_Instance = NULL;

        if(!Env_Instance)
        {
            Env_Instance = new LuaEnv();

            if (ParseParam(appCmdLine(), TEXT("nolua")))
            {
                Env_Instance->m_EnvImpl = new LuaEnvImplNull();
            }
            else
            {
                Env_Instance->m_EnvImpl = new LuaEnvImpl();
            }

            check(Env_Instance && Env_Instance->m_EnvImpl);
        }

        return Env_Instance;
        unguard;
    }

    void LuaEnv::DynamicBind(UObject* Object, const TCHAR* ModuleName)
    {
        if(IsBind(Object))
        {
            return;
        }

        m_EnvImpl->DynamicBind(Object, ModuleName);
    }

    void LuaEnv::NotifyUObjectCreated(UObject* Object)
    {
        m_EnvImpl->NotifyUObjectCreated(Object);
    }

	void LuaEnv::NotifyUObjectDeleted(UObject* Object)
	{
        m_EnvImpl->NotifyUObjectDeleted(Object);
	}

    void LuaEnv::ExecCallLua(UObject* Context, UFunction* NativeFunc, FFrame& Stack, RESULT_DECL )
    {
        m_EnvImpl->ExecCallLua(Context, NativeFunc, Stack, Result);
    }

    bool LuaEnv::IsBind(UObject* Context)
    {
        return m_EnvImpl->IsBind(Context);
    }

    void LuaEnv::DoCleanUp()
    {
        m_EnvImpl->DoCleanUp();
    }

    void LuaEnv::LuaEngineTick(FLOAT DeltaSeconds)
    {
        m_EnvImpl->LuaEngineTick(DeltaSeconds);
    }

    void LuaEnv::LuaMain()
    {
        m_EnvImpl->LuaMain();
    }
}
