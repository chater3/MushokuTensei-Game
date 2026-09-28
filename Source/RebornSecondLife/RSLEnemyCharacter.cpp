#include "RSLEnemyCharacter.h"
#include "RSLGameMode.h"
#include "RSLGameInstance.h"

#include "Animation/AnimSequence.h"
#include "Components/CapsuleComponent.h"
#include "Components/SkeletalMeshComponent.h"
#include "Components/StaticMeshComponent.h"
#include "Engine/SkeletalMesh.h"
#include "Engine/StaticMesh.h"
#include "Engine/World.h"
#include "GameFramework/CharacterMovementComponent.h"
#include "Kismet/GameplayStatics.h"
#include "TimerManager.h"

namespace
{
	// Дефы врагов по story_data/battle.py: id, меш из /Game/RS/Characters,
	// суффикс атаки (полное имя анимации = <Mesh>CharacterArmature_<Suffix>), масштаб, HP, урон.
	const FRSLEnemyDef GEnemyDefs[] =
	{
		{ TEXT("dummy"),        TEXT("Warrior"), TEXT("Sword_AttackFast"), 1.00f,   45.f,  6.f },
		{ TEXT("rat"),          TEXT("Rogue"),   TEXT("Dagger_Attack"),    0.40f,   34.f,  4.f },
		{ TEXT("wolf"),         TEXT("Rogue"),   TEXT("Dagger_Attack"),    0.65f,   62.f,  7.f },
		{ TEXT("acidWolf"),     TEXT("Rogue"),   TEXT("Dagger_Attack2"),   0.75f,   80.f,  9.f },
		{ TEXT("fangWolf"),     TEXT("Rogue"),   TEXT("Dagger_Attack2"),   0.85f,  150.f, 10.f },
		{ TEXT("hornet"),       TEXT("Rogue"),   TEXT("Punch"),            0.50f,   58.f,  8.f },
		{ TEXT("boar"),         TEXT("Warrior"), TEXT("Sword_Attack"),     0.75f,  105.f, 10.f },
		{ TEXT("plant"),        TEXT("Cleric"),  TEXT("Staff_Attack"),     0.70f,   90.f,  9.f },
		{ TEXT("goblin"),       TEXT("Rogue"),   TEXT("Dagger_Attack"),    0.70f,   85.f, 10.f },
		{ TEXT("skeleton"),     TEXT("Warrior"), TEXT("Sword_Attack"),     0.95f,  120.f, 11.f },
		{ TEXT("bandit"),       TEXT("Rogue"),   TEXT("Dagger_Attack2"),   0.95f,  110.f, 12.f },
		{ TEXT("scorpion"),     TEXT("Monk"),    TEXT("Attack"),           0.80f,  130.f, 13.f },
		{ TEXT("golem"),        TEXT("Warrior"), TEXT("Sword_Attack"),     1.40f,  240.f, 15.f },
		{ TEXT("iceTroll"),     TEXT("Warrior"), TEXT("Sword_Attack"),     1.50f,  260.f, 17.f },
		{ TEXT("demonMarauder"),TEXT("Monk"),    TEXT("Attack2"),          1.30f,  300.f, 20.f },
		{ TEXT("shadowAdept"),  TEXT("Wizard"),  TEXT("Staff_Attack"),     1.10f,  330.f, 22.f },
		{ TEXT("tourneyAdept"), TEXT("Cleric"),  TEXT("Staff_Attack"),     1.00f,  180.f, 15.f },
		{ TEXT("assassin"),     TEXT("Rogue"),   TEXT("Dagger_Attack2"),   1.00f,  200.f, 19.f },
		{ TEXT("warlord"),      TEXT("Warrior"), TEXT("Sword_AttackFast"), 1.20f,  480.f, 26.f },
		{ TEXT("keeper"),       TEXT("Warrior"), TEXT("Sword_Attack"),     1.50f,  520.f, 23.f },
		{ TEXT("orsted"),       TEXT("Wizard"),  TEXT("Spell2"),           1.30f,  900.f, 35.f },
		{ TEXT("cycleKeeper"),  TEXT("Cleric"),  TEXT("Spell2"),           1.40f, 1100.f, 33.f },
		// Спарринг с Полом (тренировка, исход по канону — Рудеус проигрывает).
		{ TEXT("paul"),         TEXT("Warrior"), TEXT("Sword_AttackFast"), 1.10f,  400.f, 30.f },
	};
}

const FRSLEnemyDef* ARSLEnemyCharacter::FindDef(const FString& Id)
{
	for (const FRSLEnemyDef& Def : GEnemyDefs)
	{
		if (Id.Equals(Def.Id, ESearchCase::IgnoreCase))
		{
			return &Def;
		}
	}
	// Неизвестный id — воин-заглушка.
	return &GEnemyDefs[0];
}

