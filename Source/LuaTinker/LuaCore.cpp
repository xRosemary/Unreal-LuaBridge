#include "CorePrivate.h"
#include "../Inc/LuaBridge/LuaCore.h"
#include "../Inc/LuaBridge/UEObjectReferencer.h"

#if KFX_ENABLE_LUA_SOCKET
#include "../../LuaSocket/Inc/luasocket.h"
#endif

#define ENABLE_LUA_ZONE_CHECK 0
#if ENABLE_LUA_ZONE_CHECK
    #define LUA_ZONE_CHECK LuaBridge::AutoLuaZoneCheck Check(L, __FUNCTION__)
#else
    #define LUA_ZONE_CHECK 
#endif

namespace LuaBridge
{
    int REGISTRY_KEY = LUA_NOREF;
    int REFLECTION_CACHE_REGISTRY_KEY = LUA_NOREF;
    int PROXY_REGISTRY_KEY = LUA_NOREF;
    int OBJECT_MODULE_REGISTRY_KEY = LUA_NOREF;
    int LOADED_MODULE_REGISTRY_KEY = LUA_NOREF;
    int WEAK_LOADED_MODULE_REGISTRY_KEY = LUA_NOREF;
    int WEAK_SCRIPT_CONTAINER_REGISTRY_KEY = LUA_NOREF;

    LuaErrorMessage GLuaErrorMessageDelegate = NULL;

    TCHAR* FastConvertToTCHAR(const char* str)
    {
        guard(FastConvertToTCHAR, E27tQS5TuSkjse0QjJe8GcLvB6o1jWZf);
        static TCHAR buffer[128] = {0};
        for(int i = 0; i < ARRAY_COUNT(buffer); i++)
        {
            buffer[i] = str[i];
            if(str[i] == '\0')
            {
                buffer[i] = TEXT('\0');
                break;
            }
        }
        check(buffer[ARRAY_COUNT(buffer) - 1] == TEXT('\0'));
        return buffer;
        unguard;
    }

    const char* FastConvertToANSI(const TCHAR* str)
    {
        guard(FastConvertToANSI, lm0JNEYEWffsucAK2yLD2Ie3PJ1C1a9X);
        static char buffer[128] = {0};
        for(int i = 0; i < ARRAY_COUNT(buffer); i++)
        {
            buffer[i] = (char)(str[i]);
            if(str[i] == TEXT('\0'))
            {
                buffer[i] = '\0';
                break;
            }
        }
        check(buffer[ARRAY_COUNT(buffer) - 1] == '\0');
        return buffer;
        unguard;
    }

    void PushUPropertyToLua(lua_State *L, FProperty *Property, UObject *Obj)
    {
        guard(PushUPropertyToLua, VUsHjjL1IF4iTBeQTlua03BaG5XejAKI);
        BYTE *Params = (BYTE *)Obj + Property->GetCustomOffset();
        Property->PushByteToLua(L, Params);
        unguard;
    }

    void PullUPropertyFromLua(lua_State *L, FProperty *Property, UObject *Obj)
    {
        guard(PullUPropertyFromLua, F0LlCxsDbdJetEK4ft5kUAnZV7Qh7cXH);
        BYTE *Params = (BYTE *)Obj + Property->GetCustomOffset();
        Property->PullByteFromLua(L, Params, -1);
        unguard;
    }

    // lua栈的参数压入虚幻，并且给出返回值类型
    static FProperty *PrepareParmsForUE(lua_State *L, UFunction *Func, BYTE *Params)
    {
        guard(PrepareParmsForUE, v45eSbqXLEh5ug3dEJvaQC1QIky7iPhC);
        int LuaParamNum = lua_gettop(L);
        int Offset = 0;
        int i = 1;

        if (Func == NULL || Params == NULL)
        {
            return NULL;
        }

        // 不需要lua传过来的self
        if (lua_istable(L, 1) || lua_isuserdata(L, 1))
        {
            i++;
        }

        FProperty* RetType = NULL;
        for( UProperty* Property=(UProperty*)Func->Children; Property; Property=(UProperty*)Property->Next )
        {
            Offset = Property->GetCustomOffset();

            if (Offset == Func->ReturnValueOffset)
            {
                RetType = Property;
            }

            if (i <= LuaParamNum)
            {
                Property->PullByteFromLua(L, Params + Offset, i);
            }
            else
            {
                break;
            }

            i++;
        }

        return RetType;
        unguard;
    }

    static int LuaCallUFunction(lua_State *L)
    {
        guard(LuaCallUFunction, WU9UQExjQIrSavXi3vLThsfuEyjfVzP8);
        LUA_ZONE_CHECK;
        UObject *Obj = GetUObjectFromLuaProxy(L, 1);
        if(Obj == NULL)
        {
            return 0;
        }

        UFunction *Function = (UFunction *)lua_touserdata(L, lua_upvalueindex(1));
        check(Function);

        BYTE *Parms = (BYTE*)appAlloca(Function->PropertiesSize);
        appMemzero(Parms, Function->PropertiesSize);
        FProperty *RetType = PrepareParmsForUE(L, Function, Parms);

        // 调用函数
        if( Function->iNative || Function->FunctionFlags & FUNC_Native)
        {
            FFrame NewStack(Obj, Function, 0, Parms );
            (Obj->*Function->Func)( NewStack, NewStack.Locals+Function->ReturnValueOffset );
        }
        else
        {
            Obj->ProcessEvent(Function, Parms);
        }

        // 压回返回值
        if(RetType) RetType->PushByteToLua(L, Parms + Function->ReturnValueOffset);

        // 析构参数和local变量
        for( UProperty* Property=(UProperty*)Function->Children; Property; Property=(UProperty*)Property->Next )
		{
            Property->DestroyValue(Parms + Property->GetCustomOffset());
        }

        return RetType == NULL ? 0 : 1;
        unguard;
    }

    // 虚幻的参数压入lua栈，并且给出返回值类型
    FProperty* PrepareParmsForLua(lua_State* L, UFunction* Func, BYTE* Params)
    {
        guard(PrepareParmsForLua, ICskEYAx0M0X0iLi51xKYAIN8zbU5AnZ);
        LUA_ZONE_CHECK;
        if (Func == NULL || Params == NULL)
        {
            return NULL;
        }

        for ( FProperty* Property = Func->PropertyLink; Property && (Property->PropertyFlags & CPF_Parm); Property = Property->PropertyLinkNext )
        {
            if(Property->PropertyFlags & CPF_ReturnParm)
            {
                return Property;
            }

            Property->PushByteToLua(L, Params + Property->GetCustomOffset());
        }

        return NULL;
        unguard;
    }

    static int InternalPCallErrorHandler(lua_State* L)
    {
        guard(InternalPCallErrorHandler, EWepG8zgQHEnr29WKPw4A0TeFgap6Zfo);
        int Type = lua_type(L, -1);
        if (Type == LUA_TSTRING)
        {
            luaL_traceback(L, L, lua_tostring(L, -1), 1);
            FString ErrorString = FString(ANSI_TO_TCHAR(lua_tostring(L, -1)));
            if (GLuaErrorMessageDelegate)
			{
                GLuaErrorMessageDelegate(ErrorString);
            }
        }
        else if (Type == LUA_TTABLE)
        {
            // multiple errors is possible
            lua_pushnil(L);

            FString ErrorString;
            while (lua_next(L, -2) != 0)
            {
                ErrorString += ANSI_TO_TCHAR(lua_tostring(L, -1));
                lua_pop(L, 1);
            }

            if (GLuaErrorMessageDelegate)
            {
                GLuaErrorMessageDelegate(ErrorString);
            }
        }

        lua_pop(L, 1);
        return 0;
        unguard;
    }

    void PushPCallErrorHandler(lua_State* L)
    {
        guard(PushPCallErrorHandler, yHzjVdLSnf0rz3KaXloUzUGV3netZDER);
        lua_pushcfunction(L, InternalPCallErrorHandler);
        unguard;
    }

    void InternalCallLua(lua_State* L, UFunction* Function, BYTE* Params, BYTE* Result)
    {
        guard(InternalCallLua, F4ZUSlP5rRTmRGDf9oMGFyrXlEjWuQFZ);
        LUA_ZONE_CHECK;
        // -1 = [table/userdata] UObject for self
        // -2 = [function] to call
        // -3 = [function] ReportLuaCallError
        const int ErrorHandlerIndex = lua_gettop(L) - 2;
        FProperty* RetType = PrepareParmsForLua(L, Function, Params);
        const bool bHasReturnParam = RetType != NULL;
		const int NumParams = Function->NumParms - bHasReturnParam + 1;
		if (lua_pcall(L, NumParams, bHasReturnParam, -(NumParams + 2)) != LUA_OK)
		{
            lua_settop(L, ErrorHandlerIndex - 1);
            return;
        }

        if (bHasReturnParam)
        {
            RetType->PullByteFromLua(L, Result, -1);
        }

        lua_settop(L, ErrorHandlerIndex - 1);
        unguard;
    }

    void PushUFunctionToLua(lua_State *L, UFunction *Func)
    {
        guard(PushUFunctionToLua, rg0UEPhFZdGtE3MoGcrlnTrb8fFfUd9c);
        LUA_ZONE_CHECK;
        lua_pushlightuserdata(L, Func);
        lua_pushcclosure(L, LuaCallUFunction, 1);
        unguard;
    }

    void SetTableForClass(lua_State *L, const char *Name)
    {
        guard(SetTableForClass, V4zggwT5ezmfPtN4gzXW4WZ5VhkIyC95);
        lua_getglobal(L, "UE");
        lua_pushstring(L, Name);
        lua_pushvalue(L, -3);
        lua_rawset(L, -3);
        lua_pop(L, 2);
        unguard;
    }

    void PushObjectInstance(lua_State *L, UObject *Object)
    {
        guard(PushObjectInstance, JsH5uvtd4QdaWoeHF2gyOM5k8cfno86T);
        LUA_ZONE_CHECK;
        check(Object);

        // 先去缓存里找
        lua_rawgeti(L, LUA_REGISTRYINDEX, REGISTRY_KEY);
        lua_rawgetp(L, -1, Object);

        if(lua_istable(L, -1))
        {
            GObjectReferencer->AddObjectRef(Object, 0);
            lua_remove(L, -2);
            return;
        }

        // 找不到就创建
        lua_pop(L, 1);

        lua_newtable(L); // 创建实例 Instance

        lua_pushstring(L, "__NativePtr");
        lua_pushlightuserdata(L, Object);
        lua_rawset(L, -3); // Instance.__NativePtr = Object
 
        lua_pushstring(L, "super");
        lua_newtable(L);
        lua_rawset(L, -3); // Instance.super = {}

        lua_pushstring(L, "StaticClass");
        lua_pushlightuserdata(L, Object->GetClass());
        lua_pushcclosure(L, UObject_PushUpValueObject, 1);
        lua_rawset(L, -3);

        if(Object->IsA(UClass::StaticClass()))
        {
            lua_pushstring(L, "GetDefaultObject");
            lua_pushlightuserdata(L, ((UClass*)Object)->GetDefaultObject());
            lua_pushcclosure(L, UObject_PushUpValueObject, 1);
            lua_rawset(L, -3);
        }

        lua_pushstring(L, "__index");
        lua_pushcfunction(L, LuaInstance_Index);
        lua_rawset(L, -3);

        lua_pushstring(L, "__newindex");
        lua_pushcfunction(L, LuaInstance_NewIndex);
        lua_rawset(L, -3);

        lua_pushvalue(L, -1);
        lua_setmetatable(L, -2); // 将 meta table 设为自己

        lua_pushlightuserdata(L, Object);
        lua_pushvalue(L, -2);

        // 记一下两边的强引用，避免被GC
        lua_rawset(L, -4);           // ObjectMap.ObjectPtr = Instance
        GObjectReferencer->AddObjectRef(Object, 0);
        lua_remove(L, -2);
        // 此时栈顶为 Instance
        unguard;
    }

