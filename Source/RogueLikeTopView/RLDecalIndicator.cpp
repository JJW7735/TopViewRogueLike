#include "RLDecalIndicator.h"
#include "Components/DecalComponent.h"
#include "Components/SceneComponent.h"
#include "Materials/MaterialInstanceDynamic.h"

ARLDecalIndicator::ARLDecalIndicator()
{
    PrimaryActorTick.bCanEverTick = false; // 위치 갱신은 컨트롤러/스킬컴포넌트에서 직접 호출하므로 Tick 끔

    SceneRoot = CreateDefaultSubobject<USceneComponent>(TEXT("SceneRoot"));
    SetRootComponent(SceneRoot);

    DecalComp = CreateDefaultSubobject<UDecalComponent>(TEXT("DecalComp"));
    DecalComp->SetupAttachment(SceneRoot);

    // 언리얼 데칼은 기본적으로 X축 방향으로 투사되므로 바닥을 향하도록 Pitch를 -90도 회전
    DecalComp->SetRelativeRotation(FRotator(-90.f, 0.f, 0.f));

    // 기본 크기 설정 (X: 투사 높이 깊이, Y: 가로 반경, Z: 세로 반경)
    DecalComp->DecalSize = FVector(200.f, 150.f, 150.f);
}

void ARLDecalIndicator::BeginPlay()
{
    Super::BeginPlay();

    // 동적 머티리얼 인스턴스 생성 (색상/투명도 런타임 제어용)
    if (DecalComp && DecalComp->GetDecalMaterial())
    {
        DynamicMaterial = DecalComp->CreateDynamicMaterialInstance();
    }
}

void ARLDecalIndicator::UpdateIndicatorTransform(const FVector& OriginLocation, const FVector& TargetLocation, float MaxRange)
{
    FVector FinalLocation = TargetLocation;
    FVector Direction2D = (TargetLocation - OriginLocation);
    Direction2D.Z = 0.f;

    float Distance2D = Direction2D.Size();

    // 1. 최대 사거리(MaxRange) 제한 (사거리를 벗어나면 최대 사거리 지점으로 고정)
    if (MaxRange > 0.0f && Distance2D > MaxRange)
    {
        FinalLocation = OriginLocation + (Direction2D.GetSafeNormal() * MaxRange);
        FinalLocation.Z = TargetLocation.Z; // 높이는 바닥 위치 유지
    }

    // 2. 인디케이터 타입별 위치/회전 배치
    if (CurrentType == EIndicatorType::Circle)
    {
        // 원형 장판: 마우스 타겟 위치에 바로 배치
        SetActorLocation(FinalLocation);
        SetActorRotation(FRotator::ZeroRotator);
    }
    else // Arrow, Cone (방향성 인디케이터)
    {
        // 화살표/부채꼴: 플레이어 발밑에 두고 마우스 방향을 바라보게 회전
        SetActorLocation(OriginLocation);
        if (!Direction2D.IsNearlyZero())
        {
            SetActorRotation(Direction2D.Rotation());
        }
    }
}

void ARLDecalIndicator::SetupIndicator(EIndicatorType Type, float Radius, float Length)
{
    CurrentType = Type;

    if (CurrentType == EIndicatorType::Circle)
    {
        // 원형: Y와 Z를 동일한 Radius로 설정
        DecalComp->DecalSize = FVector(250.f, Radius, Radius);
    }
    else if (CurrentType == EIndicatorType::Arrow)
    {
        // 직선형: Length(길이)와 Radius(두께) 적용
        float Width = (Radius > 0.f) ? Radius : 80.f;
        DecalComp->DecalSize = FVector(250.f, Length * 0.5f, Width);
        // 화살표 중심점을 플레이어 전방으로 밀어주기 위한 상대 위치 조정 가능
    }
}

void ARLDecalIndicator::SetIndicatorMaterial(UMaterialInterface* NewMaterial)
{
    if (DecalComp && NewMaterial)
    {
        DecalComp->SetDecalMaterial(NewMaterial);
        DynamicMaterial = DecalComp->CreateDynamicMaterialInstance();
    }
}

void ARLDecalIndicator::SetIndicatorVisibility(bool bVisible)
{
    SetActorHiddenInGame(!bVisible);
    DecalComp->SetVisibility(bVisible);
}