FString ARSLEnemyCharacter::GetDisplayName() const
{
	// Русские имена для боевого HUD (порядок как в GEnemyDefs).
	static const struct { const TCHAR* Id; const TCHAR* Ru; } GNames[] =
	{
		{ TEXT("dummy"),         TEXT("Манекен") },
		{ TEXT("rat"),           TEXT("Крыса") },
		{ TEXT("wolf"),          TEXT("Волк") },
		{ TEXT("acidWolf"),      TEXT("Кислотный волк") },
		{ TEXT("fangWolf"),      TEXT("Клыкастый волк") },
		{ TEXT("hornet"),        TEXT("Оса") },
		{ TEXT("boar"),          TEXT("Кабан") },
		{ TEXT("plant"),         TEXT("Цветок-хищник") },
		{ TEXT("goblin"),        TEXT("Гоблин") },
		{ TEXT("skeleton"),      TEXT("Скелет") },
		{ TEXT("bandit"),        TEXT("Бандит") },
		{ TEXT("scorpion"),      TEXT("Скорпион") },
		{ TEXT("golem"),         TEXT("Голем") },
		{ TEXT("iceTroll"),      TEXT("Ледяной тролль") },
		{ TEXT("demonMarauder"), TEXT("Демон-налётчик") },
		{ TEXT("shadowAdept"),   TEXT("Маг Тени") },
		{ TEXT("tourneyAdept"),  TEXT("Маг турнира") },
		{ TEXT("assassin"),      TEXT("Убийца") },
		{ TEXT("warlord"),       TEXT("Воевода") },
		{ TEXT("keeper"),        TEXT("Хранитель Руин") },
		{ TEXT("orsted"),        TEXT("Орстед") },
		{ TEXT("cycleKeeper"),   TEXT("Хранитель Цикла") },
		{ TEXT("paul"),          TEXT("Пол Грэйрат") },
	};

	for (const auto& N : GNames)
	{
		if (EnemyId.Equals(N.Id, ESearchCase::IgnoreCase))
		{
			return N.Ru;
		}
	}
	return EnemyId;
}

ARSLEnemyCharacter::ARSLEnemyCharacter()
{
	PrimaryActorTick.bCanEverTick = true;
	bDead = false;
	bInAttack = false;
	bStaticVisual = false;

	GetCharacterMovement()->bOrientRotationToMovement = false;
	GetCharacterMovement()->MaxWalkSpeed = 450.f;
	bUseControllerRotationYaw = false; // сами поворачиваемся в Tick на игрока
}

void ARSLEnemyCharacter::BeginPlay()
{
	Super::BeginPlay();
}

void ARSLEnemyCharacter::Init(const FString& InEnemyId)
{
	EnemyId = InEnemyId;
	const FRSLEnemyDef* Def = FindDef(InEnemyId);

	Health = Def->HP;
	MaxHealth = Def->HP;
	Damage = Def->Damage;

	// Внешность: скачанная модель (монстр) либо персонаж-заглушка из /Game/RS/Characters.
	const FRSLEnemyVisual* Vis = FindVisual(InEnemyId);
	if (Vis)
	{
		ApplyVisual(Vis);
	}
	else
	{
		// Меш персонажа-заглушки (Quaternius RPGCharacterPack).
		const FString MeshPath = FString::Printf(TEXT("/Game/RS/Characters/%s.%s"), Def->MeshName, Def->MeshName);
		if (USkeletalMesh* MeshAsset = LoadObject<USkeletalMesh>(nullptr, *MeshPath))
		{
			GetMesh()->SetSkeletalMesh(MeshAsset);
		}
		SetActorScale3D(FVector(Def->Scale));

		// Анимации того же пака: <Mesh>CharacterArmature_<Suffix>.
		const FString AnimBase = FString::Printf(TEXT("%sCharacterArmature_"), Def->MeshName);
		auto LoadAnim = [&AnimBase](const TCHAR* Suffix) -> UAnimSequence*
		{
			const FString Name = AnimBase + Suffix;
			return LoadObject<UAnimSequence>(nullptr, *(TEXT("/Game/RS/Characters/") + Name + TEXT(".") + Name));
		};

		IdleAnim = LoadAnim(TEXT("Idle"));
		RunAnim = LoadAnim(TEXT("Run"));
		DeathAnim = LoadAnim(TEXT("Death"));
		AttackAnim = LoadAnim(Def->AttackSuffix);

		// Плей анимаций без AnimBP: одиночный узел + PlayAnimation.
		GetMesh()->SetAnimationMode(EAnimationMode::AnimationSingleNode);
		CurAnim = nullptr;
		PlayStateAnim(false);
	}
}

