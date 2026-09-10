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