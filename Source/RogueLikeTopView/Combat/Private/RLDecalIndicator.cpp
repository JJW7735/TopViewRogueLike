#include "RLDecalIndicator.h"
#include "Components/DecalComponent.h"
#include "Components/SceneComponent.h"
#include "UObject/ConstructorHelpers.h"
#include "Materials/MaterialInstanceDynamic.h"

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

void ARLDecalIndicator::SetupIndicator(EIndicatorType Type, float Radius, float MaxRange, UMaterialInterface* Material)
{
    CurrentType = Type;
    bUseRangeRing = (CurrentType == EIndicatorType::Circle && MaxRange > 0.0f);

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

    // ★ 전용 머티리얼이 넘어오면 교체 (안 넘기면 생성자 기본값/이전 값 유지)
    if (Material)
    {
        DecalComp->SetDecalMaterial(Material);
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
    bUseRangeRing = false;

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

void ARLDecalIndicator::SetupSectorIndicator(float Radius, float SafeZoneAngleDegrees, UMaterialInterface* Material)
{
    CurrentType = EIndicatorType::Cone;
    bUseRangeRing = false;

    // ★ Circle과 똑같이 반지름 기준 정사각형 풋프린트로 깔아둡니다. "부채꼴 모양으로 잘라내는" 건
    // 여기(C++)가 아니라 머티리얼에서 UV 각도로 처리합니다 — 이유: 데칼 컴포넌트 자체는 사각형/원형
    // 볼륨만 표현할 수 있고, 임의의 각도로 잘린 모양은 셰이더의 픽셀 단위 마스킹으로만 가능합니다.
    constexpr float VerticalThickness = 200.f;
    DecalComp->DecalSize = FVector(VerticalThickness, Radius, Radius);
    RangeDecalComp->SetVisibility(false);

    if (!Material)
    {
        return;
    }

    // ★ 머티리얼에 "SafeZoneAngleDeg"라는 이름의 Scalar Parameter를 만들어두면, 그 값을 이 함수가
    // 매번 갱신해서 넘겨줍니다(패턴마다 안전지대 폭이 달라도 같은 머티리얼 재사용 가능). 그러려면
    // 원본 UMaterialInterface가 아니라 MID(Material Instance Dynamic)를 만들어서 써야 합니다 —
    // 원본 머티리얼 애셋에 직접 SetScalarParameterValue를 호출하면 에디터에서 저장된 애셋 자체가
    // 바뀌어버리기 때문입니다. 머티리얼에 그 이름의 파라미터가 없으면 이 호출은 조용히 무시됩니다.
    UMaterialInstanceDynamic* MID = UMaterialInstanceDynamic::Create(Material, this);
    if (MID)
    {
        MID->SetScalarParameterValue(FName("SafeZoneAngleDeg"), SafeZoneAngleDegrees);
        DecalComp->SetDecalMaterial(MID);
    }
    else
    {
        DecalComp->SetDecalMaterial(Material);
    }
}

void ARLDecalIndicator::UpdateSectorTransform(const FVector& OriginLocation, float YawDegrees)
{
    CurrentTargetLocation = OriginLocation;

    DecalComp->SetWorldLocation(OriginLocation);
    DecalComp->SetWorldRotation(FRotator(-90.f, YawDegrees, 0.f));
}

void ARLDecalIndicator::SetIndicatorVisibility(bool bVisible)
{
    SetActorHiddenInGame(!bVisible);
    DecalComp->SetVisibility(bVisible);
    RangeDecalComp->SetVisibility(bVisible && bUseRangeRing);
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