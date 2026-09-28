#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "RSLProjectile.generated.h"

class USphereComponent;
class UStaticMeshComponent;
class UProjectileMovementComponent;

/** Снаряд магии (сфера + ProjectileMovement), наносит урон при касании. */
UCLASS()
class ARSLProjectile : public AActor
{
	GENERATED_BODY()

public:
	ARSLProjectile();

	virtual void BeginPlay() override;

	UPROPERTY() float Damage = 25.f;

protected:
	UFUNCTION()
	void HandleOverlap(UPrimitiveComponent* OverlappedComp, AActor* OtherActor,
		UPrimitiveComponent* OtherComp, int32 OtherBodyIndex,
		bool bFromSweep, const FHitResult& SweepResult);

	UFUNCTION()
	void HandleHit(UPrimitiveComponent* HitComponent, AActor* OtherActor,
		UPrimitiveComponent* OtherComp, FVector NormalImpulse, const FHitResult& Hit);

	UPROPERTY(VisibleAnywhere) USphereComponent* Collision;
	UPROPERTY(VisibleAnywhere) UStaticMeshComponent* Mesh;
	UPROPERTY(VisibleAnywhere) UProjectileMovementComponent* Movement;
};
