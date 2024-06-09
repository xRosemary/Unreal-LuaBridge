// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "Engine/GameInstance.h"
#include "LuaEnv.h"
#include "MyGameInstance.generated.h"

/**
 * 
 */
UCLASS()
class LUATINKER_API UMyGameInstance : public UGameInstance
{
	GENERATED_BODY()
	
public:
	virtual void Init();
	virtual void Shutdown();

	LuaEnv Env;
};
