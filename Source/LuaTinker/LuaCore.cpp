#include "LuaCore.h"
#include "UEObjectReferencer.h"

UE_DISABLE_OPTIMIZATION

namespace LuaBridge
{
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
            PushUObject(L, Value);
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
            *(UObject**)(OutParams) = GetUObjectFromLuaInstance(L, Index);
        }
        else if (Property->IsA<FStrProperty>() != NULL)
        {
            new(OutParams)FString(lua_tostring(L, Index));
        }
    }

    void PushUPropertyToLua(lua_State* L, FProperty* Property, UObject* Obj)
    {
        BYTE* Params = (BYTE*)Obj + Property->GetOffset_ForInternal();
        PushBytesToLua(L, Property, Params);
    }

    void PullUPropertyFromLua(lua_State* L, FProperty* Property, UObject* Obj)
    {
        BYTE* Params = (BYTE*)Obj + Property->GetOffset_ForInternal();
        PullBytesFromLua(L, Property, Params, -1);
    }

    // lua栈的参数压入虚幻，并且给出返回值类型
    static FProperty* PrepareParmsForUE(lua_State* L, UFunction* Func, BYTE* Params)
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

    static int LuaCallUFunction(lua_State* L)
    {
        UObject* Obj = (UObject*)lua_touserdata(L, lua_upvalueindex(1));
        UFunction* Func = (UFunction*)lua_touserdata(L, lua_upvalueindex(2));

        BYTE* Parms = (BYTE*)FMemory::Malloc(Func->ParmsSize);
        FMemory::Memzero(Parms, Func->ParmsSize);
        FProperty* RetType = PrepareParmsForUE(L, Func, Parms);
        Obj->ProcessEvent(Func, Parms); // 调用函数
        PushBytesToLua(L, RetType, Parms + Func->ReturnValueOffset); // 压回返回值
        FMemory::Free(Parms); // 释放参数内存

        return RetType == NULL ? 0 : 1;
    }

    void PushUFunctionToLua(lua_State* L, UFunction* Func, UObject* Obj)
    {
        lua_pushlightuserdata(L, Obj);
        lua_pushlightuserdata(L, Func);
        lua_pushcclosure(L, LuaCallUFunction, 2);
    }

    void SetTableForClass(lua_State* L, const char* Name)
    {
        lua_getglobal(L, "UE");
        lua_pushstring(L, Name);
        lua_pushvalue(L, -3);
        lua_rawset(L, -3);
        lua_pop(L, 2);
    }

    static void PushUObjectCore(lua_State* L, UObject* Object)
    {
        if (!Object)
        {
            lua_pushnil(L);
            return;
        }

        void** Dest = (void**)lua_newuserdata(L, sizeof(void*));        // create a userdata
        *Dest = Object;                                                 // store the UObject address

        luaL_getmetatable(L, "__MetaUObject");
        if (lua_istable(L, -1))
        {
            lua_setmetatable(L, -2);
            return;
        }

        lua_pop(L, 1);
        luaL_newmetatable(L, "__MetaUObject");

        lua_pushstring(L, "StaticClass");               // Key
        lua_pushlightuserdata(L, Object->GetClass());   // FClassDesc
        lua_pushcclosure(L, UObject_StaticClass, 1);    // closure
        lua_rawset(L, -3);

        lua_pushstring(L, "Cast");                      // Key
        lua_pushcfunction(L, UObject_Cast);             // C function
        lua_rawset(L, -3);

        lua_pushstring(L, "__eq");                      // Key
        lua_pushcfunction(L, UObject_Identical);        // C function
        lua_rawset(L, -3);

        lua_pushstring(L, "__gc");                      // Key
        lua_pushcfunction(L, UObject_Delete);           // C function
        lua_rawset(L, -3);

        lua_pushstring(L, "__index");                   // Key
        lua_pushcfunction(L, UObject_Index);            // C function
        lua_rawset(L, -3);

        lua_setmetatable(L, -2);
    }

    void RegisterObjectToLua(lua_State* L, UObject* Object)
    {
        check(Object);

        GetRegistryTable(L, "__ObjectMap");
        lua_pushlightuserdata(L, Object);
        lua_pushvalue(L, -1);

        lua_rawget(L, -3);
        if (!lua_isnil(L, -1))
        {
            lua_pop(L, 3);
            return;
        }

        lua_pop(L, 1);

        lua_newtable(L);                                                // 创建实例 Instance

        lua_pushstring(L, "__NativePtr");
        PushUObjectCore(L, Object);
        lua_rawset(L, -3);                                              // Instance.__NativePtr = Object

        lua_pushstring(L, "__ClassDesc");
        PushObjectModule(L, Object);                                    // Object 对应的 lua 模块
        lua_rawset(L, -3);                                              // Instance.__ClassDesc = REQUIRED_MODULE 

        lua_pushstring(L, "__index");
        lua_pushcfunction(L, LuaInstance_Index);
        lua_rawset(L, -3);

        lua_pushstring(L, "__newindex");
        lua_pushcfunction(L, LuaInstance_NewIndex);
        lua_rawset(L, -3);

        lua_pushvalue(L, -1);
        lua_setmetatable(L, -2);                                        // 将 meta table 设为自己

        lua_rawset(L, -3);                                              // ObjectMap.ObjectPtr = Instance
        lua_pop(L, 1);
    }

    void UnRegisterObjectToLua(lua_State* L, UObject* Object)
    {
        if (!Object)
        {
            return;
        }

        GetRegistryTable(L, "__ObjectMap");

        lua_pushlightuserdata(L, Object);
        lua_rawget(L, -2);                                          // ObjectMap.ObjectPtr

        if (lua_isnil(L, -1))
        {
            lua_pop(L, 2);
            return;
        }

        if (lua_istable(L, -1))
        {
            lua_pushstring(L, "__NativePtr");
            lua_pushnil(L);
            lua_rawset(L, -3);                                      // Instance.__NativePtr = nil
            lua_pop(L, 1);
        }

        lua_pushlightuserdata(L, Object);
        lua_pushnil(L);
        lua_rawset(L, -3);                                          // ObjectMap.ObjectPtr = nil
        lua_pop(L, 1);

        GObjectReferencer.RemoveObjectRef(Object);
    }

    bool GetObjectLuaInstance(lua_State* L, UObject* Object)
    {
        if (!Object)
        {
            UE_LOG(LogTemp, Warning, TEXT("%s, Invalid object!"), ANSI_TO_TCHAR(__FUNCTION__));
            return false;
        }

        GetRegistryTable(L, "__ObjectMap");
        lua_pushlightuserdata(L, Object);
        lua_rawget(L, -2);
        lua_remove(L, -2);

        return !!lua_isnil(L, -1);
    }

    UObject* GetUObjectFromLuaInstance(lua_State* L, int Index)
    {
        if (!lua_istable(L, Index))
        {
            return NULL;
        }

        void* Userdata = NULL;

        lua_pushstring(L, "__NativePtr");
        lua_rawget(L, Index);
        if (lua_islightuserdata(L, -1))
        {
            Userdata = lua_touserdata(L, -1);           // get the raw UObject
        }

        lua_pop(L, 1);

        if (!Userdata)
        {
            return NULL;
        }

        return *((UObject**)Userdata);
    }

    void GetRegistryTable(lua_State* L, const char* TableName)
    {
        lua_getfield(L, LUA_REGISTRYINDEX, TableName);
        if (lua_isnil(L, -1))
        {
            lua_pop(L, 1);
            lua_newtable(L);
            lua_pushvalue(L, -1);
            lua_setfield(L, LUA_REGISTRYINDEX, TableName);
        }
    }

    void PushUObject(lua_State* L, UObject* Object)
    {
        if (!Object)
        {
            lua_pushnil(L);
            return;
        }

        RegisterObjectToLua(L, Object);

        GetRegistryTable(L, "__ObjectMap");
        lua_pushlightuserdata(L, Object);
        lua_rawget(L, -2);
        lua_remove(L, -2);
    }

    void PushObjectModule(lua_State* L, UObject* Object)
    {
        GetRegistryTable(L, "__LoadedModule");
        const char* ModuleName = TCHAR_TO_UTF8(*Object->GetClass()->GetName());
        lua_pushstring(L, ModuleName);
        lua_rawget(L, -2);
        lua_remove(L, -2);
    }

