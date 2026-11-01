// Fill out your copyright notice in the Description page of Project Settings.
#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Character.h"
#include "AbilitySystemInterface.h"
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
class ARLMetaUpgradeStation;
class ARLShopStation;
class ARLDecalIndicator;
class UMaterialInterface;
class URLAbilitySystemComponent;
class URLAttributeSet;

UCLASS()
class ROGUELIKETOPVIEW_API ARLCharacter : public ACharacter, public IAbilitySystemInterface
{
	GENERATED_BODY()

public:
	ARLCharacter();

	// ★ GAS 진입점. 플레이어/몬스터 공통으로 캐릭터 자신이 ASC를 가집니다.
	virtual UAbilitySystemComponent* GetAbilitySystemComponent() const override;

	UFUNCTION(BlueprintPure, Category = "GAS")
	URLAttributeSet* GetAttributeSet() const { return AttributeSet; }

	UFUNCTION(BlueprintPure, Category = "Inventory")
	URLInventoryComponent* GetInventoryComponent() const { return InventoryComp; }

	// ★ 진영(팀) 구분. 같은 팀끼리는 ReceiveCustomDamage에서 서로 피해를 주지 않습니다.
	// 플레이어 캐릭터 BP는 클래스 디폴트값에서 Player로 바꿔주고, 몬스터 BP는 기본값(Enemy)을 그대로 사용하면 됩니다.
	UPROPERTY(EditDefaultsOnly, BlueprintReadWrite, Category = "Team")
	ERLTeam TeamID = ERLTeam::Enemy;

	// ★ 캐릭터 선택 UI / "새 게임 시작 시 기존 런 삭제" 경고 팝업 등에 표시할 이름.
	//   BP_Player_Blade, BP_Player_Sorceress 등 각 캐릭터 BP의 클래스 디폴트값에서
	//   "블레이드", "소서리스"처럼 설정해주세요. (기본값은 비워둠)
	UPROPERTY(EditDefaultsOnly, BlueprintReadWrite, Category = "UI")
	FText CharacterDisplayName;

	UPROPERTY(BlueprintReadWrite, Category = "Skill")
	bool bIsSkillMoving = false;

	UFUNCTION(BlueprintCallable, Category = "Skill")
	void StartDashMovement(FVector TargetLocation, float Duration);

	// ★ StartDashMovement + "이동하는 동안 몸으로 부딪히면 데미지" 기능을 더한 버전. 돌진형 보스 패턴에서
	// 사용합니다. 스윙 계열(CreateBoxHitbox 등)은 "정지 상태에서 특정 프레임 한 번" 판정이라 Notify_Hit
	// 하나로 충분했지만, 돌진은 "이동 중 스쳐 지나가면 맞아야" 자연스러워서 매 틱 체크가 필요합니다.
	// 내부적으로 StartDashMovement를 그대로 호출해 이동 로직은 100% 재사용하고, Tick()에서 이동 중일 때만
	// 추가로 데미지 판정을 켭니다. 한 번의 돌진 동안 같은 대상은 한 번만 맞습니다(DashHitActors로 방지).
	// 플레이어의 일반 회피(Dodge)는 데미지가 필요 없어서 기존 StartDashMovement를 그대로 쓰고, 이 함수는
	// 완전히 분리했습니다 — 기존 Dodge/StartDashMovement 호출부에 영향 없음.
	UFUNCTION(BlueprintCallable, Category = "Combat")
	void StartDashAttack(FVector TargetLocation, float Duration, float HitRadius, const FRLDamageData& DamageData);

	// ★ LaunchCharacter와 같지만, 날아갈 거리를 추정해서 걸을 수 있는 영역(네비메시) 밖으로 나가지 않게 수평 속도를 줄임
	// (넉백/돌진 스킬로 낭떠러지·강 너머로 떨어지는 것 방지)
	void LaunchClampedToNav(const FVector& LaunchVelocity, bool bXYOverride, bool bZOverride);

	UFUNCTION(BlueprintCallable, Category = "Combat")
	virtual void ReceiveCustomDamage(const FRLDamageData& DamageData, const FHitResult& HitInfo);

