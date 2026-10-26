#pragma once

#include "CoreMinimal.h"
#include "RLTypes.generated.h"

class UAnimMontage;
class UMaterialInterface;
class ARLThrownAxe;

// ★ 마우스 커서 → 바닥 지점 판정 전용 트레이스 채널 (DefaultEngine.ini의 Name="Ground", GameTraceChannel3).
// 기본 반응 Block(기존 Visibility처럼 모든 메시가 막음), Pawn/CharacterMesh 프로필은 Ignore.
// 바닥이 아닌 구조물(아치/기둥/천장)은 메시 콜리전에서 Ground=Ignore로 바꾸면 확실히 제외되고,
// 그러지 않아도 ARLPlayerController::GetGroundHitUnderCursor가 "위를 향한 면 + 플레이어 발 높이 근처"만 채택합니다.
#define ECC_Ground ECC_GameTraceChannel3

// ★ 진영(팀) 구분. ARLCharacter::ReceiveCustomDamage에서 같은 팀끼리는 피해를 무시하는 데 사용합니다.
UENUM(BlueprintType)
enum class ERLTeam : uint8
{
	Player,
	Enemy
};

// ★ 공격 판정 형태. BTT_Attack이 이 값을 보고 ARLCharacter::CreateBoxHitbox / CreateLocationHitbox /
// SpawnProjectileAttack / CreateSectorHitbox 중 어느 것을 호출할지 결정합니다.
// Box/Location 판정의 실제 크기는 별도 필드를 두지 않고 FRLMonsterPatternData의
// IndicatorDecalSize(X=ForwardOffset/깊이, Y·Z=Box 절반폭 또는 Radius)와 IndicatorOffset을 그대로 재사용합니다.
UENUM(BlueprintType)
enum class ERLAttackShape : uint8
{
	Box,        // 정면 박스 판정 (근접, 예: SwingCombo, Subjugation)
	Location,   // 특정 지점 중심 구체 판정 (광역/바닥찍기, 예: GroundSlam)
	Projectile, // 투사체 발사 (bIsRangedAttack == true인 패턴, 예: SoulPull)
	Sector,     // ★ 보스 자신 중심의 부채꼴(전방위) 판정. IndicatorDecalSize.Y를 반지름으로(데칼/판정 공용), SectorSafeZoneAngle을
	            // "정면 기준 뒤쪽 안전지대 폭(도)"으로 써서 그 쐐기 밖 전체를 한 번에 타격합니다 (예: BigSwing).
	            // SectorSafeZoneAngle을 0으로 두면 안전지대가 사실상 없어져서 보스 주변 360도 전체 원형 판정으로도
	            // 그대로 재사용할 수 있습니다 (예: SoulDrain — 사방의 영혼을 빨아들이는 연출)
	Dash,       // ★ 돌진. Notify_Hit 대신 Notify_DashStart에서 ARLCharacter::StartDashAttack으로 정면 방향 돌진,
	            // 이동 중 몸통 충돌로 판정합니다. 예고 데칼은 Box와 같은 직사각형 레인(IndicatorDecalSize 재사용,
	            // 돌진 거리 = IndicatorDecalSize.X * 2 = 레인 끝)을 Notify_DashStart 기준으로 띄웁니다 (예: DashAttack)
	AxeLeap,    // ★ 도끼 투척 → 도약 낙하. BTT_Attack은 몽타주만 재생하고 나머지는 전부 ARLEnemyCharacter::BeginAxeLeap이
	            // 몽타주 재생 위치를 보고 처리합니다: 플레이어 추적 원형 데칼(IndicatorDecalSize.Y = 반지름) →
	            // Notify_AxeThrow에서 데칼 고정 + 도끼 투척 → Notify_LeapUp에서 상승 후 사라짐 → Notify_LeapFall에서
	            // 데칼 위로 순간이동 후 낙하 → Notify_Hit에서 착지 판정 + 도끼 회수 (예: 보스2 AxeLeap)
	AxeReturn   // ★ 도끼 투척 → 회수(부메랑). AxeLeap과 투척까지는 같고, Notify_AxeRecall에서 박힌 도끼가 보스 손으로
	            // 날아오기 시작해 Notify_AxeCatch에 손에 도착합니다. 돌아오는 경로에 닿으면 AxeReturnDamageData 피해
	            // (예: 보스2 AxeReturn)
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

	// ★ 흡혈 비율 (0~1). 0보다 크면 ARLCharacter::ReceiveCustomDamage에서 "실제로 적용된 데미지"(방어력
	// 적용 후 최종 값) 중 이 비율만큼을 DamageCauser의 체력으로 되돌려줍니다. 예: 0.3 = 가한 데미지의
	// 30%를 가해자가 회복 (예: 보스 SoulDrain 패턴)
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Damage", meta = (ClampMin = "0.0", ClampMax = "1.0"))
	float LifestealRatio = 0.0f;

