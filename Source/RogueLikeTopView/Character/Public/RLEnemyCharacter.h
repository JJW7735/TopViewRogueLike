// Fill out your copyright notice in the Description page of Project Settings.
#pragma once

#include "CoreMinimal.h"
#include "RLCharacter.h"
#include "RLEnemyCharacter.generated.h"

// ★ ARLCharacter와 BP_EnemyBase 사이에 끼워넣는 "적 전용" C++ 계층입니다.
// 플레이어 캐릭터는 여전히 ARLCharacter를 그대로 쓰고, 몬스터(일반+보스)만 이 클래스를 거쳐 갑니다.
//
// ★ 왜 이 레이어가 필요한지:
// - BTT_SelectPattern/BTT_Attack이 컨트롤된 폰을 BP_EnemyBase로 다이나믹 캐스트해서
//   CurrentSelectedPattern/TrySelectPattern 등을 씁니다. 즉 보스도 이 재사용 로직을 그대로 쓰려면
//   보스 블루프린트가 BP_EnemyBase의 자식이어야 합니다(ARLBossCharacter 같은 별도 C++ 서브클래스를
//   BP_EnemyBase와 무관하게 새로 파면, 그 캐스트가 실패해서 패턴 선택/공격이 전부 깨집니다).
// - 그래서 "보스만의 로직"이라도 새 C++ 서브클래스를 따로 만들 수가 없고, ARLCharacter와
//   BP_EnemyBase 사이에 얇은 공용 계층을 끼워넣어야 합니다. 여기 있는 기능은 EnrageHealthPercentThreshold가
//   음수(기본값)면 아무 동작도 하지 않으므로, 일반 몬스터한테는 사실상 없는 기능처럼 동작합니다.
// - AI(BT) 정지 로직도 여기 둔 이유: 플레이어 캐릭터는 AIController/BehaviorTree가 없으니
//   ARLCharacter에 두면 의미가 없고, 몬스터 전용 계층에 두는 게 맞습니다.
class ARLRewardChest;

DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(FOnEnemyDeathEvent, ARLEnemyCharacter*, DeadEnemy);

UCLASS()
class ROGUELIKETOPVIEW_API ARLEnemyCharacter : public ARLCharacter
{
	GENERATED_BODY()

public:
	ARLEnemyCharacter();

	// 몬스터 사망 시 외부(ArenaManager 등)에 알리는 델리게이트
	UPROPERTY(BlueprintAssignable, Category = "Enemy|Death")
	FOnEnemyDeathEvent OnEnemyDied;

	// ==========================================
	// ★ 보상 상자 드롭 설정
	// ==========================================

	// 사망 시 보상 상자를 스폰할지 여부 (보스나 엘리트 몬스터의 경우 체크)
	UPROPERTY(EditDefaultsOnly, BlueprintReadWrite, Category = "Enemy|Reward")
	bool bDropChestOnDeath = false;

	// 스폰할 보상 상자 클래스 (기본값: ARLRewardChest)
	UPROPERTY(EditDefaultsOnly, BlueprintReadWrite, Category = "Enemy|Reward", meta = (EditCondition = "bDropChestOnDeath"))
	TSubclassOf<ARLRewardChest> DropChestClass;

	// 상자 드롭 확률 (0.0f ~ 1.0f, 1.0f = 100% 확정 드롭)
	UPROPERTY(EditDefaultsOnly, BlueprintReadWrite, Category = "Enemy|Reward", meta = (ClampMin = "0.0", ClampMax = "1.0", EditCondition = "bDropChestOnDeath"))
	float DropChance = 1.0f;

	// 상자 스폰 위치 오프셋
	UPROPERTY(EditDefaultsOnly, BlueprintReadWrite, Category = "Enemy|Reward", meta = (EditCondition = "bDropChestOnDeath"))
	FVector ChestSpawnOffset = FVector::ZeroVector;

	// ★ 0 이상이면 "이 퍼센트 이하로 체력이 떨어지면 분노 상태로 전환"이 활성화됩니다.
	// 기본값 -1.0f = 비활성(일반 몬스터). 보스 BP의 클래스 디폴트에서만 0.3 같은 값으로 켜주세요.
	UPROPERTY(EditDefaultsOnly, BlueprintReadWrite, Category = "Enemy|Enrage")
	float EnrageHealthPercentThreshold = -1.0f;

	// 한 번 분노하면 체력이 다시 올라도 되돌리지 않을지 여부. 기본 true.
	UPROPERTY(EditDefaultsOnly, BlueprintReadWrite, Category = "Enemy|Enrage")
	bool bEnrageIsOneWay = true;

	UPROPERTY(VisibleInstanceOnly, BlueprintReadOnly, Category = "Enemy|Enrage")
	bool bIsEnraged = false;

	// ★ 블랙보드 키 이름. 이 기능을 쓰는 BP(예: BB_Boss)에는 같은 이름의 Bool/Float 키를 만들어두세요.
	// 키가 없는 블랙보드(예: BB_Enemy)에 대해 SetValueAsBool/Float를 호출해도 에러 없이 무시됩니다.
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Enemy|Enrage")
	FName BlackboardKey_IsEnraged = FName("bIsEnraged");

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Enemy|Enrage")
	FName BlackboardKey_HealthPercent = FName("HealthPercent");

	UFUNCTION(BlueprintPure, Category = "Enemy|Enrage")
	float GetHealthPercent() const;

protected:
	virtual void BeginPlay() override;

	UFUNCTION()
	void HandleEnemyHealthChanged(float NewCurrentHealth, float NewMaxHealth, float HealthDelta);

	virtual void EnterEnrage();

	// ARLCharacter::HandleDeath()는 무브먼트/콜리전만 멈추고 AI(BT)는 멈추지 않습니다.
	// (Character/Private/RLCharacter.cpp 실제 구현 확인: DisableMovement + NoCollision + OnCharacterDeath만 호출)
	// 그래서 여기서 AIController의 BrainComponent를 명시적으로 멈춥니다. 모든 몬스터(보스 포함)에 적용됩니다.
	virtual void HandleDeath() override;

	// ★ EnrageHealthPercentThreshold가 활성화된 몬스터(주로 보스)에서만 의미 있는 훅.
	// BP에서 포효 모션/VFX/카메라 셰이크 등을 여기에 붙이세요.
	UFUNCTION(BlueprintImplementableEvent, Category = "Enemy|Enrage")
	void OnEnraged();
};
