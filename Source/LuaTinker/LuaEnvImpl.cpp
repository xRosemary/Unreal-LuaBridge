#include "CorePrivate.h"
#include "../Inc/LuaBridge/UEObjectReferencer.h"
#include "../Inc/LuaBridge/LuaEnvImpl.h"

#ifdef __cplusplus
extern "C" {
#endif
#include "../Inc/LuaBridge/Lua/lua.h"
#include "../Inc/LuaBridge/Lua/lualib.h"
#include "../Inc/LuaBridge/Lua/lauxlib.h"
#ifdef __cplusplus
}
#endif

namespace LuaBridge
{
    typedef UProperty FProperty;

    static FString GetLuaMessage(lua_State* L)
    {
        guard(GetLuaMessage, DqhoTqaBla0izEzylXaDuvjPzzgoJyyq);
        const int ArgCount = lua_gettop(L);
        FString Message;
        if (!lua_checkstack(L, ArgCount))
        {
            return TEXT("too many arguments, stack overflow");
        }
        for (int ArgIndex = 1; ArgIndex <= ArgCount; ArgIndex++)
        {
            if (ArgIndex > 1)
                Message += TEXT("\t");

            const char* MsgParam = lua_tostring(L, ArgIndex);
            if (MsgParam)
            {
                Message += ANSI_TO_TCHAR(MsgParam);
            }
        }
        return Message;
        unguard;
    }

    static void GetLoadedModuleTable(lua_State* L, bool IsWeakTable)
    {
        guard(GetLoadedModuleTable, n6SSsnD2JUtFmRMEMMvvzXUGd8Cj2IaE);
        if(IsWeakTable)
        {
            lua_rawgeti(L, LUA_REGISTRYINDEX, WEAK_LOADED_MODULE_REGISTRY_KEY);
        }
        else
        {
            lua_rawgeti(L, LUA_REGISTRYINDEX, LOADED_MODULE_REGISTRY_KEY);
        }
        unguard;
    }

    static int CustomRequire(lua_State* L)
    {
        guard(CustomRequire, NAhiGJgAaCGjoodYJDhIQ4KVBt1PULV2);
        const char* modname = luaL_checkstring(L, 1);
        bool isWeak = false;
        if(lua_gettop(L) == 2)
        {
            isWeak = !!lua_toboolean(L, 2);
        }

        lua_getfield(L, LUA_REGISTRYINDEX, LUA_LOADED_TABLE);
        lua_getfield(L, -1, modname);  /* LOADED[name] */
        if(!lua_isnil(L, -1))
        {
            lua_remove(L, -2); // 弹出LOADED表
            return 1;
        }

        // 先走Cache
        GetLoadedModuleTable(L, isWeak);
        lua_pushvalue(L, -2);
        lua_rawget(L, -2);
        if(!lua_isnil(L, -1))
        {
            lua_remove(L, -2); // 弹出LoadedModule表
            return 1;
        }
        
        // 弹出nil和LoadedModule表
        lua_pop(L, 2);

        FString ModulePath(ANSI_TO_TCHAR(modname));

#if KFX_REPLACE_LUA_LOAD_BUFFER
        // 替换掉点号, 跳过前面的包名"KFXGameX."
        for (int i = 10; i < ModulePath.Len(); i++)
        {
            if(ModulePath[i] == TEXT('.'))
            {
                ModulePath[i] = TEXT('_');
            }
        }

        ULuacResource* Resource = LoadObject<ULuacResource>(NULL, *ModulePath, NULL, LOAD_NoFail, NULL);
        if(Resource)
        {
            // 加载字节码
            if (luaL_loadbuffer(L, (const char*)Resource->Data.GetData(), Resource->Data.Num(), modname) == LUA_OK)
            {
                // 执行模块代码
                if (lua_pcall(L, 0, 1, 0) == LUA_OK)
                {
                    GetLoadedModuleTable(L, isWeak);
                    lua_pushstring(L, modname);
                    lua_pushvalue(L, -3);   // 复制一份模块
                    lua_rawset(L, -3);      // LoadedModule[modname] = module
                    lua_pop(L, 1);          // 弹出LoadedModule表
                    delete Resource;
                    return 1; // 返回模块
                }
                else
                {
                    lua_pop(L, 1); // 弹出错误信息
                    delete Resource;
                    return luaL_error(L, "error running module '%s': %s",
                                    modname, lua_tostring(L, -1));
                }
            }
            else
            {
                delete Resource;
                return luaL_error(L, "error loading module '%s': %s",
                                modname, lua_tostring(L, -1));
            }
        }
        
        return luaL_error(L, "[Error] Load Lua Module Failed: %s", modname);
#else
        ModulePath = ModulePath.Replace(TEXT("KFXGameX"), TEXT("LuaScript.Src")); // 替换前面的包名"KFXGameX"为"LuaScript.Src"
        for (int i = 0; i < ModulePath.Len(); i++) // 替换点为斜杠
        {
            if(ModulePath[i] == TEXT('.'))
            {
                ModulePath[i] = TEXT('\\');
            }
        }

        ModulePath += TEXT(".lua"); // 添加后缀
        luaL_dofile(L, TCHAR_TO_ANSI(*ModulePath));
        GetLoadedModuleTable(L, isWeak);
        lua_pushstring(L, modname);
        lua_pushvalue(L, -3);   // 复制一份模块
        lua_rawset(L, -3);      // LoadedModule[modname] = module
        lua_pop(L, 1);          // 弹出LoadedModule表
        return 1; // 返回模块
#endif
        unguard;
    }

