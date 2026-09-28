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
    void SetupIndicator(EIndicatorType Type, float Radius, float MaxRange);
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
};