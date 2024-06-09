// Copyright Epic Games, Inc. All Rights Reserved.

#include "LuaTinkerGameMode.h"
#include "LuaTinkerCharacter.h"
#include "UObject/ConstructorHelpers.h"

ALuaTinkerGameMode::ALuaTinkerGameMode()
{
	// set default pawn class to our Blueprinted character
	static ConstructorHelpers::FClassFinder<APawn> PlayerPawnBPClass(TEXT("/Game/ThirdPerson/Blueprints/BP_ThirdPersonCharacter"));
	if (PlayerPawnBPClass.Class != NULL)
	{
		DefaultPawnClass = PlayerPawnBPClass.Class;
	}
}