    void PushInstanceProxy(lua_State *L, UObject *Object)
    {
        guard(PushInstanceProxy, UYkiL3NPaO1zd5zloOw4DjELa7VYU0Kr);
        LUA_ZONE_CHECK;
        PushObjectInstance(L, Object);

        lua_newtable(L); // Instance 的代理对象

        lua_pushstring(L, "__index");
        lua_pushvalue(L, -3);
        lua_rawset(L, -3); // ProxyMeta.__index = Lua Instance

        lua_pushstring(L, "__newindex");
        lua_pushvalue(L, -3);
        lua_rawset(L, -3); // ProxyMeta.__newindex = Lua Instance

        lua_pushstring(L, "__eq");
        lua_pushcfunction(L, LuaProxy_Identical);
        lua_rawset(L, -3);

        lua_pushstring(L, "__gc");
        lua_pushcfunction(L, LuaProxy_Delete);
        lua_rawset(L, -3);

        lua_pushvalue(L, -1);    // 复制一份 Proxy
        lua_setmetatable(L, -2); // setmetatable(Proxy, Proxy);

        lua_remove(L, -2); // 移除 Lua Instance
        unguard;
    }

    void UnRegisterObjectToLua(lua_State *L, UObject *Object)
    {
        guard(UnRegisterObjectToLua, JF7F3ASHtWcBnjq4GvLyBYCnVlk3ELg9);
        LUA_ZONE_CHECK;
        if (!Object)
        {
            return;
        }

        // 类似于Actor:Destroy()，这种非Lua GC的Object需要删一下GObjectReferencer的引用
        int RefKey = LUA_NOREF;
        GObjectReferencer->RemoveObjectRef(Object, &RefKey);
        
        // 如果时UClass被卸载了，就删除相应的反射信息
        if(Object->IsA(UClass::StaticClass()))
        {
            lua_rawgeti(L, LUA_REGISTRYINDEX, REFLECTION_CACHE_REGISTRY_KEY);
            lua_pushnil(L);
            lua_rawsetp(L, -2, Object);
            lua_pop(L, 1);
        }

        // 删除Lua Instance
        lua_rawgeti(L, LUA_REGISTRYINDEX, REGISTRY_KEY);
        lua_rawgetp(L, -1, Object); // ObjectMap.ObjectPtr
        if(lua_isnil(L, -1))
        {
            lua_pop(L, 2);
            return;
        }

        lua_pushstring(L, "__NativePtr");
        lua_pushnil(L);
        lua_rawset(L, -3); // Instance.__NativePtr = nil
        lua_pop(L, 1); // 弹出 Instance

        lua_pushnil(L);
        lua_rawsetp(L, -2, Object); // ObjectMap.ObjectPtr = nil
        lua_pop(L, 1);

        lua_rawgeti(L, LUA_REGISTRYINDEX, OBJECT_MODULE_REGISTRY_KEY);
        lua_pushnil(L);
        lua_rawsetp(L, -2, Object);                  // ObjectModules.Object = nil
        lua_pop(L, 1);

        unguard;
    }

    bool HasObjectLuaModule(lua_State *L, UObject *Object)
    {
        guard(HasObjectLuaModule, FEL007bet9QJuZk03s1zdbijj2P2Zb5B);
        LUA_ZONE_CHECK;
        if (!Object)
        {
            return false;
        }

        lua_rawgeti(L, LUA_REGISTRYINDEX, OBJECT_MODULE_REGISTRY_KEY);
        lua_rawgetp(L, -1, Object);
        bool bRet = lua_istable(L, -1);
        lua_pop(L, 2);
        return bRet;
        unguard;
    }

    UObject *GetUObjectFromLuaProxy(lua_State *L, int Index)
    {
        guard(GetUObjectFromLuaProxy, aPwVQrCptQ1z7hOnIuRNY6lvG3W8IyZz);
        LUA_ZONE_CHECK;
        if (!lua_istable(L, Index))
        {
            return NULL;
        }

        void *Userdata = NULL;

        lua_getfield(L, Index, "__NativePtr");
        if (lua_islightuserdata(L, -1))
        {
            Userdata = lua_touserdata(L, -1); // get the raw UObject
        }

        lua_pop(L, 1);
        return (UObject *)Userdata;
        unguard;
    }

    static void CreateWeakValueTable(lua_State *L)
    {
        guard(CreateWeakValueTable, hff5SkhkUi1Duwws3aCktkMcTFu4oToJ);
        lua_newtable(L);
        lua_newtable(L);
        lua_pushstring(L, "__mode");
        lua_pushstring(L, "v");
        lua_rawset(L, -3);
        lua_setmetatable(L, -2);
        unguard;
    }

    void InitRegistryTable(lua_State *L)
    {
        guard(InitRegistryTable, NCIZFNiHSb8GZKfaKuk8eUJTPkr9oiR3);
        AutoLuaStack as(L);

        lua_newtable(L);
        REGISTRY_KEY = luaL_ref(L, LUA_REGISTRYINDEX);

        lua_newtable(L);
        REFLECTION_CACHE_REGISTRY_KEY = luaL_ref(L, LUA_REGISTRYINDEX);

        CreateWeakValueTable(L);
        PROXY_REGISTRY_KEY = luaL_ref(L, LUA_REGISTRYINDEX);

        lua_newtable(L);
        OBJECT_MODULE_REGISTRY_KEY = luaL_ref(L, LUA_REGISTRYINDEX);

        lua_newtable(L);
        LOADED_MODULE_REGISTRY_KEY = luaL_ref(L, LUA_REGISTRYINDEX);

        CreateWeakValueTable(L);
        WEAK_LOADED_MODULE_REGISTRY_KEY = luaL_ref(L, LUA_REGISTRYINDEX);

        CreateWeakValueTable(L);
        WEAK_SCRIPT_CONTAINER_REGISTRY_KEY = luaL_ref(L, LUA_REGISTRYINDEX);
        unguard;
    }

    void PushUObject(lua_State *L, UObject *Object)
    {
        guard(PushUObject, gK1G5gclC71Ag9ostg9xaG0jROI9IULM);
        LUA_ZONE_CHECK;
        if (!Object)
        {
            lua_pushnil(L);
            return;
        }

        lua_rawgeti(L, LUA_REGISTRYINDEX, PROXY_REGISTRY_KEY);
        lua_rawgetp(L, -1, Object);

        if (lua_isnil(L, -1))
        {
            lua_pop(L, 1);
            PushInstanceProxy(L, Object); // 此时栈顶为 Lua Instance Proxy
            lua_pushvalue(L, -1); // 拷贝一份 Lua Instance Proxy
            lua_rawsetp(L, -3, Object);   // ObjectProxyMap.ObjectPtr = Instance Proxy
        }

        lua_remove(L, -2); // 移除 ObjectMap
        unguard;
    }

    void PushObjectModule(lua_State *L, UObject *Object)
    {
        guard(PushObjectModule, cP8HnwaDBe7IvR78TNcJ2K2UoGhRAKgf);
        LUA_ZONE_CHECK;
        lua_rawgeti(L, LUA_REGISTRYINDEX, OBJECT_MODULE_REGISTRY_KEY);
        lua_rawgetp(L, -1, Object);
        lua_remove(L, -2);
        unguard;
    }

    // #pragma region lua_load
    /**
     * Global glue function to load a UClass
     */
    int Global_LoadClass(lua_State *L)
    {
        guard(Global_LoadClass, QbWamc0K6EA47UAsHSLy25vX7pq90kVH);
        LUA_ZONE_CHECK;
        int NumParams = lua_gettop(L);
        if (NumParams != 1)
        {
            debugf(NAME_Error, TEXT("%s: Invalid parameters!"), ANSI_TO_TCHAR(__FUNCTION__));
            return 0;
        }

        const char *ClassName = lua_tostring(L, 1);
        if (!ClassName)
        {
            debugf(NAME_Error, TEXT("%s: Invalid class name!"), ANSI_TO_TCHAR(__FUNCTION__));
            return 0;
        }

        FString ClassPath(ClassName);
        UClass *ClassPtr = LoadObject<UClass>(NULL, *ClassPath, NULL, LOAD_None, NULL);
        if (!ClassPtr)
        {
            return 0;
        }

        PushUObject(L, ClassPtr);
        return 1;
        unguard;
    }

    int Global_NewObject(lua_State *L)
    {
        guard(Global_NewObject, oWuP49Oc4zBIRGhdXWh866JVPd5qAlDI);
        LUA_ZONE_CHECK;
        int NumParams = lua_gettop(L);
        if (NumParams < 1)
        {
            debugf(NAME_Error, TEXT("%s: Invalid parameters!"), ANSI_TO_TCHAR(__FUNCTION__));
            return 0;
        }

        UClass *Class = Cast<UClass>(GetUObjectFromLuaProxy(L, 1));
        if (!Class)
        {
            debugf(NAME_Error, TEXT("%s: Invalid class!"), ANSI_TO_TCHAR(__FUNCTION__));
            return 0;
        }

        UObject *Outer = GetUObjectFromLuaProxy(L, 2);
        if (!Outer)
            Outer = UObject::GetTransientPackage();

        FName Name = NumParams > 2 ? FName(ANSI_TO_TCHAR(lua_tostring(L, 3))) : NAME_None;
        {
            UObject *Object = UObject::StaticConstructObject(Class, Outer, Name);

            if (Object)
            {
                PushUObject(L, Object);
            }
            else
            {
                debugf(NAME_Error, TEXT("%s: Failed to new object for class %s!"), ANSI_TO_TCHAR(__FUNCTION__), *Class->GetName());
                return 0;
            }
        }

        return 1;
        unguard;
    }

    int Global_IsObjectValid(lua_State *L)
    {
        guard(Global_IsObjectValid, rS9a6chgroagelOxaHHR5WjijIMs7wPX);
        LUA_ZONE_CHECK;
        int NumParams = lua_gettop(L);
        if (NumParams < 1)
        {
            lua_pushboolean(L, false);
            return 1;
        }

        UObject *Outer = GetUObjectFromLuaProxy(L, 1);
        lua_pushboolean(L, CheckObjIsValid(Outer));
        return 1;
        unguard;
    }
    // #pragma endregion

