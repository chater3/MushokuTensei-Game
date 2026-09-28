#pragma once

#include "CoreMinimal.h"
#include "GameFramework/GameModeBase.h"
#include "RSLGameMode.generated.h"

class URSLStoryWidget;
class ARSLEnemyCharacter;

/**
 * Режим игры REBORN: SECOND LIFE.
 * Создаёт виджет сюжета, запускает движок сюжета, спавнит врагов по боям
 * из story_data и отслеживает их смерть.
 */
UCLASS()
class ARSLGameMode : public AGameModeBase
{
	GENERATED_BODY()

public:
	ARSLGameMode();

	virtual void BeginPlay() override;

	/** Все враги боя убиты — сообщаем движку сюжета. */
	void OnEnemyDead(ARSLEnemyCharacter* Enemy);

	/** Живые враги текущего боя — для боевого HUD виджета. */
	const TArray<TWeakObjectPtr<ARSLEnemyCharacter>>& GetAliveEnemies() const { return AliveEnemies; }

protected:
	UFUNCTION()
	void HandleBattleGo(const TArray<FString>& EnemyIds);

	UFUNCTION()
	void HandleBattleLose();

	void SpawnBattle(const TArray<FString>& EnemyIds);

	UPROPERTY() URSLStoryWidget* StoryWidget = nullptr;

	UPROPERTY() TArray<TWeakObjectPtr<ARSLEnemyCharacter>> AliveEnemies;
};
