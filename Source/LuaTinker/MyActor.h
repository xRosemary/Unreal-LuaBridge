// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "MyActor.generated.h"

UCLASS()
class LUATINKER_API AMyActor : public AActor
{
	GENERATED_BODY()
	
public:	
	// Sets default values for this actor's properties
	AMyActor();

protected:
	// Called when the game starts or when spawned
	virtual void BeginPlay() override;

public:
	UPROPERTY()
	int TestVar = 10;

	UPROPERTY()
	int TestVar2 = 15;

public:	
	// Called every frame
	virtual void Tick(float DeltaTime) override;

	UFUNCTION(BlueprintCallable)
	FString GetModuleName() { return StaticClass()->GetName(); }

	UFUNCTION(BlueprintCallable)
	void TestFunc() { UE_LOG(LogTemp, Error, TEXT("Origin TestFunc")); }

	UFUNCTION(BlueprintCallable)
	int TestFuncWithParam(int p1, FString p2, bool p3)
	{
		UE_LOG(LogTemp, Error, TEXT("TestFuncWithParam: %d, %s, %d"), p1, *p2, p3);
		return p1 + p3;
	}

	UFUNCTION(BlueprintCallable)
	int TestFuncWithParam2(int p1, FString p2, bool p3)
	{
		UE_LOG(LogTemp, Error, TEXT("TestFuncWithParam2: %d, %s, %d"), p1, *p2, p3);
		return p1 + p3;
	}
};