	// ★ 피격 시 경직(넉백 + 피격 몽타주 + 시전 취소)을 받을 수 있는지 여부.
	// 기본은 true. ARLEnemyCharacter에서 보스/엘리트는 false를 돌려주도록 오버라이드합니다(슈퍼아머).
	// ReceiveCustomDamage의 넉백/시전 취소가 이 값을 보고, BP_EnemyBase의 피격 몽타주 재생도 이 값으로 Branch 하세요.
	UFUNCTION(BlueprintPure, Category = "Combat")
	virtual bool CanBeStaggered() const { return true; }

	// ★ 블루프린트에서 FHitResult를 직접 만들기 번거로우니(대부분의 필드가 BP에 노출 안 됨),
	// 충격 위치(ImpactPoint)만 넘기면 내부에서 FHitResult를 만들어 ReceiveCustomDamage를 호출해주는 래퍼입니다.
	// 인페르노/메테오처럼 GroundActorClass로 스폰되는 장판형 스킬 BP에서 이걸 쓰면 됩니다.
	UFUNCTION(BlueprintCallable, Category = "Combat")
	void ReceiveCustomDamageAtLocation(const FRLDamageData& DamageData, FVector ImpactPoint);

	// ★ 무적은 State.Invulnerable 게임플레이 태그로 관리합니다 (예전 bIsInvulnerable bool 대체).
	UFUNCTION(BlueprintCallable, Category = "Combat")
	void SetInvulnerable(bool bNewState);

	UFUNCTION(BlueprintPure, Category = "Combat")
	bool IsInvulnerable() const;

	// ★ 경직: Duration초 동안 State.Stunned 태그 → 이동/회피/스킬 불가 + 이동 경로/시전 취소.
	// 이미 경직 중이면 남은 시간보다 길 때만 연장. 경직 면역(CanBeStaggered == false)이면 무시.
	// 보통 FRLDamageData::StunDuration으로 ReceiveCustomDamage에서 자동 호출됩니다.
	UFUNCTION(BlueprintCallable, Category = "Combat")
	void ApplyStun(float Duration);

	UFUNCTION(BlueprintPure, Category = "Combat")
	bool IsStunned() const;

	// 경직 시작 시 재생할 몽타주 (비우면 몽타주 없이 입력만 막힘). 예: 플레이어 BP에 비틀거리는 피격 모션
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Combat")
	TObjectPtr<UAnimMontage> StunMontage = nullptr;

	// ★ 경직 시작/종료 훅. BP에서 머리 위 별 VFX, 화면 흔들림 등을 붙이세요
	UFUNCTION(BlueprintImplementableEvent, Category = "Combat")
	void OnStunStarted(float Duration);

	UFUNCTION(BlueprintImplementableEvent, Category = "Combat")
	void OnStunEnded();

	UFUNCTION(BlueprintCallable, Category = "Combat")
	void CreateBoxHitbox(FVector BoxExtent, float ForwardOffset, const FRLDamageData& DamageData, bool bDrawDebug = true);

	UFUNCTION(BlueprintCallable, Category = "Combat")
	void TestAttack();

	UFUNCTION(BlueprintCallable, Category = "Combat")
	void CreateLocationHitbox(FVector TargetLocation, float Radius, const FRLDamageData& DamageData, bool bDrawDebug = true);

	// ★ 보스 "자기 자신"을 중심으로 한 부채꼴(전방위) 판정. Radius 안에 있는 대상 중, 정면 기준
	// 뒤쪽 SafeZoneAngleDegrees도 안(안전지대)에 있는 대상만 제외하고 나머지 전부에게 한 번에 데미지를
	// 줍니다. 예: SafeZoneAngleDegrees=90 → 정면 기준 좌우 135도씩(총 270도)이 위험지대.
	UFUNCTION(BlueprintCallable, Category = "Combat")
	void CreateSectorHitbox(float Radius, float SafeZoneAngleDegrees, const FRLDamageData& DamageData, bool bDrawDebug = true, float InnerSafeRadius = 0.0f);