    bool CheckObjIsValid(UObject *pObj)
    {
        guard(CheckObjIsValid, zvbJYDj55HuE9lYXTsBq18rJiS6bsCgH);
        if (!pObj)
        {
            return false;
        }
        else if (!GObjObjects.IsValidIndex(pObj->GetIndex()))
        {
            return false;
        }
        else if (GObjObjects(pObj->GetIndex()) == NULL)
        {
            return false;
        }
        else if (GObjObjects(pObj->GetIndex()) != pObj)
        {
            return false;
        }
        else
            return true;
        unguard;
    }

    int Global_LuaUnRef(lua_State *L)
    {
        guard(Global_LuaUnRef, VDxnodwrUyV3Kpb27D00ZWlXJtuGED6S);
        UObject *Object = GetUObjectFromLuaProxy(L, -1);
        lua_pop(L, 1);
        if (!Object)
        {
            return 0;
        }

        UnRegisterObjectToLua(L, Object);
        return 0;
        unguard;
    }

    // 压入缓存的反射信息
    static void PushUClassRefectionCacheToLua(lua_State *L, UObject* Object)
    {
        guard(PushUClassRefectionCacheToLua, xaOX7W2J7n1aW7M0OrnZj0EwWMLd8MPJ);
        LUA_ZONE_CHECK;
        lua_rawgeti(L, LUA_REGISTRYINDEX, REFLECTION_CACHE_REGISTRY_KEY);
        lua_rawgetp(L, -1, Object->GetClass());
        if(lua_isnil(L, -1))
        {
            lua_pop(L, 1); // 弹出nil
            lua_newtable(L); // 创建一个新的缓存表
            for (TFieldIterator<UField> It(Object->GetClass()); It; ++It)
            {
                UProperty* PropPtr = FlagCast<UProperty,CLASS_IsAUProperty>(*It);
                if(PropPtr)
                {
                    UDelegateProperty* DelegateProperty = Cast<UDelegateProperty>(PropPtr);
                    if(DelegateProperty && DelegateProperty->Function)
                    {
                        FString DelegateName = FString::Printf(TEXT("%s_Ref"), DelegateProperty->Function->GetName());
                        lua_pushstring(L, FastConvertToANSI(*DelegateName));
                    }
                    else
                    {
                        lua_pushstring(L, FastConvertToANSI(PropPtr->GetName()));
                    }
                    
                    lua_pushlightuserdata(L, PropPtr);
                    lua_rawset(L, -3);
                    continue;
                }

                UFunction* FuncPtr = FlagCast<UFunction,CLASS_IsAUFunction>(*It);
                if(FuncPtr)
                {
                    lua_pushstring(L, FastConvertToANSI(FuncPtr->GetName()));
                    PushUFunctionToLua(L, FuncPtr);
                    lua_rawset(L, -3);
                    continue;
                }
            }
        
            lua_pushvalue(L, -1);
            lua_rawsetp(L, -3, Object->GetClass()); // 缓存到全局表中
        }

        lua_remove(L, -2); // 移除全局缓存表
        unguard;
    }

    int UObject_Index(lua_State *L, UObject* Object)
    {
        guard(UObject_Index, miGSWJmzkC2EBywIqJELkQFmwPNekAh3);
        LUA_ZONE_CHECK;
        // 将对应UProperty的数值压栈
        // 找的时候顺便做一下缓存
        PushUClassRefectionCacheToLua(L, Object);
        if (lua_istable(L, -1))
        {
            lua_pushvalue(L, -2);   // push key
            lua_rawget(L, -2);
            lua_remove(L, -2);      // 移除Cache表
            if (lua_islightuserdata(L, -1))
            {
                FProperty* Property = (FProperty *)lua_touserdata(L, -1);
                if(Property)
                {
                    lua_pop(L, 1);  // 弹出Property的指针
                    PushUPropertyToLua(L, Property, Object);
                    return 1;
                }
            }
            else
            {
                return 1;
            }
        }

        lua_pop(L, 1);
        lua_pushnil(L);
        return 1;
        unguard;
    }

    int UObject_PushUpValueObject(lua_State *L)
    {
        guard(UObject_PushUpValueObject, CB90o8axeWB4Sa5TpwtjAWEVWo8o6L8v);
        LUA_ZONE_CHECK;
        UObject *ClassObject = (UObject *)lua_touserdata(L, lua_upvalueindex(1));
        PushUObject(L, ClassObject);
        return 1;
        unguard;
    }

    int LuaProxy_Identical(lua_State *L)
    {
        guard(LuaProxy_Identical, HRLXU7UnnuwOjbePrgpIRuXOGVxZrw6d);
        LUA_ZONE_CHECK;
        const int NumParams = lua_gettop(L);
        if (NumParams != 2)
            return luaL_error(L, "invalid parameters");

        if (lua_rawequal(L, 1, 2))
        {
            lua_pushboolean(L, true);
            return 1;
        }

        const UObject *A = GetUObjectFromLuaProxy(L, 1);
        if (!A)
        {
            lua_pushboolean(L, false);
            return 1;
        }

        const UObject *B = GetUObjectFromLuaProxy(L, 2);
        if (!B)
        {
            lua_pushboolean(L, false);
            return 1;
        }

        lua_pushboolean(L, A == B);
        return 1;
        unguard;
    }

    int LuaProxy_Delete(lua_State *L)
    {
        guard(LuaProxy_Delete, TwKZvxsbrtXGfWJ6Qw60U8tDpRJuaaBi);
        LUA_ZONE_CHECK;
        UObject *Object = GetUObjectFromLuaProxy(L, 1);
        // 删一下C++的引用使其能被GC
        int RefKey = LUA_NOREF;
        GObjectReferencer->RemoveObjectRef(Object, &RefKey);
        return 0;
        unguard;
    }

    int LuaInstance_Index(lua_State *L)
    {
        guard(LuaInstance_Index, sq0zxXIMzIdxFDE6UjJkbCnSegI9vFwN);
        LUA_ZONE_CHECK;
        // 获取 Lua 表对象
        luaL_checktype(L, -2, LUA_TTABLE);

        // 先从 ObjectModule 里找
        lua_pushstring(L, "__NativePtr");
        lua_rawget(L, -3);
        if(lua_isnil(L, -1))
        {
            return 1;
        }

        UObject *Object = (UObject *)lua_touserdata(L, -1);
        check(Object);
        lua_pop(L, 1);
       
        PushObjectModule(L, Object);
        if (lua_istable(L, -1))
        {
            lua_pushvalue(L, -2);
            lua_gettable(L, -2);
            lua_remove(L, -2); // 移除 ObjectModule
            if (!lua_isnil(L, -1))
            {
                return 1;
            }
        }

        lua_pop(L, 1); // 弹出nil
        // 找不到了就通过反射找
        return UObject_Index(L, Object);
        unguard;
    }

    int LuaInstance_NewIndex(lua_State* L)
    {
        guard(LuaInstance_NewIndex, HKUFrizJZgyMljf5TQaHz0mqDISXd0SE);
        LUA_ZONE_CHECK;
        // 获取 Lua 表对象
        luaL_checktype(L, -3, LUA_TTABLE);

        // 尝试从UObject里拿值
        lua_pushstring(L, "__NativePtr");
        lua_rawget(L, -4);
        if(lua_isnil(L, -1))
        {
            lua_pop(L, 1);
            return 0;
        }

        UObject *Obj = (UObject *)lua_touserdata(L, -1);
        lua_pop(L, 1);

        // 修改对应UProperty的数值
        FProperty* Property = NULL;

        // 找的时候顺便做一下缓存
        PushUClassRefectionCacheToLua(L, Obj);
        if (lua_istable(L, -1))
        {
            lua_pushvalue(L, -3);   // push key
            lua_rawget(L, -2);
            if (lua_islightuserdata(L, -1))
            {
                Property = (FProperty *)lua_touserdata(L, -1);
            }

            lua_pop(L, 1);  // 弹出nil或者Property
        }
        
        lua_pop(L, 1); // 弹出缓存表

        if(Property)
        {
            PullUPropertyFromLua(L, Property, Obj);
            if ( Property->PropertyFlags & CPF_Net )
                Obj->NetDirty(Property);
            
            return 0;
        }

        // 没有找到对应的属性，就当作lua原生表的newIndex操作
        lua_pushvalue(L, 2);
        lua_pushvalue(L, 3);
        lua_rawset(L, 1);
        return 0;
        unguard;
    }
}

namespace
{
    struct FLuaArray
    {
        FArray* ScriptArray;
        UProperty* Inner;
        int IsLuaNative;
    };

    struct FLuaStruct
    {
        void* ScriptStruct;
        int IsLuaNative;
    };

    struct FLuaColor
    {
    #if _MSC_VER
	    union { struct{ BYTE B,G,R,A; }; DWORD AlignmentDummy; };
    #else
        BYTE B GCC_ALIGN(4);
        BYTE G,R,A;
    #endif

        FLuaColor() {}
        FLuaColor(BYTE InR,BYTE InG,BYTE InB,BYTE InA)
        {
            R = InR;
            G = InG;
            B = InB;
            A = InA;
        }

        FLuaColor(BYTE InR,BYTE InG,BYTE InB)
        {
            R = InR;
            G = InG;
            B = InB;
            A = 255;
        }
    };

    struct FLuaScriptDelegate
    {
        FScriptDelegate* NativeDelegate;
        UFunction* Inner;
    };

    // 将结构体入栈，返回值不为NULL时表示需要进行手动构造
    template<typename T>
    static T* NewScriptContainerWithCache(lua_State *L, void *Key, const char* MetatableName)
    {
        guard(NewScriptContainerWithCache, rSDtZqAO6nplnxT1XJlHtnwBUMwPh2Ei);
        if(!Key)
        {
            debugf(NAME_Error, TEXT("[Error] invalid container key"));
            lua_pushnil(L);
            return NULL;
        }

        T *Value = NULL;
        lua_rawgeti(L, LUA_REGISTRYINDEX, LuaBridge::WEAK_SCRIPT_CONTAINER_REGISTRY_KEY);
        lua_rawgetp(L, -1, Key);
        if(lua_isnil(L, -1))
        {
            lua_pop(L, 1);

            Value = (T*)lua_newuserdata(L, sizeof(T));
            appMemzero(Value, sizeof(T));
            luaL_setmetatable(L, MetatableName);
            lua_pushvalue(L, -1);
            lua_rawsetp(L, -3, Key);
        }

        lua_remove(L, -2);
        return Value;
        unguard;
    }

