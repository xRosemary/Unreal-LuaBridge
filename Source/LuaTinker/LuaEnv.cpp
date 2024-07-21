// Fill out your copyright notice in the Description page of Project Settings.


#include "LuaEnv.h"
#include "MyActor.h"
#include "UEObjectReferencer.h"
#include "Kismet/KismetSystemLibrary.h"

UE_DISABLE_OPTIMIZATION

namespace LuaBridge
{
    int print_lua_stack(lua_State* L) {
        int top = lua_gettop(L); // 获取堆栈顶部索引

        for (int i = 1; i <= top; i++) {
            int type = lua_type(L, i); // 获取值的类型
            UE_LOG(LogTemp, Error, TEXT("print_lua_stack: %d"), type);
        }

        UE_LOG(LogTemp, Error, TEXT("print_lua_stack Size: %d"), lua_gettop(L));

        return 0;
    }

    // 虚幻的参数压入lua栈，并且给出返回值类型
    FProperty* PrepareParmsForLua(lua_State* L, UFunction* Func, BYTE* Params)
    {
        int Offset = 0;

        if (Func == NULL || Params == NULL)
        {
            return NULL;
        }

        for (TFieldIterator<FProperty> IteratorOfParam(Func); IteratorOfParam; ++IteratorOfParam) {
            FProperty* Property = *IteratorOfParam;
            Offset = Property->GetOffset_ForInternal();

            if (Offset >= Func->ReturnValueOffset)
            {
                return Property;
            }

            PushBytesToLua(L, Property, Params + Offset);
        }

        return NULL;
    }

    void InternalCallLua(lua_State* L, UFunction* Function, BYTE* Params, BYTE* Result)
    {
        FProperty* RetType = PrepareParmsForLua(L, Function, Params);

        const bool bHasReturnParam = Function->ReturnValueOffset != MAX_uint16;
        BYTE* ReturnValueAddress = bHasReturnParam ? ((BYTE*)Params + Function->ReturnValueOffset) : NULL;

        checkSlow((RetType == NULL) == bHasReturnParam);

        lua_pcall(L, Function->NumParms - bHasReturnParam + 1, bHasReturnParam, 0);

        if (bHasReturnParam)
        {
            PullBytesFromLua(L, RetType, Result, -1);
            lua_pop(L, 1);
        }
    }

    static FString GetMessage(lua_State* L)
    {
        const auto ArgCount = lua_gettop(L);
        FString Message;
        if (!lua_checkstack(L, ArgCount))
        {
            luaL_error(L, "too many arguments, stack overflow");
            return Message;
        }
        for (int ArgIndex = 1; ArgIndex <= ArgCount; ArgIndex++)
        {
            if (ArgIndex > 1)
                Message += "\t";
            Message += UTF8_TO_TCHAR(luaL_tolstring(L, ArgIndex, NULL));
        }
        return Message;
    }

    static int LogInfo(lua_State* L)
    {
        const auto Msg = GetMessage(L);
        UE_LOG(LogTemp, Log, TEXT("%s"), *Msg);
        return 0;
    }

    static int ErrorInfo(lua_State* L)
    {
        const auto Msg = GetMessage(L);
        UE_LOG(LogTemp, Error, TEXT("%s"), *Msg);
        return 0;
    }

    static const struct luaL_Reg UE_Base_Lib[] = {
      {"log", LogInfo},
      {"error", ErrorInfo},
      {"dump", print_lua_stack},
      {NULL, NULL}  /* sentinel */
    };

    int luaopen_UE_BaseLib(lua_State* L) {
        luaL_newlib(L, UE_Base_Lib);
        return 1;
    }

    LuaEnv::LuaEnv()
    {
        if (L == nullptr)
        {
            L = luaL_newstate();
            luaL_openlibs(L);
            luaL_requiref(L, "UE", luaopen_UE_BaseLib, 1);
            lua_pop(L, 1);
        }
    }

    LuaEnv::~LuaEnv()
    {
        if (L)
        {
            lua_close(L);
            L = nullptr;
        }
    }