	// ★ 원거리 공격: 정면 방향으로 투사체(ARLProjectile 파생 클래스)를 스폰합니다.
	// URLSkillComponent의 ESkillCastType::Projectile 처리 로직과 동일한 방식으로 동작합니다.
	UFUNCTION(BlueprintCallable, Category = "Combat")
	void SpawnProjectileAttack(TSubclassOf<AActor> ProjectileClass, float ForwardOffset, float ProjectileSpeed, const FRLDamageData& DamageData);

	// ★ 자기(보스) 주변 SpawnRadius 반경 안의 무작위 지점에 지원군(잔몹)을 Count만큼 소환합니다.
	// 정면 방향과 무관하게 사방에 흩뿌려지며, 네비메시가 있으면 그 위로 위치를 보정합니다(허공/벽 스폰 방지).
	// 근접(Box 등) 판정과는 완전히 독립적으로 동작하므로, 몽타주의 아무 노티파이에서나(전용 Notify_Summon 등)
	// 호출하면 "때리는 것과 별개로" 잔몹을 부르는 연출이 됩니다.
	// 예: Subjugation 패턴 — Notify_Hit에서 CreateBoxHitbox(근접 타격), Notify_Summon에서 SummonMinions(지원군 소환).
	// 참고: 여기서 소환한 액터는 ARLArenaManager의 ActiveEnemies/RemainingEnemies 카운트에 포함되지 않습니다
	// (ArenaManager는 자기가 직접 스폰한 몬스터만 추적합니다). 즉 이 잔몹들은 "웨이브 클리어 조건"에 영향을
	// 주지 않는 순수 보스전 지원군입니다 — 보통 의도한 동작이지만, 혹시 이 잔몹도 클리어 조건에 포함시키고
	// 싶다면 별도로 ArenaManager 쪽 연동이 필요합니다.
	UFUNCTION(BlueprintCallable, Category = "Combat|Summon")
	void SummonMinions(TSubclassOf<AActor> MinionClass, int32 Count, float SpawnRadius);

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

	// ★ 예고 데칼 기준점 = 캡슐 바닥(발 위치). 데칼은 기준점 위아래로 일정 두께만 투사하므로, 캡슐 중심을 쓰면
	// 덩치 큰 캐릭터(예: 3배 크기 보스2)는 데칼이 바닥까지 닿지 않아 안 보입니다.
	UFUNCTION(BlueprintPure, Category = "Combat|Indicator")
	FVector GetIndicatorOrigin() const;

	// ★ CreateSectorHitbox와 같은 규격(Radius/SafeZoneAngleDegrees)의 부채꼴 예고 데칼을 표시합니다.
	// ShowBoxAttackIndicator의 부채꼴 버전 — 자기 자신 중심이라 ForwardOffset이 없습니다.
	UFUNCTION(BlueprintCallable, Category = "Combat|Indicator")
	void ShowSectorAttackIndicator(float Radius, float SafeZoneAngleDegrees, UMaterialInterface* DecalMaterial = nullptr, float InnerSafeRadius = 0.0f);

	// ★ CreateLocationHitbox와 같은 규격(Radius)의 원형 예고 데칼을 지정된 위치(Location)에 표시합니다.
	// Box/Sector와 달리 보스 자신이 아니라 임의의 월드 좌표가 중심입니다. 보통 직접 호출하지 않고
	// StartLocationAttackTelegraph를 통해 매 프레임 자동으로 호출됩니다.
	UFUNCTION(BlueprintCallable, Category = "Combat|Indicator")
	void ShowLocationAttackIndicator(FVector Location, float Radius, UMaterialInterface* DecalMaterial = nullptr);