    static int TArray_Get(lua_State *L)
    {
        guard(TArray_Get, bbWfRmeQ6FZ6EqHNDOjJAC87wbJgiHz1);
        if(!lua_isinteger(L, 2))
        {
            lua_getmetatable(L, 1);
            lua_pushvalue(L, 2);
            lua_rawget(L, -2);
            lua_remove(L, -2);
            return 1;
        }

        FLuaArray* Array = (FLuaArray*)lua_touserdata(L, -2);
        if(Array == NULL || Array->ScriptArray == NULL)
        {
            return luaL_error(L, "invalid TArray");
        }

        if(!LuaBridge::CheckObjIsValid(Array->Inner))
        {
            return luaL_error(L, "invalid TArray element type");
        }

        // 获取下标
        int ArrayIndex = lua_tointeger(L, -1) - 1;
        if(!Array->ScriptArray->IsValidIndex(ArrayIndex))
        {
            return luaL_error(L, "TArray Invalid Index!: %d", ArrayIndex);
        }

        Array->Inner->PushByteToLua(L, ((BYTE *)Array->ScriptArray->GetData()) + ArrayIndex * Array->Inner->ElementSize);
        return 1;
        unguard;
    }

    static int TArray_Set(lua_State *L)
    {
        guard(TArray_Set, f3TYi44vazW9s6SzxkYLnWF1UkBfsyZI);
        FLuaArray* Array = (FLuaArray*)lua_touserdata(L, -3);
        if(Array == NULL || Array->ScriptArray == NULL)
        {
            debugf(NAME_Error, TEXT("[Error] invalid TArray"));
            return 0;
        }

        if(!LuaBridge::CheckObjIsValid(Array->Inner))
        {
            debugf(NAME_Error, TEXT("[Error] invalid TArray element type"));
            return 0;
        }

        // 获取下标
        int ArrayIndex = lua_tointeger(L, -2) - 1;
        if(!Array->ScriptArray->IsValidIndex(ArrayIndex))
        {
            debugf(NAME_Error, TEXT("TArray Invalid Index!: %d"), ArrayIndex);
            return 0;
        }

        Array->Inner->PullByteFromLua(L, ((BYTE *)Array->ScriptArray->GetData()) + ArrayIndex * Array->Inner->ElementSize, -1);
        return 0;
        unguard;
    }

    static int TArray_Length(lua_State* L)
    {
        guard(TArray_Length, evfi5JUIjfUKIBq40UxSS4fuv75FIV7w);
        int NumParams = lua_gettop(L);
        if (NumParams != 1)
            return luaL_error(L, "invalid parameters");

        FLuaArray* Array = (FLuaArray*)lua_touserdata(L, -1);
        if(Array == NULL || Array->ScriptArray == NULL)
        {
            return luaL_error(L, "invalid TArray");
        }

        lua_pushinteger(L, Array->ScriptArray->Num());
        return 1;
        unguard;
    }

    static int TArray_Insert(lua_State* L)
    {
        guard(TArray_Insert, xckEz2eq0IgA9byV1ETGJBDqhybsoYnw);
        int NumParams = lua_gettop(L);
        if (NumParams != 3)
            return luaL_error(L, "invalid parameters");

        FLuaArray* Array = (FLuaArray*)lua_touserdata(L, 1);
        if(Array == NULL || Array->ScriptArray == NULL)
        {
            debugf(NAME_Error, TEXT("[Error] invalid TArray"));
            return 0;
        }

        if(!LuaBridge::CheckObjIsValid(Array->Inner))
        {
            debugf(NAME_Error, TEXT("[Error] invalid TArray element type"));
            return 0;
        }

        int Index = lua_tointeger(L, 2) - 1;
        int Count = lua_tointeger(L, 3);

        if ( Count < 0 )
        {
            debugf( NAME_Error, TEXT("Attempt to insert a negative number of elements '%s'"), Array->Inner->GetName() );
            return 0;
        }
        if ( Index < 0 || Index > Array->ScriptArray->Num() )
        {
            debugf( NAME_Error, TEXT("Attempt to insert %i elements at %i an %i-element array '%s'"), Count, Index, Array->ScriptArray->Num(), Array->Inner->GetName() );
            Index = Clamp(Index, 0,Array->ScriptArray->Num());
        }

        Array->ScriptArray->InsertZeroed( Index, Count, Array->Inner->ElementSize);
        return 0;
        unguard;
    }

	static int TArray_Push(lua_State* L)
	{
        guard(TArray_Push, EyhmaowNPNJR5XRX9rlCAVG0eMdrZMfO);
		int NumParams = lua_gettop(L);
		if (NumParams != 2)
			return luaL_error(L, "invalid parameters");

		FLuaArray* Array = (FLuaArray*)lua_touserdata(L, 1);
		if (Array == NULL || Array->ScriptArray == NULL)
		{
			debugf(NAME_Error, TEXT("[Error] invalid TArray"));
			return 0;
		}

		if (!LuaBridge::CheckObjIsValid(Array->Inner))
		{
			debugf(NAME_Error, TEXT("[Error] invalid TArray element type"));
			return 0;
		}

        int CurrentSize = Array->ScriptArray->Num();
		Array->ScriptArray->InsertZeroed(CurrentSize, 1, Array->Inner->ElementSize);
        Array->Inner->PullByteFromLua(L, ((BYTE *)Array->ScriptArray->GetData()) + CurrentSize * Array->Inner->ElementSize, -1);
		return 0;
        unguard;
	}

    static int TArray_Remove(lua_State* L)
    {
        guard(TArray_Remove, yuptUk3z8lcNbyk94md9lYURYhaPM8IA);
        int NumParams = lua_gettop(L);
        if (NumParams != 3)
            return luaL_error(L, "invalid parameters");

        FLuaArray* Array = (FLuaArray*)lua_touserdata(L, 1);
        if(Array == NULL || Array->ScriptArray == NULL)
        {
            debugf(NAME_Error, TEXT("[Error] invalid TArray"));
            return 0;
        }

        if(!LuaBridge::CheckObjIsValid(Array->Inner))
        {
            debugf( NAME_Error, TEXT("[Error] invalid TArray element type"));
            return 0;
        }

        int Index = lua_tointeger(L, 2) - 1;
        int Count = lua_tointeger(L, 3);

        if ( Count < 0 )
		{
			debugf( NAME_Error, TEXT("Attempt to remove a negative number of elements '%s'"), Array->Inner->GetName() );
			return 0;
		}
		if ( Index < 0 || Index >= Array->ScriptArray->Num() || Index + Count > Array->ScriptArray->Num() )
		{
			if (Count == 1)
				debugf( NAME_Error, TEXT("Attempt to remove element %i in an %i-element array '%s'"), Index, Array->ScriptArray->Num(), Array->Inner->GetName() );
			else
				debugf( NAME_Error, TEXT("Attempt to remove elements %i through %i in an %i-element array '%s'"), Index, Index+Count-1, Array->ScriptArray->Num(), Array->Inner->GetName() );
			Index = Clamp(Index, 0,Array->ScriptArray->Num());
			if ( Index + Count > Array->ScriptArray->Num() )
				Count = Array->ScriptArray->Num() - Index;
		}

		for (INT i=Index+Count-1; i>=Index; i--)
			Array->Inner->DestroyValue((BYTE*)Array->ScriptArray->GetData() + Array->Inner->ElementSize*i);
		Array->ScriptArray->Remove( Index, Count, Array->Inner->ElementSize);
        return 0;
        unguard;
    }

    static int TArray_Release(lua_State *L)
    {
        guard(TArray_Release, yySTT69lC6bfxZDCTvtOofAALr4fCDLv);
        FLuaArray* Array = (FLuaArray*)lua_touserdata(L, -1);
        if(Array && Array->ScriptArray && Array->IsLuaNative && LuaBridge::CheckObjIsValid(Array->Inner))
        {
            if( Array->Inner->PropertyFlags & CPF_NeedCtorLink )
            {
                BYTE* DestData = (BYTE*)Array->ScriptArray->GetData();
                INT   Size     = Array->Inner->ElementSize;
                for( INT i=0; i<Array->ScriptArray->Num(); i++ )
                    Array->Inner->DestroyValue( DestData+i*Size );
            }
            delete Array->ScriptArray;
            Array->ScriptArray = NULL;
        }

        return 0;
        unguard;
    }

    static int TArray_Find(lua_State* L)
    {
        guard(TArray_Find, OZdBBDRLPcLMEvSwf5TqEvZgRNGp5TNA);
        int NumParams = lua_gettop(L);
        if (NumParams != 2)
            return luaL_error(L, "invalid parameters");

        FLuaArray* Array = (FLuaArray*)lua_touserdata(L, 1);
        if(Array == NULL || Array->ScriptArray == NULL)
        {
            return luaL_error(L, "invalid TArray");
        }

        if(!LuaBridge::CheckObjIsValid(Array->Inner))
        {
            return luaL_error(L, "invalid TArray element type");
        }

        int Index = INDEX_NONE;
        BYTE* DataBuffer = (BYTE*)appMalloc(Array->Inner->ElementSize, TEXT("TArray_Find"));
        Array->Inner->PullByteFromLua(L, DataBuffer, -1);
        for( int i=0; i<Array->ScriptArray->Num(); i++ )
        {
            void* RawElement = (BYTE*)Array->ScriptArray->GetData() + Array->Inner->ElementSize * i;
            if(Array->Inner->Identical(RawElement, DataBuffer))
            {
                Index = i;
                break;
            }
        }
        Array->Inner->DestroyValue(DataBuffer);
        appFree(DataBuffer);
        lua_pushinteger(L, Index+1);
        return 1;
        unguard;
    }

    static int LuaDelegate_Index(lua_State* L)
    {
        guard(LuaDelegate_Index, lvLNmYVXGmOCHO7CPmfUAClrdrCw0tnP);
        lua_getmetatable(L, 1);
        lua_pushvalue(L, 2);
        lua_rawget(L, -2);
        lua_remove(L, -2);
        return 1;
        unguard;
    }

    static int LuaDelegate_Bind(lua_State* L)
    {
        guard(LuaDelegate_Bind, BRkv3Kxcs85CVJTnNIToJ2VfSxhHzibx);
        FLuaScriptDelegate* UserData = (FLuaScriptDelegate*)lua_touserdata(L, 1);
        if(!UserData || UserData->NativeDelegate == NULL)
        {
            return luaL_error(L, "invalid delegate");
        }

        if(!lua_isfunction(L, 3))
        {
            return luaL_error(L, "invalid function");
        }

        UObject *Outer = LuaBridge::GetUObjectFromLuaProxy(L, 2);
        if(!Outer)
        {
            return luaL_error(L, "invalid object");
        }

        UDynamicDelegateProxy* ObjectProxy = Cast<UDynamicDelegateProxy>(UserData->NativeDelegate->Object);
        if(ObjectProxy == NULL)
        {
            ObjectProxy = (UDynamicDelegateProxy*)UObject::StaticConstructObject(UDynamicDelegateProxy::StaticClass());
        }

        ObjectProxy->L = L;
        ObjectProxy->ProxyOwner = Outer;
        ObjectProxy->SignatureFunction = UserData->Inner;
        lua_pushvalue(L, 3);
        ObjectProxy->ScriptFuncRef = luaL_ref(L, LUA_REGISTRYINDEX);

        // 绑定代理对象
        UserData->NativeDelegate->Object = ObjectProxy;
        return 0;
        unguard;
    }

