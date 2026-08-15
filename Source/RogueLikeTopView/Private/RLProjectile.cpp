// Fill out your copyright notice in the Description page of Project Settings.

#include "RLProjectile.h"
#include "Components/SphereComponent.h"
#include "Components/StaticMeshComponent.h"
#include "GameFramework/ProjectileMovementComponent.h"
#include "RLCharacter.h"

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
		Destroy();
	}
}