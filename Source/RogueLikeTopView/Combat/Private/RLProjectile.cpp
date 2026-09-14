// Fill out your copyright notice in the Description page of Project Settings.

#include "RLProjectile.h"
#include "Components/SphereComponent.h"
#include "Components/StaticMeshComponent.h"
#include "GameFramework/ProjectileMovementComponent.h"
#include "RLCharacter.h"
#include "Kismet/KismetSystemLibrary.h"
#include "DrawDebugHelpers.h"

ARLProjectile::ARLProjectile()
{
	PrimaryActorTick.bCanEverTick = false;

	CollisionComp = CreateDefaultSubobject<USphereComponent>(TEXT("CollisionComp"));
	CollisionComp->InitSphereRadius(20.f);
	CollisionComp->SetCollisionProfileName(TEXT("RLProjectile"));
	CollisionComp->OnComponentBeginOverlap.AddDynamic(this, &ARLProjectile::OnHit);
	RootComponent = CollisionComp;

	MeshComp = CreateDefaultSubobject<UStaticMeshComponent>(TEXT("MeshComp"));
	MeshComp->SetupAttachment(RootComponent);
	MeshComp->SetCollisionEnabled(ECollisionEnabled::NoCollision);
	MeshComp->SetGenerateOverlapEvents(false);

	MovementComp = CreateDefaultSubobject<UProjectileMovementComponent>(TEXT("MovementComp"));
	MovementComp->UpdatedComponent = CollisionComp;
	MovementComp->InitialSpeed = 1500.f;
	MovementComp->MaxSpeed = 1500.f;
	MovementComp->ProjectileGravityScale = 0.f;
	MovementComp->bRotationFollowsVelocity = true;

	InitialLifeSpan = 3.0f;
}

void ARLProjectile::BeginPlay()
{
	Super::BeginPlay();
	SetLifeSpan(MaxLifeTime);

	if (GetInstigator())
	{
		CollisionComp->IgnoreActorWhenMoving(GetInstigator(), true);
	}
}

void ARLProjectile::OnHit(UPrimitiveComponent* OverlappedComp, AActor* OtherActor,
	UPrimitiveComponent* OtherComp, int32 OtherBodyIndex, bool bFromSweep,
	const FHitResult& SweepResult)
{
	// ★ 이미 이 발이 터지기로 확정됐으면, 같은 프레임에 뒤늦게 들어오는 다른 액터의
	// BeginOverlap 이벤트는 전부 무시합니다 (Destroy()가 즉시 이후 이벤트를 막아주지 않기 때문).
	if (bHasExploded)
	{
		return;
	}

	if (!OtherActor || OtherActor == this || OtherActor == GetInstigator())
	{
		return;
	}

	ARLCharacter* HitCharacter = Cast<ARLCharacter>(OtherActor);
	if (!HitCharacter)
	{
		return;
	}

	FRLDamageData FinalDamageData = DamageData;
	FinalDamageData.DamageCauser = GetInstigator();

	HitCharacter->ReceiveCustomDamage(FinalDamageData, SweepResult);

	CurrentHitCount++;
	if (CurrentHitCount >= MaxPierceCount)
	{
		// ★ 파괴가 확정되는 이 시점에 플래그를 먼저 세워서, 스플래시 처리 중이나 그 직후에
		// 같은 프레임에서 들어올 수 있는 다른 오버랩 이벤트를 확실히 차단합니다.
		bHasExploded = true;

		if (bSplashDamage)
		{
			ApplySplashDamage(HitCharacter, SweepResult.ImpactPoint);
		}

		Destroy();
	}
}

void ARLProjectile::ApplySplashDamage(AActor* DirectHitActor, const FVector& Origin)
{
	TArray<AActor*> OverlappedActors;
	TArray<TEnumAsByte<EObjectTypeQuery>> ObjectTypes;
	ObjectTypes.Add(UEngineTypes::ConvertToObjectType(ECC_Pawn));

	TArray<AActor*> ActorsToIgnore;
	ActorsToIgnore.Add(DirectHitActor); // 직격 대상은 이미 데미지를 받았으니 중복 제외
	if (GetInstigator())
	{
		ActorsToIgnore.Add(GetInstigator()); // 캐스터 본인 제외
	}

	UKismetSystemLibrary::SphereOverlapActors(
		this,
		Origin,
		SplashRadius,
		ObjectTypes,
		ARLCharacter::StaticClass(),
		ActorsToIgnore,
		OverlappedActors
	);

	// ★ 디버그 시각화: 폭발 반경(주황색, 2초)과, 실제로 스플래시에 걸린 대상 위치(초록색 점)를 그려줍니다.
	// 눈으로 "반경이 어디까지인지"와 "누가 실제로 판정에 걸렸는지"를 바로 확인할 수 있습니다.
	if (bDrawSplashDebug)
	{
		DrawDebugSphere(GetWorld(), Origin, SplashRadius, 24, FColor::Orange, false, 2.0f, 0, 2.0f);
		for (AActor* DebugActor : OverlappedActors)
		{
			if (DebugActor)
			{
				DrawDebugSphere(GetWorld(), DebugActor->GetActorLocation(), 30.0f, 12, FColor::Green, false, 2.0f, 0, 3.0f);
			}
		}
		// 직격 대상도 빨간 점으로 같이 표시
		if (DirectHitActor)
		{
			DrawDebugSphere(GetWorld(), DirectHitActor->GetActorLocation(), 30.0f, 12, FColor::Red, false, 2.0f, 0, 3.0f);
		}
	}

	FRLDamageData SplashDamageData = DamageData;
	SplashDamageData.DamageCauser = GetInstigator();
	SplashDamageData.BaseDamage *= SplashDamageMultiplier;

	// 폭발 중심(Origin) 기준으로 넉백 방향이 계산되도록 ImpactPoint를 채워줍니다
	// (ARLCharacter::ReceiveCustomDamage가 HitInfo.ImpactPoint로 넉백 방향을 계산하기 때문)
	FHitResult SplashHit;
	SplashHit.ImpactPoint = Origin;

	for (AActor* SplashTarget : OverlappedActors)
	{
		if (ARLCharacter* SplashCharacter = Cast<ARLCharacter>(SplashTarget))
		{
			SplashCharacter->ReceiveCustomDamage(SplashDamageData, SplashHit);
		}
	}
}