    static int LuaDelegate_Unbind(lua_State* L)
    {
        guard(LuaDelegate_Unbind, U4uB7jHP97oh025G5y9Dt5tOlYgvoaZJ);
        FLuaScriptDelegate* UserData = (FLuaScriptDelegate*)lua_touserdata(L, 1);
        if(!UserData || UserData->NativeDelegate == NULL)
        {
            return 0;
        }

        UDynamicDelegateProxy* ObjectProxy = Cast<UDynamicDelegateProxy>(UserData->NativeDelegate->Object);
        if(ObjectProxy)
        {
            ObjectProxy->ProxyOwner = NULL;
            ObjectProxy->SignatureFunction = NULL;
            if(ObjectProxy->ScriptFuncRef != LUA_NOREF)
            {
                luaL_unref(L, LUA_REGISTRYINDEX, ObjectProxy->ScriptFuncRef);
                ObjectProxy->ScriptFuncRef = LUA_NOREF;
            }
        }

        UserData->NativeDelegate->FunctionName = NAME_None;
        UserData->NativeDelegate->Object = NULL;
        return 0;
        unguard;
    }

    static int LuaDelegate_Invoke(lua_State* L)
    {
        guard(LuaDelegate_Invoke, xeUwLYexZCnDh0VTBbE4TObnqDcoXQJr);
        FLuaScriptDelegate* UserData = (FLuaScriptDelegate*)lua_touserdata(L, 1);
        if(!UserData || UserData->NativeDelegate == NULL || UserData->NativeDelegate->Object == NULL)
        {
            return 0;
        }

        if(!LuaBridge::CheckObjIsValid(UserData->Inner) || !LuaBridge::CheckObjIsValid(UserData->NativeDelegate->Object))
        {
            UserData->NativeDelegate->Object = NULL;
            UserData->NativeDelegate->FunctionName = NAME_None;
            return luaL_error(L, "invalid delegate description");
        }

        BYTE *Parms = (BYTE*)appAlloca(UserData->Inner->PropertiesSize);
        appMemzero(Parms, UserData->Inner->PropertiesSize);
        LuaBridge::FProperty *RetType = LuaBridge::PrepareParmsForUE(L, UserData->Inner, Parms);

        UserData->NativeDelegate->Object->ProcessDelegate(NAME_None, UserData->NativeDelegate, Parms);

        // 压回返回值
        if(RetType) RetType->PushByteToLua(L, Parms + UserData->Inner->ReturnValueOffset);

        // 析构参数和local变量
        for( UProperty* Property=(UProperty*)UserData->Inner->Children; Property; Property=(UProperty*)Property->Next )
		{
            Property->DestroyValue(Parms + Property->GetCustomOffset());
        }

        return RetType == NULL ? 0 : 1;
        unguard;
    }

    static int FVector_New(lua_State* L)
    {
        guard(FVector_New, xpba40V4ePu1e5TLQNCssRtNiTcNO6ZH);
        FVector* Vector = new FVector(luaL_optnumber(L, 1, 0.0f), luaL_optnumber(L, 2, 0.0f), luaL_optnumber(L, 3, 0.0f));
        FLuaStruct* VectorPtr = NewScriptContainerWithCache<FLuaStruct>(L, Vector, "Vector");
        if(VectorPtr)
        {
            VectorPtr->ScriptStruct = Vector;
            VectorPtr->IsLuaNative = 1;
        }
        return 1;
        unguard;
    }

    static int FVector_Index(lua_State* L)
    {
        guard(FVector_Index, ChB9IDNlpJZewV8iF84ttgSKWBhkHHU6);
        {
            lua_getmetatable(L, 1);
            lua_pushvalue(L, 2);
            lua_rawget(L, -2);
            if(!lua_isnil(L, -1))
            {
                lua_remove(L, -2);
                return 1;
            }
            
            lua_pop(L, 2);
        }

        FLuaStruct* Vector = (FLuaStruct*)lua_touserdata(L, -2);
        if(Vector == NULL || Vector->ScriptStruct == NULL)
        {
            return luaL_error(L, "invalid FVector");
        }

        const char* Key = lua_tostring(L, -1);
        if(Key == NULL)
        {
            return luaL_error(L, "invalid key");
        }

        if(*Key == 'X' || *Key == 'x')
        {
            lua_pushnumber(L, ((FVector*)(Vector->ScriptStruct))->X);
        }
        else if(*Key == 'Y' || *Key == 'y')
        {
            lua_pushnumber(L, ((FVector*)(Vector->ScriptStruct))->Y);
        }
        else if(*Key == 'Z' || *Key == 'z')
        {
            lua_pushnumber(L, ((FVector*)(Vector->ScriptStruct))->Z);
        }
        else
        {
            return luaL_error(L, "invalid key");
        }

        return 1;
        unguard;
    }

    static int FVector_NewIndex(lua_State* L)
    {
        guard(FVector_NewIndex, R3chOB4KjrNoKTflrbtQqfVphVOhJxF9);
        FLuaStruct* Vector = (FLuaStruct*)lua_touserdata(L, -3);
        if(Vector == NULL || Vector->ScriptStruct == NULL)
        {
            return luaL_error(L, "invalid FVector");
        }

        const char* Key = lua_tostring(L, -2);
        if(Key == NULL)
        {
            return luaL_error(L, "invalid key");
        }

        if(*Key == 'X' || *Key == 'x')
        {
            ((FVector*)(Vector->ScriptStruct))->X = luaL_checknumber(L, -1);
        }
        else if(*Key == 'Y' || *Key == 'y')
        {
            ((FVector*)(Vector->ScriptStruct))->Y = luaL_checknumber(L, -1);
        }
        else if(*Key == 'Z' || *Key == 'z')
        {
            ((FVector*)(Vector->ScriptStruct))->Z = luaL_checknumber(L, -1);
        }
        else
        {
            return luaL_error(L, "invalid key");
        }

        return 0;
        unguard;
    }

    static int FVector_Length(lua_State* L)
    {
        guard(FVector_Length, KY5t2P5v8JSrYWkFynBFagJcBrRaDKcF);
        FLuaStruct* Vector = (FLuaStruct*)lua_touserdata(L, -1);
        if(Vector == NULL || Vector->ScriptStruct == NULL)
        {
            return luaL_error(L, "invalid FVector");
        }

        lua_pushnumber(L, ((FVector*)(Vector->ScriptStruct))->Size());
        return 1;
        unguard;
    }

    static int FLuaStruct_Release(lua_State* L)
    {
        guard(FLuaStruct_Release, SABnPgSCrBUkP2guTWOo1f9trob7vCkk);
        FLuaStruct* Vector = (FLuaStruct*)lua_touserdata(L, -1);
        if(Vector && Vector->ScriptStruct && Vector->IsLuaNative)
        {
            appFree(Vector->ScriptStruct);
            Vector->ScriptStruct = NULL;
        }

        return 0;
        unguard;
    }

    static int FVector_Add(lua_State* L)
    {
        guard(FVector_Add, jqt3gMbFCmH2RDfjVBeR2rltcc4mirKO);
        // 确保有两个参数
        if (lua_gettop(L) != 2)
        {
            return luaL_error(L, "invalid parameters");
        }

        FLuaStruct* Vector1 = (FLuaStruct*)lua_touserdata(L, 1);
        FLuaStruct* Vector2 = (FLuaStruct*)lua_touserdata(L, 2);
        if(Vector1 == NULL || Vector1->ScriptStruct == NULL || Vector2 == NULL || Vector2->ScriptStruct == NULL)
        {
            return luaL_error(L, "invalid FVector");
        }
        
        FVector* Result = new FVector(
            ((FVector*)(Vector1->ScriptStruct))->X + ((FVector*)(Vector2->ScriptStruct))->X,
            ((FVector*)(Vector1->ScriptStruct))->Y + ((FVector*)(Vector2->ScriptStruct))->Y,
            ((FVector*)(Vector1->ScriptStruct))->Z + ((FVector*)(Vector2->ScriptStruct))->Z);

        FLuaStruct* ResultPtr = NewScriptContainerWithCache<FLuaStruct>(L, Result, "Vector");
        if(ResultPtr)
        {
            ResultPtr->ScriptStruct = Result;
            ResultPtr->IsLuaNative = 1;
        }

        return 1;
        unguard;
    }

    static int FVector_Sub(lua_State* L)
    {
        guard(FVector_Sub, ZhiteTcsw9dpezsoBzx5saJfc8BqIXYJ);
        // 确保有两个参数
        if (lua_gettop(L) != 2)
        {
            return luaL_error(L, "invalid parameters");
        }

        FLuaStruct* Vector1 = (FLuaStruct*)lua_touserdata(L, 1);
        FLuaStruct* Vector2 = (FLuaStruct*)lua_touserdata(L, 2);
        if(Vector1 == NULL || Vector1->ScriptStruct == NULL || Vector2 == NULL || Vector2->ScriptStruct == NULL)
        {
            return luaL_error(L, "invalid FVector");
        }

        FVector* Result = new FVector(
            ((FVector*)(Vector1->ScriptStruct))->X - ((FVector*)(Vector2->ScriptStruct))->X,
            ((FVector*)(Vector1->ScriptStruct))->Y - ((FVector*)(Vector2->ScriptStruct))->Y,
            ((FVector*)(Vector1->ScriptStruct))->Z - ((FVector*)(Vector2->ScriptStruct))->Z);

        FLuaStruct* ResultPtr = NewScriptContainerWithCache<FLuaStruct>(L, Result, "Vector");
        if(ResultPtr)
        {
            ResultPtr->ScriptStruct = Result;
            ResultPtr->IsLuaNative = 1;
        }

        return 1;
        unguard;
    }

    static int FVector_UNM(lua_State* L)
    {
        guard(FVector_UNM, lcLYBeabsUxxPAOH1QIEFwNn9mKt0why);
        // 确保有一个参数
        if (lua_gettop(L) != 1)
        {
            return luaL_error(L, "invalid parameters");
        }

        FLuaStruct* Vector = (FLuaStruct*)lua_touserdata(L, 1);
        if(Vector == NULL || Vector->ScriptStruct == NULL)
        {
            return luaL_error(L, "invalid FVector");
        }

        FVector* Result = new FVector(-((FVector*)(Vector->ScriptStruct))->X, -((FVector*)(Vector->ScriptStruct))->Y, -((FVector*)(Vector->ScriptStruct))->Z);
        FLuaStruct* ResultPtr = NewScriptContainerWithCache<FLuaStruct>(L, Result, "Vector");
        if(ResultPtr)
        {
            ResultPtr->ScriptStruct = Result;
            ResultPtr->IsLuaNative = 1;
        }

        return 1;
        unguard;
    }

    static int FVector_ToString(lua_State* L)
    {
        guard(FVector_ToString, n4SB9RiRQarELivIwKlzJCcmOyxKsbwc);
        FLuaStruct* Vector = (FLuaStruct*)lua_touserdata(L, 1);
        if(Vector == NULL || Vector->ScriptStruct == NULL)
        {
            return luaL_error(L, "invalid FVector");
        }

        lua_pushfstring(L, "FVector(%f, %f, %f)", ((FVector*)(Vector->ScriptStruct))->X, ((FVector*)(Vector->ScriptStruct))->Y, ((FVector*)(Vector->ScriptStruct))->Z);
        return 1;
        unguard;
    }