	// ★ 영혼의 균열(Soul Crack) 유발 가능 여부. URLSkillComponent::CalculateFinalDamage가 스킬의
	// URLSkillData::bCanTriggerSoulCrack 값을 여기로 복사합니다. 데미지 구조체에 실어 보내기 때문에
	// 근접 박스/투사체/장판(GroundAreaEffect) 어떤 경로로 맞혀도 ARLEnemyCharacter::ReceiveCustomDamage에서
	// 똑같이 균열 판정을 받습니다.
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Damage")
	bool bCanTriggerSoulCrack = false;

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

	// ★ 예고 데칼을 각 Notify_Hit보다 몇 초 먼저 띄울지 (ARLCharacter::StartBoxAttackTelegraph /
	// StartSectorAttackTelegraph에 전달. Box/Sector 공용). 콤보라서 히트 간격이 이 값보다 짧으면
	// 직전 히트 직후부터 표시됩니다.
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Indicator", meta = (ClampMin = "0.0"))
	float IndicatorLeadTime = 0.7f;

	// ★ Sector(부채꼴) 판정 전용: 보스 정면 기준 "뒤쪽" 안전지대의 폭(도). 예: 90이면 정면 기준 좌우
	// 135도씩(총 270도)이 위험지대, 바로 뒤 90도만 안전지대. AttackShape가 Sector일 때만 사용됩니다.
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Pattern|Sector", meta = (EditCondition = "AttackShape == ERLAttackShape::Sector", ClampMin = "0.0", ClampMax = "359.0"))
	float SectorSafeZoneAngle = 90.0f;

	// ★ Sector 판정 전용: 보스 중심 "안쪽" 안전 반경(cm). 0보다 크면 이 반경 안의 대상은 맞지 않습니다
	// (SectorSafeZoneAngle의 각도 안전지대와 별개로 거리 기준 안전지대). SectorSafeZoneAngle=0과 같이 쓰면
	// "보스 가까이가 안전, 바깥 전체가 위험"인 반전 원형 판정이 됩니다 (예: Subjugation 균열 강화판).
	// 데칼 머티리얼에 "InnerSafeRatio" Scalar Parameter(=이 값/반지름, 0~1)가 있으면 안쪽을 비워 그릴 수 있습니다.
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Pattern|Sector", meta = (EditCondition = "AttackShape == ERLAttackShape::Sector", ClampMin = "0.0"))
	float SectorInnerSafeRadius = 0.0f;

	// ★ 히트별 범위 배율. 몽타주의 "Notify_Hit"으로 시작하는 노티파이(Notify_Hit, Notify_Hit_Soul, ...)를
	// 시간 순서대로 0, 1, 2...번째 히트로 보고, i번째 히트의 판정/예고 데칼 크기에 HitRangeScales[i]를 곱합니다
	// (Box는 BoxExtent·ForwardOffset, Sector/Location은 반지름). 배열보다 히트가 많으면 마지막 값을 계속 씁니다.
	// 비워두면 전부 1.0(기존 동작). 예: SwingCombo 강화판 [1.0, 1.3, 1.6] → 3연타가 점점 커짐
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Pattern")
	TArray<float> HitRangeScales;

	// ★ Box 판정 전용: 히트별 "좌우 폭"에만 추가로 곱하는 배율 (HitRangeScales 위에 한 번 더 곱함).
	// 예: HitRangeScales [1.0, 1.3, 1.6] + HitWidthScales [1.0, 1.0, 1.8] → 3타 폭 = 1.6 × 1.8배.
	// 비워두면 1.0. 배열보다 히트가 많으면 마지막 값을 계속 씁니다.
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Pattern")
	TArray<float> HitWidthScales;

	// ★ 몽타주를 좌우 반전해서 재생 (예: 오른손 도끼 던지기 → 왼손). 몽타주가 재생되는 동안 AnimBP의 Mirror 노드로
	// 포즈를 좌우 반전하므로 별도 애니메이션이 필요 없습니다. 반전된 포즈에서는 반대쪽 손이 동작하므로
	// 본 이름도 반대쪽으로 맞춰주세요 (예: AxeWeaponBoneName = weapon_l)
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Pattern")
	bool bMirrorAnimation = false;

	// ★ 공격 중 전진 속도(cm/s). 0보다 크면 몽타주가 재생되는 동안 보스가 정면 방향으로 이 속도로 천천히
	// 전진합니다(ARLEnemyCharacter::StartAttackChase). 정면 방향은 텔레그래프 회전 로직(히트 사이엔 대상 추적,
	// 데칼이 뜨면 고정)을 그대로 따르므로 "따라오면서 휘두르는" 연출이 됩니다. 0 = 제자리 공격(기존 동작).
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Pattern|Chase", meta = (ClampMin = "0.0"))
	float AttackChaseSpeed = 0.0f;

