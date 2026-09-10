// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "RLTypes.h"
#include "RLProjectile.generated.h"

class USphereComponent;
class UStaticMeshComponent;
class UProjectileMovementComponent;

UCLASS()
class ROGUELIKETOPVIEW_API ARLProjectile : public AActor
{
	GENERATED_BODY()

public:
	ARLProjectile();

protected:
	virtual void BeginPlay() override;

public:
	// 충돌 판정용 콜리전 (오버랩 감지)
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Projectile")
	USphereComponent* CollisionComp;

	// 임시 시각화용 (나중에 메시/나이아가라로 교체)
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Projectile")
	UStaticMeshComponent* MeshComp;

	// 실제 이동을 담당하는 컴포넌트
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Projectile")
	UProjectileMovementComponent* MovementComp;

	// 스킬 데이터에서 넘겨받는 데미지 정보
	UPROPERTY(BlueprintReadWrite, Category = "Projectile")
	FRLDamageData DamageData;

	// 최대 생존 시간 (초). 아무것도 안 맞아도 이 시간 지나면 자동 소멸
	UPROPERTY(EditDefaultsOnly, Category = "Projectile")
	float MaxLifeTime = 3.0f;

	// 몇 명 맞추면 사라질지 (관통 여부). 기본 1 = 맞으면 바로 소멸
	UPROPERTY(EditDefaultsOnly, Category = "Projectile")
	int32 MaxPierceCount = 1;

private:
	int32 CurrentHitCount = 0;

	UFUNCTION()
	void OnHit(UPrimitiveComponent* OverlappedComp, AActor* OtherActor,
		UPrimitiveComponent* OtherComp, int32 OtherBodyIndex, bool bFromSweep,
		const FHitResult& SweepResult);
};