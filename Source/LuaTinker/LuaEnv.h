// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"

extern "C"
{
#include "ThirdParty/lua.h"
#include "ThirdParty/lualib.h"
#include "ThirdParty/lauxlib.h"
};

#include "LuaEnv.generated.h"

class LUATINKER_API LuaEnv : public FUObjectArray::FUObjectCreateListener
{
	GENERATED_BODY()
protected:
	static TMap<UObject*, void*> ObjToTable;
	static struct lua_State* L;

public:
	//void SetTableForClass(lua_State* L, const char* Name);
	void PushMetatable(UObject* Object, const char* MetatableName);
	bool BindTableForObject(UObject* Object, const char* InModuleName);

public:
	LuaEnv();
	~LuaEnv();

	bool TryToBind(UObject* Object);
	bool BindInternal(UObject* Object, UClass* Class, const TCHAR* InModuleName);

	virtual void NotifyUObjectCreated(const class UObjectBase* Object, int32 Index);

	virtual void OnUObjectArrayShutdown() { GUObjectArray.RemoveUObjectCreateListener(this); IsActive = false;  };

	bool IsActive = false;

	DECLARE_FUNCTION(execCallLua);
};
