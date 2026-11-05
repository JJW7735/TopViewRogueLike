// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "RLTypes.h"
#include "RLProjectile.generated.h"

class USphereComponent;
class UStaticMeshComponent;
class UProjectileMovementComponent;
class URLSkillData;

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

	// 이 투사체를 발사한 원본 스킬 데이터 (피격 시 HitVFX / HitSound / 타격감 연동용)
	UPROPERTY(BlueprintReadWrite, Category = "Projectile")
	TObjectPtr<const URLSkillData> SourceSkill = nullptr;

	// 최대 생존 시간 (초). 아무것도 안 맞아도 이 시간 지나면 자동 소멸
	UPROPERTY(EditDefaultsOnly, Category = "Projectile")
	float MaxLifeTime = 3.0f;

	// 몇 명 맞추면 사라질지 (관통 여부). 기본 1 = 맞으면 바로 소멸
	UPROPERTY(EditDefaultsOnly, Category = "Projectile")
	int32 MaxPierceCount = 1;

	// ★ 스플래시(범위) 데미지 여부. true면 직격 대상 외 주변에도 추가 데미지를 줍니다. (예: 파이어볼)
	UPROPERTY(EditDefaultsOnly, Category = "Projectile|Splash")
	bool bSplashDamage = false;

	// 스플래시 데미지 반경
	UPROPERTY(EditDefaultsOnly, Category = "Projectile|Splash", meta = (EditCondition = "bSplashDamage"))
	float SplashRadius = 250.0f;

	// 스플래시 데미지 배율 (직격 데미지 기준. 1.0 = 직격과 동일한 데미지를 주변에도 적용)
	UPROPERTY(EditDefaultsOnly, Category = "Projectile|Splash", meta = (EditCondition = "bSplashDamage"))
	float SplashDamageMultiplier = 1.0f;

	// ★ 디버그용: 폭발 시 스플래시 반경과 실제 판정된 대상을 화면에 그려서 보여줍니다.
	// 확인 끝나면 false로 꺼두세요 (패키징 빌드에는 영향 없지만 에디터/개발 빌드에서 화면이 어지러워집니다).
	UPROPERTY(EditDefaultsOnly, Category = "Projectile|Splash", meta = (EditCondition = "bSplashDamage"))
	bool bDrawSplashDebug = true;

private:
	int32 CurrentHitCount = 0;

	// ★ 이미 폭발(파괴) 처리가 확정됐는지. 같은 프레임에 여러 명이 동시에 겹쳤을 때
	// OnHit이 중복으로 여러 번 실행되면서 스플래시가 여러 번 뿌려지는 걸 막기 위한 가드입니다.
	bool bHasExploded = false;

	UFUNCTION()
	void OnHit(UPrimitiveComponent* OverlappedComp, AActor* OtherActor,
		UPrimitiveComponent* OtherComp, int32 OtherBodyIndex, bool bFromSweep,
		const FHitResult& SweepResult);

	// ★ 스플래시 데미지 처리: 직격 대상을 제외한 반경 내 RLCharacter들에게 추가 데미지 적용
	void ApplySplashDamage(AActor* DirectHitActor, const FVector& Origin);
};
