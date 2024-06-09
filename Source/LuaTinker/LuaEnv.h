// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"

/**
 * 
 */
class LUATINKER_API LuaEnv : public FUObjectArray::FUObjectCreateListener
{
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