    static int FVector_ToRotator(lua_State* L)
    {
        guard(FVector_ToRotator, HqqGbMMpLzqGvbK9phtDcJMWuCS6ptgJ);
        FLuaStruct* Vector = (FLuaStruct*)lua_touserdata(L, 1);
        if(Vector == NULL || Vector->ScriptStruct == NULL)
        {
            return luaL_error(L, "invalid FVector");
        }

        FRotator* Result = new FRotator(((FVector*)(Vector->ScriptStruct))->Rotation());
        FLuaStruct* ResultPtr = NewScriptContainerWithCache<FLuaStruct>(L, Result, "Rotator");
        if(ResultPtr)
        {
            ResultPtr->ScriptStruct = Result;
            ResultPtr->IsLuaNative = 1;
        }

        return 1;
        unguard;
    }

    static int FVector_Normalize(lua_State* L)
    {
        guard(FVector_Normalize, TP8mTwy5upoghZfW2BRORKOQ2Pqu2of9);
        FLuaStruct* Vector = (FLuaStruct*)lua_touserdata(L, 1);
        if(Vector == NULL || Vector->ScriptStruct == NULL)
        {
            return luaL_error(L, "invalid FVector");
        }

        ((FVector*)(Vector->ScriptStruct))->Normalize();
        return 0;
        unguard;
    }

    static int FRotator_New(lua_State* L)
    {
        guard(FRotator_New, LMN4nvb7ndnAQYiEEZWVSX4rae70K4ii);
        FRotator* Rot = new FRotator(
            luaL_optinteger(L, 1, 0),
            luaL_optinteger(L, 2, 0),
            luaL_optinteger(L, 3, 0)
        );
        FLuaStruct* RotPtr = NewScriptContainerWithCache<FLuaStruct>(L, Rot, "Rotator");
        if(RotPtr) {
            RotPtr->ScriptStruct = Rot;
            RotPtr->IsLuaNative = 1;
        }
        return 1;
        unguard;
    }

    static int FRotator_Index(lua_State* L)
    {
        guard(FRotator_Index, z7NEKJnn2ENpDwLP7RKUnB2BL59z4Yrj);
        {
            lua_getmetatable(L, 1);
            lua_pushvalue(L, 2);
            lua_rawget(L, -2);
            if(!lua_isnil(L, -1))
            {
                lua_remove(L, -2);
                return 1;
            }
            
            lua_pop(L, 2);
        }

        FLuaStruct* Rot = (FLuaStruct*)lua_touserdata(L, 1);
        const char* key = lua_tostring(L, 2);
        if(key && Rot && Rot->ScriptStruct) {
            if(strcmp(key, "Pitch") == 0) lua_pushinteger(L, ((FRotator*)Rot->ScriptStruct)->Pitch);
            else if(strcmp(key, "Yaw") == 0) lua_pushinteger(L, ((FRotator*)Rot->ScriptStruct)->Yaw);
            else if(strcmp(key, "Roll") == 0) lua_pushinteger(L, ((FRotator*)Rot->ScriptStruct)->Roll);
            else lua_pushnil(L);
            return 1;
        }
        return 0;
        unguard;
    }

    static int FRotator_NewIndex(lua_State* L)
    {
        guard(FRotator_NewIndex, LOxhKgYE3KgwEsmjFbpkyRGw5IHgNywz);
        FLuaStruct* Rot = (FLuaStruct*)lua_touserdata(L, 1);
        const char* key = lua_tostring(L, 2);
        if(key && Rot && Rot->ScriptStruct) {
            int val = lua_tointeger(L, 3);
            if(strcmp(key, "Pitch") == 0) ((FRotator*)Rot->ScriptStruct)->Pitch = val;
            else if(strcmp(key, "Yaw") == 0) ((FRotator*)Rot->ScriptStruct)->Yaw = val;
            else if(strcmp(key, "Roll") == 0) ((FRotator*)Rot->ScriptStruct)->Roll = val;
        }
        return 0;
        unguard;
    }

    static int FRotator_ToString(lua_State* L)
    {
        guard(FRotator_ToString, MgbAq20pCKrur9IKS2682U5KM5HTFFgh);
        FLuaStruct* Rot = (FLuaStruct*)lua_touserdata(L, 1);
        if(Rot == NULL || Rot->ScriptStruct == NULL)
        {
            return luaL_error(L, "invalid FRotator");
        }

        lua_pushfstring(L, "FRotator(%d, %d, %d)", ((FRotator*)(Rot->ScriptStruct))->Pitch, ((FRotator*)(Rot->ScriptStruct))->Yaw, ((FRotator*)(Rot->ScriptStruct))->Roll);
        return 1;
        unguard;
    }

    static int FRotator_ToVector(lua_State* L)
    {
        guard(FRotator_ToVector, tVgxnVfQgX2MlhRMK4cCKhld4u8Tcqv6);
        FLuaStruct* Rot = (FLuaStruct*)lua_touserdata(L, 1);
        if(Rot == NULL || Rot->ScriptStruct == NULL)
        {
            return luaL_error(L, "invalid FRotator");
        }

        FVector* Result = new FVector(((FRotator*)(Rot->ScriptStruct))->Vector());
        FLuaStruct* ResultPtr = NewScriptContainerWithCache<FLuaStruct>(L, Result, "Vector");
        if(ResultPtr)
        {
            ResultPtr->ScriptStruct = Result;
            ResultPtr->IsLuaNative = 1;
        }

        return 1;
        unguard;
    }

    static int FColor_New(lua_State* L)
    {
        guard(FColor_New, Fc6BT6V8dT8PFduRvNPXkBbRfJb2Z44K);
        FLuaColor* Color = new FLuaColor(
            luaL_optinteger(L, 1, 0),
            luaL_optinteger(L, 2, 0),
            luaL_optinteger(L, 3, 0),
            luaL_optinteger(L, 4, 255)
        );
        FLuaStruct* ColorPtr = NewScriptContainerWithCache<FLuaStruct>(L, Color, "Color");
        if(ColorPtr) {
            ColorPtr->ScriptStruct = Color;
            ColorPtr->IsLuaNative = 1;
        }
        return 1;
        unguard;
    }

    static int FColor_Index(lua_State* L)
    {
        guard(FColor_Index, CldCKKIAGEsfwR9yyLBdDr5UkIfPk1ME);
        FLuaStruct* Color = (FLuaStruct*)lua_touserdata(L, 1);
        const char* key = lua_tostring(L, 2);
        if(key && Color && Color->ScriptStruct) {
            if(strcmp(key, "R") == 0) lua_pushinteger(L, ((FLuaColor*)Color->ScriptStruct)->R);
            else if(strcmp(key, "G") == 0) lua_pushinteger(L, ((FLuaColor*)Color->ScriptStruct)->G);
            else if(strcmp(key, "B") == 0) lua_pushinteger(L, ((FLuaColor*)Color->ScriptStruct)->B);
            else if(strcmp(key, "A") == 0) lua_pushinteger(L, ((FLuaColor*)Color->ScriptStruct)->A);
            else lua_pushnil(L);
            return 1;
        }
        return 0;
        unguard;
    }

    static int FColor_NewIndex(lua_State* L)
    {
        guard(FColor_NewIndex, Q1eEbXWCwpX8PGrw9ZM5XEf6XAveV7Ee);
        FLuaStruct* Color = (FLuaStruct*)lua_touserdata(L, 1);
        const char* key = lua_tostring(L, 2);
        if(key && Color && Color->ScriptStruct) {
            int val = lua_tointeger(L, 3);
            if(strcmp(key, "R") == 0) ((FLuaColor*)Color->ScriptStruct)->R = val;
            else if(strcmp(key, "G") == 0) ((FLuaColor*)Color->ScriptStruct)->G = val;
            else if(strcmp(key, "B") == 0) ((FLuaColor*)Color->ScriptStruct)->B = val;
            else if(strcmp(key, "A") == 0) ((FLuaColor*)Color->ScriptStruct)->A = val;
        }
        return 0;
        unguard;
    }

    static int FColor_ToString(lua_State* L)
    {
        guard(FColor_ToString, nm8OG1OnH8g8ExbomLvsYfMTqWzhSfgO);
        FLuaStruct* Color = (FLuaStruct*)lua_touserdata(L, 1);
        if(Color == NULL || Color->ScriptStruct == NULL)
        {
            return luaL_error(L, "invalid FColor");
        }

        lua_pushfstring(L, "FColor(%d, %d, %d, %d)", ((FLuaColor*)(Color->ScriptStruct))->R, ((FLuaColor*)(Color->ScriptStruct))->G, ((FLuaColor*)(Color->ScriptStruct))->B, ((FLuaColor*)(Color->ScriptStruct))->A);
        return 1;
        unguard;
    }

    void PushFArrayToLua(lua_State *L, FArray *Array, UProperty *Inner, bool IsLuaNative)
    {
        guard(PushFArrayToLua, smJuQiggEXDG0PjU7c6Turd961SkWMQm);
        FLuaArray* UserData = NewScriptContainerWithCache<FLuaArray>(L, Array, "TArray");
        if(UserData)
        {
            UserData->ScriptArray = Array;
            UserData->Inner = Inner;
            UserData->IsLuaNative = IsLuaNative;
        }
        unguard;
    }

    void PullFArrayFromLua(lua_State *L, UArrayProperty* ArrProperty, FArray *OutArray, int Index)
    {
        guard(PullFArrayFromLua, O74eOOSqPoXWOkJ3sx6mIRM1EwY3J73S);
        FLuaArray* Array = (FLuaArray*)lua_touserdata(L, Index);
        if (!Array || !Array->ScriptArray)
        {
            if (ArrProperty->Inner->PropertyFlags & CPF_NeedCtorLink)
            {
                ArrProperty->DestroyValue(OutArray);
            }

            return;
        }

        ArrProperty->CopyCompleteValue(OutArray, Array->ScriptArray);
        unguard;
    }

    static int TArray_New(lua_State* L)
    {
        guard(TArray_New, Rriy4bmhNb3qT5nzhSTfQvekUM5Gwolw);
        UProperty* InnerType = Cast<UProperty>((UProperty*)lua_touserdata(L, -1));
        if (!LuaBridge::CheckObjIsValid(InnerType))
            return luaL_error(L, "invalid parameters");
        
        PushFArrayToLua(L, new FArray(), InnerType, true);
        return 1;
        unguard;
    }