	// ★ 몽타주 재생 직후 한 번 호출하면, 몽타주 안의 HitNotifyName 노티파이마다
	// [히트 - LeadTime, 히트) 구간 동안 Box 예고 데칼을 자동으로 켜고 끕니다 (3연타면 3번 표시/소멸).
	// 몽타주 재생 위치 기준으로 Tick에서 판단하므로 재생 속도/히트스톱과 어긋나지 않고,
	// 몽타주가 끝나거나 끊기면 자동으로 정리됩니다.
	// FaceTarget을 넘기면 히트 사이 구간에는 TurnSpeed(도/초)로 대상을 향해 돌고,
	// 데칼이 뜨는 순간 대상을 정면으로 맞춘 뒤 그 방향으로 고정합니다 (콤보 중 방향 전환).
	UFUNCTION(BlueprintCallable, Category = "Combat|Indicator")
	void StartBoxAttackTelegraph(UAnimMontage* Montage, FName HitNotifyName, float LeadTime, FVector BoxExtent, float ForwardOffset, UMaterialInterface* DecalMaterial = nullptr, AActor* FaceTarget = nullptr, float TurnSpeed = 540.0f);

	// ★ StartBoxAttackTelegraph의 부채꼴(Sector) 버전. 사용법은 동일(HitNotifyName마다 [히트-LeadTime, 히트)
	// 구간에 자동으로 표시/소멸, 콤보면 그 횟수만큼 반복)하고, Box 대신 CreateSectorHitbox와 같은 규격의
	// Radius/SafeZoneAngleDegrees 부채꼴 데칼을 띄웁니다. 예: BigSwing처럼 "선딜레이 후 자기 주변
	// 전방위(뒤쪽 일부 제외)를 한 번에 타격"하는 패턴에 씁니다.
	UFUNCTION(BlueprintCallable, Category = "Combat|Indicator")
	void StartSectorAttackTelegraph(UAnimMontage* Montage, FName HitNotifyName, float LeadTime, float Radius, float SafeZoneAngleDegrees, UMaterialInterface* DecalMaterial = nullptr, AActor* FaceTarget = nullptr, float TurnSpeed = 540.0f, float InnerSafeRadius = 0.0f);

	// ★ StartBoxAttackTelegraph의 "추적형" 버전. Box/Sector와 달리 보스 자신이 아니라 TrackTarget(보통
	// 플레이어)의 현재 위치를 계속 따라다니는 원형 데칼입니다. 몽타주 재생 시작 직후 호출하면:
	//   1) [표시 시작, 히트-LockLeadTime) 구간: 매 프레임 TrackTarget의 현재 위치로 데칼을 옮깁니다 (추적)
	//   2) [히트-LockLeadTime, 히트) 구간: 그 시점의 위치에 고정하고 더 이상 움직이지 않습니다 (경고 후 확정)
	// Notify_Hit 시점에는 GetTelegraphLockedLocation()으로 그 고정된 좌표를 읽어서
	// CreateLocationHitbox(그 좌표, Radius, ...)를 호출하면, 플레이어가 마지막 0.5초 동안 본 자리에
	// 정확히 판정이 생깁니다. LeadTime을 몽타주 전체 길이보다 크게 주면 "사전동작 시작부터" 계속 추적합니다.
	// (예: SoulDrain — 손 꼼지락 사전동작 동안 플레이어 발밑을 따라다니다가, 타격 0.5초 전에 고정되어 터짐)
	// bFaceTrackTarget이 true면 몽타주 동안 TurnSpeed(도/초)로 TrackTarget을 향해 계속 돌고,
	// 위치가 고정된 뒤에는 그 고정 좌표를 바라봅니다.
	UFUNCTION(BlueprintCallable, Category = "Combat|Indicator")
	void StartLocationAttackTelegraph(UAnimMontage* Montage, FName HitNotifyName, float LeadTime, float LockLeadTime, float Radius, UMaterialInterface* DecalMaterial = nullptr, AActor* TrackTarget = nullptr, bool bFaceTrackTarget = true, float TurnSpeed = 540.0f);

	// ★ StartLocationAttackTelegraph가 고정한(또는 아직 고정 전이면 현재 추적 중인) 좌표를 읽습니다.
	// Notify_Hit에서 CreateLocationHitbox에 넘길 좌표로 씁니다.
	UFUNCTION(BlueprintPure, Category = "Combat|Indicator")
	FVector GetTelegraphLockedLocation() const { return TelegraphLockedLocation; }

	UFUNCTION(BlueprintCallable, Category = "Combat|Indicator")
	void StopAttackTelegraph();

