#include "RSLPlayerCharacter.h"
#include "RSLGameInstance.h"
#include "RSLProjectile.h"
#include "RSLEnemyCharacter.h"

#include "Camera/CameraComponent.h"
#include "Components/CapsuleComponent.h"
#include "Engine/LocalPlayer.h"
#include "Engine/World.h"
#include "EnhancedInputComponent.h"
#include "EnhancedInputSubsystems.h"
#include "GameFramework/CharacterMovementComponent.h"
#include "GameFramework/PlayerController.h"
#include "InputAction.h"
#include "InputMappingContext.h"
#include "InputModifiers.h"
#include "Kismet/GameplayStatics.h"
#include "Math/RotationMatrix.h"

ARSLPlayerCharacter::ARSLPlayerCharacter()
{
	PrimaryActorTick.bCanEverTick = true;

	// Вид от первого лица: корпус следует за взглядом по yaw.
	bUseControllerRotationYaw = true;
	GetCharacterMovement()->bOrientRotationToMovement = false;
	GetCharacterMovement()->MaxWalkSpeed = 500.f;

	FPSCamera = CreateDefaultSubobject<UCameraComponent>(TEXT("FPSCamera"));
	FPSCamera->SetupAttachment(GetCapsuleComponent());
	FPSCamera->SetRelativeLocation(FVector(0.f, 0.f, 64.f));
	FPSCamera->bUsePawnControlRotation = true;
}

// ---------------- Ввод (Enhanced Input создаётся в C++) ----------------

void ARSLPlayerCharacter::BuildInput()
{
	if (IMC)
	{
		return;
	}

	auto MakeAction = [this](EInputActionValueType Type) -> UInputAction*
	{
		UInputAction* A = NewObject<UInputAction>(this, NAME_None, RF_Transient);
		A->ValueType = Type;
		return A;
	};

	IA_MoveF = MakeAction(EInputActionValueType::Boolean);
	IA_MoveB = MakeAction(EInputActionValueType::Boolean);
	IA_MoveL = MakeAction(EInputActionValueType::Boolean);
	IA_MoveR = MakeAction(EInputActionValueType::Boolean);
	IA_Look = MakeAction(EInputActionValueType::Axis2D);
	IA_Jump = MakeAction(EInputActionValueType::Boolean);
	IA_Advance = MakeAction(EInputActionValueType::Boolean);
	IA_Attack = MakeAction(EInputActionValueType::Boolean);
	IA_Cast = MakeAction(EInputActionValueType::Boolean);
	IA_C1 = MakeAction(EInputActionValueType::Boolean);
	IA_C2 = MakeAction(EInputActionValueType::Boolean);
	IA_C3 = MakeAction(EInputActionValueType::Boolean);
	IA_C4 = MakeAction(EInputActionValueType::Boolean);
	IA_C5 = MakeAction(EInputActionValueType::Boolean);
	IA_Voice = MakeAction(EInputActionValueType::Boolean);
	IA_Map = MakeAction(EInputActionValueType::Boolean);
	IA_Hero = MakeAction(EInputActionValueType::Boolean);
	IA_Magic = MakeAction(EInputActionValueType::Boolean);

	IMC = NewObject<UInputMappingContext>(this, NAME_None, RF_Transient);

	IMC->MapKey(IA_MoveF, EKeys::W);
	IMC->MapKey(IA_MoveB, EKeys::S);
	IMC->MapKey(IA_MoveL, EKeys::A);
	IMC->MapKey(IA_MoveR, EKeys::D);
	IMC->MapKey(IA_Look, EKeys::MouseX);                 // ось X мыши
	{
		// ось Y мыши: свайп компонентов X→Y (YXZ), как в стандартном FPS-маппинге
		FEnhancedActionKeyMapping& LookY = IMC->MapKey(IA_Look, EKeys::MouseY);
		UInputModifierSwizzleAxis* Swizzle = NewObject<UInputModifierSwizzleAxis>(IMC);
		Swizzle->Order = EInputAxisSwizzle::YXZ;
		LookY.Modifiers.Add(Swizzle);
	}
	IMC->MapKey(IA_Jump, EKeys::SpaceBar);   // Space: прыжок, а во время текста — продолжение
	IMC->MapKey(IA_Advance, EKeys::Enter);   // Enter: продолжить реплику / подтвердить бой
	IMC->MapKey(IA_Attack, EKeys::LeftMouseButton);
	IMC->MapKey(IA_Cast, EKeys::RightMouseButton);
	IMC->MapKey(IA_C1, EKeys::One);
	IMC->MapKey(IA_C2, EKeys::Two);
	IMC->MapKey(IA_C3, EKeys::Three);
	IMC->MapKey(IA_C4, EKeys::Four);
	IMC->MapKey(IA_C5, EKeys::Five);
	IMC->MapKey(IA_Voice, EKeys::V);   // V: озвучка вкл/выкл (как в pygame)
	IMC->MapKey(IA_Map, EKeys::M);     // M: карта мира (как в pygame)
	IMC->MapKey(IA_Hero, EKeys::G);    // G: панель героя (характеристики)
	IMC->MapKey(IA_Magic, EKeys::K);   // K: панель магии (заклинания/стихии)
}

