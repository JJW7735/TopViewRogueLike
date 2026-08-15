#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "RLDecalIndicator.generated.h"

class UDecalComponent;
class USceneComponent;

// 인디케이터 형태 구분
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

    // 마우스 커서 위치 및 플레이어 위치를 받아 인디케이터 좌표/회전 갱신
    UFUNCTION(BlueprintCallable, Category = "Indicator")
    void UpdateIndicatorTransform(const FVector& OriginLocation, const FVector& TargetLocation, float MaxRange);

    // 반경(크기) 및 형태 설정
    UFUNCTION(BlueprintCallable, Category = "Indicator")
    void SetupIndicator(EIndicatorType Type, float Radius, float Length = 0.f);

    // 머티리얼 변경 (스킬별 전용 마법진/화살표 교체용)
    UFUNCTION(BlueprintCallable, Category = "Indicator")
    void SetIndicatorMaterial(UMaterialInterface* NewMaterial);

    // 활성화 / 비활성화
    UFUNCTION(BlueprintCallable, Category = "Indicator")
    void SetIndicatorVisibility(bool bVisible);

protected:
    virtual void BeginPlay() override;

    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Components")
    USceneComponent* SceneRoot;

    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Components")
    UDecalComponent* DecalComp;

    UPROPERTY(EditDefaultsOnly, Category = "Indicator")
    EIndicatorType CurrentType = EIndicatorType::Circle;

private:
    UPROPERTY()
    UMaterialInstanceDynamic* DynamicMaterial;
};