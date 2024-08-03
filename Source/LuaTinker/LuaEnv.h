// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "LuaCore.h"

#ifdef __cplusplus
extern "C" {
#endif
#include "ThirdParty/lua.h"
#include "ThirdParty/lualib.h"
#include "ThirdParty/lauxlib.h"
#ifdef __cplusplus
}
#endif

namespace LuaBridge
{
	class LUATINKER_API LuaEnv : public FUObjectArray::FUObjectCreateListener
	{
	protected:
		inline static lua_State* L = nullptr;

	public:
		bool LoadTableForObject(UObject* Object, const char* InModuleName);

	public:
		LuaEnv();
		~LuaEnv();

		bool TryToBind(UObject* Object);
		bool BindInternal(UObject* Object, UClass* Class, const TCHAR* InModuleName);

		virtual void NotifyUObjectCreated(const class UObjectBase* Object, int32 Index);

		virtual void OnUObjectArrayShutdown() { GUObjectArray.RemoveUObjectCreateListener(this); IsActive = false; };

		bool IsActive = false;

		DECLARE_FUNCTION(execCallLua);
	};
}