void ARSLPlayerCharacter::EnsureMappingContext()
{
	if (!IMC)
	{
		BuildInput();
	}
	if (APlayerController* PC = Cast<APlayerController>(GetController()))
	{
		if (ULocalPlayer* LP = PC->GetLocalPlayer())
		{
			if (UEnhancedInputLocalPlayerSubsystem* Sub = LP->GetSubsystem<UEnhancedInputLocalPlayerSubsystem>())
			{
				Sub->AddMappingContext(IMC, 0);
			}
		}
	}
}

void ARSLPlayerCharacter::SetupPlayerInputComponent(UInputComponent* PlayerInputComponent)
{
	Super::SetupPlayerInputComponent(PlayerInputComponent);

	BuildInput();
	EnsureMappingContext();

	UEnhancedInputComponent* EIC = Cast<UEnhancedInputComponent>(PlayerInputComponent);
	if (!EIC)
	{
		return;
	}

	EIC->BindAction(IA_MoveF, ETriggerEvent::Started, this, &ARSLPlayerCharacter::MoveFPressed);
	EIC->BindAction(IA_MoveF, ETriggerEvent::Completed, this, &ARSLPlayerCharacter::MoveFReleased);
	EIC->BindAction(IA_MoveB, ETriggerEvent::Started, this, &ARSLPlayerCharacter::MoveBPressed);
	EIC->BindAction(IA_MoveB, ETriggerEvent::Completed, this, &ARSLPlayerCharacter::MoveBReleased);
	EIC->BindAction(IA_MoveL, ETriggerEvent::Started, this, &ARSLPlayerCharacter::MoveLPressed);
	EIC->BindAction(IA_MoveL, ETriggerEvent::Completed, this, &ARSLPlayerCharacter::MoveLReleased);
	EIC->BindAction(IA_MoveR, ETriggerEvent::Started, this, &ARSLPlayerCharacter::MoveRPressed);
	EIC->BindAction(IA_MoveR, ETriggerEvent::Completed, this, &ARSLPlayerCharacter::MoveRReleased);

	EIC->BindAction(IA_Look, ETriggerEvent::Triggered, this, &ARSLPlayerCharacter::Look);
	EIC->BindAction(IA_Jump, ETriggerEvent::Started, this, &ARSLPlayerCharacter::JumpPressed);
	EIC->BindAction(IA_Jump, ETriggerEvent::Completed, this, &ARSLPlayerCharacter::JumpReleased);
	EIC->BindAction(IA_Advance, ETriggerEvent::Started, this, &ARSLPlayerCharacter::AdvancePressed);
	EIC->BindAction(IA_Attack, ETriggerEvent::Started, this, &ARSLPlayerCharacter::AttackPressed);
	EIC->BindAction(IA_Cast, ETriggerEvent::Started, this, &ARSLPlayerCharacter::CastPressed);

	EIC->BindAction(IA_C1, ETriggerEvent::Started, this, &ARSLPlayerCharacter::Choice1);
	EIC->BindAction(IA_C2, ETriggerEvent::Started, this, &ARSLPlayerCharacter::Choice2);
	EIC->BindAction(IA_C3, ETriggerEvent::Started, this, &ARSLPlayerCharacter::Choice3);
	EIC->BindAction(IA_C4, ETriggerEvent::Started, this, &ARSLPlayerCharacter::Choice4);
	EIC->BindAction(IA_C5, ETriggerEvent::Started, this, &ARSLPlayerCharacter::Choice5);
	EIC->BindAction(IA_Voice, ETriggerEvent::Started, this, &ARSLPlayerCharacter::VoiceToggled);
	EIC->BindAction(IA_Map, ETriggerEvent::Started, this, &ARSLPlayerCharacter::MapToggled);
	EIC->BindAction(IA_Hero, ETriggerEvent::Started, this, &ARSLPlayerCharacter::HeroToggled);
	EIC->BindAction(IA_Magic, ETriggerEvent::Started, this, &ARSLPlayerCharacter::MagicToggled);
}