    bool LuaEnv::TryToBind(UObject* Object)
    {
        if (!IsValid(Object))           // filter out CDO and ArchetypeObjects
        {
            return false;
        }

        UClass* Class = Object->GetClass();
        if (Class->IsChildOf<UPackage>() || Class->IsChildOf<UClass>())             // filter out UPackage and UClass
        {
            return false;
        }

        if (Object->IsDefaultSubobject())
        {
            return false;
        }

        UFunction* Func = Class->FindFunctionByName(FName("GetModuleName"));    // find UFunction 'GetModuleName'. hard coded!!!
        if (Func)
        {
            if (Func->GetNativeFunc() && IsInGameThread())
            {
                FString ModuleName;
                UObject* DefaultObject = Class->GetDefaultObject();             // get CDO
                DefaultObject->UObject::ProcessEvent(Func, &ModuleName);        // force to invoke UObject::ProcessEvent(...)

                UE_LOG(LogTemp, Error, TEXT("ProcessEvent ModuleName: %s"), *ModuleName);

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
    }

    bool LuaEnv::BindInternal(UObject* Object, UClass* Class, const TCHAR* InModuleName)
    {
        check(Object);

        UE_LOG(LogTemp, Error, TEXT("InModuleName: %s"), InModuleName);

        bool IsLoad = LoadTableForObject(Object, TCHAR_TO_UTF8(InModuleName));
        if (!IsLoad)
        {
            lua_pop(L, 1);
            return false;
        }

        PushUObject(L, Object);             // 此时-1位置为Object的lua实例，-2位置为Module表

        // 函数重载
        for (TFieldIterator<UFunction> It(Class, EFieldIteratorFlags::IncludeSuper, EFieldIteratorFlags::ExcludeDeprecated, EFieldIteratorFlags::ExcludeInterfaces); It; ++It)
        {
            lua_pushstring(L, TCHAR_TO_UTF8(*It->GetName()));
            lua_pushvalue(L, -1);
            lua_rawget(L, -4);

            if (lua_isfunction(L, -1))
            {
                It->SetNativeFunc(&LuaEnv::execCallLua);
                lua_pop(L, 1);
                PushUFunctionToLua(L, *It, Object);
                lua_rawset(L, -3);
            }
            else
            {
                lua_pop(L, 2);
            }
        }

        lua_pop(L, 2);                      // 弹出Object的lua实例和Module表
        return true;
    }

    DEFINE_FUNCTION(LuaEnv::execCallLua)
    {
        UFunction* NativeFunc = Stack.CurrentNativeFunction;

        UE_LOG(LogTemp, Error, TEXT("execCallLua: Obj %s, Native Func %s"), *Context->GetName(), *NativeFunc->GetName());

        PushObjectModule(L, Context);
        if (lua_isnil(L, -1))
        {
            lua_pop(L, 1);
            P_FINISH;
            return;
        }

        lua_pushstring(L, TCHAR_TO_UTF8(*NativeFunc->GetName()));
        lua_gettable(L, -2); // 在表中查找对应函数
        if (lua_isfunction(L, -1))
        {
            PushUObject(L, Context);

            BYTE* InParms;
            const bool bUnpackParams = Stack.CurrentNativeFunction && Stack.Node != Stack.CurrentNativeFunction;
            if (bUnpackParams)
            {
                InParms = (BYTE*)FMemory::Malloc(NativeFunc->ParmsSize);
                FMemory::Memzero(InParms, NativeFunc->ParmsSize);

                for (FProperty* Property = (FProperty*)(NativeFunc->ChildProperties);
                    *Stack.Code != EX_EndFunctionParms;
                    Property = (FProperty*)(Property->Next))
                {
                    Stack.Step(Stack.Object, Property->ContainerPtrToValuePtr<BYTE>(InParms));
                }
                P_FINISH; // skip EX_EndFunctionParms
            }
            else
            {
                InParms = Stack.Locals;
            }

            InternalCallLua(L, NativeFunc, InParms, (BYTE*)RESULT_PARAM);
        }
        else
        {
            lua_pop(L, 1); // 弹出nil
        }

        lua_pop(L, 1); // 弹出Table
    }

    void LuaEnv::NotifyUObjectCreated(const UObjectBase* ObjectBase, int Index)
    {
        static UClass* InterfaceClass = AMyActor::StaticClass();
        if (!ObjectBase->GetClass()->IsChildOf<AMyActor>())
        {
            return;
        }

        TryToBind((UObject*)(ObjectBase));
    }

    bool LuaEnv::LoadTableForObject(UObject* Object, const char* InModuleName)
    {
        GetRegistryTable(L, "__LoadedModule");
        const char* ModuleName = TCHAR_TO_UTF8(*Object->GetClass()->GetName());
        lua_pushstring(L, ModuleName);
        lua_rawget(L, -2);
        if (!lua_isnil(L, -1))
        {
            lua_remove(L, -2);
            return true;
        }

        lua_pop(L, 1);

        //lua_getglobal(L, "require");
        FString ProjectDir = UKismetSystemLibrary::GetProjectDirectory();
        ProjectDir += InModuleName;
        ProjectDir += ".lua";
        //lua_pushstring(L, TCHAR_TO_UTF8(*ProjectDir));

        // 调用require函数，两个字符串参数和require函数本身在栈上
        //if (lua_pcall(L, 1, 1, 0) != LUA_OK)
        //{
        //    return false;
        //}

        luaL_dofile(L, TCHAR_TO_UTF8(*ProjectDir));

        if (!lua_istable(L, -1))
        {
            lua_settop(L, 0);
            return false;
        }

        // 加载成功，返回值为表
        lua_pushstring(L, ModuleName);
        lua_pushvalue(L, -2);
        luaL_checktype(L, -4, LUA_TTABLE);
        lua_rawset(L, -4);
        lua_remove(L, -2);
        return true;
    }

}