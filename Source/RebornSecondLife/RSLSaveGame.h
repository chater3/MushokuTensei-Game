#pragma once

#include "CoreMinimal.h"
#include "GameFramework/SaveGame.h"
#include "RSLSaveGame.generated.h"

/** Файл сохранения REBORN: SECOND LIFE (слот SL0). */
UCLASS()
class USLSaveGameData : public USaveGame
{
	GENERATED_BODY()

public:
	UPROPERTY(SaveGame) int32 SceneIndex = 0;

	UPROPERTY(SaveGame) float HP = 100.f;
	UPROPERTY(SaveGame) float MaxHP = 100.f;
	UPROPERTY(SaveGame) float MP = 50.f;
	UPROPERTY(SaveGame) float MaxMP = 50.f;

	UPROPERTY(SaveGame) int32 XP = 0;
	UPROPERTY(SaveGame) int32 Gold = 0;
	UPROPERTY(SaveGame) int32 SP = 0;
	UPROPERTY(SaveGame) int32 GuildPts = 0;

	UPROPERTY(SaveGame) int32 Level = 1;
	UPROPERTY(SaveGame) int32 StatPts = 0;

	UPROPERTY(SaveGame) TMap<FString, int32> Flags;
	UPROPERTY(SaveGame) TMap<FString, int32> Items;
	UPROPERTY(SaveGame) TMap<FString, int32> Trust;
	UPROPERTY(SaveGame) TMap<FString, int32> Stats;
	UPROPERTY(SaveGame) TMap<FString, int32> Kills;
	UPROPERTY(SaveGame) TMap<FString, int32> Elements;

	UPROPERTY(SaveGame) TSet<FString> Spells;
	UPROPERTY(SaveGame) TSet<FString> Techs;
	UPROPERTY(SaveGame) TSet<FString> Styles;
	UPROPERTY(SaveGame) TSet<FString> Records;
	UPROPERTY(SaveGame) TSet<FString> Artifacts;
	UPROPERTY(SaveGame) TSet<FString> Unlocks;

	UPROPERTY(SaveGame) FString Ending;

	UPROPERTY(SaveGame) FString CurrentLoc = TEXT("village");
};
