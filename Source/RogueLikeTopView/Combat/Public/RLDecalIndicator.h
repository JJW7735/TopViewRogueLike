#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "RLDecalIndicator.generated.h"

class UDecalComponent;
class USceneComponent;
class UMaterialInterface;

UENUM(BlueprintType)
enum class EIndicatorType : uint8
{
    Circle UMETA(DisplayName = "원형 장판 (Circle)"),
    Arrow  UMETA(DisplayName = "직선/화살표 (Arrow/Line)"),
    Cone   UMETA(DisplayName = "부채꼴 (Cone/Sector)"),
    Box    UMETA(DisplayName = "직사각형 (Box)")
};

UCLASS()
class ROGUELIKETOPVIEW_API ARLDecalIndicator : public AActor
{
    GENERATED_BODY()

public:
    ARLDecalIndicator();

    void UpdateIndicatorTransform(const FVector& OriginLocation, const FVector& TargetLocation, float MaxRange);

    // ★ Material 파라미터 추가 (기본 nullptr = 기존 동작 그대로, 생성자에서 세팅된 기본 원형 머티리얼
    // M_Decal_Circle 유지). 기존 호출부(URLSkillComponent, 플레이어 스킬 조준용)는 이 값을 안 넘기므로
    // 영향 없음. 보스 등이 전용 색(위험 신호용 빨간 계열)을 쓰고 싶을 때만 넘기면 됩니다.
    void SetupIndicator(EIndicatorType Type, float Radius, float MaxRange, UMaterialInterface* Material = nullptr);
    void SetIndicatorVisibility(bool bVisible);

    // ★ 이 함수 선언 추가!
    void LockTargetLocation(const FVector& LockedLocation);

    // ★ 근접 박스 판정(ARLCharacter::CreateBoxHitbox) 예고용 사각형 데칼.
    // BoxExtent는 CreateBoxHitbox와 완전히 같은 규격입니다: X = 전방 절반 길이, Y = 좌우 절반 폭
    // (Z는 사용하지 않음). BTT_Attack에서 CreateBoxHitbox에 넘기는 값을 그대로 넘기면
    // 실제 판정 박스와 항상 동일한 크기의 데칼이 나옵니다.
    // Material을 nullptr로 넘기면 기존에 SetDecalMaterial로 설정돼있던 머티리얼을 그대로 씁니다.
    void SetupBoxIndicator(const FVector& BoxExtent, UMaterialInterface* Material = nullptr);

    // ★ 사각형 데칼을 소유자(보스 등)의 현재 위치 + 정면 방향(YawDegrees) + ForwardOffset 기준으로
    // 재배치합니다. CreateBoxHitbox(BoxExtent, ForwardOffset, ...)를 호출할 때 쓰는 것과 같은
    // ForwardOffset을 넘기면 데칼 중심이 실제 판정 박스 중심과 일치합니다.
    void UpdateBoxTransform(const FVector& OriginLocation, float ForwardOffset, float YawDegrees);

    // ★ 근접 박스가 아니라 소유자 "자기 자신"을 중심으로 한 부채꼴(전방위) 판정
    // (ARLCharacter::CreateSectorHitbox) 예고용 데칼. Radius는 CreateSectorHitbox와 동일한 반지름,
    // SafeZoneAngleDegrees도 정면 기준 뒤쪽 안전지대 폭(도)으로 동일합니다.
    // 실제 부채꼴(안전지대 쐐기 제외) 모양 자체는 머티리얼에서 UV 각도를 계산해서 마스킹합니다 —
    // 여기서는 데칼을 원형 풋프린트로 깔아두고, 머티리얼이 "SafeZoneAngleDeg"라는 이름의
    // Scalar Parameter를 갖고 있으면 그 값을 넘겨줍니다(없는 머티리얼이면 그냥 무시됨).
    void SetupSectorIndicator(float Radius, float SafeZoneAngleDegrees, UMaterialInterface* Material = nullptr, float InnerSafeRadius = 0.0f);

    // ★ 부채꼴 데칼을 소유자의 현재 위치 + 정면 방향(YawDegrees)에 맞춰 재배치합니다.
    // (ForwardOffset이 없는 이유: Box와 달리 Sector는 소유자 위치가 곧 부채꼴의 중심입니다)
    void UpdateSectorTransform(const FVector& OriginLocation, float YawDegrees);

    virtual FVector GetTargetLocation(AActor* RequestedBy = nullptr) const override { return CurrentTargetLocation; }

protected:
    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Components")
    USceneComponent* SceneRoot;

    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Components")
    UDecalComponent* DecalComp;

    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Components")
    UDecalComponent* RangeDecalComp;

    UPROPERTY(EditDefaultsOnly, Category = "Indicator")
    EIndicatorType CurrentType = EIndicatorType::Circle;

    UPROPERTY()
    FVector CurrentTargetLocation = FVector::ZeroVector;

    // ★ 사거리 링(RangeDecalComp)을 쓰는 인디케이터인지. Setup* 함수들이 세팅하고,
    // SetIndicatorVisibility(true)는 이 값이 true일 때만 링을 켭니다.
    // (Box/Sector에서 링이 켜지면 액터 원점(0,0,0)에 흰 원이 뜨는 버그 방지)
    bool bUseRangeRing = false;
};