	// ★ 이번 공격 패턴의 히트별 범위 배율(FRLMonsterPatternData::HitRangeScales)을 설정하고 히트 카운터를 0으로
	// 되돌립니다. 이후 CreateBoxHitbox/CreateSectorHitbox/CreateLocationHitbox가 호출될 때마다 다음 배율을 하나씩
	// 꺼내 쓰고, 텔레그래프 데칼도 같은 순번의 배율로 그립니다. 빈 배열이면 전부 1.0(기존 동작, 플레이어도 이 상태).
	UFUNCTION(BlueprintCallable, Category = "Combat")
	void SetAttackHitScales(const TArray<float>& Scales);

	// ★ Box 판정의 히트별 좌우 폭 추가 배율(FRLMonsterPatternData::HitWidthScales). SetAttackHitScales와 같은 순번을 씁니다.
	UFUNCTION(BlueprintCallable, Category = "Combat")
	void SetAttackHitWidthScales(const TArray<float>& WidthScales);

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
	void SetNearbyMetaStation(ARLMetaUpgradeStation* Station);

	UFUNCTION(BlueprintPure, Category = "Interaction")
	ARLMetaUpgradeStation* GetNearbyMetaStation() const { return NearbyMetaStation; }

	UFUNCTION(BlueprintCallable, Category = "Interaction")
	void SetNearbyShop(ARLShopStation* Shop);

	UFUNCTION(BlueprintPure, Category = "Interaction")
	ARLShopStation* GetNearbyShop() const { return NearbyShop; }

	UFUNCTION(BlueprintCallable, Category = "Interaction")
	void TryInteract();

	// TryInteract와 같은 우선순위(상자 > 포탈 > 강화소)로 표시할 안내 문구를 골라 OnInteractPromptChanged로 알림
	UFUNCTION(BlueprintCallable, Category = "Interaction")
	void RefreshInteractPrompt();

	UFUNCTION(BlueprintCallable, Exec, Category = "Reward")
	void TestOpenRewardUI();

	// ★ 인벤토리 및 아이템 즉시 테스트용 콘솔 명령어
	UFUNCTION(BlueprintCallable, Exec, Category = "Cheat|Inventory")
	void TestGiveTestItems();

	UFUNCTION(BlueprintCallable, Exec, Category = "Cheat|Inventory")
	void TestUseItemSlot(int32 SlotIndex = 0);

	UFUNCTION(BlueprintCallable, Exec, Category = "Cheat|Inventory")
	void TestAddGold(int32 Amount = 100);

	// 최대 체력 +80 패시브 아이템을 없으면 지급, 있으면 제거 (아이템 GE 획득/해제 확인용)
	UFUNCTION(BlueprintCallable, Exec, Category = "Cheat|Inventory")
	void TestToggleMaxHealthItem();

	UFUNCTION(BlueprintCallable, Exec, Category = "Cheat|Stats")
	void TestAddExp(float Amount = 100.0f);

	// ★ 상점 시스템 테스트용 콘솔 명령어
	UFUNCTION(BlueprintCallable, Exec, Category = "Cheat|Shop")
	void TestOpenShop();

	UFUNCTION(BlueprintCallable, Exec, Category = "Cheat|Shop")
	void TestBuyShopItem(int32 SlotIndex = 0);

	UFUNCTION(BlueprintCallable, Exec, Category = "Cheat|Shop")
	void TestRerollShop();

	// ==========================================
	// 세이브 시스템 연동 및 테스트 콘솔 명령어
	// ==========================================
	UPROPERTY(EditDefaultsOnly, BlueprintReadWrite, Category = "Save")
	bool bAutoRestoreRunSaveOnSpawn = true;

	UFUNCTION(BlueprintCallable, Exec, Category = "Save")
	void SaveCurrentRun();

	UFUNCTION(BlueprintCallable, Exec, Category = "Save")
	void LoadCurrentRun();

	UFUNCTION(BlueprintCallable, Exec, Category = "Save")
	void ClearCurrentRun();

	UFUNCTION(BlueprintCallable, Exec, Category = "Save")
	void AddSouls(int32 Amount = 100);