    static void RegisterPropertyMeta(lua_State *L)
    {
        guard(RegisterPropertyMeta, aEJCs1qvgjxCFAdA6RTUMoEIXgB181Co);
        UStruct* PropertyMetaRoot = (UStruct*)UObject::StaticConstructObject(FPropertyMetaRoot::StaticClass(), UObject::GetTransientPackage(), FName(TEXT("FPropertyMetaRoot")));
        PropertyMetaRoot->FriendlyName = FName(TEXT("FPropertyMetaRoot"));
        UProperty* Property = NULL;

        Property = new(PropertyMetaRoot, NAME_None, RF_Public)UByteProperty(EC_CppProperty, 0, TEXT(""), CPF_Transient);
        Property->ElementSize = sizeof(BYTE);
        Property->AddToRoot();
        lua_pushlightuserdata(L, Property);
        lua_setfield(L, -2, "UByteProperty");

        Property = new(PropertyMetaRoot, NAME_None, RF_Public)UPointerProperty(EC_CppProperty, 0, TEXT(""), CPF_Transient);
        Property->ElementSize = sizeof(PTRINT);
        Property->AddToRoot();
        lua_pushlightuserdata(L, Property);
        lua_setfield(L, -2, "UPointerProperty");

        Property = new(PropertyMetaRoot, NAME_None, RF_Public)UIntProperty(EC_CppProperty, 0, TEXT(""), CPF_Transient);
        Property->ElementSize = sizeof(INT);
        Property->AddToRoot();
        lua_pushlightuserdata(L, Property);
        lua_setfield(L, -2, "UIntProperty");

        Property = new(PropertyMetaRoot, NAME_None, RF_Public)UFloatProperty(EC_CppProperty, 0, TEXT(""), CPF_Transient);
        Property->ElementSize = sizeof(FLOAT);
        Property->AddToRoot();
        lua_pushlightuserdata(L, Property);
        lua_setfield(L, -2, "UFloatProperty");

        Property = new(PropertyMetaRoot, NAME_None, RF_Public)UBoolProperty(EC_CppProperty, 0, TEXT(""), CPF_Transient);
        Property->ElementSize = sizeof(BITFIELD);
        Property->AddToRoot();
        lua_pushlightuserdata(L, Property);
        lua_setfield(L, -2, "UBoolProperty");

        Property = new(PropertyMetaRoot, NAME_None, RF_Public)UStrProperty(EC_CppProperty, 0, TEXT(""), CPF_Transient);
        Property->ElementSize = sizeof(FString);
        Property->AddToRoot();
        lua_pushlightuserdata(L, Property);
        lua_setfield(L, -2, "UStrProperty");

        Property = new(PropertyMetaRoot, NAME_None, RF_Public)UNameProperty(EC_CppProperty, 0, TEXT(""), CPF_Transient);
        Property->ElementSize = sizeof(FName);
        Property->AddToRoot();
        lua_pushlightuserdata(L, Property);
        lua_setfield(L, -2, "UNameProperty");

        Property = new(PropertyMetaRoot, NAME_None, RF_Public)UObjectProperty(EC_CppProperty, 0, TEXT(""), CPF_Transient,UObject::StaticClass());
        Property->ElementSize = sizeof(UObject*);
        Property->AddToRoot();
        lua_pushlightuserdata(L, Property);
        lua_setfield(L, -2, "UObjectProperty");
        unguard;
    }

    static void RegisterFVectorLib(lua_State *L)
    {
        guard(RegisterFVectorLib, mM75P9yNamdrn7CjHVNQHyuNBygnCUz9);
        luaL_newmetatable(L, "Vector");
        lua_pushcfunction(L, FVector_Length);
        lua_setfield(L, -2, "Size");
        lua_pushcfunction(L, FLuaStruct_Release);
        lua_setfield(L, -2, "__gc");
        lua_pushcfunction(L, FVector_Add);
        lua_setfield(L, -2, "__add");
        lua_pushcfunction(L, FVector_Sub);
        lua_setfield(L, -2, "__sub");
        lua_pushcfunction(L, FVector_UNM);
        lua_setfield(L, -2, "__unm");
        lua_pushcfunction(L, FVector_ToString);
        lua_setfield(L, -2, "__tostring");
        lua_pushcfunction(L, FVector_ToRotator);
        lua_setfield(L, -2, "ToRotator");
        lua_pushcfunction(L, FVector_Normalize);
        lua_setfield(L, -2, "Normalize");
		lua_pushcfunction(L, FVector_Index);
        lua_setfield(L, -2, "__index");
        lua_pushcfunction(L, FVector_NewIndex);
        lua_setfield(L, -2, "__newindex");
        lua_pop(L, 1);
        unguard;
    }

    static void RegisterFRotatorLib(lua_State* L)
    {
        guard(RegisterFRotatorLib, r6RcFE5yr5Xm1agjlUn5ooztlPxaP8Cp);
        luaL_newmetatable(L, "Rotator");
        lua_pushcfunction(L, FLuaStruct_Release);
        lua_setfield(L, -2, "__gc");
        lua_pushcfunction(L, FRotator_ToVector);
        lua_setfield(L, -2, "ToVector");
        lua_pushcfunction(L, FRotator_ToString);
        lua_setfield(L, -2, "__tostring");
        lua_pushcfunction(L, FRotator_Index);
        lua_setfield(L, -2, "__index");
        lua_pushcfunction(L, FRotator_NewIndex);
        lua_setfield(L, -2, "__newindex");
        lua_pop(L, 1);
        unguard;
    }

    static void RegisterFColorLib(lua_State* L)
    {
        guard(RegisterFColorLib, Q1cCpJWL6457Ch30OlKLjblCgzg6Vwl1);
        luaL_newmetatable(L, "Color");
        lua_pushcfunction(L, FLuaStruct_Release);
        lua_setfield(L, -2, "__gc");
        lua_pushcfunction(L, FColor_ToString);
        lua_setfield(L, -2, "__tostring");
        lua_pushcfunction(L, FColor_Index);
        lua_setfield(L, -2, "__index");
        lua_pushcfunction(L, FColor_NewIndex);
        lua_setfield(L, -2, "__newindex");
        lua_pop(L, 1);
        unguard;
    }

    static void RegisterTArrayLib(lua_State *L)
    {
        guard(RegisterTArrayLib, XptKM8zii6dzBts2bCh4vNuY5bTbCiee);
        luaL_newmetatable(L, "TArray");
		lua_pushcfunction(L, TArray_Release);
        lua_setfield(L, -2, "__gc");
        lua_pushcfunction(L, TArray_Insert);
        lua_setfield(L, -2, "Insert");
        lua_pushcfunction(L, TArray_Remove);
        lua_setfield(L, -2, "Remove");
        lua_pushcfunction(L, TArray_Push);
        lua_setfield(L, -2, "Push");
        lua_pushcfunction(L, TArray_Find);
        lua_setfield(L, -2, "Find");
        lua_pushcfunction(L, TArray_Length);
        lua_setfield(L, -2, "Num");
		lua_pushcfunction(L, TArray_Get);
        lua_setfield(L, -2, "__index");
		lua_pushcfunction(L, TArray_Set);
        lua_setfield(L, -2, "__newindex");
        lua_pop(L, 1);
        unguard;
    }

    static void RegisterLuaDelegateLib(lua_State *L)
    {
        guard(RegisterLuaDelegateLib, gyy7sALreRtOkudZdZR3pMwCncjLLd9G);
        luaL_newmetatable(L, "LuaDelegate");
        lua_pushcfunction(L, LuaDelegate_Bind);
        lua_setfield(L, -2, "Bind");
        lua_pushcfunction(L, LuaDelegate_Unbind);
        lua_setfield(L, -2, "Unbind");
        lua_pushcfunction(L, LuaDelegate_Invoke);
        lua_setfield(L, -2, "Invoke");
        lua_pushcfunction(L, LuaDelegate_Index);
        lua_setfield(L, -2, "__index");
        lua_pop(L, 1);
        unguard;
    }

    static const struct luaL_Reg UE_Base_Lib[] = {
        {"LoadClass", &LuaBridge::Global_LoadClass},
        {"NewObject", &LuaBridge::Global_NewObject},
        {"IsValid",   &LuaBridge::Global_IsObjectValid},
        {"TArray"   , &TArray_New},
        {"Vector"  , &FVector_New},
        {"Rotator",  &FRotator_New},
        {"Color",    &FColor_New},
        {NULL, NULL}  /* sentinel */
    };

    int luaopen_UE_BaseLib(lua_State* L) {
        guard(luaopen_UE_BaseLib, nalCMMma87x4eEjddYycmZaEjOs3EWb3);
        RegisterTArrayLib(L);
        RegisterFVectorLib(L);
        RegisterFRotatorLib(L);
        RegisterFColorLib(L);
        RegisterLuaDelegateLib(L);
        luaL_newlib(L, UE_Base_Lib);
        return 1;
        unguard;
    }
}

void LuaBridge::RegisterCommonLib(lua_State *L)
{
    guard(LuaBridge::RegisterCommonLib, x0P5uOKAyEtVHItwC4c8jsAYxzrctFYl);
    luaL_requiref(L, "UE", luaopen_UE_BaseLib, 1);
    RegisterPropertyMeta(L);
    lua_pop(L, 1);
    unguard;
}

void LuaBridge::RegisterExtLib(lua_State *L)
{
    guard(LuaBridge::RegisterExtLib, gNuKTaIjqxKwne7S2VdtCWZChuBqOga5);
#if KFX_ENABLE_LUA_SOCKET
    LuaBridge::AutoLuaStack as(L);
    lua_register(L, "LuaSocketCore", NS_SLUA::luaopen_socket_core);
#endif
    unguard;
}

void UProperty::PushByteToLua(struct lua_State*L, BYTE* Src)
{
    lua_pushnil(L);
}
void UProperty::PullByteFromLua(struct lua_State*L, BYTE* Dest, int Index)
{
}

void UByteProperty::PushByteToLua(struct lua_State*L, BYTE* Src)
{
    guard(UByteProperty::PushByteToLua, T3Rd4jcmvtfUOOuAR67uSWlDks0XOHkm);
    BYTE Value = *Src;
    lua_pushinteger(L, Value);
    unguard;
}
void UByteProperty::PullByteFromLua(struct lua_State*L, BYTE* Dest, int Index)
{
    guard(UByteProperty::PullByteFromLua, kraR4sPEih7yY4Cyx2QwexYZd9MdCuXy);
    *Dest = (BYTE)lua_tointeger(L, Index);
    unguard;
}

void UPointerProperty::PushByteToLua(struct lua_State*L, BYTE* Src)
{
    guard(UPointerProperty::PushByteToLua, xNxi6nqNab6XWVhkqpLU8e9xqlbYfHqO);
    PTRINT Value = *(PTRINT *)(Src);
    lua_islightuserdata(L, Value);
    unguard;
}
void UPointerProperty::PullByteFromLua(struct lua_State*L, BYTE* Dest, int Index)
{
    guard(UPointerProperty::PullByteFromLua, LG0flUhFBDgO42XmJ7pDje6lgNcYa2Bw);
    *(PTRINT *)(Dest) = *(PTRINT *)lua_touserdata(L, Index);
    unguard;
}

