#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "RLDecalIndicator.generated.h"

class UDecalComponent;
class USceneComponent;

UENUM(BlueprintType)
enum class EIndicatorType : uint8
{
    Circle UMETA(DisplayName = "원형 장판 (Circle)"),
    Arrow  UMETA(DisplayName = "직선/화살표 (Arrow/Line)"),
    Cone   UMETA(DisplayName = "부채꼴 (Cone/Sector)")
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