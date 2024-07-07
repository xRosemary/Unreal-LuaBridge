// Fill out your copyright notice in the Description page of Project Settings.


#include "LuaEnv.h"
#include "MyActor.h"
#include "Kismet/KismetSystemLibrary.h"

UE_DISABLE_OPTIMIZATION

int print_lua_stack(lua_State* L) {
    int top = lua_gettop(L); // 获取堆栈顶部索引
    
    for (int i = 1; i <= top; i++) {
        int type = lua_type(L, i); // 获取值的类型
        UE_LOG(LogTemp, Error, TEXT("print_lua_stack: %d"), type);
    }

    UE_LOG(LogTemp, Error, TEXT("print_lua_stack Size: %d"), lua_gettop(L));

    return 0;
}


void PushUPropertyToLua(lua_State* L, FProperty* Property, UObject* Obj)
{
    if (FFloatProperty* FloatProperty = CastField<FFloatProperty>(Property))
    {
        float Value = FloatProperty->GetFloatingPointPropertyValue(Property->ContainerPtrToValuePtr<float>(Obj));
        lua_pushnumber(L, Value);
    }
    else if (FDoubleProperty* DoubleProperty = CastField<FDoubleProperty>(Property))
    {
        float Value = DoubleProperty->GetFloatingPointPropertyValue(Property->ContainerPtrToValuePtr<double>(Obj));
        lua_pushnumber(L, Value);
    }
    else if (FIntProperty* IntProperty = CastField<FIntProperty>(Property))
    {
        int Value = IntProperty->GetSignedIntPropertyValue(Property->ContainerPtrToValuePtr<int>(Obj));
        lua_pushnumber(L, Value);
    }
    else if (FBoolProperty* BoolProperty = CastField<FBoolProperty>(Property))
    {
        bool Value = BoolProperty->GetPropertyValue(Property->ContainerPtrToValuePtr<void>(Obj));
        lua_pushboolean(L, Value);
    }
    else if (FObjectProperty* ObjectProperty = CastField<FObjectProperty>(Property))
    {
        UObject* Value = ObjectProperty->GetObjectPropertyValue(Property->ContainerPtrToValuePtr<UObject*>(Obj));
        LuaEnv::PushUserData(Value);
    }
    else if (FStrProperty* StringProperty = CastField<FStrProperty>(Property))
    {
        FString Value = StringProperty->GetPropertyValue(Property->ContainerPtrToValuePtr<UObject*>(Obj));
        lua_pushstring(L, TCHAR_TO_UTF8(*Value));
    }
}

void PushBytesToLua(lua_State* L, FProperty* Property, BYTE* Params)
{
    if (Property == NULL || Params == NULL)
    {
        return;
    }
    else if (Property->IsA<FFloatProperty>())
    {
        float Value = *(float*)Params;
        lua_pushnumber(L, Value);
    }
    else if (Property->IsA<FDoubleProperty>())
    {
        double Value = *(double*)Params;
        lua_pushnumber(L, Value);
    }
    else if (Property->IsA<FIntProperty>())
    {
        int Value = *(int*)Params;
        lua_pushnumber(L, Value);
    }
    else if (Property->IsA<FBoolProperty>())
    {
        bool Value = *(bool*)Params;
        lua_pushboolean(L, Value);
    }
    else if (Property->IsA<FObjectProperty>())
    {
        UObject* Value = *(UObject**)Params;
        LuaEnv::PushUserData(Value);
    }
    else if (Property->IsA<FStrProperty>())
    {
        FString Value = *(FString*)Params;
        lua_pushstring(L, TCHAR_TO_UTF8(*Value));
    }
}

void PullBytesFromLua(lua_State* L, FProperty* Property, BYTE* OutParams, int Index)
{
    if (Property == NULL || OutParams == NULL)
    {
        return;
    }
    if (Property->IsA<FFloatProperty>())
    {
        *(float*)(OutParams) = (float)lua_tonumber(L, Index);
    }
    else if (Property->IsA<FDoubleProperty>() != NULL)
    {
        *(double*)(OutParams) = (double)lua_tonumber(L, Index);
    }
    else if (Property->IsA<FIntProperty>() != NULL)
    {
        *(int*)(OutParams) = (int)lua_tointeger(L, Index);
    }
    else if (Property->IsA<FBoolProperty>() != NULL)
    {
        *(bool*)(OutParams) = (bool)lua_toboolean(L, Index);
    }
    else if (Property->IsA<FObjectProperty>() != NULL)
    {
        *(UObject**)(OutParams) = (UObject*)lua_touserdata(L, Index);
    }
    else if (Property->IsA<FStrProperty>() != NULL)
    {
        new(OutParams)FString(lua_tostring(L, Index));
    }
}

// lua栈的参数压入虚幻，并且给出返回值类型
FProperty* PrepareParmsForUE(lua_State* L, UFunction* Func, BYTE* Params)
{
    int LuaParamNum = lua_gettop(L);
    int Offset = 0;
    int i = 1;

    if (Func == NULL || Params == NULL)
    {
        return NULL;
    }

    // 不需要lua传过来的self
    if (lua_istable(L, 1))
    {
        i++;
    }

    for (TFieldIterator<FProperty> IteratorOfParam(Func); IteratorOfParam; ++IteratorOfParam) {
        FProperty* Property = *IteratorOfParam;
        Offset = Property->GetOffset_ForInternal();

        if (Offset >= Func->ReturnValueOffset)
        {
            lua_settop(L, 0);
            return Property;
        }

        if (i > LuaParamNum)
        {
            break;
        }

        PullBytesFromLua(L, Property, Params + Offset, i);

        i++;
    }

    lua_settop(L, 0);

    return NULL;
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

int LuaCallUFunction(lua_State* L)
{
    UObject*   Obj  = (UObject*)lua_touserdata(L, lua_upvalueindex(1));
    UFunction* Func = (UFunction*)lua_touserdata(L, lua_upvalueindex(2));

    BYTE* Parms = (BYTE*)FMemory::Malloc(Func->ParmsSize);
    FMemory::Memzero(Parms, Func->ParmsSize);
    FProperty* RetType = PrepareParmsForUE(L, Func, Parms);
    Obj->ProcessEvent(Func, Parms); // 调用函数
    PushBytesToLua(L, RetType, Parms + Func->ReturnValueOffset); // 压回返回值
    FMemory::Free(Parms); // 释放参数内存

    return RetType == NULL ? 0 : 1;
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

void PushUFunctionToLua(lua_State* L, UFunction* Func, UObject* Obj)
{
    lua_pushlightuserdata(L, Obj);
    lua_pushlightuserdata(L, Func);
    lua_pushcclosure(L, LuaCallUFunction, 2);
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
    luaL_checktype(L, 1, LUA_TTABLE);

    // 获取键名和值
    const char* key = luaL_checkstring(L, 2);

    int value = luaL_checkinteger(L, 3);

    // 在此处可以实现自定义的赋值行为，比如设置特定的键对应的值

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
    lua_close(L);
    L = nullptr;
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
    //P_FINISH;

    UE_LOG(LogTemp, Error, TEXT("execCallLua: Obj %s, Native Func %s"), *Context->GetName(), *NativeFunc->GetName());

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
    lua_getglobal(L, InModuleName);
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
    lua_setglobal(L, InModuleName);
    return true;
}
