#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Character.h"
#include "RSLEnemyCharacter.generated.h"

class UAnimSequence;
class UStaticMeshComponent;

/** Тип врага: id из story_data, меш из /Game/RS/Characters, суффикс атаки, масштаб, HP, урон. */
struct FRSLEnemyDef
{
	const TCHAR* Id;
	const TCHAR* MeshName;
	const TCHAR* AttackSuffix;
	float Scale;
	float HP;
	float Damage;
};

/**
 * Оверрайд внешности врага скачанным моделями (PolyPizza/UltimateMonsters/Bestiary).
 * либо SkeletalMesh + свои анимации (полные пути к объектам),
 * либо StaticMesh-части (разделённые ';'), если модели без скелета.
 */
struct FRSLEnemyVisual
{
	const TCHAR* Id;
	/** Полный путь к USkeletalMesh, либо nullptr. */
	const TCHAR* SkeletalMesh;
	/** Полные пути к UAnimSequence (SkeletalMesh-вариант). */
	const TCHAR* IdleAnim;
	const TCHAR* RunAnim;
	const TCHAR* AttackAnim;
	const TCHAR* DeathAnim;
	/** ';'-список полных путей к UStaticMesh, либо nullptr. */
	const TCHAR* StaticParts;
	/** Наибольший габарит модели в см: меш масштабируется под него. */
	float TargetSize;
	/** Доворот модели (yaw), чтобы смотрела вперёд (+X). */
	float Yaw;
};

/** Враг: ближний бой с простым преследованием, без Behavior Tree и навмеша. */
UCLASS()
class ARSLEnemyCharacter : public ACharacter
{
	GENERATED_BODY()

public:
	ARSLEnemyCharacter();

	virtual void BeginPlay() override;
	virtual void Tick(float DeltaSeconds) override;
	virtual float TakeDamage(float Damage, FDamageEvent const& DamageEvent,
		AController* EventInstigator, AActor* DamageCauser) override;

	/** Загружает меш/анимации и статы по id врага из story_data (rat, wolf, ...). */
	void Init(const FString& InEnemyId);

	UPROPERTY() FString EnemyId;

	// --- боевой HUD ---
	float GetHealth() const { return Health; }
	float GetMaxHealth() const { return MaxHealth; }
	bool IsDead() const { return bDead != 0; }
	/** Русское имя для боевого HUD: «Гоблин», «Ледяной тролль», ... */
	FString GetDisplayName() const;

protected:
	void PlayStateAnim(bool bRun);
	void PlayAttackAnim();
	void PlayDeathAnim();
	void Die();

	/** Применяет скачанный меш (skel+анимации либо static-части) вместо заглушки. */
	void ApplyVisual(const struct FRSLEnemyVisual* Vis);

	UPROPERTY() UAnimSequence* IdleAnim = nullptr;
	UPROPERTY() UAnimSequence* RunAnim = nullptr;
	UPROPERTY() UAnimSequence* AttackAnim = nullptr;
	UPROPERTY() UAnimSequence* DeathAnim = nullptr;
	UPROPERTY() UAnimSequence* CurAnim = nullptr;

	/** Части статического меша (для монстров без скелета). */
	UPROPERTY() TArray<UStaticMeshComponent*> StaticParts;

	/** true, если внешность — статическая модель без анимаций. */
	uint8 bStaticVisual : 1;

	float Health = 30.f;
	/** Стартовое HP врага — знаменатель полосы в боевом HUD. */
	float MaxHealth = 30.f;
	float Damage = 8.f;
	float LastAttackTime = -10.f;

	uint8 bDead : 1;
	uint8 bInAttack : 1;

	FTimerHandle AttackRecoverHandle;

	void OnAttackRecover() { bInAttack = false; }

	static const FRSLEnemyDef* FindDef(const FString& Id);
	static const FRSLEnemyVisual* FindVisual(const FString& Id);
};
