// Fill out your copyright notice in the Description page of Project Settings.


#include "LuaEnv.h"
#include "MyActor.h"

LuaEnv::LuaEnv()
{
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

    return true;
}


DEFINE_FUNCTION(LuaEnv::execCallLua)
{
    UFunction* Func = Stack.Node;
    UObject* Obj = Stack.Object;
    UFunction* NativeFunc = Stack.CurrentNativeFunction;
    P_FINISH;
    UE_LOG(LogTemp, Error, TEXT("execCallLua: Obj %s, Func %s, Native Func %s"), *Obj->GetName(), *Func->GetName(), *NativeFunc->GetName());
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