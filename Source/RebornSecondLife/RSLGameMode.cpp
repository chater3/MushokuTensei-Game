#include "RSLGameMode.h"
#include "RSLGameInstance.h"
#include "RSLPlayerCharacter.h"
#include "RSLStoryWidget.h"
#include "RSLEnemyCharacter.h"

#include "Blueprint/UserWidget.h"
#include "Camera/PlayerCameraManager.h"
#include "Camera/CameraComponent.h"
#include "Components/DirectionalLightComponent.h"
#include "Engine/DirectionalLight.h"
#include "Engine/ExponentialHeightFog.h"
#include "Engine/Engine.h"
#include "Engine/GameViewportClient.h"
#include "Engine/SkyLight.h"
#include "Engine/StaticMeshActor.h"
#include "Engine/PostProcessVolume.h"
#include "Engine/World.h"
#include "EngineUtils.h"
#include "GameFramework/CharacterMovementComponent.h"
#include "GameFramework/PlayerController.h"
#include "GameFramework/PlayerStart.h"
#include "HAL/IConsoleManager.h"
#include "ImageCore.h"
#include "ImageUtils.h"
#include "Kismet/GameplayStatics.h"
#include "Math/RotationMatrix.h"
#include "Components/ExponentialHeightFogComponent.h"
#include "Components/SceneCaptureComponent2D.h"
#include "Components/SkyLightComponent.h"
#include "Engine/TextureRenderTarget2D.h"
#include "RenderingThread.h"
#include "Slate/SceneViewport.h"
#include "TimerManager.h"
#include "TextureResource.h"
#include "UnrealClient.h"

ARSLGameMode::ARSLGameMode()
{
	// Игровым персонажом по умолчанию становится FPS-персонаж (Рудеус), вид от первого лица.
	DefaultPawnClass = ARSLPlayerCharacter::StaticClass();
}

void ARSLGameMode::BeginPlay()
{
	Super::BeginPlay();


	URSLGameInstance* GI = Cast<URSLGameInstance>(GetGameInstance());

	// Виджет сюжета создаётся до запуска истории, чтобы не пропустить первую строку.
	if (APlayerController* PC = UGameplayStatics::GetPlayerController(this, 0))
	{
		StoryWidget = CreateWidget<URSLStoryWidget>(PC, URSLStoryWidget::StaticClass());
		if (StoryWidget)
		{
			StoryWidget->AddToViewport();
		}
	}

	if (GI)
	{
		GI->OnBattleGo.AddDynamic(this, &ARSLGameMode::HandleBattleGo);
		GI->OnBattleLose.AddDynamic(this, &ARSLGameMode::HandleBattleLose);
		GI->StartStory();
	}
}

void ARSLGameMode::HandleBattleGo(const TArray<FString>& EnemyIds)
{
	SpawnBattle(EnemyIds);
}

void ARSLGameMode::HandleBattleLose()
{
	// Проигрыш сюжетного боя: убираем всех врагов, текст lose_text покажет виджет.
	for (const TWeakObjectPtr<ARSLEnemyCharacter>& Ptr : AliveEnemies)
	{
		if (Ptr.IsValid())
		{
			Ptr->Destroy();
		}
	}
	AliveEnemies.Reset();

	if (StoryWidget)
	{
		StoryWidget->SetInBattle(false);
	}
}

void ARSLGameMode::SpawnBattle(const TArray<FString>& EnemyIds)
{
	if (StoryWidget)
	{
		StoryWidget->SetInBattle(true);
	}

	APlayerController* PC = UGameplayStatics::GetPlayerController(this, 0);
	APawn* PlayerPawn = PC ? PC->GetPawn() : nullptr;
	const FVector Origin = PlayerPawn ? PlayerPawn->GetActorLocation() : FVector(0.f, 0.f, 100.f);
	const float BaseYaw = PlayerPawn ? PlayerPawn->GetActorRotation().Yaw : 0.f;
	const FRotator YawRot(0.f, BaseYaw, 0.f);
	const FRotationMatrix YawMat(YawRot);

	const int32 N = FMath::Max(1, EnemyIds.Num());
	const int32 Cols = FMath::Clamp(N, 1, 4);

	FActorSpawnParameters Params;
	Params.SpawnCollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::AdjustIfPossibleButAlwaysSpawn;

	for (int32 i = 0; i < N; ++i)
	{
		// Квадрат/ряды перед игроком: первая линия в 600 ед., далее шаг вглубь.
		// Z = 0: центр капсулы врага = центру капсулы игрока -> ступни на земле (не парит).
		const int32 Row = i / Cols;
		const int32 Col = i % Cols;
		const int32 ColsThisRow = FMath::Min(N - Row * Cols, Cols);

		const float Forward = 600.f + Row * 350.f;
		const float Side = (Col - (ColsThisRow - 1) * 0.5f) * 320.f;
		const FVector Local(Forward, Side, 0.f);
		const FVector SpawnLoc = Origin + YawMat.TransformVector(Local);

		ARSLEnemyCharacter* Enemy = GetWorld()->SpawnActor<ARSLEnemyCharacter>(
			SpawnLoc, FRotator(0.f, BaseYaw + 180.f, 0.f), Params);
		if (Enemy)
		{
			Enemy->Init(EnemyIds[i]);
			AliveEnemies.Add(Enemy);
		}
	}
}

void ARSLGameMode::OnEnemyDead(ARSLEnemyCharacter* Enemy)
{
	AliveEnemies.RemoveAll([Enemy](const TWeakObjectPtr<ARSLEnemyCharacter>& Ptr)
	{
		return !Ptr.IsValid() || Ptr.Get() == Enemy;
	});

	if (AliveEnemies.Num() == 0)
	{
		if (StoryWidget)
		{
			StoryWidget->SetInBattle(false);
		}
		if (URSLGameInstance* GI = Cast<URSLGameInstance>(GetGameInstance()))
		{
			// Покажет win_text и переведёт состояние в BattleOutro.
			GI->NotifyAllEnemiesDead();
		}
	}
}
