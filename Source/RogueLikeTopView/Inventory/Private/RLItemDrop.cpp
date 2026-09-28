#include "RLItemDrop.h"
#include "RLCharacter.h"
#include "RLInventoryComponent.h"
#include "RLItemDataAsset.h"
#include "Components/SphereComponent.h"
#include "Components/StaticMeshComponent.h"

ARLItemDrop::ARLItemDrop()
{
	PrimaryActorTick.bCanEverTick = true;

	CollisionSphere = CreateDefaultSubobject<USphereComponent>(TEXT("CollisionSphere"));
	CollisionSphere->InitSphereRadius(45.0f);
	CollisionSphere->SetCollisionProfileName(TEXT("OverlapAllDynamic"));
	CollisionSphere->SetGenerateOverlapEvents(true);
	RootComponent = CollisionSphere;

	MeshComp = CreateDefaultSubobject<UStaticMeshComponent>(TEXT("MeshComp"));
	MeshComp->SetupAttachment(RootComponent);
	MeshComp->SetCollisionEnabled(ECollisionEnabled::NoCollision);

	MagnetSphere = CreateDefaultSubobject<USphereComponent>(TEXT("MagnetSphere"));
	MagnetSphere->SetupAttachment(RootComponent);
	MagnetSphere->InitSphereRadius(MagnetRadius);
	MagnetSphere->SetCollisionProfileName(TEXT("OverlapAllDynamic"));
	MagnetSphere->SetGenerateOverlapEvents(true);
}

void ARLItemDrop::BeginPlay()
{
	Super::BeginPlay();

	SpawnGroundLocation = GetActorLocation();

	if (MagnetSphere)
	{
		MagnetSphere->SetSphereRadius(MagnetRadius);
		MagnetSphere->OnComponentBeginOverlap.AddDynamic(this, &ARLItemDrop::OnMagnetOverlapBegin);
	}

	if (CollisionSphere)
	{
		CollisionSphere->OnComponentBeginOverlap.AddDynamic(this, &ARLItemDrop::OnPickupOverlapBegin);
	}
}

void ARLItemDrop::Tick(float DeltaTime)
{
	Super::Tick(DeltaTime);

	// 1. 자석 흡수 상태: 플레이어를 향해 가속하여 날아감
	if (bIsBeingAttracted && TargetPlayer.IsValid())
	{
		const FVector MyLoc = GetActorLocation();
		const FVector TargetLoc = TargetPlayer->GetActorLocation() + FVector(0.f, 0.f, 30.f);
		const FVector Dir = (TargetLoc - MyLoc).GetSafeNormal();
		const float Distance = FVector::Dist(TargetLoc, MyLoc);

		const float MoveStep = FMath::Min(MagnetSpeed * DeltaTime, Distance);
		SetActorLocation(MyLoc + Dir * MoveStep, true);

		// 근접 도달 시 직접 획득 처리
		if (Distance <= 50.0f)
		{
			if (TargetPlayer->GetInventoryComponent())
			{
				if (bIsGoldDrop)
				{
					TargetPlayer->GetInventoryComponent()->AddGold(GoldAmount);
				}
				else if (ItemData)
				{
					TargetPlayer->GetInventoryComponent()->AddItem(ItemData, ItemAmount);
				}
			}

			OnItemPickedUp(TargetPlayer.Get());
			Destroy();
		}
		return;
	}

	// 2. 초기 포물선 점프 시뮬레이션
	if (!bHasLanded)
	{
		CurrentVelocity.Z -= 980.0f * DeltaTime; // 중력
		FVector NewLoc = GetActorLocation() + CurrentVelocity * DeltaTime;

		if (NewLoc.Z <= SpawnGroundLocation.Z)
		{
			NewLoc.Z = SpawnGroundLocation.Z;
			bHasLanded = true;
			CurrentVelocity = FVector::ZeroVector;
		}

		SetActorLocation(NewLoc, true);
		return;
	}

	// 3. 바닥 안착 후 대기 상태: 둥둥 뜨기 & 제자리 회전 연출
	RunningTime += DeltaTime;
	AddActorLocalRotation(FRotator(0.0f, 100.0f * DeltaTime, 0.0f));

	const float BobbingOffset = FMath::Sin(RunningTime * 3.5f) * 6.0f;
	SetActorLocation(SpawnGroundLocation + FVector(0.0f, 0.0f, 25.0f + BobbingOffset));
}

void ARLItemDrop::InitializeItemDrop(URLItemDataAsset* InItemData, int32 InAmount)
{
	bIsGoldDrop = false;
	ItemData = InItemData;
	ItemAmount = FMath::Max(1, InAmount);
}

void ARLItemDrop::InitializeGoldDrop(int32 InGoldAmount)
{
	bIsGoldDrop = true;
	GoldAmount = FMath::Max(1, InGoldAmount);
	ItemData = nullptr;
	ItemAmount = 0;
}

void ARLItemDrop::LaunchDrop(FVector LaunchVelocity)
{
	CurrentVelocity = LaunchVelocity;
	bHasLanded = false;
	SpawnGroundLocation = GetActorLocation();
}

void ARLItemDrop::OnMagnetOverlapBegin(UPrimitiveComponent* OverlappedComp, AActor* OtherActor,
	UPrimitiveComponent* OtherComp, int32 OtherBodyIndex, bool bFromSweep, const FHitResult& SweepResult)
{
	if (bIsBeingAttracted)
	{
		return;
	}

	if (ARLCharacter* PlayerChar = Cast<ARLCharacter>(OtherActor))
	{
		// 플레이어 진영인 경우에만 자석 반응
		if (PlayerChar->TeamID == ERLTeam::Player || PlayerChar->IsPlayerControlled())
		{
			bIsBeingAttracted = true;
			TargetPlayer = PlayerChar;
		}
	}
}

void ARLItemDrop::OnPickupOverlapBegin(UPrimitiveComponent* OverlappedComp, AActor* OtherActor,
	UPrimitiveComponent* OtherComp, int32 OtherBodyIndex, bool bFromSweep, const FHitResult& SweepResult)
{
	if (ARLCharacter* PlayerChar = Cast<ARLCharacter>(OtherActor))
	{
		if (PlayerChar->TeamID == ERLTeam::Player || PlayerChar->IsPlayerControlled())
		{
			if (PlayerChar->GetInventoryComponent())
			{
				if (bIsGoldDrop)
				{
					PlayerChar->GetInventoryComponent()->AddGold(GoldAmount);
				}
				else if (ItemData)
				{
					PlayerChar->GetInventoryComponent()->AddItem(ItemData, ItemAmount);
				}
			}

			OnItemPickedUp(PlayerChar);
			Destroy();
		}
	}
}
