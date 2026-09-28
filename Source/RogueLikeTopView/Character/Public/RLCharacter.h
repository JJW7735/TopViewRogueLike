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
class ARLPortal;
class ARLRewardChest;
class ARLDecalIndicator;
class UMaterialInterface;

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

	// ★ 블루프린트에서 FHitResult를 직접 만들기 번거로우니(대부분의 필드가 BP에 노출 안 됨),
	// 충격 위치(ImpactPoint)만 넘기면 내부에서 FHitResult를 만들어 ReceiveCustomDamage를 호출해주는 래퍼입니다.
	// 인페르노/메테오처럼 GroundActorClass로 스폰되는 장판형 스킬 BP에서 이걸 쓰면 됩니다.
	UFUNCTION(BlueprintCallable, Category = "Combat")
	void ReceiveCustomDamageAtLocation(const FRLDamageData& DamageData, FVector ImpactPoint);

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

	// ★ 근접 박스 공격 예고(텔레그래프) 데칼을 표시합니다. CreateBoxHitbox와 같은 규격의
	// BoxExtent/ForwardOffset을 넘기면, 실제 판정 박스와 정확히 같은 자리/크기에 사각형 데칼이 뜹니다.
	// 보통 BTT_Attack에서 "몽타주 재생 시작" 시점(선딜레이 구간)에 호출해서 플레이어에게 미리 경고하고,
	// Notify_Hit(실제 판정 시점)에 HideAttackIndicator를 호출해 감춰주는 식으로 씁니다.
	// DecalMaterial을 비워두면(nullptr) 이전에 설정돼 있던 머티리얼을 그대로 재사용합니다.
	UFUNCTION(BlueprintCallable, Category = "Combat|Indicator")
	void ShowBoxAttackIndicator(FVector BoxExtent, float ForwardOffset, UMaterialInterface* DecalMaterial = nullptr);

	// ★ ShowBoxAttackIndicator로 띄운 예고 데칼을 숨깁니다. (액터 자체를 파괴하진 않고 다음 공격 때 재사용합니다)
	UFUNCTION(BlueprintCallable, Category = "Combat|Indicator")
	void HideAttackIndicator();

	// ★ 몽타주 재생 직후 한 번 호출하면, 몽타주 안의 HitNotifyName 노티파이마다
	// [히트 - LeadTime, 히트) 구간 동안 Box 예고 데칼을 자동으로 켜고 끕니다 (3연타면 3번 표시/소멸).
	// 몽타주 재생 위치 기준으로 Tick에서 판단하므로 재생 속도/히트스톱과 어긋나지 않고,
	// 몽타주가 끝나거나 끊기면 자동으로 정리됩니다.
	UFUNCTION(BlueprintCallable, Category = "Combat|Indicator")
	void StartBoxAttackTelegraph(UAnimMontage* Montage, FName HitNotifyName, float LeadTime, FVector BoxExtent, float ForwardOffset, UMaterialInterface* DecalMaterial = nullptr);

	UFUNCTION(BlueprintCallable, Category = "Combat|Indicator")
	void StopAttackTelegraph();

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

	UFUNCTION(BlueprintCallable, Category = "Interaction")
	void SetNearbyPortal(ARLPortal* Portal);

	UFUNCTION(BlueprintCallable, Category = "Interaction")
	void SetNearbyChest(ARLRewardChest* Chest);

	UFUNCTION(BlueprintPure, Category = "Interaction")
	ARLRewardChest* GetNearbyChest() const { return NearbyChest; }

	UFUNCTION(BlueprintCallable, Category = "Interaction")
	void TryInteract();

	UFUNCTION(BlueprintCallable, Exec, Category = "Reward")
	void TestOpenRewardUI();

	UFUNCTION(BlueprintImplementableEvent, Category = "Interaction")
	void OnNearbyPortalChanged(bool bInRange);

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

	UFUNCTION()
	void HandleStatsChanged();

	UFUNCTION(BlueprintImplementableEvent, Category = "Death")
	void OnCharacterDeath();

	UPROPERTY()
	ARLPortal* NearbyPortal = nullptr;

	UPROPERTY()
	ARLRewardChest* NearbyChest = nullptr;

	// ★ ShowBoxAttackIndicator 호출 시 처음 한 번만 스폰되고 이후로는 재사용되는 예고 데칼 액터.
	// (URLSkillComponent::DecalIndicator와 같은 지연 스폰 패턴)
	UPROPERTY()
	ARLDecalIndicator* AttackIndicator = nullptr;

	// ★ StartBoxAttackTelegraph 상태. TelegraphWindows는 몽타주 시간(초) 기준 [X=표시 시작, Y=히트) 구간 목록.
	UPROPERTY()
	UAnimMontage* TelegraphMontage = nullptr;

	UPROPERTY()
	UMaterialInterface* TelegraphMaterial = nullptr;

	TArray<FVector2D> TelegraphWindows;
	FVector TelegraphBoxExtent = FVector::ZeroVector;
	float TelegraphForwardOffset = 0.0f;
	bool bTelegraphVisible = false;

	void UpdateAttackTelegraph();

public:
	virtual void Tick(float DeltaTime) override;
	virtual void SetupPlayerInputComponent(class UInputComponent* PlayerInputComponent) override;
	virtual void AddMovementInput(FVector WorldDirection, float ScaleValue = 1.0f, bool bForce = false) override;
};