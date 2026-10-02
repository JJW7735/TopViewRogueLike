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
class ARLItemDrop;
class URLItemDataAsset;

DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(FOnEnemyDeathEvent, ARLEnemyCharacter*, DeadEnemy);
DECLARE_DYNAMIC_MULTICAST_DELEGATE_TwoParams(FOnSoulCrackStackChanged, int32, NewStack, int32, MaxStack);

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
	// ★ 경험치 & 보스 설정
	// ==========================================

	// 보스 몬스터 여부 (보스는 100% 확정 보상 상자 + 대량 경험치 드롭)
	UPROPERTY(EditDefaultsOnly, BlueprintReadWrite, Category = "Enemy|Reward")
	bool bIsBoss = false;

	// 엘리트 몬스터 여부 (보스처럼 플레이어 공격에 경직되지 않음)
	UPROPERTY(EditDefaultsOnly, BlueprintReadWrite, Category = "Enemy|Combat")
	bool bIsElite = false;

	// 일반 몬스터라도 개별적으로 경직 면역(슈퍼아머)을 주고 싶을 때 체크
	UPROPERTY(EditDefaultsOnly, BlueprintReadWrite, Category = "Enemy|Combat")
	bool bImmuneToStagger = false;

	// 보스 판정 (bIsBoss 체크 또는 클래스/액터 이름에 "Boss" 포함)
	UFUNCTION(BlueprintPure, Category = "Enemy")
	bool IsBossEnemy() const;

	// 보스/엘리트/bImmuneToStagger면 false → 넉백, 피격 몽타주, 시전 취소 없음
	virtual bool CanBeStaggered() const override;

	// 처치 시 플레이어에게 지급할 경험치 (일반 몬스터 기본 30, 보스 350)
	UPROPERTY(EditDefaultsOnly, BlueprintReadWrite, Category = "Enemy|Reward", meta = (ClampMin = "0.0"))
	float ExpReward = 30.0f;

	// ==========================================
	// ★ 필드 아이템 / 골드 드롭 설정
	// ==========================================

	// 사망 시 일반 아이템/골드 드롭 확률 (0.0f ~ 1.0f, 일반몹 기본 35% = 0.35f, 보스는 1.0f)
	UPROPERTY(EditDefaultsOnly, BlueprintReadWrite, Category = "Enemy|Reward", meta = (ClampMin = "0.0", ClampMax = "1.0"))
	float ItemDropChance = 0.35f;

	// 드롭할 수 있는 아이템 후보 풀
	UPROPERTY(EditDefaultsOnly, BlueprintReadWrite, Category = "Enemy|Reward")
	TArray<TObjectPtr<URLItemDataAsset>> PossibleItemDrops;

	// 드롭될 골드 범위
	UPROPERTY(EditDefaultsOnly, BlueprintReadWrite, Category = "Enemy|Reward", meta = (ClampMin = "1"))
	int32 MinGoldDrop = 5;

	UPROPERTY(EditDefaultsOnly, BlueprintReadWrite, Category = "Enemy|Reward", meta = (ClampMin = "1"))
	int32 MaxGoldDrop = 20;

	// 스폰할 필드 드롭 액터 클래스 (기본값: ARLItemDrop)
	UPROPERTY(EditDefaultsOnly, BlueprintReadWrite, Category = "Enemy|Reward")
	TSubclassOf<ARLItemDrop> ItemDropClass;

	// ==========================================
	// ★ 보상 상자 드롭 설정
	// ==========================================

	// 사망 시 보상 상자를 스폰할지 여부 (보스는 자동 true)
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

	// 테스트용: 남은 체력만큼 데미지를 넣어 정상 사망 루트(OnDeath → 몽타주/보상/페이드)로 즉사시킵니다.
	UFUNCTION(BlueprintCallable, Category = "Cheat|Enemy")
	bool DebugKill();

	// ==========================================
	// ★ 사망 연출 (사르르 사라지기)
	// ==========================================

	// 사망 후 메시 머티리얼 페이드 + 바닥으로 가라앉기 연출 사용 여부
	UPROPERTY(EditDefaultsOnly, BlueprintReadWrite, Category = "Enemy|Death")
	bool bUseDeathFade = true;

	// 사망 시점부터 페이드 시작까지 대기 시간(초). 사망 몽타주가 거의 끝날 때쯤으로 맞추면 자연스럽습니다.
	UPROPERTY(EditDefaultsOnly, BlueprintReadWrite, Category = "Enemy|Death", meta = (ClampMin = "0.0", EditCondition = "bUseDeathFade"))
	float DeathFadeDelay = 0.7f;

	// 페이드가 0→1까지 진행되는 시간(초). 끝나면 액터가 제거됩니다.
	UPROPERTY(EditDefaultsOnly, BlueprintReadWrite, Category = "Enemy|Death", meta = (ClampMin = "0.1", EditCondition = "bUseDeathFade"))
	float DeathFadeDuration = 1.5f;

	// 0→1로 올릴 머티리얼 스칼라 파라미터 이름. Paragon 에셋(Sevarog 등)은 MF_DeathFade의 "FadeOut"을 사용합니다.
	// 파라미터가 없는 머티리얼(마네킹 등)은 에러 없이 무시되고 가라앉기만 적용됩니다.
	UPROPERTY(EditDefaultsOnly, BlueprintReadWrite, Category = "Enemy|Death", meta = (EditCondition = "bUseDeathFade"))
	FName DeathFadeParamName = FName("FadeOut");

	// 페이드 동안 메시가 바닥 아래로 가라앉는 거리(cm). 0이면 가라앉지 않습니다.
	UPROPERTY(EditDefaultsOnly, BlueprintReadWrite, Category = "Enemy|Death", meta = (ClampMin = "0.0", EditCondition = "bUseDeathFade"))
	float DeathSinkDistance = 120.0f;

	// ==========================================
	// ★ 영혼의 균열 (Soul Crack) — 보스 카운터 기믹
	// ==========================================
	// 흐름: BTT_Attack이 몽타주 재생 직후 BeginSoulCrackPattern(패턴)을 호출 → 패턴이 강화판(bSoulCrackVariant)이면
	// 녹색 신호 훅(OnSoulCrackVariantStarted) + 균열 판정 구간이 열림 → 그 구간 동안 플레이어가
	// bCanTriggerSoulCrack 스킬로 올바른 각도 구역에서 맞히면 ReceiveCustomDamage에서 TriggerSoulCrackHit
	// (CanBeStaggered 무시 강제 경직 + 스택 +1) → SoulCrackMaxStack 도달 시 TriggerSoulCrackShatter
	// (큰 경직 + 피해감소 디버프, 한 전투 1회 한정).

	// 스택 변경 알림 (UI 아이콘 갱신용)
	UPROPERTY(BlueprintAssignable, Category = "Enemy|SoulCrack")
	FOnSoulCrackStackChanged OnSoulCrackStackChanged;

	// 대균열(Shatter)까지 필요한 스택 수
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Enemy|SoulCrack", meta = (ClampMin = "1"))
	int32 SoulCrackMaxStack = 3;

	// 균열 성공 시 짧은 경직 몽타주 (비우면 몽타주 없이 공격만 끊고 AI만 잠시 멈춤)
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Enemy|SoulCrack")
	TObjectPtr<UAnimMontage> SoulCrackHitMontage = nullptr;

	// 균열 성공 시 AI(BT)를 멈춰두는 시간(초)
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Enemy|SoulCrack", meta = (ClampMin = "0.0"))
	float SoulCrackStaggerDuration = 1.0f;

	// 대균열 시 큰 경직(그로기) 몽타주 (예: AM_Boss_Stun)
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Enemy|SoulCrack")
	TObjectPtr<UAnimMontage> SoulShatterMontage = nullptr;

	// 대균열 시 AI(BT)를 멈춰두는 시간(초) = 프리딜 타임
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Enemy|SoulCrack", meta = (ClampMin = "0.0"))
	float SoulShatterStunDuration = 5.0f;

	// 대균열 디버프 동안 StatComp->DamageReduction을 이 값으로 덮어씀 (기본 0 = 피해감소 제거)
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Enemy|SoulCrack", meta = (ClampMin = "0.0", ClampMax = "80.0"))
	float SoulShatterDamageReduction = 0.0f;

	// 대균열 디버프 지속 시간(초). 끝나면 원래 DamageReduction으로 복구
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Enemy|SoulCrack", meta = (ClampMin = "0.0"))
	float SoulShatterDebuffDuration = 8.0f;

	UPROPERTY(VisibleInstanceOnly, BlueprintReadOnly, Category = "Enemy|SoulCrack")
	int32 SoulCrackStackCount = 0;

	// 이번 전투에서 이미 대균열이 터졌는지 (1회 한정). 터진 뒤에도 균열 성공 시 짧은 경직은 걸리지만 스택은 더 안 쌓임
	UPROPERTY(VisibleInstanceOnly, BlueprintReadOnly, Category = "Enemy|SoulCrack")
	bool bHasShatteredThisFight = false;

	// ★ BTT_Attack에서 공격 몽타주 재생 직후 호출. Pattern.bSoulCrackVariant가 false면 아무것도 안 하고 false 반환
	// (그래서 모든 패턴에 대해 무조건 호출해도 됨). HitNotifyName은 텔레그래프에 넘기는 것과 같은 히트 노티파이 이름.
	UFUNCTION(BlueprintCallable, Category = "Enemy|SoulCrack")
	bool BeginSoulCrackPattern(const FRLMonsterPatternData& Pattern, FName HitNotifyName = FName("Notify_Hit"));

	// 강화판 상태 정리 (몽타주가 끝나면 Tick에서 자동 호출되므로 보통 직접 부를 필요 없음)
	UFUNCTION(BlueprintCallable, Category = "Enemy|SoulCrack")
	void EndSoulCrackPattern();

	// 지금 균열 판정 구간이 열려있는지 (강화판 진행 중 + 히트 직전 구간 + 이번 패턴에서 아직 균열 안 남)
	UFUNCTION(BlueprintPure, Category = "Enemy|SoulCrack")
	bool IsSoulCrackWindowOpen() const;

	// Attacker 위치가 현재 강화판의 정면/후방 균열 구역 안인지
	UFUNCTION(BlueprintPure, Category = "Enemy|SoulCrack")
	bool IsInSoulCrackZone(const AActor* Attacker) const;

	// 균열 성공 처리 (CanBeStaggered 무시 강제 경직 + 스택 +1). 테스트용으로 BP/콘솔에서 직접 호출 가능
	UFUNCTION(BlueprintCallable, Category = "Enemy|SoulCrack")
	void TriggerSoulCrackHit();

	// 대균열 처리 (큰 경직 + 피해감소 디버프). 보통 TriggerSoulCrackHit이 스택이 차면 자동 호출
	UFUNCTION(BlueprintCallable, Category = "Enemy|SoulCrack")
	void TriggerSoulCrackShatter();

	// ★ BTT_Attack에서 공격 몽타주 재생 직후 한 번 호출하는 진입점. 히트별 범위 배율(SetAttackHitScales),
	// 공격 중 전진(StartAttackChase), 영혼의 균열 강화판(BeginSoulCrackPattern)을 한 번에 처리합니다.
	// 각 기능은 패턴 데이터가 비활성 값이면 아무것도 안 하므로 모든 패턴에 대해 무조건 호출해도 됩니다.
	UFUNCTION(BlueprintCallable, Category = "Enemy|Combat")
	void BeginAttackPattern(const FRLMonsterPatternData& Pattern, FName HitNotifyName = FName("Notify_Hit"));

	// ★ BTT_Attack의 OnNotifyBegin 분기용: "Notify_Hit"으로 시작하는 노티파이(Notify_Hit, Notify_Hit_Soul, ...)면 true
	UFUNCTION(BlueprintPure, Category = "Enemy|Combat")
	static bool IsAttackHitNotify(FName NotifyName);

	// ★ BTT_Attack에서 공격 몽타주 재생 직후 호출. Pattern.AttackChaseSpeed가 0이면 아무것도 안 함
	// (그래서 모든 패턴에 대해 무조건 호출해도 됨). 몽타주가 끝나거나 끊기면 자동으로 멈춥니다.
	UFUNCTION(BlueprintCallable, Category = "Enemy|Combat")
	void StartAttackChase(const FRLMonsterPatternData& Pattern);

	UFUNCTION(BlueprintCallable, Category = "Enemy|Combat")
	void StopAttackChase();

	virtual void ReceiveCustomDamage(const FRLDamageData& DamageData, const FHitResult& HitInfo) override;
	virtual void Tick(float DeltaTime) override;

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

	// ★ 사망 페이드가 시작될 때 호출되는 훅. BP에서 먼지/영혼 파티클, 사운드 등을 붙이세요.
	UFUNCTION(BlueprintImplementableEvent, Category = "Enemy|Death")
	void OnDeathFadeStarted();

	// ★ 강화판 패턴 시작 훅. BP에서 녹색 아우라 Niagara를 켜세요.
	UFUNCTION(BlueprintImplementableEvent, Category = "Enemy|SoulCrack")
	void OnSoulCrackVariantStarted();

	// ★ 강화판 패턴 종료 훅 (몽타주 종료/균열 성공/사망). BP에서 녹색 아우라를 끄세요.
	UFUNCTION(BlueprintImplementableEvent, Category = "Enemy|SoulCrack")
	void OnSoulCrackVariantEnded();

	// ★ 균열 성공 훅 (균열 VFX/사운드/카메라 셰이크)
	UFUNCTION(BlueprintImplementableEvent, Category = "Enemy|SoulCrack")
	void OnSoulCrackHit(int32 NewStack);

	// ★ 대균열 훅 (보호막 깨지는 VFX/사운드)
	UFUNCTION(BlueprintImplementableEvent, Category = "Enemy|SoulCrack")
	void OnSoulCrackShattered();

	// ★ 대균열 디버프 종료 훅
	UFUNCTION(BlueprintImplementableEvent, Category = "Enemy|SoulCrack")
	void OnSoulShatterDebuffEnded();