#pragma region lua_load
    // /**
    //  * Global glue function to load a UClass
    //  */
    // int Global_LoadClass(lua_State *L)
    // {
    //     int NumParams = lua_gettop(L);
    //     if (NumParams != 1)
    //     {
    //         UE_LOG(LogUnLua, Log, TEXT("%s: Invalid parameters!"), ANSI_TO_TCHAR(__FUNCTION__));
    //         return 0;
    //     }

    //     const char *ClassName = lua_tostring(L, 1);
    //     if (!ClassName)
    //     {
    //         UE_LOG(LogUnLua, Log, TEXT("%s: Invalid class name!"), ANSI_TO_TCHAR(__FUNCTION__));
    //         return 0;
    //     }

    //     FString ClassPath(ClassName);
    //     // @todo 加载UClass的实现
    //     UClass* ClassPtr = NULL;
    //     if (ClassPtr)
    //     {
    //         LuaBridge::PushUObject(L, ClassPtr);
    //     }
    //     else
    //     {
    //         lua_pushnil(L);
    //     }

    //     return 1;
    // }

    // /**
    //  * Global glue function to create a UObject
    //  */
    // int Global_NewObject(lua_State *L)
    // {
    //     int NumParams = lua_gettop(L);
    //     if (NumParams < 1)
    //     {
    //         UE_LOG(LogTemp, Log, TEXT("%s: Invalid parameters!"), ANSI_TO_TCHAR(__FUNCTION__));
    //         return 0;
    //     }

    //     UClass *Class = Cast<UClass>(LuaBridge::GetUObject(L, 1));
    //     if (!Class)
    //     {
    //         UE_LOG(LogTemp, Log, TEXT("%s: Invalid class!"), ANSI_TO_TCHAR(__FUNCTION__));
    //         return 0;
    //     }

    //     UObject *Outer = NumParams > 1 ? LuaBridge::GetUObject(L, 2) : (UObject*)GetTransientPackage();
    //     if (!Outer)
    //     {
    //         UE_LOG(LogTemp, Log, TEXT("%s: Invalid outer!"), ANSI_TO_TCHAR(__FUNCTION__));
    //         return 0;
    //     }

    //     FName Name = NumParams > 2 ? FName(lua_tostring(L, 3)) : NAME_None;
    //     //EObjectFlags Flags = NumParams > 3 ? EObjectFlags(lua_tointeger(L, 4)) : RF_NoFlags;

    //     {
    //         const char *ModuleName = NumParams > 3 ? lua_tostring(L, 4) : NULL;
    //         int TableRef = INDEX_NONE;
    //         if (NumParams > 4 && lua_type(L, 5) == LUA_TTABLE)
    //         {
    //             lua_pushvalue(L, 5);
    //             TableRef = luaL_ref(L, LUA_REGISTRYINDEX);
    //         }
    //         FScopedLuaDynamicBinding Binding(L, Class, ANSI_TO_TCHAR(ModuleName), TableRef);
    //         UObject *Object = StaticConstructObject_Internal(Class, Outer, Name);
    //         if (Object)
    //         {
    //             LuaBridge::PushUObject(L, Object);
    //         }
    //         else
    //         {
    //             UE_LOG(LogTemp, Log, TEXT("%s: Failed to new object for class %s!"), ANSI_TO_TCHAR(__FUNCTION__), *Class->GetName());
    //             return 0;
    //         }
    //     }

    //     return 1;
    // }