    static int LuaLogInfo(lua_State* L)
    {
        guard(LuaLogInfo, naLpwPxjBQOPcHMTstemPlFdAkVizNpM);
        const FString& Msg = GetLuaMessage(L);
        debugf(TEXT("%s"), *Msg);
        return 0;
        unguard;
    }

    LuaEnvImpl::LuaEnvImpl()
    {
        guard(LuaEnvImpl::LuaEnvImpl, IGqjs8WMWurukLHF9lQYlyoateBwiEVk);
        L = luaL_newstate();
        AutoLuaStack as(L);
        luaL_openlibs(L);
        InitRegistryTable(L);
        RegisterCommonLib(L);
        RegisterExtLib(L);
        lua_register(L, "print", LuaLogInfo);
        lua_register(L, "require", CustomRequire);

        // GC相关参数初始化
        lua_gc(L, LUA_GCGEN, 100, 50);
		lua_gc(L, LUA_GCCOLLECT);
        stepGCTimeLimit = 0.001;    // 每帧最多 1 毫秒用于 GC
        stepGCCountLimit = 20;      // 每帧最多执行 20 步 GC
        fullGCInterval = 30.0;      // 每 30 秒触发一次完整 GC
        lastFullGCSeconds = 0.0;    // 系统自动更新，无需手动设置

        unguard;
    }

    LuaEnvImpl::~LuaEnvImpl()
    {
        guard(LuaEnvImpl::~LuaEnvImpl, XA0miryMTgN4QzBIGEVy1HJUEAkkDjES);
        if (L)
        {
            lua_close(L);
            L = NULL;
            UObjectReferencer::ReleaseEnv();
        }
        unguard;
    }

    void LuaEnvImpl::DynamicBind(class UObject* ObjectBase, const TCHAR* ModuleName)
    {
        guard(LuaEnvImpl::DynamicBind, JK7tXIipaZH4uW04czJYn8kZlmipJrnf);
        if (!CheckObjIsValid(ObjectBase))           // filter out CDO and ArchetypeObjects
        {
            return;
        }

        if (ObjectBase->IsA(UPackage::StaticClass()) || ObjectBase->IsA(UClass::StaticClass()))    // filter out UPackage and UClass
        {
            return;
        }

        UClass* Class = ObjectBase->GetClass();
        if (ObjectBase == Class->GetDefaultObject())
        {
            return;
        }

        BindInternal(ObjectBase, Class, ModuleName);
        unguard;
    }