// ---------------- Каждый кадр ----------------

void ARSLPlayerCharacter::Tick(float DeltaSeconds)
{
	Super::Tick(DeltaSeconds);

	URSLGameInstance* GI = Cast<URSLGameInstance>(GetGameInstance());
	const bool bFrozen = GI && GI->WantsTextInput();

	// Во время чтения реплик персонаж стоит (как в текстовых сценах оригинала).
	if (!bFrozen && (bF || bB || bL || bR))
	{
		const FRotator YawRot(0.f, GetControlRotation().Yaw, 0.f);
		const FRotationMatrix YawMat(YawRot);
		const FVector Fwd = YawMat.GetUnitAxis(EAxis::X);
		const FVector Right = YawMat.GetUnitAxis(EAxis::Y);

		if (bF) { AddMovementInput(Fwd, 1.f); }
		if (bB) { AddMovementInput(Fwd, -1.f); }
		if (bR) { AddMovementInput(Right, 1.f); }
		if (bL) { AddMovementInput(Right, -1.f); }
	}

	// Синхронизация HUD при любых изменениях MP (в т.ч. внешних).
	if (GI)
	{
		const int32 MPNow = (int32)GI->MP;
		if (MPNow != LastShownMP)
		{
			LastShownMP = MPNow;
			GI->OnStats.Broadcast();
		}
	}
}

// ---------------- Реакция на ввод ----------------

void ARSLPlayerCharacter::Look(const FInputActionValue& Value)
{
	const FVector2D Axis = Value.Get<FVector2D>();
	AddControllerYawInput(Axis.X * LookSensitivity);
	AddControllerPitchInput(-Axis.Y * LookSensitivity);
}

void ARSLPlayerCharacter::JumpPressed()
{
	if (URSLGameInstance* GI = Cast<URSLGameInstance>(GetGameInstance()))
	{
		if (GI->WantsTextInput())
		{
			GI->Advance(); // Space во время текста — продолжить
			return;
		}
	}
	Jump();
}

void ARSLPlayerCharacter::JumpReleased()
{
	StopJumping();
}

void ARSLPlayerCharacter::AdvancePressed()
{
	if (URSLGameInstance* GI = Cast<URSLGameInstance>(GetGameInstance()))
	{
		if (GI->WantsTextInput())
		{
			GI->Advance();
		}
	}
}

void ARSLPlayerCharacter::AttackPressed()
{
	URSLGameInstance* GI = Cast<URSLGameInstance>(GetGameInstance());
	if (GI)
	{
		// ЛКМ продолжает обычную реплику. Во время выбора клик обрабатывает кнопка виджета.
		if (GI->State == ESLStoryState::AwaitingLine
			|| GI->State == ESLStoryState::BattleIntro
			|| GI->State == ESLStoryState::BattleOutro)
		{
			GI->Advance();
			return;
		}
		if (GI->WantsTextInput())
		{
			return;
		}
	}
	DoMelee();
}

void ARSLPlayerCharacter::CastPressed()
{
	URSLGameInstance* GI = Cast<URSLGameInstance>(GetGameInstance());
	if (GI && GI->WantsTextInput())
	{
		return;
	}
	DoCast();
}