	// 플레이어와의 2D 거리가 이 값 이하이면 전진을 멈춥니다 (밀고 들어가지 않도록)
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Pattern|Chase", meta = (ClampMin = "0.0", EditCondition = "AttackChaseSpeed > 0"))
	float AttackChaseStopDistance = 150.0f;

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

	// ★ 이 패턴 실행 중 잔몹(지원군)을 소환할지 여부. AttackShape의 근접/원거리 판정과는 완전히
	// 독립적인 별도 효과라서, 예를 들어 Box 근접 판정 + 소환을 동시에 가진 패턴도 만들 수 있습니다
	// (예: Subjugation — 몽타주 중 Notify_Hit에서 근접 타격, 별도의 Notify_Summon에서 잔몹 소환).
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Pattern|Summon")
	bool bSummonsMinions = false;

	// 소환할 잔몹 클래스 (예: BP_Enemy_Ranged)
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Pattern|Summon", meta = (EditCondition = "bSummonsMinions"))
	TSubclassOf<AActor> SummonClass;

	// 한 번에 소환할 잔몹 수
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Pattern|Summon", meta = (EditCondition = "bSummonsMinions", ClampMin = "1"))
	int32 SummonCount = 2;

	// 보스 중심 소환 반경 (이 범위 안의 무작위 지점에 흩뿌려 소환)
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Pattern|Summon", meta = (EditCondition = "bSummonsMinions", ClampMin = "0.0"))
	float SummonRadius = 300.0f;

	// ==========================================
	// ★ 도끼 투척 → 도약 낙하 (AttackShape == AxeLeap) 설정
	// ==========================================
	// 몽타주에 Notify_AxeThrow / Notify_LeapUp / Notify_LeapFall / Notify_Hit 4개가 순서대로 있어야 합니다.
	// 도끼 판정 반지름 = IndicatorDecalSize.Y, 착지 판정 반지름 = 그 × LeapRadiusScale.
	// 착지 데미지는 DamageData, 도끼 데미지는 AxeDamageData. 예고 데칼은 bUseIndicator + IndicatorDecalMaterial을 쓰고,
	// 도끼 범위 데칼은 투척 전~도끼가 박힐 때까지, 착지 범위 데칼은 착지 IndicatorLeadTime초 전부터 표시됩니다.

	// 도끼가 땅에 박힐 때 주는 데미지 (반지름 IndicatorDecalSize.Y)
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Pattern|AxeLeap", meta = (EditCondition = "AttackShape == ERLAttackShape::AxeLeap || AttackShape == ERLAttackShape::AxeReturn"))
	FRLDamageData AxeDamageData;

	// 착지 판정 반지름 배율 (도끼 판정 반지름 기준). 도끼가 박히면 예고 데칼도 이 크기로 커집니다
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Pattern|AxeLeap", meta = (EditCondition = "AttackShape == ERLAttackShape::AxeLeap", ClampMin = "0.1"))
	float LeapRadiusScale = 2.0f;

	// 던질 도끼 액터 클래스 (비우면 ARLThrownAxe 기본 클래스 = 보스 메시에서 도끼 부분만 복사해서 씀)
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Pattern|AxeLeap", meta = (EditCondition = "AttackShape == ERLAttackShape::AxeLeap || AttackShape == ERLAttackShape::AxeReturn"))
	TSubclassOf<ARLThrownAxe> ThrownAxeClass;

	// 던지는 손 도끼의 본 이름. 투척 순간 이 본을 숨기고, 착지하면 다시 보이게 합니다
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Pattern|AxeLeap", meta = (EditCondition = "AttackShape == ERLAttackShape::AxeLeap || AttackShape == ERLAttackShape::AxeReturn"))
	FName AxeWeaponBoneName = FName("weapon_r");

	// 사거리(AttackRange) 끝까지 던질 때 도끼가 날아가는 시간(초). 가까울수록 거리에 비례해 짧아짐 (일정한 속도)
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Pattern|AxeLeap", meta = (EditCondition = "AttackShape == ERLAttackShape::AxeLeap || AttackShape == ERLAttackShape::AxeReturn", ClampMin = "0.05"))
	float AxeFlightTime = 0.45f;

	// 도끼 포물선 최고 높이(cm)
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Pattern|AxeLeap", meta = (EditCondition = "AttackShape == ERLAttackShape::AxeLeap || AttackShape == ERLAttackShape::AxeReturn", ClampMin = "0.0"))
	float AxeArcHeight = 250.0f;

