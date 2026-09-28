#pragma once

#include "CoreMinimal.h"
#include "RLTypes.generated.h"

class UAnimMontage;
class UMaterialInterface;

// ★ 진영(팀) 구분. ARLCharacter::ReceiveCustomDamage에서 같은 팀끼리는 피해를 무시하는 데 사용합니다.
UENUM(BlueprintType)
enum class ERLTeam : uint8
{
	Player,
	Enemy
};

// ★ 공격 판정 형태. BTT_Attack이 이 값을 보고 ARLCharacter::CreateBoxHitbox / CreateLocationHitbox /
// SpawnProjectileAttack 중 어느 것을 호출할지 결정합니다.
// Box/Location 판정의 실제 크기는 별도 필드를 두지 않고 FRLMonsterPatternData의
// IndicatorDecalSize(X=ForwardOffset/깊이, Y·Z=Box 절반폭 또는 Radius)와 IndicatorOffset을 그대로 재사용합니다.
UENUM(BlueprintType)
enum class ERLAttackShape : uint8
{
	Box,        // 정면 박스 판정 (근접, 예: SwingCombo, Subjugation)
	Location,   // 특정 지점 중심 구체 판정 (광역/바닥찍기, 예: GroundSlam)
	Projectile  // 투사체 발사 (bIsRangedAttack == true인 패턴, 예: SoulPull)
};

// 공격 판정 및 데미지 정보를 담는 공용 구조체
USTRUCT(BlueprintType)
struct FRLDamageData
{
	GENERATED_BODY()

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Damage")
	float BaseDamage = 15.0f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Damage")
	float KnockbackForce = 600.0f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Damage")
	float HitStopDuration = 0.05f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Damage")
	bool bIgnoreInvincibility = false;

	// ★ 치명타 발동 여부 (UI 데미지 플로팅, 연출, 효과 등에 사용)
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Damage")
	bool bIsCritical = false;

	// ★ 고정 피해 여부 (true면 피격자의 방어력/피해감소를 무시하고 100% 피해 적용)
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Damage")
	bool bTrueDamage = false;

	UPROPERTY(BlueprintReadOnly, Category = "Damage")
	AActor* DamageCauser = nullptr;
};

// ★ 몬스터 개별 공격 패턴 데이터 구조체
USTRUCT(BlueprintType)
struct FRLMonsterPatternData
{
	GENERATED_BODY()

	// 패턴 식별 이름 (예: "Slash", "Rush", "JumpSmash")
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Pattern")
	FName PatternID = NAME_None;

	// 공격 모션 몽타주
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Pattern")
	TObjectPtr<UAnimMontage> AttackMontage = nullptr;

	// 공격 유효 사거리 (이 거리 이내여야 패턴 사용)
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Pattern")
	float AttackRange = 250.0f;

	// 패턴 재사용 대기 시간 (초)
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Pattern")
	float Cooldown = 3.0f;

	// 패턴 사용 확률 가중치 (기본 1.0)
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Pattern")
	float SelectionWeight = 1.0f;

	// 이 패턴 실행 시 적용할 데미지 정보
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Pattern")
	FRLDamageData DamageData;

	// ★ 공격 판정 형태 (BTT_Attack이 CreateBoxHitbox/CreateLocationHitbox/SpawnProjectileAttack 중
	// 무엇을 호출할지 이 값으로 분기합니다). Projectile로 설정할 경우 bIsRangedAttack도 true로 맞춰주세요.
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Pattern")
	ERLAttackShape AttackShape = ERLAttackShape::Box;

	// 타격 예고 장판(인디케이터) 사용 여부
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Indicator")
	bool bUseIndicator = false;

	// 장판 데칼 머티리얼 (원형, 사각형 등)
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Indicator", meta = (EditCondition = "bUseIndicator"))
	TObjectPtr<UMaterialInterface> IndicatorDecalMaterial = nullptr;

	// 데칼 크기 (X: 투사 깊이, Y: 가로 폭/반경, Z: 세로 길이/반경)
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Indicator", meta = (EditCondition = "bUseIndicator"))
	FVector IndicatorDecalSize = FVector(128.0f, 256.0f, 256.0f);

	// 데칼 위치 오프셋 (돌진기처럼 앞으로 뻗어야 할 때 사용)
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Indicator", meta = (EditCondition = "bUseIndicator"))
	FVector IndicatorOffset = FVector::ZeroVector;

	// ★ Box 예고 데칼을 각 Notify_Hit보다 몇 초 먼저 띄울지 (ARLCharacter::StartBoxAttackTelegraph에 전달).
	// 콤보라서 히트 간격이 이 값보다 짧으면 직전 히트 직후부터 표시됩니다.
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Indicator", meta = (ClampMin = "0.0"))
	float IndicatorLeadTime = 0.7f;

	// ★ 원거리(투사체) 패턴 여부. true면 이 패턴은 근접 히트박스 대신 투사체를 발사합니다.
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Pattern|Projectile")
	bool bIsRangedAttack = false;

	// 발사할 투사체 액터 클래스 (예: BP_RLProjectile_Base)
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Pattern|Projectile", meta = (EditCondition = "bIsRangedAttack"))
	TSubclassOf<AActor> ProjectileClass;

	// 투사체 발사 속도
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Pattern|Projectile", meta = (EditCondition = "bIsRangedAttack"))
	float ProjectileSpeed = 1200.0f;

	// 투사체 스폰 위치 오프셋 (캐릭터 정면 기준 앞으로 얼마나 띄워서 스폰할지)
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Pattern|Projectile", meta = (EditCondition = "bIsRangedAttack"))
	float ProjectileSpawnForwardOffset = 80.0f;
};