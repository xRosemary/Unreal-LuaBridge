// Fill out your copyright notice in the Description page of Project Settings.


#include "LuaEnv.h"
#include "MyActor.h"
#include "Kismet/KismetSystemLibrary.h"

LuaEnv::LuaEnv()
{
    if (L == nullptr)
    {
        L = luaL_newstate();
        luaL_openlibs(L);
    }
}

LuaEnv::~LuaEnv()
{
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

    // functions
    for (TFieldIterator<UFunction> It(Class, EFieldIteratorFlags::IncludeSuper, EFieldIteratorFlags::ExcludeDeprecated, EFieldIteratorFlags::ExcludeInterfaces); It; ++It)
    {
        //UE_LOG(LogTemp, Error, TEXT("TFieldIterator: %s"), *It->GetName());

        if (It->GetName() == FString("TestFunc"))
        {
            It->SetNativeFunc(&LuaEnv::execCallLua);
        }
    }

    return BindTableForObject(Object, TCHAR_TO_UTF8(InModuleName));
}


DEFINE_FUNCTION(LuaEnv::execCallLua)
{
    UFunction* NativeFunc = Stack.CurrentNativeFunction;
    P_FINISH;

    UE_LOG(LogTemp, Error, TEXT("execCallLua: Obj %s, Native Func %s"), *Context->GetName(), *NativeFunc->GetName());

    //lua_pushlightuserdata(L, (*TablePtr));

    const char* ModuleName = TCHAR_TO_UTF8(*Context->GetClass()->GetName());
    if (lua_getglobal(L, ModuleName) != LUA_TTABLE)
    {
        lua_pop(L, 1);
        return;
    }

    lua_pushstring(L, TCHAR_TO_UTF8(*NativeFunc->GetName()));
    lua_gettable(L, -2); // 在表中查找对应函数
    if (lua_isfunction(L, -1))
    {
        lua_pushvalue(L, -2); // 将表压入栈作为 self 参数
        lua_pushstring(L, "123");
        lua_pcall(L, 2, 1, 0);
        const char* LuaRet = lua_tostring(L, -1);
        lua_pop(L, 1);

        UE_LOG(LogTemp, Error, TEXT("Lua Ret %s"), *FString(LuaRet));
    }
}

void LuaEnv::NotifyUObjectCreated(const UObjectBase* ObjectBase, int32 Index)
{
    static UClass* InterfaceClass = AMyActor::StaticClass();
    if (!ObjectBase->GetClass()->IsChildOf<AMyActor>())
    {
        return;
    }

    UE_LOG(LogTemp, Error, TEXT("Locate for %s"), *ObjectBase->GetClass()->GetName());
    TryToBind((UObject*)(ObjectBase));
}

void LuaEnv::PushMetatable(UObject* Object, const char* MetatableName)
{
    // 有的话就不创建了
    int Type = luaL_getmetatable(L, MetatableName);
    if (Type == LUA_TTABLE)
    {
        return;
    }
    lua_pop(L, 1);

    luaL_newmetatable(L, MetatableName);
    lua_pushstring(L, "NativePtr");
    lua_pushlightuserdata(L, Object);
    lua_rawset(L, -3);


    /*lua_pushstring(L, "__index");
    lua_pushcfunction(L, Class_Index);
    lua_rawset(L, -3);

    lua_pushstring(L, "__newindex");
    //lua_pushcfunction(L, Class_NewIndex);
    //lua_rawset(L, -3);

    uint64 TypeHash = (uint64)ClassDesc->AsStruct();
    lua_pushstring(L, "TypeHash");
    lua_pushnumber(L, TypeHash);
    lua_rawset(L, -3);

    lua_pushstring(L, "ClassDesc");
    lua_pushlightuserdata(L, ClassDesc);
    lua_rawset(L, -3);*/

    //UClass* Class = ClassDesc->AsClass();

    //if (Class != UObject::StaticClass() && Class != UClass::StaticClass())
    //{
    //    lua_pushstring(L, "StaticClass");
    //    lua_pushlightuserdata(L, ClassDesc);
    //    lua_pushcclosure(L, Class_StaticClass, 1);
    //    lua_rawset(L, -3);

    //    lua_pushstring(L, "Cast");
    //    lua_pushcfunction(L, Class_Cast);
    //    lua_rawset(L, -3);

    //    lua_pushstring(L, "__eq");
    //    lua_pushcfunction(L, UObject_Identical);
    //    lua_rawset(L, -3);

    //    lua_pushstring(L, "__gc");
    //    lua_pushcfunction(L, UObject_Delete);
    //    lua_rawset(L, -3);
    //}*/

    lua_pushvalue(L, -1); // set metatable to self
    lua_setmetatable(L, -2);
}

bool LuaEnv::BindTableForObject(UObject* Object, const char* InModuleName)
{
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
        return false;
    }

    // 处理返回的 Lua 表
    lua_pushvalue(L, -1);
    luaL_ref(L, LUA_REGISTRYINDEX);

    //PushMetatable(Object, InModuleName);
    lua_setglobal(L, InModuleName);

    return true;
}