    bool LuaEnvImpl::TryToBind(UObject* Object)
    {
        guard(LuaEnvImpl::TryToBind, a0f2qJpi80HvshA0EnxrWyTMIFMUFffP);
        if (!CheckObjIsValid(Object))           // filter out CDO and ArchetypeObjects
        {
            return false;
        }

        if (Object->IsA(UPackage::StaticClass()) || Object->IsA(UClass::StaticClass()))    // filter out UPackage and UClass
        {
            return false;
        }

        UClass* Class = Object->GetClass();
        if (Object == Class->GetDefaultObject())
        {
            return false;
        }

        UFunction* Func = Object->FindFunction(FName(TEXT("GetModuleName")));    // find UFunction 'GetModuleName'. hard coded!!!
        if (Func)
        {
            if (Func->GetNativeFunc() /* && IsInGameThread() */)
            {
                FString ModuleName;
                UObject* DefaultObject = Class->GetDefaultObject();             // get CDO
                DefaultObject->UObject::ProcessEvent(Func, &ModuleName);        // force to invoke UObject::ProcessEvent(...)

                debugf(TEXT("ProcessEvent ModuleName: %s"), *ModuleName);

                if (ModuleName.Len() < 1)
                {
                    ModuleName = Class->GetName();
                }
                return BindInternal(Object, Class, *ModuleName);   // bind!!!
            }
            else
            {
                return false;
            }
        }

        return false;
        unguard;
    }

    // 遍历Lua表中的所有键值对，拿到所有的重载函数名
    static void GetAllLuaFunctionNames(lua_State* L, TArray<FName>& Names)
    {
        guard(GetAllLuaFunctionNames, Vlage0hXu9GA6QzQxLLzP72UchdtuVHj);
        AutoLuaStack as(L);

        // 拷贝一份Module表
        lua_pushvalue(L, -3);

        // 遍历Module表
        lua_pushnil(L);
        while (lua_next(L, -2) != 0)
        {
            if (lua_type(L, -1) == LUA_TFUNCTION && !lua_iscfunction(L, -1))
            {
                Names.AddItem(FName(FastConvertToTCHAR(lua_tostring(L, -2))));
            }
            lua_pop(L, 1);
        }
        unguard;
    }

    bool LuaEnvImpl::BindInternal(UObject* Object, UClass* Class, const TCHAR* InModuleName)
    {
        guard(LuaEnvImpl::BindInternal, NXCtveip2N7k4s3jT3KoybNRGU4DZGNy);
        check(Object && Class);
        AutoLuaStack as(L);
        debugf(TEXT("InModuleName: %s"), InModuleName);

        if (!LoadTableForObject(Object, InModuleName))
        {
            return false;
        }

        luaL_checktype(L, -1, LUA_TTABLE);         // 此时-1为Module表，栈大小为1
        PushUObject(L, Object);                    // Object Instance 注册到Lua
        lua_getfield(L, -1, "super");              // 获取super表
        // 栈大小为3，分别为 super, LuaInstance, Module

        // 重载函数，并将父类的函数放入super表中
        TArray<FName> FuncNames;
        GetAllLuaFunctionNames(L, FuncNames);
        for(int i = 0; i < FuncNames.Num(); i++)
        {
            UFunction* ObjFuncPtr = Object->FindFunction(FuncNames(i));
            if(!ObjFuncPtr) continue;

            ObjFuncPtr->SetNativeLuaFunc();         // 设置Lua标记位

            // 添加 super Function
            UFunction* SuperFuncPtr = ObjFuncPtr->GetSuperFunction();
            if(SuperFuncPtr)
            {
                lua_pushstring(L, FastConvertToANSI(ObjFuncPtr->GetName()));
                PushUFunctionToLua(L, SuperFuncPtr);
                lua_rawset(L, -3);                  // super.FuncName = SuperFuncPtr
            }
        }

        return true;
        unguard;
    }

    void LuaEnvImpl::ExecCallLua(UObject* Context, UFunction* NativeFunc, FFrame& Stack, RESULT_DECL )
    {
        guard(LuaEnvImpl::ExecCallLua, 1YAw5oB4fOEUn0rW6t5hO7n5h95ku4qR);
        PushObjectModule(L, Context);
        if (lua_isnil(L, -1))
        {
            lua_pop(L, 1);
            P_FINISH;
            return;
        }

        PushPCallErrorHandler(L);
        lua_pushstring(L, FastConvertToANSI(NativeFunc->GetName()));
        lua_rawget(L, -3); // 在表中查找对应函数
        if (lua_isfunction(L, -1))
        {
            PushUObject(L, Context);

            const bool bUnpackParams = NativeFunc && Stack.Node != NativeFunc;
            BYTE* InParms = bUnpackParams ? (BYTE*)appAlloca(NativeFunc->PropertiesSize) : Stack.Locals;
            if (bUnpackParams)
            {
                appMemzero(InParms, NativeFunc->PropertiesSize);
                for( UProperty* Property=(UProperty*)NativeFunc->Children; *Stack.Code!=EX_EndFunctionParms; Property=(UProperty*)Property->Next )
                {
                    GPropAddr = NULL;
                    GPropObject = NULL;
                    BYTE* Param = InParms + Property->GetCustomOffset();
                    Stack.Step( Stack.Object, Param );
                }
                P_FINISH; // skip EX_EndFunctionParms
            }
            else
            {
                InParms = Stack.Locals;
            }

            InternalCallLua(L, NativeFunc, InParms, (BYTE*)Result);

            if (bUnpackParams)
            {
				for( UProperty* Property=(UProperty*)NativeFunc->Children; Property; Property=(UProperty*)Property->Next )
                {
                    Property->DestroyValue(InParms + Property->GetCustomOffset());
				}
            }
        }
        else
        {
            lua_pop(L, 2); // 弹出nil和错误处理函数
        }

        lua_pop(L, 1); // 弹出Table
        unguard;
    }