private:
	// 경직 동안 BT를 멈췄다가 Duration 뒤 재시작
	void PauseAIForSoulCrack(float Duration);
	void ResumeAIAfterSoulCrack();
	void EndSoulShatterDebuff();

	UPROPERTY()
	TObjectPtr<UAnimMontage> SoulCrackMontage = nullptr; // 현재 강화판 공격 몽타주
	FVector2D SoulCrackWindow = FVector2D::ZeroVector;   // 몽타주 시간 기준 [열림, 닫힘)
	float SoulCrackFrontAngle = 0.0f;
	float SoulCrackBackAngle = 0.0f;
	bool bSoulCrackConsumed = false;                     // 이번 강화판에서 이미 균열 성공했는지

	// ★ StartAttackChase 상태
	void UpdateAttackChase(float DeltaTime);

	UPROPERTY()
	TObjectPtr<UAnimMontage> ChaseMontage = nullptr;
	float ChaseSpeed = 0.0f;
	float ChaseStopDistance = 0.0f;

	bool bSoulCrackAIStopped = false;                    // 균열 경직으로 BT를 멈춰둔 상태인지 (Tick에서 재가동 감시)
	float SoulCrackAIStopEndTime = 0.0f;
	bool bSoulShatterDebuffActive = false;
	float SavedDamageReduction = 0.0f;

	FTimerHandle SoulCrackAITimerHandle;
	FTimerHandle SoulShatterDebuffTimerHandle;

	void StartDeathFade();
	void TickDeathFade();

	FTimerHandle DeathFadeTimerHandle;
	float DeathFadeElapsed = 0.0f;
	FVector DeathFadeMeshStartLocation = FVector::ZeroVector;
};
