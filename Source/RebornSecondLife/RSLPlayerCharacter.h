#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Character.h"
#include "InputActionValue.h"
#include "RSLPlayerCharacter.generated.h"

class UCameraComponent;
class UInputMappingContext;
class UInputAction;

/** Герой (Рудеус) — вид от первого лица: движение, взгляд, бой, магия, ввод сюжета. */
UCLASS()
class ARSLPlayerCharacter : public ACharacter
{
	GENERATED_BODY()

public:
	ARSLPlayerCharacter();

	virtual void Tick(float DeltaSeconds) override;
	virtual void SetupPlayerInputComponent(UInputComponent* PlayerInputComponent) override;
	virtual float TakeDamage(float Damage, FDamageEvent const& DamageEvent,
		AController* EventInstigator, AActor* DamageCauser) override;

protected:
	UPROPERTY(VisibleAnywhere)
	UCameraComponent* FPSCamera;

	// --- ввод (Enhanced Input, создаётся в C++) ---
	UInputMappingContext* IMC = nullptr;
	UInputAction* IA_MoveF = nullptr;
	UInputAction* IA_MoveB = nullptr;
	UInputAction* IA_MoveL = nullptr;
	UInputAction* IA_MoveR = nullptr;
	UInputAction* IA_Look = nullptr;
	UInputAction* IA_Jump = nullptr;
	UInputAction* IA_Advance = nullptr;
	UInputAction* IA_Attack = nullptr;
	UInputAction* IA_Cast = nullptr;
	UInputAction* IA_C1 = nullptr;
	UInputAction* IA_C2 = nullptr;
	UInputAction* IA_C3 = nullptr;
	UInputAction* IA_C4 = nullptr;
	UInputAction* IA_C5 = nullptr;
	UInputAction* IA_Voice = nullptr;
	UInputAction* IA_Map = nullptr;
	UInputAction* IA_Hero = nullptr;
	UInputAction* IA_Magic = nullptr;

	void BuildInput();
	void EnsureMappingContext();

	void MoveFPressed() { bF = true; }
	void MoveFReleased() { bF = false; }
	void MoveBPressed() { bB = true; }
	void MoveBReleased() { bB = false; }
	void MoveLPressed() { bL = true; }
	void MoveLReleased() { bL = false; }
	void MoveRPressed() { bR = true; }
	void MoveRReleased() { bR = false; }

	void Look(const FInputActionValue& Value);
	void JumpPressed();
	void JumpReleased();
	void AdvancePressed();
	void AttackPressed();
	void CastPressed();
	void Choice1() { MakeChoice(0); }
	void Choice2() { MakeChoice(1); }
	void Choice3() { MakeChoice(2); }
	void Choice4() { MakeChoice(3); }
	void Choice5() { MakeChoice(4); }
	void MakeChoice(int32 Index);
	void VoiceToggled();
	void MapToggled();
	/** G: панель «Герой» — характеристики, очки, статы. */
	void HeroToggled();
	/** K: панель «Магия» — заклинания, стихии, тренировка. */
	void MagicToggled();

	void DoMelee();
	void DoCast();

	bool bF = false, bB = false, bL = false, bR = false;
	float LookSensitivity = 0.07f;
	float LastMeleeTime = -10.f;
	int32 LastShownMP = -1;
};