    void LuaEnvImpl::NotifyUObjectCreated(UObject* Object)
    {
        TryToBind(Object);
    }

    void LuaEnvImpl::NotifyUObjectDeleted(UObject* Object)
    {
        UnRegisterObjectToLua(L, Object);
    }

    // lua栈大小+1，加载成功时栈顶为Module表，否则栈顶为报错信息
    static bool LoadLuaModule(lua_State* L, const TCHAR* InModuleName)
    {
        guard(LoadLuaModule, kazysIdVSUw6quPD4ivUBWImMnXqnhec);
        lua_getglobal(L, "require");
        lua_pushstring(L, TCHAR_TO_ANSI(InModuleName));
        lua_pushboolean(L, true);
        return lua_pcall(L, 2, 1, 0) == LUA_OK;
        unguard;
    }

    bool LuaEnvImpl::LoadTableForObject(UObject* Object, const TCHAR* InModuleName)
    {
        guard(LuaEnvImpl::LoadTableForObject, DN3AvdKRRbJynLZna3o8sFOa4QpYEENY);
        if(!LoadLuaModule(L, InModuleName))
        {
            if(GLuaErrorMessageDelegate)
            {
                const char* ErrorMsgRawStr = lua_tostring(L, -1);
                FString ErrorMsg = FString::Printf(TEXT("[Error] Load Lua Module Failed: %s"), ANSI_TO_TCHAR(ErrorMsgRawStr));
                GLuaErrorMessageDelegate(ErrorMsg);
            }

            lua_pop(L, 1); // 弹出错误信息
            return false;
        }

        luaL_checktype(L, -1, LUA_TTABLE);  // 此时-1为Module表，栈大小为1
        lua_rawgeti(L, LUA_REGISTRYINDEX, OBJECT_MODULE_REGISTRY_KEY);
        lua_pushvalue(L, -2);
        lua_rawsetp(L, -2, Object); // ObjectModules.Object = Module
        lua_pop(L, 1);
        return true;
        unguard;
    }

    bool LuaEnvImpl::IsBind(UObject* Context)
    {
        return HasObjectLuaModule(L, Context);
    }

    void LuaEnvImpl::RunStepGC()
    {
        guard(LuaEnvImpl::RunStepGC, pUrrp9ClcmZgDMggWbrbrBELSdPNCwYr);
        // use step gc every frame
        int runtimes = 0;
        double stepCost = 0.0;
        int stepCount = 0;
        for (double start = appSeconds(), now = start; stepCount < stepGCCountLimit &&
            now - start + stepCost < stepGCTimeLimit; stepCount++)
        {
            if (lua_gc(L, LUA_GCSTEP, 0)) {
                lastFullGCSeconds = appSeconds();
                break;
            }

            double currentSeconds = appSeconds();
            stepCost = currentSeconds - now;
            now = currentSeconds;
            runtimes++;
        }

        if(stepCost > stepGCTimeLimit)
        {
            debugf(TEXT("GC Step Cost runtimes: %d, stepCost: %f"), runtimes, stepCost);
        }

        unguard;
    }

