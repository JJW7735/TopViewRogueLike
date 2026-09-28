#include "RLDecalIndicator.h"
#include "Components/DecalComponent.h"
#include "Components/SceneComponent.h"
#include "UObject/ConstructorHelpers.h"

ARLDecalIndicator::ARLDecalIndicator()
{
    PrimaryActorTick.bCanEverTick = false;

    SceneRoot = CreateDefaultSubobject<USceneComponent>(TEXT("SceneRoot"));
    SetRootComponent(SceneRoot);

    DecalComp = CreateDefaultSubobject<UDecalComponent>(TEXT("DecalComp"));
    DecalComp->SetupAttachment(SceneRoot);
    DecalComp->SetRelativeRotation(FRotator(-90.f, 0.f, 0.f));
    DecalComp->DecalSize = FVector(250.f, 150.f, 150.f);

    RangeDecalComp = CreateDefaultSubobject<UDecalComponent>(TEXT("RangeDecalComp"));
    RangeDecalComp->SetupAttachment(SceneRoot);
    RangeDecalComp->SetRelativeRotation(FRotator(-90.f, 0.f, 0.f));
    RangeDecalComp->DecalSize = FVector(250.f, 800.f, 800.f);

    static ConstructorHelpers::FObjectFinder<UMaterialInterface> CircleMat(TEXT("/Game/Effect/Material/M_Decal_Circle.M_Decal_Circle"));
    if (CircleMat.Succeeded())
    {
        DecalComp->SetDecalMaterial(CircleMat.Object);
    }

    static ConstructorHelpers::FObjectFinder<UMaterialInterface> RangeMat(TEXT("/Game/Effect/Material/M_Decal_RangeRing.M_Decal_RangeRing"));
    if (RangeMat.Succeeded())
    {
        RangeDecalComp->SetDecalMaterial(RangeMat.Object);
    }
}

void ARLDecalIndicator::SetupIndicator(EIndicatorType Type, float Radius, float MaxRange)
{
    CurrentType = Type;

    if (CurrentType == EIndicatorType::Circle)
    {
        DecalComp->DecalSize = FVector(250.f, Radius, Radius);

        if (MaxRange > 0.0f)
        {
            RangeDecalComp->SetVisibility(true);
            RangeDecalComp->DecalSize = FVector(250.f, MaxRange, MaxRange);
        }
        else
        {
            RangeDecalComp->SetVisibility(false);
        }
    }
    else if (CurrentType == EIndicatorType::Arrow)
    {
        RangeDecalComp->SetVisibility(false);
        DecalComp->DecalSize = FVector(250.f, MaxRange * 0.5f, Radius);
    }
}

void ARLDecalIndicator::UpdateIndicatorTransform(const FVector& OriginLocation, const FVector& TargetLocation, float MaxRange)
{
    FVector FinalTargetLocation = TargetLocation;
    FVector Direction2D = (TargetLocation - OriginLocation);
    Direction2D.Z = 0.f;

    float Distance2D = Direction2D.Size();

    if (MaxRange > 0.0f && Distance2D > MaxRange)
    {
        FinalTargetLocation = OriginLocation + (Direction2D.GetSafeNormal() * MaxRange);
        FinalTargetLocation.Z = TargetLocation.Z;
    }

    // ★ 조준 좌표 저장
    CurrentTargetLocation = FinalTargetLocation;

    if (CurrentType == EIndicatorType::Circle)
    {
        RangeDecalComp->SetWorldLocation(OriginLocation);
        RangeDecalComp->SetWorldRotation(FRotator(-90.f, 0.f, 0.f));

        DecalComp->SetWorldLocation(FinalTargetLocation);
        DecalComp->SetWorldRotation(FRotator(-90.f, 0.f, 0.f));
    }
}

void ARLDecalIndicator::SetupBoxIndicator(const FVector& BoxExtent, UMaterialInterface* Material)
{
    CurrentType = EIndicatorType::Box;

    // ★ DecalComponent는 생성자에서 RelativeRotation = (Pitch -90, Yaw 0, Roll 0)으로 "바닥을 내려다보는"
    // 상태입니다. 여기에 UpdateBoxTransform에서 소유자 Yaw를 추가로 곱해주면(아래 함수 참고),
    // 로컬 축이 다음과 같이 정렬됩니다 (UE FRotator 합성 규칙: Yaw는 월드 Z축 기준 마지막에 적용됨):
    //   - 로컬 X(투사 방향/깊이) → 항상 월드 -Z(아래) : DecalSize.X는 "두께"라서 값 자체는 안 중요하고
    //     바닥 굴곡을 덮을 만큼만 크면 됩니다.
    //   - 로컬 Y → 소유자의 RightVector 방향 → 즉 "좌우 폭"
    //   - 로컬 Z → 소유자의 ForwardVector 방향 → 즉 "전방 길이"
    // 그래서 CreateBoxHitbox의 BoxExtent(X=전방 절반길이, Y=좌우 절반폭)를 DecalSize.Z, DecalSize.Y에
    // 그대로 매핑합니다. (주의: 이 축 매핑은 FRotator 합성 규칙을 손으로 유도한 것이라, PIE에서 처음
    // 켜봤을 때 가로/세로가 뒤바뀐 것처럼 보이면 이 함수의 BoxExtent.X ↔ BoxExtent.Y만 서로 바꿔서
    // 다시 테스트해보면 됩니다.)
    constexpr float VerticalThickness = 200.f;
    DecalComp->DecalSize = FVector(VerticalThickness, BoxExtent.Y, BoxExtent.X);

    if (Material)
    {
        DecalComp->SetDecalMaterial(Material);
    }

    // 사각형 판정에는 원형 사거리 링이 필요 없으므로 꺼둡니다.
    RangeDecalComp->SetVisibility(false);
}

void ARLDecalIndicator::UpdateBoxTransform(const FVector& OriginLocation, float ForwardOffset, float YawDegrees)
{
    const FRotator YawOnlyRotation(0.f, YawDegrees, 0.f);
    const FVector ForwardVector = YawOnlyRotation.Vector();
    const FVector Center = OriginLocation + (ForwardVector * ForwardOffset);

    CurrentTargetLocation = Center;

    DecalComp->SetWorldLocation(Center);
    DecalComp->SetWorldRotation(FRotator(-90.f, YawDegrees, 0.f));
}

void ARLDecalIndicator::SetIndicatorVisibility(bool bVisible)
{
    SetActorHiddenInGame(!bVisible);
    DecalComp->SetVisibility(bVisible);
    RangeDecalComp->SetVisibility(bVisible);
}

void ARLDecalIndicator::LockTargetLocation(const FVector& LockedLocation)
{
    CurrentTargetLocation = LockedLocation;

    if (DecalComp)
    {
        DecalComp->SetWorldLocation(LockedLocation);
        DecalComp->SetWorldRotation(FRotator(-90.f, 0.f, 0.f));
        DecalComp->SetVisibility(true);
    }

    // 캐스팅 중에는 플레이어 발밑의 사거리 원 숨김
    if (RangeDecalComp)
    {
        RangeDecalComp->SetVisibility(false);
    }
}