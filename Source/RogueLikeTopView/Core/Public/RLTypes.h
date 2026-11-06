#pragma once

#include "CoreMinimal.h"
#include "RLTypes.generated.h"

class UAnimMontage;
class UMaterialInterface;
class ARLThrownAxe;
class UNiagaraSystem;
class UStaticMesh;

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
	AxeReturn,  // ★ 도끼 투척 → 회수. AxeLeap과 투척까지는 같고, Notify_AxeRecall에서 박힌 도끼가 흔들리며 회수 예고
	            // 데칼(도끼 주변 원형 + 도끼~보스 직사각형)이 뜹니다. Notify_AxeCatch 직전(던질 때와 같은 속도로 역산)에
	            // 도끼가 뽑히며 원형 범위 + 돌아오는 경로에 AxeReturnDamageData 피해, Notify_AxeCatch 순간 손에 도착
	            // (예: 보스2 AxeReturn)
	AxeDoubleReturn // ★ 쌍도끼 투척 → 동시 회수. AxeReturn과 같지만 Notify_AxeThrow가 두 번(오른손 → 왼손) 있고,
	            // 두 도끼가 플레이어 위치(AxeDoubleSideOffset만큼 좌우로 벌릴 수 있음)에 박혔다가 각자 던진 손으로 동시에 돌아옵니다.
	            // 왼손 투척은 몽타주의 Notify_MirrorStart ~ Notify_MirrorEnd 구간을 좌우 반전해서 표현 (예: 보스2 AxeDouble)
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

	// ★ 피격 대상 경직 시간(초). 0보다 크면 넉백 후 이 시간 동안 State.Stunned (이동/회피/스킬 불가).
	// 경직 면역(CanBeStaggered == false: 보스/엘리트)인 대상에게는 무시됩니다. 예: 보스2 울부짖음
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Damage", meta = (ClampMin = "0.0"))
	float StunDuration = 0.0f;

	// ★ 일정 거리 밀어내기. true면 KnockbackForce(튕겨내기) 대신, 판정 중심(원형 판정의 중심 / 박스 판정은 공격자)에서
	// 바깥 방향으로 PushAwayDistance만큼 PushAwayDuration초 동안 미끄러지듯 밀어냄. 반복 판정(패턴 PulseInterval)과
	// 같이 쓰면 "범위 안에 있는 동안 계속 조금씩 밀려나는" 장판이 됨. 네비메시 경계(낭떠러지)와 벽은 넘지 않음. 예: 보스2 울부짖음
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Damage")
	bool bPushAwayFromCenter = false;

	// 한 번 맞을 때 밀려나는 거리(cm)
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Damage", meta = (EditCondition = "bPushAwayFromCenter", ClampMin = "0.0"))
	float PushAwayDistance = 150.0f;

	// 밀려나는 데 걸리는 시간(초). 반복 판정 간격보다 짧게 두면 끊김 없이 이어서 밀림
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Damage", meta = (EditCondition = "bPushAwayFromCenter", ClampMin = "0.05"))
	float PushAwayDuration = 0.12f;

	// 판정 함수(Create*Hitbox)가 채워 넣는 밀어내기 중심 (직접 편집하지 않음)
	UPROPERTY(BlueprintReadOnly, Category = "Damage")
	FVector PushOrigin = FVector::ZeroVector;

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

	// ★ 반복 판정(펄스) 간격(초). 0보다 크면 몽타주의 Notify_PulseStart ~ Notify_PulseEnd 구간 동안 이 간격마다 같은
	// 판정(AttackShape가 Sector 또는 Box)을 반복합니다. 예고 데칼은 구간 시작 IndicatorLeadTime초 전부터 구간 끝까지 계속 표시.
	// Notify_Hit 없이 단독으로 쓰는 용도 (Notify_Hit도 있으면 그 타격은 따로 한 번 더 들어감). 예: 보스2 울부짖음
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Pattern|Pulse", meta = (ClampMin = "0.0"))
	float PulseInterval = 0.0f;

	// ★ 반복 판정 구간 동안 띄울 돔(반구) 이펙트 머티리얼 (Sector 전용, 반지름 = 판정 반지름). 비우면 표시 안 함.
	// 반투명 머티리얼을 넣으세요 (예: MI_Howl_Dome). 판정할 때마다 살짝 부풀었다 돌아와서 맥동하는 느낌을 줌
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Pattern|Pulse", meta = (EditCondition = "PulseInterval > 0"))
	TObjectPtr<UMaterialInterface> PulseDomeMaterial = nullptr;

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

	// ★ 연타 반복(콤보 루프). 0보다 크면 몽타주의 ComboLoopSection 섹션을 이 횟수만큼 반복하고(한 바퀴마다 재생 속도
	// +ComboPlayRateStep, 최대 ComboMaxPlayRate), 끝나면 ComboFinishSection 섹션(마무리 공격)을 ComboFinishPlayRate로 재생.
	// 몽타주에 두 섹션을 만들고 각 타격에 Notify_Hit을 넣으면 됨 (예: 보스2 광란 연타 = Melee A/B/C 반복 → 마무리)
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Pattern|ComboLoop", meta = (ClampMin = "0"))
	int32 ComboLoopCount = 0;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Pattern|ComboLoop", meta = (EditCondition = "ComboLoopCount > 0"))
	FName ComboLoopSection = FName("Loop");

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Pattern|ComboLoop", meta = (EditCondition = "ComboLoopCount > 0"))
	FName ComboFinishSection = FName("Finish");

	// 한 바퀴 돌 때마다 더할 재생 속도
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Pattern|ComboLoop", meta = (EditCondition = "ComboLoopCount > 0", ClampMin = "0.0"))
	float ComboPlayRateStep = 0.15f;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Pattern|ComboLoop", meta = (EditCondition = "ComboLoopCount > 0", ClampMin = "0.1"))
	float ComboMaxPlayRate = 2.0f;

	// 마무리 섹션 재생 속도
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Pattern|ComboLoop", meta = (EditCondition = "ComboLoopCount > 0", ClampMin = "0.1"))
	float ComboFinishPlayRate = 1.0f;

	// ★ 한 바퀴 돌 때마다 타격 범위(판정+데칼)에 더할 배율. 마지막 타격(마무리)에는 적용 안 됨
	// (마지막 바퀴에 Finish 섹션에서 이어지는 타격도 그 바퀴 크기로 나감).
	// 예: 0.05면 1바퀴 1.0 → 2바퀴 1.05 → 3바퀴 1.1 ... (ComboRangeMaxScale까지)
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Pattern|ComboLoop", meta = (EditCondition = "ComboLoopCount > 0", ClampMin = "0.0"))
	float ComboRangeScaleStep = 0.0f;

	// 반복 구간 범위 배율 상한 (마무리 공격의 HitRangeScales보다 작게 두면 마무리가 항상 더 큼)
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Pattern|ComboLoop", meta = (EditCondition = "ComboLoopCount > 0", ClampMin = "1.0"))
	float ComboRangeMaxScale = 1.5f;

	// ★ 이 패턴이 끝나면 공격 후 대기(BT의 Wait)를 건너뛰고 바로 다음 패턴으로 이어감. 거리 좁히기용 돌진처럼
	// 다음 공격으로 연결하는 패턴에 사용 (경직/사망 등으로 끊기면 적용 안 됨). 예: 보스2 Pounce
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Pattern")
	bool bChainNextAttack = false;

	// 공격이 빗나갔을 때만 다음 패턴으로 연결 (맞았으면 모션을 끝까지 재생하고 평소처럼 대기). 예: 보스2 Pounce
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Pattern", meta = (EditCondition = "bChainNextAttack"))
	bool bChainOnlyOnMiss = false;

	// 마지막 공격 노티파이(Notify_DashStart / Notify_Hit) 후 이 시간(초)이 지나면 남은 모션을 끊고 다음 패턴으로.
	// 몽타주에 Notify_ChainCancel을 넣으면 그 위치가 우선. 공격 노티파이가 없으면 몽타주가 끝나고 이 시간 뒤
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Pattern", meta = (EditCondition = "bChainNextAttack", ClampMin = "0.0"))
	float ChainNextAttackDelay = 0.5f;

	// ★ Dash 전용: 돌진 거리(IndicatorDecalSize.X × 2)는 그대로 두고, 판정과 예고 데칼만 정면으로 이만큼 더 늘림(cm).
	// 도끼를 앞으로 내려치는 도약처럼 몸보다 앞까지 닿는 돌진에 사용 (예: 보스2 Pounce)
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Pattern", meta = (EditCondition = "AttackShape == ERLAttackShape::Dash", ClampMin = "0.0"))
	float DashForwardReach = 0.0f;

	// ★ 타격마다 한 걸음 내딛기(cm). 0보다 크면 각 Notify_Hit 직전(AttackStepLeadTime초 전)에 정면으로 이만큼
	// AttackStepDuration초 동안 짧게 이동. 계속 미끄러지는 AttackChaseSpeed 대신 "한 대 칠 때마다 한 발" 연출 (예: 보스2 연타).
	// 플레이어에게 AttackChaseStopDistance보다 가까워지지 않게 줄여서 내딛음.
	// 내딛는 동안은 이동 중으로 처리되므로, 몽타주 슬롯을 UpperBody로 두면 하체는 이동 모션 + 상체는 공격으로 섞임
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Pattern|Chase", meta = (ClampMin = "0.0"))
	float AttackStepDistance = 0.0f;

	// 타격 몇 초 전에 내딛기 시작할지
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Pattern|Chase", meta = (EditCondition = "AttackStepDistance > 0", ClampMin = "0.0"))
	float AttackStepLeadTime = 0.15f;

	// 한 걸음에 걸리는 시간(초)
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Pattern|Chase", meta = (EditCondition = "AttackStepDistance > 0", ClampMin = "0.05"))
	float AttackStepDuration = 0.15f;

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
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Pattern|AxeLeap", meta = (EditCondition = "AttackShape == ERLAttackShape::AxeLeap || AttackShape == ERLAttackShape::AxeReturn || AttackShape == ERLAttackShape::AxeDoubleReturn"))
	FRLDamageData AxeDamageData;

	// 착지 판정 반지름 배율 (도끼 판정 반지름 기준). 도끼가 박히면 예고 데칼도 이 크기로 커집니다
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Pattern|AxeLeap", meta = (EditCondition = "AttackShape == ERLAttackShape::AxeLeap", ClampMin = "0.1"))
	float LeapRadiusScale = 2.0f;

	// 던질 도끼 액터 클래스 (비우면 ARLThrownAxe 기본 클래스 = 보스 메시에서 도끼 부분만 복사해서 씀)
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Pattern|AxeLeap", meta = (EditCondition = "AttackShape == ERLAttackShape::AxeLeap || AttackShape == ERLAttackShape::AxeReturn || AttackShape == ERLAttackShape::AxeDoubleReturn"))
	TSubclassOf<ARLThrownAxe> ThrownAxeClass;

	// 던지는 손 도끼의 본 이름. 투척 순간 이 본을 숨기고, 착지하면 다시 보이게 합니다
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Pattern|AxeLeap", meta = (EditCondition = "AttackShape == ERLAttackShape::AxeLeap || AttackShape == ERLAttackShape::AxeReturn || AttackShape == ERLAttackShape::AxeDoubleReturn"))
	FName AxeWeaponBoneName = FName("weapon_r");

	// 사거리(AttackRange) 끝까지 던질 때 도끼가 날아가는 시간(초). 가까울수록 거리에 비례해 짧아짐 (일정한 속도)
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Pattern|AxeLeap", meta = (EditCondition = "AttackShape == ERLAttackShape::AxeLeap || AttackShape == ERLAttackShape::AxeReturn || AttackShape == ERLAttackShape::AxeDoubleReturn", ClampMin = "0.05"))
	float AxeFlightTime = 0.45f;

	// 도끼 포물선 최고 높이(cm)
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Pattern|AxeLeap", meta = (EditCondition = "AttackShape == ERLAttackShape::AxeLeap || AttackShape == ERLAttackShape::AxeReturn || AttackShape == ERLAttackShape::AxeDoubleReturn", ClampMin = "0.0"))
	float AxeArcHeight = 250.0f;

	// 도약 높이(cm). 이 높이까지 올라갔다가 사라지고, 같은 높이에서 데칼 위로 떨어집니다
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Pattern|AxeLeap", meta = (EditCondition = "AttackShape == ERLAttackShape::AxeLeap", ClampMin = "0.0"))
	float LeapHeight = 1500.0f;

	// Notify_LeapUp ~ Notify_LeapFall 구간 중 상승에 쓰는 비율. 나머지 구간은 사라진 채로 체공합니다
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Pattern|AxeLeap", meta = (EditCondition = "AttackShape == ERLAttackShape::AxeLeap", ClampMin = "0.1", ClampMax = "1.0"))
	float LeapRiseRatio = 0.7f;

	// 투척 전 데칼이 플레이어를 따라다니는 동안 보스가 플레이어 쪽으로 도는 속도(도/초)
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Pattern|AxeLeap", meta = (EditCondition = "AttackShape == ERLAttackShape::AxeLeap || AttackShape == ERLAttackShape::AxeReturn || AttackShape == ERLAttackShape::AxeDoubleReturn", ClampMin = "0.0"))
	float AxeLeapTurnSpeed = 540.0f;

	// ==========================================
	// ★ 도끼 투척 → 회수 (AttackShape == AxeReturn) 설정
	// ==========================================
	// 몽타주에 Notify_AxeThrow / Notify_AxeRecall / Notify_AxeCatch 3개가 순서대로 있어야 합니다.
	// 투척/도끼 판정은 위의 AxeLeap 공용 항목(AxeDamageData, ThrownAxeClass, AxeFlightTime 등)을 그대로 씁니다.

	// 도끼가 뽑힐 때 원형 범위(IndicatorDecalSize.Y) + 돌아오는 경로에 닿았을 때 주는 피해 (한 번 회수하는 동안 대상당 1회)
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Pattern|AxeReturn", meta = (EditCondition = "AttackShape == ERLAttackShape::AxeReturn || AttackShape == ERLAttackShape::AxeDoubleReturn"))
	FRLDamageData AxeReturnDamageData;

	// 돌아오는 도끼의 판정 반지름(cm). 귀환 경로 예고 데칼의 폭(= 반지름 × 2)도 이 값을 씁니다
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Pattern|AxeReturn", meta = (EditCondition = "AttackShape == ERLAttackShape::AxeReturn || AttackShape == ERLAttackShape::AxeDoubleReturn", ClampMin = "10.0"))
	float AxeReturnHitRadius = 150.0f;

	// Notify_AxeRecall부터 회수될 때까지 도끼 ~ 보스 사이에 띄울 직사각형 예고 데칼 (비우면 표시 안 함)
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Pattern|AxeReturn", meta = (EditCondition = "AttackShape == ERLAttackShape::AxeReturn || AttackShape == ERLAttackShape::AxeDoubleReturn"))
	TObjectPtr<UMaterialInterface> AxeReturnLaneMaterial = nullptr;

	// ★ 회수 경로를 플레이어 쪽으로 휘게 할지. Notify_AxeRecall 순간 플레이어가 도끼와 보스 사이에 있으면
	// "도끼 → 그 순간 플레이어 위치 → 보스"를 지나는 곡선으로 돌아오고(예고 데칼도 곡선), 사이에 없으면 직선
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Pattern|AxeReturn", meta = (EditCondition = "AttackShape == ERLAttackShape::AxeReturn || AttackShape == ERLAttackShape::AxeDoubleReturn"))
	bool bAxeReturnCurveThroughPlayer = false;

	// 곡선으로 휘는 최대 거리(cm). 플레이어가 도끼~보스 직선에서 이보다 옆으로 멀리 있으면 "사이에 없음" = 직선
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Pattern|AxeReturn", meta = (EditCondition = "bAxeReturnCurveThroughPlayer", ClampMin = "0.0"))
	float AxeReturnCurveMaxOffset = 700.0f;

	// ==========================================
	// ★ 쌍도끼 투척 → 동시 회수 (AttackShape == AxeDoubleReturn) 설정
	// ==========================================
	// 몽타주: Notify_AxeThrow(오른손) → Notify_MirrorStart → Notify_AxeThrow(왼손) → Notify_MirrorEnd → Notify_AxeRecall → Notify_AxeCatch.
	// 첫 번째 도끼는 위의 AxeWeaponBoneName/ThrownAxeClass, 두 번째 도끼는 아래 Second* 항목을 씁니다.

	// 플레이어 위치에서 좌우로 벌려 박을 거리(cm). 오른손 도끼는 보스 시점 왼쪽, 왼손 도끼는 오른쪽에 박힘.
	// 0이면 두 도끼 모두 각자 던지는 순간의 플레이어 위치를 노림 (bAxeReturnCurveThroughPlayer와 함께 쓰면 좋음)
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Pattern|AxeDouble", meta = (EditCondition = "AttackShape == ERLAttackShape::AxeDoubleReturn", ClampMin = "0.0"))
	float AxeDoubleSideOffset = 400.0f;

	// 두 번째(왼손) 도끼 본 이름
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Pattern|AxeDouble", meta = (EditCondition = "AttackShape == ERLAttackShape::AxeDoubleReturn"))
	FName SecondAxeWeaponBoneName = FName("weapon_l");

	// 두 번째(왼손) 도끼 액터 클래스 (본 축이 오른손과 반대라 보통 왼손 전용 BP를 넣음. 비우면 ThrownAxeClass 사용)
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Pattern|AxeDouble", meta = (EditCondition = "AttackShape == ERLAttackShape::AxeDoubleReturn"))
	TSubclassOf<ARLThrownAxe> SecondThrownAxeClass;

	// ==========================================
	// ★ 잠행 강습(Vanish Strike): 몽타주의 Notify_Vanish 지점에서 몽타주를 멈추고 보스가 어둠 속으로 가라앉아 사라짐
	// → 플레이어 머리 위에 표식 → VanishHiddenTime 뒤 플레이어 등 뒤에서 솟아오르며 나타나 몽타주 이어서 재생.
	// 사라져 있는 동안은 무적 + 플레이어와 충돌 안 함. 예: 보스2 처형 일격 (R_Ability_Intro → 사라짐 → R_Ability)
	// ==========================================

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Pattern|Vanish")
	bool bVanishStrike = false;

	// 가라앉으며 사라지는 시간 (이 동안 어두운 오버레이가 짙어짐)
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Pattern|Vanish", meta = (EditCondition = "bVanishStrike", ClampMin = "0.0"))
	float VanishFadeOutTime = 0.5f;

	// 완전히 사라져 있는 시간 (표식이 떠 있는 시간)
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Pattern|Vanish", meta = (EditCondition = "bVanishStrike", ClampMin = "0.0"))
	float VanishHiddenTime = 2.0f;

	// 솟아오르며 나타나는 시간 (몽타주는 나타나기 시작할 때 바로 이어서 재생)
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Pattern|Vanish", meta = (EditCondition = "bVanishStrike", ClampMin = "0.0"))
	float VanishFadeInTime = 0.3f;

	// 가라앉는 깊이(cm). 0이면 캡슐 전체 높이만큼
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Pattern|Vanish", meta = (EditCondition = "bVanishStrike", ClampMin = "0.0"))
	float VanishSinkDepth = 0.0f;

	// 플레이어 등 뒤 거리(cm, 두 캡슐 표면 사이). 플레이어가 바라보는 방향의 반대편에 나타남
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Pattern|Vanish", meta = (EditCondition = "bVanishStrike", ClampMin = "0.0"))
	float VanishAppearDistance = 120.0f;

	// 사라지고 나타날 때 몸에 씌우는 어두운 오버레이 (M_SoulCrack_Overlay 계열 MI. 비우면 오버레이 없이 가라앉기만)
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Pattern|Vanish", meta = (EditCondition = "bVanishStrike"))
	TObjectPtr<UMaterialInterface> VanishOverlayMaterial = nullptr;

	// 사라지는 위치 / 나타나는 위치(발밑)에 재생할 이펙트 (선택)
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Pattern|Vanish", meta = (EditCondition = "bVanishStrike"))
	TObjectPtr<UNiagaraSystem> VanishFX = nullptr;

	// 이펙트를 가라앉기/솟아오르기 시작보다 몇 초 먼저 터뜨릴지 (연기가 먼저 피어오른 뒤 그 속으로 사라지고, 그 속에서 나타남).
	// 나타날 위치는 이 시점에 미리 정해짐 (연기가 나타날 자리를 예고)
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Pattern|Vanish", meta = (EditCondition = "bVanishStrike", ClampMin = "0.0"))
	float VanishFXLeadTime = 0.4f;

	// 이펙트 크기 배율 (나이아가라 유저 파라미터 User.Scale_All에 전달. 해당 파라미터가 없는 시스템은 영향 없음)
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Pattern|Vanish", meta = (EditCondition = "bVanishStrike", ClampMin = "0.01"))
	float VanishFXScale = 3.0f;

	// ★ 잠행 중 주변 어둡게: 사라지기 연기가 피어오를 때부터 어두워지기 시작해 숨어 있는 동안 유지되고, 다시 나타나면
	// 밝아짐. 노출 보정값(음수일수록 어두움, 0 = 끔)과 화면 가장자리 비네팅 세기
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Pattern|Vanish", meta = (EditCondition = "bVanishStrike", ClampMax = "0.0"))
	float VanishDarkenExposure = -1.5f;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Pattern|Vanish", meta = (EditCondition = "bVanishStrike", ClampMin = "0.0"))
	float VanishDarkenVignette = 1.0f;

	// 어두워지고 / 밝아지는 데 걸리는 시간
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Pattern|Vanish", meta = (EditCondition = "bVanishStrike", ClampMin = "0.01"))
	float VanishDarkenFadeTime = 0.5f;

	// 플레이어 머리 위 표식 메시 / 머티리얼 / 크기 / 머리 위 높이. 메시를 비우면 엔진 기본 원뿔(거꾸로)
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Pattern|Vanish", meta = (EditCondition = "bVanishStrike"))
	TObjectPtr<UStaticMesh> VanishMarkMesh = nullptr;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Pattern|Vanish", meta = (EditCondition = "bVanishStrike"))
	TObjectPtr<UMaterialInterface> VanishMarkMaterial = nullptr;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Pattern|Vanish", meta = (EditCondition = "bVanishStrike", ClampMin = "0.01"))
	float VanishMarkScale = 0.6f;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Pattern|Vanish", meta = (EditCondition = "bVanishStrike"))
	float VanishMarkHeight = 80.0f;

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