// ---------------- Скачанные модели врагов ----------------

void ARSLEnemyCharacter::ApplyVisual(const FRSLEnemyVisual* Vis)
{
	if (!Vis)
	{
		return;
	}

	bStaticVisual = false;
	GetMesh()->SetVisibility(true);

	if (Vis->SkeletalMesh && Vis->SkeletalMesh[0])
	{
		USkeletalMesh* MeshAsset = LoadObject<USkeletalMesh>(nullptr, Vis->SkeletalMesh);
		if (!MeshAsset)
		{
			return;
		}
		GetMesh()->SetSkeletalMesh(MeshAsset);

		// Масштаб под заданный наибольший габарит.
		const FBox Bounds = MeshAsset->GetBounds().GetBox();
		const FVector Extent = Bounds.GetSize();
		const float MaxDim = FMath::Max3(Extent.X, Extent.Y, Extent.Z);
		const float S = (MaxDim > 1.f && Vis->TargetSize > 1.f) ? (Vis->TargetSize / MaxDim) : 1.f;

		// Ось модели: центрируем по горизонтали, низ — на земле (дно капсулы).
		const FVector Center = Bounds.GetCenter();
		const FRotator YawRot(0.f, Vis->Yaw, 0.f);
		const FVector RotatedCenter = YawRot.RotateVector(Center);
		const float HalfH = GetCapsuleComponent()->GetScaledCapsuleHalfHeight();

		GetMesh()->SetRelativeScale3D(FVector(S));
		GetMesh()->SetRelativeRotation(YawRot);
		GetMesh()->SetRelativeLocation(FVector(-RotatedCenter.X * S, -RotatedCenter.Y * S,
			-HalfH - Bounds.Min.Z * S));

		auto LoadA = [](const TCHAR* Path) -> UAnimSequence*
		{
			return (Path && Path[0]) ? LoadObject<UAnimSequence>(nullptr, Path) : nullptr;
		};
		IdleAnim = LoadA(Vis->IdleAnim);
		RunAnim = LoadA(Vis->RunAnim);
		AttackAnim = LoadA(Vis->AttackAnim);
		DeathAnim = LoadA(Vis->DeathAnim);

		GetMesh()->SetAnimationMode(EAnimationMode::AnimationSingleNode);
		CurAnim = nullptr;
		PlayStateAnim(false);
		return;
	}

	if (Vis->StaticParts && Vis->StaticParts[0])
	{
		// Модель без скелета: несколько статических частей, собранных в общем пространстве.
		TArray<FString> Parts;
		FString(Vis->StaticParts).ParseIntoArray(Parts, TEXT(";"), true);

		FBox Bounds(ForceInit);
		int32 Loaded = 0;
		for (const FString& PartPath : Parts)
		{
			UStaticMesh* PartMesh = LoadObject<UStaticMesh>(nullptr, *PartPath);
			if (!PartMesh)
			{
				continue;
			}
			UStaticMeshComponent* Part = NewObject<UStaticMeshComponent>(this, NAME_None, RF_Transient);
			Part->SetStaticMesh(PartMesh);
			Part->SetCollisionEnabled(ECollisionEnabled::NoCollision);
			Part->SetCastShadow(true);
			Part->SetupAttachment(GetRootComponent());
			Part->SetRelativeLocation(FVector::ZeroVector);
			Part->SetRelativeRotation(FRotator::ZeroRotator);
			Part->RegisterComponent();
			StaticParts.Add(Part);
			Bounds += PartMesh->GetBounds().GetBox();
			++Loaded;
		}
		if (!Loaded)
		{
			return;
		}

		bStaticVisual = true;
		IdleAnim = RunAnim = AttackAnim = DeathAnim = nullptr;

		const FVector Extent = Bounds.GetSize();
		const float MaxDim = FMath::Max3(Extent.X, Extent.Y, Extent.Z);
		const float S = (MaxDim > 1.f && Vis->TargetSize > 1.f) ? (Vis->TargetSize / MaxDim) : 1.f;

		const FVector Center = Bounds.GetCenter();
		const FRotator YawRot(0.f, Vis->Yaw, 0.f);
		const FVector RotatedCenter = YawRot.RotateVector(Center);
		const float HalfH = GetCapsuleComponent()->GetScaledCapsuleHalfHeight();

		const FVector RelLoc(-RotatedCenter.X * S, -RotatedCenter.Y * S, -HalfH - Bounds.Min.Z * S);
		for (UStaticMeshComponent* Part : StaticParts)
		{
			Part->SetRelativeScale3D(FVector(S));
			Part->SetRelativeRotation(YawRot);
			Part->SetRelativeLocation(RelLoc);
		}
	}
}