	// ★ 영구 메타 특성 강화 테스트용 콘솔 명령어
	UFUNCTION(BlueprintCallable, Exec, Category = "Cheat|Meta")
	void UpgradeMetaTrait(FName UpgradeKey);

	UFUNCTION(BlueprintCallable, Exec, Category = "Cheat|Meta")
	void ResetMetaUpgrades();

	UFUNCTION(BlueprintCallable, Exec, Category = "Cheat|Meta")
	void ShowMetaUpgrades();

	UFUNCTION(BlueprintImplementableEvent, Category = "Interaction")
	void OnInteractPromptChanged(bool bVisible, const FText& Prompt);

protected:
	virtual void BeginPlay() override;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Camera")
	class USpringArmComponent* CameraBoom;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Camera")
	class UCameraComponent* TopDownCamera;

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

	// ★ GAS: 어빌리티 시스템 + 전투 스탯 (HealthComp/StatComp를 단계적으로 대체 중)
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "GAS")
	URLAbilitySystemComponent* AbilitySystemComp;

	UPROPERTY()
	URLAttributeSet* AttributeSet;

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

	// ★ StartDashAttack 전용 상태. bDashDealsDamage가 true인 동안(=돌진 이동 중) Tick에서 매 틱
	// PerformDashHitCheck()로 몸통 충돌을 체크합니다. Dodge/StartDashMovement로 시작한 일반 이동은
	// 이 값이 false라서 데미지 판정이 전혀 실행되지 않습니다(기존 동작에 영향 없음).
	bool bDashDealsDamage = false;
	float DashHitRadius = 0.0f;
	FRLDamageData DashDamageData;
	TSet<TWeakObjectPtr<AActor>> DashHitActors; // 이번 돌진 동안 이미 맞힌 대상 (중복 타격 방지)

	void PerformDashHitCheck();

	// ★ 쿨타임 및 무적 타이머
	FTimerHandle DashCooldownTimerHandle;
	FTimerHandle InvincibilityTimerHandle;

	// ★ 경직 타이머
	FTimerHandle StunTimerHandle;
	void EndStun();

	// 레벨업 시 보상 선택창을 살짝 지연 팝업시키기 위한 타이머 (처치 연출이 먼저 보이도록)
	FTimerHandle LevelUpRewardTimerHandle;

	void ResetDashCooldown();
	void EndInvincibility();

	UFUNCTION()
	virtual void HandleDeath();

	UFUNCTION()
	void HandleStatsChanged();

	UFUNCTION()
	void HandleLevelUp(int32 NewLevel);

	// ★ SoulsEarnedThisRun: 이번 런에서 모은 영혼(RunSouls). HandleDeath()에서 RunSouls가
	//   0으로 리셋되기 전에 미리 읽어서 넘겨준다 — 게임오버 화면에서 GetRunSouls()를 직접
	//   호출하면 이미 0이 된 뒤라 항상 +0으로 보이던 문제 때문에 추가됨.
	UFUNCTION(BlueprintImplementableEvent, Category = "Death")
	void OnCharacterDeath(int32 SoulsEarnedThisRun);

	UFUNCTION(BlueprintImplementableEvent, Category = "Level")
	void OnCharacterLevelUp(int32 NewLevel);

	UPROPERTY()
	ARLPortal* NearbyPortal = nullptr;

	UPROPERTY()
	ARLRewardChest* NearbyChest = nullptr;

	UPROPERTY()
	ARLMetaUpgradeStation* NearbyMetaStation = nullptr;

	UPROPERTY()
	ARLShopStation* NearbyShop = nullptr;

	// ★ ShowBoxAttackIndicator 호출 시 처음 한 번만 스폰되고 이후로는 재사용되는 예고 데칼 액터.
	// (URLSkillComponent::DecalIndicator와 같은 지연 스폰 패턴)
	UPROPERTY()
	ARLDecalIndicator* AttackIndicator = nullptr;

	// ★ 텔레그래프(예고 데칼)가 Box 판정용인지 Sector 판정용인지 구분. StartBoxAttackTelegraph /
	// StartSectorAttackTelegraph가 각각 세팅하고, UpdateAttackTelegraph가 이 값으로 분기해서
	// ShowBoxAttackIndicator / ShowSectorAttackIndicator 중 알맞은 쪽을 호출합니다.
	// (BlueprintType이 아닌 내부 전용 enum이라 UENUM 없이 plain enum class로 선언합니다.)
	enum class ETelegraphShape : uint8
	{
		Box,
		Sector,
		Location // ★ 보스 자신이 아니라 TelegraphTrackTarget의 위치를 따라다니다 고정되는 판정 (예: SoulDrain)
	};

	// ★ StartBoxAttackTelegraph 상태. TelegraphWindows는 몽타주 시간(초) 기준 [X=표시 시작, Y=히트) 구간 목록.
	UPROPERTY()
	UAnimMontage* TelegraphMontage = nullptr;

	UPROPERTY()
	UMaterialInterface* TelegraphMaterial = nullptr;

	TArray<FVector2D> TelegraphWindows;
	ETelegraphShape TelegraphShape = ETelegraphShape::Box;
	FVector TelegraphBoxExtent = FVector::ZeroVector;
	float TelegraphForwardOffset = 0.0f;
	float TelegraphRadius = 0.0f;
	float TelegraphSafeZoneAngle = 0.0f;
	float TelegraphInnerSafeRadius = 0.0f;
	float TelegraphShownScale = 1.0f; // 현재 떠 있는 예고 데칼에 적용된 히트 배율
	bool bTelegraphVisible = false;
	TWeakObjectPtr<AActor> TelegraphFaceTarget;
	float TelegraphTurnSpeed = 0.0f;

	// ★ StartLocationAttackTelegraph(Location Shape) 전용 상태
	TWeakObjectPtr<AActor> TelegraphTrackTarget; // 추적할 대상 (보통 플레이어)
	float TelegraphLockLeadTime = 0.0f;          // 히트 몇 초 전에 위치를 고정할지
	bool bTelegraphLocationLocked = false;       // 이번 표시 구간에서 이미 고정됐는지
	FVector TelegraphLockedLocation = FVector::ZeroVector; // 고정된(또는 아직 고정 전이면 마지막으로 추적한) 좌표

	void UpdateAttackTelegraph(float DeltaTime);

	// DeltaTime < 0이면 즉시 정면으로 맞추고, 아니면 TelegraphTurnSpeed로 조금씩 회전
	void FaceTelegraphTarget(float DeltaTime);

	// ★ StartBoxAttackTelegraph/StartSectorAttackTelegraph 공용 헬퍼. Montage의 노티파이 트랙을 스캔해서
	// HitNotifyName과 이름이 일치하는 모든 노티파이의 발동 시각(초)을 찾고, 각각에 대해
	// [max(히트 - LeadTime, 직전 히트 시각), 히트) 구간을 TelegraphWindows 형식으로 반환합니다.
	// (콤보처럼 히트 간격이 LeadTime보다 짧으면 직전 히트 직후부터 바로 다음 표시 구간이 시작됩니다.)
	TArray<FVector2D> ComputeTelegraphWindows(const UAnimMontage* Montage, FName HitNotifyName, float LeadTime) const;

	// ★ SetAttackHitScales 상태. Consume은 Create*Hitbox에서 호출(다음 히트 배율을 꺼내고 카운터 +1),
	// Get은 텔레그래프에서 호출(i번째 히트 배율 조회, 카운터 변경 없음)
	TArray<float> AttackHitScales;
	TArray<float> AttackHitWidthScales;
	int32 AttackHitCounter = 0;
	float ConsumeAttackHitScale();
	float GetAttackHitScale(int32 HitIndex) const;
	float GetAttackHitWidthScale(int32 HitIndex) const;

public:
	virtual void Tick(float DeltaTime) override;
	virtual void SetupPlayerInputComponent(class UInputComponent* PlayerInputComponent) override;
	virtual void AddMovementInput(FVector WorldDirection, float ScaleValue = 1.0f, bool bForce = false) override;
};