#pragma endregion

    int Global_LuaRef(lua_State* L)
    {
        UObject* Object = GetUObjectFromLuaInstance(L, -1);
        lua_pop(L, 1);
        if (!Object)
        {
            return 0;
        }

        // Object Map 上的引用置空
        GetRegistryTable(L, "__ObjectMap");
        lua_pushlightuserdata(L, Object);
        lua_pushnil(L);
        lua_rawset(L, -3);
        lua_pop(L, 1);

        // 让 GObjectReferencer 持有引用，避免GC
        GObjectReferencer.AddObjectRef(Object);
        return 0;
    }

    int Global_LuaUnRef(lua_State* L)
    {
        UObject* Object = GetUObjectFromLuaInstance(L, -1);
        lua_pop(L, 1);
        if (!Object)
        {
            return 0;
        }

        GObjectReferencer.RemoveObjectRef(Object);
        return 0;
    }

    int UObject_Index(lua_State* L)
    {
        // 获取 Lua 表对象
        luaL_checktype(L, -2, LUA_TUSERDATA);

        UObject* Object = *(UObject**)lua_touserdata(L, -2);

        // 获取键名
        const char* key = lua_tostring(L, -1);

        
        // 将对应UProperty的数值压栈
        FName PropertyName(key);
        for (TFieldIterator<FProperty> Property(Object->GetClass()); Property; ++Property)
        {
            if (Property->GetFName() == PropertyName)
            {
                lua_pop(L, 1);
                PushUPropertyToLua(L, *Property, Object); // 再把值塞进去
                return 1;
            }
        }

        UFunction* UFunc = Object->FindFunction(PropertyName);
        if (UFunc)
        {
            lua_pop(L, 1);
            PushUFunctionToLua(L, UFunc, Object);
            return 1;
        }

        lua_pushnil(L);
        return 1;
    }

    static bool UObject_NewIndex(lua_State* L)
    {
        // 获取 Lua 表对象
        luaL_checktype(L, -3, LUA_TTABLE);

        // 获取键名
        const char* key = lua_tostring(L, -2);

        lua_pushvalue(L, -3); // 把表再放一个在lua栈上面，方便后续查找

        // 尝试从UObject里拿值
        // 拿对应的UObject指针
        lua_pushstring(L, "__NativePtr");
        lua_rawget(L, -2);
        if (!lua_isuserdata(L, -1))
        {
            lua_settop(L, 3);
            return false;
        }

        UObject* Obj = *((UObject**)lua_touserdata(L, -1));
        lua_settop(L, 3);

        // 修改对应UProperty的数值
        FName PropertyName(key);
        for (TFieldIterator<FProperty> Property(Obj->GetClass()); Property; ++Property)
        {
            if (Property->GetFName() == PropertyName)
            {
                PullUPropertyFromLua(L, *Property, Obj);
                return true;
            }
        }

        return false;
    }

    int UObject_StaticClass(lua_State* L)
    {
        UClass* ClassObject = (UClass*)lua_touserdata(L, lua_upvalueindex(1));
        PushUObject(L, ClassObject);
        return 1;
    }

    int UObject_Cast(lua_State* L)
    {
        int NumParams = lua_gettop(L);
        if (NumParams < 2)
        {
            UE_LOG(LogTemp, Warning, TEXT("%s: Invalid parameters!"), ANSI_TO_TCHAR(__FUNCTION__));
            return 0;
        }

        UObject* Object = GetUObjectFromLuaInstance(L, 1);
        if (!Object)
        {
            return 0;
        }

        UClass* Class = Cast<UClass>(GetUObjectFromLuaInstance(L, 2));
        if (Class && (Object->IsA(Class) || (Class->HasAnyClassFlags(CLASS_Interface) && Class != UInterface::StaticClass() && Object->GetClass()->ImplementsInterface(Class))))
        {
            lua_pushvalue(L, 1);
        }
        else
        {
            lua_pushnil(L);
        }
        return 1;
    }
    int UObject_Identical(lua_State* L)
    {
        const int NumParams = lua_gettop(L);
        if (NumParams != 2)
            return luaL_error(L, "invalid parameters");

        if (lua_rawequal(L, 1, 2))
        {
            lua_pushboolean(L, true);
            return 1;
        }

        const UObject* A = GetUObjectFromLuaInstance(L, 1);
        if (!A)
        {
            lua_pushboolean(L, false);
            return 1;
        }

        const UObject* B = GetUObjectFromLuaInstance(L, 2);
        if (!B)
        {
            lua_pushboolean(L, false);
            return 1;
        }

        lua_pushboolean(L, A == B);
        return 1;
    }
    
    int UObject_Delete(lua_State* L)
    {
        UObject* Object = GetUObjectFromLuaInstance(L, 1);
        UnRegisterObjectToLua(L, Object);
        return 0;
    }

    int LuaInstance_Index(lua_State* L)
    {
        // 获取 Lua 表对象
        luaL_checktype(L, -2, LUA_TTABLE);

        // 先从 ClassDesc 里找
        lua_pushstring(L, "__ClassDesc");
        lua_rawget(L, -3);
        if (lua_istable(L, -1))
        {
            lua_pushvalue(L, -2);
            lua_gettable(L, -2);

            if (!lua_isnil(L, -1))
            {
                lua_remove(L, -2);                                      // 移除 ClassDesc
                return 1;
            }
        }

        lua_settop(L, 2);

        // 利用反射从UObject里找
        lua_pushstring(L, "__NativePtr");
        lua_rawget(L, -3);
        if (lua_isuserdata(L, -1))
        {
            lua_pushvalue(L, -2);
            lua_gettable(L, -2);

            if (!lua_isnil(L, -1))
            {
                lua_remove(L, -2);                                      // 移除 NativePtr
                return 1;
            }
        }

        lua_settop(L, 2);
        lua_pushnil(L);

        return 1;
    }

    int LuaInstance_NewIndex(lua_State* L)
    {
        if (UObject_NewIndex(L))
        {
            return 0;
        }

        lua_rawset(L, -3);
        return 0;
    }
}