    void LuaEnvImpl::LuaEngineTick(FLOAT DeltaSeconds)
    {
        guard(LuaEnvImpl::LuaEngineTick, XxrIwDD8cyzVQUsfD640o83huTsVwYCq);
        const int LuaGCSpause = 8;
        if (stepGCTimeLimit > 0.0)
        {
            if (fullGCInterval > 0.0f)
            {
                if (appSeconds() - lastFullGCSeconds > fullGCInterval)
                {
                    RunStepGC(); // 这一行可尝试替换成全量GC
                }
                else if (lua_gcstate(L) != LuaGCSpause)
                {
                    RunStepGC();
                }
            }
            else
            {
                RunStepGC();
            }
        }

        unguard;
    }

    void LuaEnvImpl::LuaMain()
    {
        guard(LuaEnvImpl::LuaMain, izWU2BqWMxeky6iaDqNo6kTLlB9GBRUi);
        if(!GIsEditor)
        {
            // main 文件
            lua_getglobal(L, "require");
            lua_pushstring(L, "KFXGameX.main");
            if(lua_pcall(L, 1, 0, 0) != LUA_OK)
            {
                if(GLuaErrorMessageDelegate)
                {
                    const char* ErrorMsgRawStr = lua_tostring(L, -1);
                    FString ErrorMsg = FString::Printf(TEXT("require KFXGameX.main failed: %s"), ANSI_TO_TCHAR(ErrorMsgRawStr));
                    GLuaErrorMessageDelegate(ErrorMsg);
                }
            }
#if KFX_ENABLE_LUA_SOCKET && KFX_ENABLE_LUA_DEBUG
            else
            {
                luaL_dostring(L, "require(\"KFXGameX.LuaPanda\").start(\"127.0.0.1\",8818)");
            }
#endif
        }
        unguard;
    }

    void LuaEnvImpl::DoCleanUp()
    {
        guard(LuaEnvImpl::DoCleanUp, 6dhQlWDF0hJ1BWE0RM2oYjrzGhK8nMT0);
        AutoLuaStack as(L);

#if !KFX_REPLACE_LUA_LOAD_BUFFER
        // 遍历 __ObjectModules 表，拿到所有的Module指针，用于后续的比对
        TArray<const void*> LoadedModules;
        lua_rawgeti(L, LUA_REGISTRYINDEX, OBJECT_MODULE_REGISTRY_KEY);
        lua_pushnil(L);
        while (lua_next(L, -2) != 0) {
            const void* objectValue = lua_topointer(L, -1);
            LoadedModules.AddUniqueItem(objectValue);
            lua_pop(L, 1); // 弹出值，保留键
        }

        // 遍历注册表，重加载所有的UObject的Module
        GetLoadedModuleTable(L, true); // key为ModuleName，value为Module
        lua_pushnil(L);
        while (lua_next(L, -2) != 0)
        {
            // 获取 __LoadedModule 表中的值
            const void* loadedValue = lua_topointer(L, -1);
            const char* InModuleName = lua_tostring(L, -2);

            if(LoadedModules.FindItemIndex(loadedValue) == INDEX_NONE)
            {
                lua_pop(L, 1); // 弹出值，保留键
                continue;
            }

            // 从 __WeakLoadedModule 表中移除
            lua_rawgeti(L, LUA_REGISTRYINDEX, WEAK_LOADED_MODULE_REGISTRY_KEY);
            lua_pushstring(L, InModuleName);
            lua_pushnil(L);
            lua_rawset(L, -3);
            lua_pop(L, 1);

            // 重新加载模块
            const TCHAR* InModuleWideName = ANSI_TO_TCHAR(InModuleName);
            LoadLuaModule(L, InModuleWideName);
            lua_pop(L, 1);

            // 遍历 __ObjectModules 表
            lua_rawgeti(L, LUA_REGISTRYINDEX, OBJECT_MODULE_REGISTRY_KEY);
            lua_pushnil(L);
            while (lua_next(L, -2) != 0) {
                const void* objectValue = lua_topointer(L, -1);
                if (objectValue == loadedValue) {
                    // 如果值存在于 __LoadedModule 中，将其置为 新的 Module
                    lua_pushvalue(L, -2); // 复制键
                    LoadLuaModule(L, InModuleWideName);
                    lua_rawset(L, -5);
                } 

                lua_pop(L, 1); // 弹出值，保留键
            }
            lua_pop(L, 1); // 弹出 __ObjectModules 表

            lua_pop(L, 1); // 弹出 __LoadedModule 表中的值，保留键
        }
#endif

        // 全量GC一下
        lua_gc(L, LUA_GCCOLLECT);

        unguard;
    }
}
