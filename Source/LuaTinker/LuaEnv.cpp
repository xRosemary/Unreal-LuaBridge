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

    /**
     * __index meta methods for class
     */
    int Class_Index(lua_State* L)
    {
        // 获取 Lua 表对象
        luaL_checktype(L, -2, LUA_TTABLE);

        // 获取键名
        const char* key = lua_tostring(L, -1);

        lua_pushvalue(L, -2); // 把表再放一个在lua栈上面，方便后续查找

        // 尝试从UObject里拿值
        // 拿对应的UObject指针
        lua_pushstring(L, "NativePtr");
        lua_rawget(L, -2);
        if (lua_islightuserdata(L, -1))
        {
            UObject* Obj = (UObject*)lua_touserdata(L, -1);
            lua_pop(L, 1);
            // 将对应UProperty的数值压栈
            FName PropertyName(key);
            for (TFieldIterator<FProperty> Property(Obj->GetClass()); Property; ++Property)
            {
                if (Property->GetFName() == PropertyName)
                {
                    lua_pop(L, 1);
                    PushUPropertyToLua(L, *Property, Obj); // 再把值塞进去
                    return 1;
                }
            }

            UFunction* UFunc = Obj->FindFunction(PropertyName);
            if (UFunc)
            {
                lua_pop(L, 1);
                PushUFunctionToLua(L, UFunc, Obj);
                return 1;
            }
        }
        else
        {
            lua_pop(L, 1); // 弹出nil
        }

        lua_pushstring(L, "__ClassDesc");
        lua_rawget(L, -2);
        if (!lua_istable(L, -1))
        {
            lua_pop(L, 2);
            lua_pushnil(L);
            return 1;
        }

        lua_pushvalue(L, -3); // 压入key
        lua_gettable(L, -2);
        lua_remove(L, -2); // 把__ClassDesc拿到的表移出去
        lua_remove(L, -2); // 把多压的拷贝表移出去

        return 1;
    }

    /**
     * __newindex meta methods for class
     */
    int Class_NewIndex(lua_State* L)
    {
        // 获取 Lua 表对象
        luaL_checktype(L, -3, LUA_TTABLE);

        // 获取键名
        const char* key = lua_tostring(L, -2);

        lua_pushvalue(L, -3); // 把表再放一个在lua栈上面，方便后续查找

        // 尝试从UObject里拿值
        // 拿对应的UObject指针
        lua_pushstring(L, "NativePtr");
        lua_rawget(L, -2);
        if (!lua_islightuserdata(L, -1))
        {
            lua_settop(L, 3);
            return 0;
        }

        UObject* Obj = (UObject*)lua_touserdata(L, -1);
        lua_settop(L, 3);

        // 修改对应UProperty的数值
        FName PropertyName(key);
        for (TFieldIterator<FProperty> Property(Obj->GetClass()); Property; ++Property)
        {
            if (Property->GetFName() == PropertyName)
            {
                PullUPropertyFromLua(L, *Property, Obj);
                break;
            }
        }

        return 0;  // 返回值数量为0
    }

    static void CreateUnrealMetaTable(lua_State* L)
    {
        int Type = luaL_getmetatable(L, "__UMT");
        if (Type == LUA_TTABLE)
        {
            return;
        }

        // 没有的话先把nil弹出，然后创建一个metatable
        lua_pop(L, 1);
        luaL_newmetatable(L, "__UMT");
        lua_pushvalue(L, -1);
        lua_setmetatable(L, -2);

        //lua_pushstring(L, "__index");
        //lua_pushcfunction(L, Class_Index);
        //lua_rawset(L, -3);
    }

    static int CreateUClass(lua_State* L)
    {
        lua_createtable(L, 0, 0);
        lua_createtable(L, 0, 0);
        lua_pushstring(L, "__index");
        CreateUnrealMetaTable(L);
        lua_rawset(L, -3);
        lua_setmetatable(L, -2);
        return 1;
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
      {"UClass", CreateUClass},
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
            return false;
        }

        // 函数重载
        for (TFieldIterator<UFunction> It(Class, EFieldIteratorFlags::IncludeSuper, EFieldIteratorFlags::ExcludeDeprecated, EFieldIteratorFlags::ExcludeInterfaces); It; ++It)
        {
            lua_pushstring(L, TCHAR_TO_UTF8(*It->GetName()));
            lua_rawget(L, -2);

            if (lua_isfunction(L, -1))
            {
                It->SetNativeFunc(&LuaEnv::execCallLua);
            }

            lua_pop(L, 1);
        }

        return true;
    }

    DEFINE_FUNCTION(LuaEnv::execCallLua)
    {
        UFunction* NativeFunc = Stack.CurrentNativeFunction;

        UE_LOG(LogTemp, Error, TEXT("execCallLua: Obj %s, Native Func %s"), *Context->GetName(), *NativeFunc->GetName());

        const char* ModuleName = TCHAR_TO_UTF8(*Context->GetClass()->GetName());
        if (lua_getglobal(L, ModuleName) != LUA_TTABLE)
        {
            lua_pop(L, 1);
            P_FINISH;
            return;
        }

        lua_pushstring(L, TCHAR_TO_UTF8(*NativeFunc->GetName()));
        lua_gettable(L, -2); // 在表中查找对应函数
        if (lua_isfunction(L, -1))
        {
            PushUserData(Context); // 将UObject的指针作为Self参数

            BYTE* Params = (BYTE*)FMemory::Malloc(NativeFunc->ParmsSize);
            FMemory::Memzero(Params, NativeFunc->ParmsSize);
            for (TFieldIterator<FProperty> It(NativeFunc); It && (It->PropertyFlags & CPF_Parm) == CPF_Parm; ++It)
            {
                Stack.Step(Stack.Object, It->ContainerPtrToValuePtr<BYTE>(Params));
            }
            Stack.SkipCode(1);          // skip EX_EndFunctionParms

            InternalCallLua(L, NativeFunc, Params, (BYTE*)RESULT_PARAM);

            FMemory::Free(Params);
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

    void LuaEnv::PushUserData(UObject* Object)
    {
        lua_createtable(L, 0, 0);
        lua_pushstring(L, "NativePtr");
        lua_pushlightuserdata(L, Object);
        lua_rawset(L, -3);

        lua_pushstring(L, "__index");
        lua_pushcfunction(L, Class_Index);
        lua_rawset(L, -3);

        lua_pushstring(L, "__newindex");
        lua_pushcfunction(L, Class_NewIndex);
        lua_rawset(L, -3);

        lua_pushvalue(L, -1);
        lua_setmetatable(L, -2);

        lua_pushstring(L, "__ClassDesc");
        int Type = lua_getglobal(L, TCHAR_TO_UTF8(*Object->GetClass()->GetName()));
        if (Type == LUA_TTABLE)
        {
            lua_rawset(L, -3);
        }
        else
        {
            lua_pop(L, 2);
        }
    }

    bool LuaEnv::LoadTableForObject(UObject* Object, const char* InModuleName)
    {
        const char* TableName = TCHAR_TO_UTF8(*Object->GetClass()->GetName());
        lua_getglobal(L, TableName);
        if (lua_istable(L, -1)) // 已经有了
        {
            return true;
        }

        lua_settop(L, 0);

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

        // 加载成功，返回值为表
        if (!lua_istable(L, -1))
        {
            lua_settop(L, 0);
            return false;
        }

        lua_pushvalue(L, -1);
        lua_setglobal(L, TableName);
        return true;
    }

}