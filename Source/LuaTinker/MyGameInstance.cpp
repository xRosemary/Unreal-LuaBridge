// Fill out your copyright notice in the Description page of Project Settings.


#include "MyGameInstance.h"

void UMyGameInstance::Init()
{
	Super::Init();

	if (!Env.IsActive)
	{
		Env.IsActive = true;
		GUObjectArray.AddUObjectCreateListener(&Env);
		GUObjectArray.AddUObjectDeleteListener(&Env);
	}
}

void UMyGameInstance::Shutdown()
{
	if (Env.IsActive)
	{
		GUObjectArray.RemoveUObjectCreateListener(&Env);
		GUObjectArray.RemoveUObjectDeleteListener(&Env);
		Env.IsActive = false;
	}
	Super::Shutdown();
}