// ---------------- Анимации ----------------

void ARSLEnemyCharacter::PlayStateAnim(bool bRun)
{
	if (bDead || bInAttack)
	{
		return;
	}
	UAnimSequence* Want = bRun ? RunAnim : IdleAnim;
	if (Want && CurAnim != Want)
	{
		CurAnim = Want;
		GetMesh()->PlayAnimation(Want, true);
	}
}

void ARSLEnemyCharacter::PlayAttackAnim()
{
	if (bDead || !AttackAnim)
	{
		return;
	}
	bInAttack = true;
	CurAnim = AttackAnim;
	GetMesh()->PlayAnimation(AttackAnim, false);
	GetWorldTimerManager().SetTimer(AttackRecoverHandle, this,
		&ARSLEnemyCharacter::OnAttackRecover, 1.0f, false);
}

void ARSLEnemyCharacter::PlayDeathAnim()
{
	if (DeathAnim)
	{
		CurAnim = DeathAnim;
		GetMesh()->PlayAnimation(DeathAnim, false);
	}
}

// ---------------- Жизненный цикл ----------------

void ARSLEnemyCharacter::Tick(float DeltaSeconds)
{
	Super::Tick(DeltaSeconds);

	if (bDead || EnemyId.IsEmpty())
	{
		return;
	}

	APawn* Target = UGameplayStatics::GetPlayerPawn(this, 0);
	if (!Target)
	{
		return;
	}

	FVector To = Target->GetActorLocation() - GetActorLocation();
	To.Z = 0.f;
	const float Dist = To.Size();

	// Поворот лицом к игроку.
	if (Dist > 1.f)
	{
		FRotator Want = To.Rotation();
		Want.Pitch = 0.f;
		Want.Roll = 0.f;
		SetActorRotation(FMath::RInterpTo(GetActorRotation(), Want, DeltaSeconds, 5.f));
	}

	const float AttackRange = 160.f;
	if (Dist > AttackRange)
	{
		if (!To.IsNearlyZero())
		{
			AddMovementInput(To.GetSafeNormal());
		}
		PlayStateAnim(GetVelocity().Size2D() > 50.f);
	}
	else
	{
		PlayStateAnim(false);

		const float Now = GetWorld()->GetTimeSeconds();
		if (!bInAttack && Now - LastAttackTime > 1.5f)
		{
			LastAttackTime = Now;
			PlayAttackAnim();
			UGameplayStatics::ApplyDamage(Target, Damage, nullptr, this, nullptr);
		}
	}
}

float ARSLEnemyCharacter::TakeDamage(float DamageAmount, FDamageEvent const& DamageEvent,
	AController* EventInstigator, AActor* DamageCauser)
{
	if (bDead)
	{
		return 0.f;
	}

	URSLGameInstance* GI = Cast<URSLGameInstance>(GetGameInstance());
	Health -= DamageAmount;
	if (GI)
	{
		GI->PlaySFX(TEXT("hit"), 0.5f);
	}
	if (Health <= 0.f)
	{
		// Орстед (канон, т.6): Рудеус не может его победить — бой идёт до падения игрока.
		if (EnemyId == TEXT("orsted"))
		{
			Health = 1.f;
		}
		// Тренировка с каноничным исходом 'lose': противник неубиваем —
		// бой завершится по таймауту или когда HP игрока достигнет 1.
		else if (GI && GI->IsTrainingUnkillable())
		{
			Health = 1.f;
		}
		else
		{
			Die();
		}
	}
	return DamageAmount;
}

void ARSLEnemyCharacter::Die()
{
	if (bDead)
	{
		return;
	}
	bDead = true;
	bInAttack = false;

	GetCapsuleComponent()->SetCollisionEnabled(ECollisionEnabled::NoCollision);
	GetCharacterMovement()->DisableMovement();
	PlayDeathAnim();

	// Счётчик убийств для квестов гильдии.
	if (URSLGameInstance* GI = Cast<URSLGameInstance>(GetGameInstance()))
	{
		GI->OnEnemyKilled(EnemyId);
	}

	// Сообщаем режиму игры; если это был последний враг — покажется win_text.
	if (ARSLGameMode* GM = GetWorld()->GetAuthGameMode<ARSLGameMode>())
	{
		GM->OnEnemyDead(this);
	}

	SetLifeSpan(4.f); // труп исчезает через 4 с
}

// Таблица скачанных моделей врагов + FindVisual (генерируется скриптом).
#include "RSLEnemyVisuals.inl"