void ARSLPlayerCharacter::MakeChoice(int32 Index)
{
	if (URSLGameInstance* GI = Cast<URSLGameInstance>(GetGameInstance()))
	{
		GI->SelectChoice(Index); // без состояния AwaitingChoice игнорируется
	}
}

void ARSLPlayerCharacter::VoiceToggled()
{
	if (URSLGameInstance* GI = Cast<URSLGameInstance>(GetGameInstance()))
	{
		GI->ToggleVoice();
	}
}

void ARSLPlayerCharacter::MapToggled()
{
	if (URSLGameInstance* GI = Cast<URSLGameInstance>(GetGameInstance()))
	{
		GI->ToggleMap();
	}
}

void ARSLPlayerCharacter::HeroToggled()
{
	if (URSLGameInstance* GI = Cast<URSLGameInstance>(GetGameInstance()))
	{
		GI->ToggleMenu(1); // «Герой»
	}
}

void ARSLPlayerCharacter::MagicToggled()
{
	if (URSLGameInstance* GI = Cast<URSLGameInstance>(GetGameInstance()))
	{
		GI->ToggleMenu(2); // «Магия»
	}
}

// ---------------- Бой ----------------

void ARSLPlayerCharacter::DoMelee()
{
	const float Now = GetWorld()->GetTimeSeconds();
	if (Now - LastMeleeTime < 0.45f)
	{
		return;
	}
	LastMeleeTime = Now;

	const FVector CamLoc = FPSCamera->GetComponentLocation();
	const FVector Fwd = FPSCamera->GetForwardVector();

	URSLGameInstance* GI = Cast<URSLGameInstance>(GetGameInstance());
	const float BaseDamage = 15.f + (GI ? GI->Stats.FindRef(TEXT("str")) * 1.5f : 0.f);

	TArray<AActor*> Enemies;
	UGameplayStatics::GetAllActorsOfClass(this, ARSLEnemyCharacter::StaticClass(), Enemies);

	for (AActor* Actor : Enemies)
	{
		ARSLEnemyCharacter* Enemy = Cast<ARSLEnemyCharacter>(Actor);
		if (!Enemy)
		{
			continue;
		}
		const FVector To = Enemy->GetActorLocation() - CamLoc;
		const float Dist = To.Size();
		if (Dist > 190.f)
		{
			continue;
		}
		const float Dot = Dist > KINDA_SMALL_NUMBER
			? FVector::DotProduct(To / Dist, Fwd)
			: 1.f;
		if (Dot > 0.55f) // бьём примерно в лоб прицела
		{
			UGameplayStatics::ApplyDamage(Enemy, BaseDamage, GetController(), this, nullptr);
		}
	}
}

void ARSLPlayerCharacter::DoCast()
{
	URSLGameInstance* GI = Cast<URSLGameInstance>(GetGameInstance());
	if (!GI)
	{
		return;
	}

	const float Cost = 8.f;
	if (GI->MP < Cost)
	{
		return; // недостаточно маны
	}
	GI->MP -= Cost;

	const FVector SpawnLoc = FPSCamera->GetComponentLocation() + FPSCamera->GetForwardVector() * 100.f;
	const FRotator SpawnRot = FPSCamera->GetComponentRotation();

	FActorSpawnParameters Params;
	Params.Owner = this;
	Params.Instigator = this;
	Params.SpawnCollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::AlwaysSpawn;

	ARSLProjectile* Projectile = GetWorld()->SpawnActor<ARSLProjectile>(SpawnLoc, SpawnRot, Params);
	if (Projectile)
	{
		Projectile->Damage = 20.f + GI->Stats.FindRef(TEXT("int")) * 2.f;
	}

	GI->OnStats.Broadcast();
}

float ARSLPlayerCharacter::TakeDamage(float DamageAmount, FDamageEvent const& DamageEvent,
	AController* EventInstigator, AActor* DamageCauser)
{
	if (URSLGameInstance* GI = Cast<URSLGameInstance>(GetGameInstance()))
	{
		GI->OnPlayerDamaged(DamageAmount);
	}
	return DamageAmount;
}
