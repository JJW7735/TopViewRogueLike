// Fill out your copyright notice in the Description page of Project Settings.
#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Character.h"
#include "RLTypes.h"
#include "RLCharacter.generated.h"

class USpringArmComponent;
class UCameraComponent;
class URLHealthComponent;
class URLSkillComponent;
class URLSkillData;
class URLStatComponent;

UCLASS()
class ROGUELIKETOPVIEW_API ARLCharacter : public ACharacter
{
	GENERATED_BODY()

public:
	ARLCharacter();

	// ★ 진영(팀) 구분. 같은 팀끼리는 ReceiveCustomDamage에서 서로 피해를 주지 않습니다.
	// 플레이어 캐릭터 BP는 클래스 디폴트값에서 Player로 바꿔주고, 몬스터 BP는 기본값(Enemy)을 그대로 사용하면 됩니다.
	UPROPERTY(EditDefaultsOnly, BlueprintReadWrite, Category = "Team")
	ERLTeam TeamID = ERLTeam::Enemy;

	UPROPERTY(BlueprintReadWrite, Category = "Skill")
	bool bIsSkillMoving = false;

	UFUNCTION(BlueprintCallable, Category = "Skill")
	void StartDashMovement(FVector TargetLocation, float Duration);

	UFUNCTION(BlueprintCallable, Category = "Combat")
	virtual void ReceiveCustomDamage(const FRLDamageData& DamageData, const FHitResult& HitInfo);

	UFUNCTION(BlueprintCallable, Category = "Combat")
	void SetInvulnerable(bool bNewState) { bIsInvulnerable = bNewState; }

	UFUNCTION(BlueprintPure, Category = "Combat")
	bool IsInvulnerable() const { return bIsInvulnerable; }

	UFUNCTION(BlueprintCallable, Category = "Combat")
	void CreateBoxHitbox(FVector BoxExtent, float ForwardOffset, const FRLDamageData& DamageData, bool bDrawDebug = true);

	UFUNCTION(BlueprintCallable, Category = "Combat")
	void TestAttack();

	UFUNCTION(BlueprintCallable, Category = "Combat")
	void CreateLocationHitbox(FVector TargetLocation, float Radius, const FRLDamageData& DamageData, bool bDrawDebug = true);

	// ★ 원거리 공격: 정면 방향으로 투사체(ARLProjectile 파생 클래스)를 스폰합니다.
	// URLSkillComponent의 ESkillCastType::Projectile 처리 로직과 동일한 방식으로 동작합니다.
	UFUNCTION(BlueprintCallable, Category = "Combat")
	void SpawnProjectileAttack(TSubclassOf<AActor> ProjectileClass, float ForwardOffset, float ProjectileSpeed, const FRLDamageData& DamageData);

	UFUNCTION(BlueprintCallable, Category = "Action")
	void Dodge(FVector CursorLocation);

	UFUNCTION(BlueprintPure, Category = "Skill")
	URLSkillComponent* GetSkillComponent() const { return SkillComp; }

	UFUNCTION(BlueprintPure, Category = "Stats")
	URLStatComponent* GetStatComponent() const { return StatComp; }

	// ★ 언리얼 엔진의 기본 대미지 파이프라인 가로채기 (절대 방어막)
	virtual float TakeDamage(float DamageAmount, struct FDamageEvent const& DamageEvent, class AController* EventInstigator, AActor* DamageCauser) override;

	// ★ 얼마나 멀리 갈 것인가? (대쉬 거리)
	UPROPERTY(EditDefaultsOnly, BlueprintReadWrite, Category = "Action|Dash")
	float DashDistance = 700.0f;

	// ★ 얼마나 빨리 갈 것인가? (대쉬 속도)
	UPROPERTY(EditDefaultsOnly, BlueprintReadWrite, Category = "Action|Dash")
	float DashSpeed = 3500.0f;

	UPROPERTY(EditDefaultsOnly, BlueprintReadWrite, Category = "Action|Dash")
	float DashCooldown = 1.0f;

	UPROPERTY(EditDefaultsOnly, BlueprintReadWrite, Category = "Action|Dash")
	float InvincibilityDuration = 0.4f;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Action|Dash")
	bool bCanDash = true;

	// ★ 추가: 대쉬할 때 재생할 애니메이션 몽타주 빈칸
	UPROPERTY(EditDefaultsOnly, BlueprintReadWrite, Category = "Action|Dash")
	class UAnimMontage* DashMontage;
	bool IsDashing() const { return bIsDashing; }

protected:
	virtual void BeginPlay() override;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Camera")
	class USpringArmComponent* CameraBoom;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Camera")
	class UCameraComponent* TopDownCamera;

	UPROPERTY(VisibleInstanceOnly, BlueprintReadOnly, Category = "Combat")
	bool bIsInvulnerable = false;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Components")
	class URLHealthComponent* HealthComp;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Components")
	URLSkillComponent* SkillComp;

	// ★ 추가: 스탯 컴포넌트
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Components")
	class URLStatComponent* StatComp;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Skill|Default")
	URLSkillData* DefaultSkill_Q;
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Skill|Default")
	URLSkillData* DefaultSkill_W;
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Skill|Default")
	URLSkillData* DefaultSkill_E;
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Skill|Default")
	URLSkillData* DefaultSkill_R;
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Skill|Default")
	URLSkillData* DefaultSkill_LMB;

	bool bIsHoldingRightClick = false;
	bool bIsDashing = false;
	FVector DashStartLocation;
	FVector DashTargetLocation;
	float DashElapsedTime = 0.0f;
	float DashDuration = 0.0f;

	// ★ 쿨타임 및 무적 타이머
	FTimerHandle DashCooldownTimerHandle;
	FTimerHandle InvincibilityTimerHandle;

	void ResetDashCooldown();
	void EndInvincibility();

	UFUNCTION()
	virtual void HandleDeath();

	UFUNCTION(BlueprintImplementableEvent, Category = "Death")
	void OnCharacterDeath();

public:
	virtual void Tick(float DeltaTime) override;
	virtual void SetupPlayerInputComponent(class UInputComponent* PlayerInputComponent) override;
	virtual void AddMovementInput(FVector WorldDirection, float ScaleValue = 1.0f, bool bForce = false) override;
};