void UIntProperty::PushByteToLua(struct lua_State*L, BYTE* Src)
{
    guard(UIntProperty::PushByteToLua, vlzMj3B0tQguXdqbKFlJ8PfEKJKHUhA7);
    int Value = *(int *)(Src);
    lua_pushinteger(L, Value);
    unguard;
}
void UIntProperty::PullByteFromLua(struct lua_State*L, BYTE* Dest, int Index)
{
    guard(UIntProperty::PullByteFromLua, s36qKuaMGCDApwl3RNu2Lm5Wfvzo61WX);
    *(int *)(Dest) = (int)lua_tointeger(L, Index);
    unguard;
}

void UFloatProperty::PushByteToLua(struct lua_State*L, BYTE* Src)
{
    guard(UFloatProperty::PushByteToLua, cz4g3ABbiyzfWuboSYgFRandZCy5lrcZ);
    float Value = *(float *)(Src);
    lua_pushnumber(L, Value);
    unguard;
}
void UFloatProperty::PullByteFromLua(struct lua_State*L, BYTE* Dest, int Index)
{
    guard(UFloatProperty::PullByteFromLua, JhX173h7QuACemtz79ZQBOtBIPvWsY4R);
    *(float *)(Dest) = (float)lua_tonumber(L, Index);
    unguard;
}

void UBoolProperty::PushByteToLua(struct lua_State*L, BYTE* Src)
{
    guard(UBoolProperty::PushByteToLua, TpLH9kgk5GmZQ8cdveWR7qiWszbxWOD7);
    bool Value = !!(*(BITFIELD*)Src & BitMask);
    lua_pushboolean(L, Value);
    unguard;
}
void UBoolProperty::PullByteFromLua(struct lua_State*L, BYTE* Dest, int Index)
{
    guard(UBoolProperty::PullByteFromLua, aXpldBeeKMwljQNjRmZ32WXVBvyGAovY);
    if (lua_toboolean(L, Index))
    {
        *(BITFIELD*)Dest |= BitMask; // 设置位
    }
    else
    {
        *(BITFIELD*)Dest &= ~BitMask; // 清除位
    }
    unguard;
}

void UObjectProperty::PushByteToLua(struct lua_State*L, BYTE* Src)
{
    guard(UObjectProperty::PushByteToLua, rQuR8zz8VkmMtoIx0Rn4ytSeOelYZ7kB);
    UObject *Value = *(UObject **)(Src);
    LuaBridge::PushUObject(L, Value);
    unguard;
}
void UObjectProperty::PullByteFromLua(struct lua_State*L, BYTE* Dest, int Index)
{
    guard(UObjectProperty::PullByteFromLua, N5B32SHzH8kn0kc6EpEjreS1es6B6qWN);
    *(UObject **)(Dest) = LuaBridge::GetUObjectFromLuaProxy(L, Index);
    unguard;
}

void UStrProperty::PushByteToLua(struct lua_State*L, BYTE* Src)
{
    guard(UStrProperty::PushByteToLua, YL8TtUKBxfdXlrOMgIHqNvnMvXCI7BtH);
    lua_pushstring(L, TCHAR_TO_ANSI(**(FString *)(Src)));
    unguard;
}
void UStrProperty::PullByteFromLua(struct lua_State*L, BYTE* Dest, int Index)
{
    guard(UStrProperty::PullByteFromLua, jay2RD2Swm87csls5LpwGHJ4dzOU81tC);
    const char* RawStr = lua_tostring(L, Index);
    *(FString *)Dest = FString(ANSI_TO_TCHAR(RawStr));
    unguard;
}

void UArrayProperty::PushByteToLua(struct lua_State*L, BYTE* Src)
{
    guard(UArrayProperty::PushByteToLua, MAOPXde6Xxgr7jOcrBXlGLwlf5q49zTe);
    FArray *Value = (FArray *)(Src);
    PushFArrayToLua(L, Value, Inner, false);
    unguard;
}
void UArrayProperty::PullByteFromLua(struct lua_State*L, BYTE* Dest, int Index)
{
    guard(UArrayProperty::PullByteFromLua, hSeo8pe5GfERw0eJ5HO9ayArFxLlKhi1);
    PullFArrayFromLua(L, this, (FArray *)Dest, Index);
    unguard;
}

void UNameProperty::PushByteToLua(struct lua_State*L, BYTE* Src)
{
    guard(UNameProperty::PushByteToLua, tk24LQIb79ZxMjKUvCYwx6Ji6Q2JRz0g);
    FName *Value = (FName *)(Src);
    lua_pushstring(L, TCHAR_TO_ANSI(**Value));
    unguard;
}
void UNameProperty::PullByteFromLua(struct lua_State*L, BYTE* Dest, int Index)
{
    guard(UNameProperty::PullByteFromLua, 4Nd2q14IX5YxuC18HZ3UTr3mfTUrgCWC);
    const char* RawName = lua_tostring(L, Index);
    *(FName *)Dest = FName(ANSI_TO_TCHAR(RawName));
    unguard;
}

void UStructProperty::PushByteToLua(struct lua_State*L, BYTE* Src)
{
    guard(UStructProperty::PushByteToLua, IroLum03HK89vu7Q64cPgFJknbpujmzL);
    switch( Struct->GetFName().GetIndex() )
	{
	    case NAME_Vector:
	    case NAME_Rotator:
        case NAME_Color:
        {
            FLuaStruct* StructPtr = NewScriptContainerWithCache<FLuaStruct>(L, Src, LuaBridge::FastConvertToANSI(Struct->GetName()));
            if(StructPtr)
            {
                StructPtr->ScriptStruct = Src;
            }
            break;
        }
        default:
            lua_pushnil(L);
	}
    unguard;
}
void UStructProperty::PullByteFromLua(struct lua_State*L, BYTE* Dest, int Index)
{
    guard(UStructProperty::PullByteFromLua, G1U7q9kL7bvnGaKsavNLoXdDJo0JYs7i);
    switch( Struct->GetFName().GetIndex() )
	{
	    case NAME_Vector:
	    case NAME_Rotator:
	    case NAME_Color:
        {
            FLuaStruct* RawData = (FLuaStruct*)lua_touserdata(L, Index);
            if(RawData && RawData->ScriptStruct)
            {
                appMemcpy(Dest, RawData->ScriptStruct, Struct->GetPropertiesSize());
            }

	        break;
        }
	}
    unguard;
}

void UDelegateProperty::PushByteToLua(struct lua_State*L, BYTE* Src)
{
    guard(UDelegateProperty::PushByteToLua, VIpvmLmlwrTjaGcd18qgFkKXlI2NIX53);
    FScriptDelegate *Value = (FScriptDelegate*)Src;
    FLuaScriptDelegate* UserData = NewScriptContainerWithCache<FLuaScriptDelegate>(L, Value, "LuaDelegate");
    if(UserData)
    {
        UserData->NativeDelegate = Value;
        UserData->Inner = Function;
    }
    unguard;
}
void UDelegateProperty::PullByteFromLua(struct lua_State*L, BYTE* Dest, int Index)
{
}

void UDynamicDelegateProxy::ProcessSignatureFunction(FScriptDelegate* Delegate, void* Parms)
{
    guard(UDynamicDelegateProxy::ProcessSignatureFunction, n1BKBuVv5WruUA0hziGxdBq6nhWPz88q);
    LUA_ZONE_CHECK;
    if(!L || !LuaBridge::CheckObjIsValid(ProxyOwner)
        || !LuaBridge::CheckObjIsValid(SignatureFunction)
        || ScriptFuncRef == LUA_NOREF)
    {
        Delegate->Object = NULL;
        Delegate->FunctionName = NAME_None;
        return;
    }

    UObject* Object = ProxyOwner;
    UFunction* NativeFunc = SignatureFunction;

    LuaBridge::PushPCallErrorHandler(L);
    lua_rawgeti(L, LUA_REGISTRYINDEX, ScriptFuncRef);
    if (lua_isfunction(L, -1))
    {
        LuaBridge::PushUObject(L, Object);
        LuaBridge::InternalCallLua(L, NativeFunc, (BYTE*)Parms, (BYTE*)Parms+NativeFunc->ReturnValueOffset);
    }
    else
    {
        lua_pop(L, 2); // 弹出nil和错误处理函数
    }
    unguard;
}

void UDynamicDelegateProxy::ProcessDelegate( FName DelegateName, FScriptDelegate* Delegate, void* Parms, void* Result )
{
    guard(UDynamicDelegateProxy::ProcessDelegate, eFMQbQhN0g27WLZhHCxVAxod6psQtqag);
    LUA_ZONE_CHECK;
    if( !LuaBridge::CheckObjIsValid(ProxyOwner) )
	{
		Delegate->Object = NULL;
		Delegate->FunctionName = NAME_None;
        return;
	}

    ProxyOwner->ProcessDelegate( DelegateName, Delegate, Parms, Result );
    unguard;
}

void UDynamicDelegateProxy::ProcessSignatureFunction(FScriptDelegate* Delegate, FFrame& Stack, void* Result)
{
    guard(UDynamicDelegateProxy::ProcessSignatureFunction, k2N8lP1fSntp6p8CN4y5eK9RclLXpo1j);
    LUA_ZONE_CHECK;
	const bool bUnpackParams = SignatureFunction && Stack.Node != SignatureFunction;
    BYTE* InParms = bUnpackParams ? (BYTE*)appAlloca(SignatureFunction->PropertiesSize) : Stack.Locals;
    if (bUnpackParams)
    {
        appMemzero(InParms, SignatureFunction->PropertiesSize);
        for( UProperty* Property=(UProperty*)SignatureFunction->Children; *Stack.Code!=EX_EndFunctionParms; Property=(UProperty*)Property->Next )
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

    if( ScriptFuncRef != LUA_NOREF && LuaBridge::CheckObjIsValid(ProxyOwner) && LuaBridge::CheckObjIsValid(SignatureFunction) )
    {
        LuaBridge::PushPCallErrorHandler(L);
        lua_rawgeti(L, LUA_REGISTRYINDEX, ScriptFuncRef);
        LuaBridge::PushUObject(L, ProxyOwner);
        LuaBridge::InternalCallLua(L, SignatureFunction, InParms, (BYTE*)Result);
    }

    if (bUnpackParams)
    {
        for( UProperty* Property=(UProperty*)SignatureFunction->Children; Property; Property=(UProperty*)Property->Next )
		{
            Property->DestroyValue(InParms + Property->GetCustomOffset());
        }
    }
    unguard;
}

void UDynamicDelegateProxy::Destroy()
{
    guard(UDynamicDelegateProxy::Destroy, I2bV0IBYEO2O8AqKWAgNbwHtB0UxwGuH);
	ProxyOwner = NULL;
    SignatureFunction = NULL;
    if(L && ScriptFuncRef != LUA_NOREF)
    {
        luaL_unref(L, LUA_REGISTRYINDEX, ScriptFuncRef);
        ScriptFuncRef = LUA_NOREF;
    }

	Super::Destroy();
    unguard;
}

IMPLEMENT_CLASS(UDynamicDelegateProxy);
IMPLEMENT_CLASS(FPropertyMetaRoot);
IMPLEMENT_CLASS(UObjectReferencer);