	// 도약 높이(cm). 이 높이까지 올라갔다가 사라지고, 같은 높이에서 데칼 위로 떨어집니다
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Pattern|AxeLeap", meta = (EditCondition = "AttackShape == ERLAttackShape::AxeLeap", ClampMin = "0.0"))
	float LeapHeight = 1500.0f;

	// Notify_LeapUp ~ Notify_LeapFall 구간 중 상승에 쓰는 비율. 나머지 구간은 사라진 채로 체공합니다
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Pattern|AxeLeap", meta = (EditCondition = "AttackShape == ERLAttackShape::AxeLeap", ClampMin = "0.1", ClampMax = "1.0"))
	float LeapRiseRatio = 0.7f;

	// 투척 전 데칼이 플레이어를 따라다니는 동안 보스가 플레이어 쪽으로 도는 속도(도/초)
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Pattern|AxeLeap", meta = (EditCondition = "AttackShape == ERLAttackShape::AxeLeap || AttackShape == ERLAttackShape::AxeReturn", ClampMin = "0.0"))
	float AxeLeapTurnSpeed = 540.0f;

	// ==========================================
	// ★ 도끼 투척 → 회수 (AttackShape == AxeReturn) 설정
	// ==========================================
	// 몽타주에 Notify_AxeThrow / Notify_AxeRecall / Notify_AxeCatch 3개가 순서대로 있어야 합니다.
	// 투척/도끼 판정은 위의 AxeLeap 공용 항목(AxeDamageData, ThrownAxeClass, AxeFlightTime 등)을 그대로 씁니다.

	// 도끼가 돌아오는 경로에 닿았을 때 주는 피해 (한 번 회수하는 동안 대상당 1회)
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Pattern|AxeReturn", meta = (EditCondition = "AttackShape == ERLAttackShape::AxeReturn"))
	FRLDamageData AxeReturnDamageData;

	// 돌아오는 도끼의 판정 반지름(cm). 귀환 경로 예고 데칼의 폭(= 반지름 × 2)도 이 값을 씁니다
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Pattern|AxeReturn", meta = (EditCondition = "AttackShape == ERLAttackShape::AxeReturn", ClampMin = "10.0"))
	float AxeReturnHitRadius = 150.0f;

	// 도끼가 박힌 뒤 회수될 때까지 보스 ~ 도끼 사이에 띄울 직사각형 예고 데칼 (비우면 표시 안 함)
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Pattern|AxeReturn", meta = (EditCondition = "AttackShape == ERLAttackShape::AxeReturn"))
	TObjectPtr<UMaterialInterface> AxeReturnLaneMaterial = nullptr;

	// ==========================================
	// ★ 영혼의 균열(Soul Crack) 강화판 설정
	// ==========================================
	// 강화판은 원본 패턴과 별개의 PatternID + 낮은 SelectionWeight를 가진 새 데이터 행으로 추가합니다
	// (쿨타임이 PatternID 기준이라 원본과 독립적으로 돎). BTT_Attack이 몽타주 재생 직후
	// ARLEnemyCharacter::BeginSoulCrackPattern(이 패턴)을 호출하면, bSoulCrackVariant가 true인 행만
	// 녹색 신호(OnSoulCrackVariantStarted) + 균열 판정 구간이 열립니다. false인 행은 아무 일도 안 합니다.

	// 이 패턴이 균열 강화판인지 여부
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Pattern|SoulCrack")
	bool bSoulCrackVariant = false;

	// 균열 판정 구간을 몇 번째 히트 노티파이 앞에 열지 (0부터, -1 = 마지막 히트). 예: SwingCombo 강화판의 3타 = 2 또는 -1
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Pattern|SoulCrack", meta = (EditCondition = "bSoulCrackVariant"))
	int32 SoulCrackHitIndex = -1;

	// 균열 판정 구간 길이(초). [히트 - 이 값, 히트) 동안 균열 스킬이 맞으면 성공 (차징/선딜레이 구간)
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Pattern|SoulCrack", meta = (EditCondition = "bSoulCrackVariant", ClampMin = "0.0"))
	float SoulCrackWindowLeadTime = 1.5f;

	// 균열 성공 가능한 "정면" 구역의 전체 폭(도). 예: 90 = 정면 기준 좌우 45도씩. 0이면 정면 불가
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Pattern|SoulCrack", meta = (EditCondition = "bSoulCrackVariant", ClampMin = "0.0", ClampMax = "360.0"))
	float SoulCrackFrontAngle = 90.0f;

	// 균열 성공 가능한 "후방" 구역의 전체 폭(도). 0이면 후방 불가 (예: BigSwing 강화판만 정면+후방 허용)
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Pattern|SoulCrack", meta = (EditCondition = "bSoulCrackVariant", ClampMin = "0.0", ClampMax = "360.0"))
	float SoulCrackBackAngle = 0.0f;
};