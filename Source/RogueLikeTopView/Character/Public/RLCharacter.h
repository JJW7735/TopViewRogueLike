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
class URLInventoryComponent;
class ARLPortal;
class ARLRewardChest;

UCLASS()
class ROGUELIKETOPVIEW_API ARLCharacter : public ACharacter
{
	GENERATED_BODY()

public:
	ARLCharacter();

	UFUNCTION(BlueprintPure, Category = "Inventory")
	URLInventoryComponent* GetInventoryComponent() const { return InventoryComp; }

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

	// ★ 인벤토리 및 아이템 즉시 테스트용 콘솔 명령어
	UFUNCTION(BlueprintCallable, Exec, Category = "Cheat|Inventory")
	void TestGiveTestItems();

	UFUNCTION(BlueprintCallable, Exec, Category = "Cheat|Inventory")
	void TestUseItemSlot(int32 SlotIndex = 0);

	UFUNCTION(BlueprintCallable, Exec, Category = "Cheat|Inventory")
	void TestAddGold(int32 Amount = 100);

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

	// ★ 추가: 인벤토리 컴포넌트
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Components")
	class URLInventoryComponent* InventoryComp;

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

public:
	virtual void Tick(float DeltaTime) override;
	virtual void SetupPlayerInputComponent(class UInputComponent* PlayerInputComponent) override;
	virtual void AddMovementInput(FVector WorldDirection, float ScaleValue = 1.0f, bool bForce = false) override;
};