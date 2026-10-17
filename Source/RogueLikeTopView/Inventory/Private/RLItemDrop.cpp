#include "RLItemDrop.h"
#include "RLCharacter.h"
#include "RLInventoryComponent.h"
#include "RLItemDataAsset.h"
#include "Components/SphereComponent.h"
#include "Components/StaticMeshComponent.h"
#include "Components/PointLightComponent.h"
#include "Materials/MaterialInstanceDynamic.h"
#include "Materials/MaterialInterface.h"
#include "Engine/StaticMesh.h"
#include "UObject/ConstructorHelpers.h"

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
	MeshComp->SetCastShadow(false);

	MagnetSphere = CreateDefaultSubobject<USphereComponent>(TEXT("MagnetSphere"));
	MagnetSphere->SetupAttachment(RootComponent);
	MagnetSphere->InitSphereRadius(MagnetRadius);
	MagnetSphere->SetCollisionProfileName(TEXT("OverlapAllDynamic"));
	MagnetSphere->SetGenerateOverlapEvents(true);

	GlowLight = CreateDefaultSubobject<UPointLightComponent>(TEXT("GlowLight"));
	GlowLight->SetupAttachment(RootComponent);
	GlowLight->SetIntensity(3000.0f);
	GlowLight->SetAttenuationRadius(180.0f);
	GlowLight->SetCastShadows(false);

	// 메시가 비어 있으면 드롭이 완전히 투명해지므로 엔진 기본 도형을 기본값으로 지정
	static ConstructorHelpers::FObjectFinder<UStaticMesh> CoinMeshFinder(TEXT("/Engine/BasicShapes/Cylinder.Cylinder"));
	static ConstructorHelpers::FObjectFinder<UStaticMesh> OrbMeshFinder(TEXT("/Engine/BasicShapes/Sphere.Sphere"));
	if (CoinMeshFinder.Succeeded())
	{
		GoldMesh = CoinMeshFinder.Object;
	}
	if (OrbMeshFinder.Succeeded())
	{
		ItemMesh = OrbMeshFinder.Object;
	}

	static ConstructorHelpers::FObjectFinder<UMaterialInterface> GlowMaterialFinder(TEXT("/Game/Effect/Material/M_DropGlow.M_DropGlow"));
	if (GlowMaterialFinder.Succeeded())
	{
		DropMaterial = GlowMaterialFinder.Object;
	}
}

void ARLItemDrop::BeginPlay()
{
	Super::BeginPlay();

	SpawnGroundLocation = FindGroundBelow(GetActorLocation());

	if (MagnetSphere)
	{
		MagnetSphere->SetSphereRadius(MagnetRadius);
		MagnetSphere->OnComponentBeginOverlap.AddUniqueDynamic(this, &ARLItemDrop::OnMagnetOverlapBegin);
	}

	if (CollisionSphere)
	{
		CollisionSphere->OnComponentBeginOverlap.AddUniqueDynamic(this, &ARLItemDrop::OnPickupOverlapBegin);
	}

	ApplyDropColor();
}

void ARLItemDrop::Tick(float DeltaTime)
{
	Super::Tick(DeltaTime);

	Age += DeltaTime;

	// 0. 스폰 순간부터 자석 범위 안에 있던 플레이어는 BeginOverlap이 다시 오지 않으므로,
	//    최소 노출 시간이 지난 뒤 현재 겹쳐 있는 플레이어를 직접 확인
	if (!bIsBeingAttracted && Age >= MinVisibleTime && MagnetSphere)
	{
		TArray<AActor*> OverlappingChars;
		MagnetSphere->GetOverlappingActors(OverlappingChars, ARLCharacter::StaticClass());
		for (AActor* Actor : OverlappingChars)
		{
			TryStartAttraction(Cast<ARLCharacter>(Actor));
			if (bIsBeingAttracted)
			{
				break;
			}
		}
	}

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
			GrantTo(TargetPlayer.Get());
		}
		return;
	}

	// 2. 초기 포물선 점프 시뮬레이션 (바닥 + HoverHeight 높이에 닿으면 착지)
	const float RestZ = SpawnGroundLocation.Z + HoverHeight;
	if (!bHasLanded)
	{
		CurrentVelocity.Z -= 980.0f * DeltaTime; // 중력
		FVector NewLoc = GetActorLocation() + CurrentVelocity * DeltaTime;

		if (CurrentVelocity.Z <= 0.0f && NewLoc.Z <= RestZ)
		{
			NewLoc.Z = RestZ;
			bHasLanded = true;
			CurrentVelocity = FVector::ZeroVector;

			// 둥둥 연출이 스폰 지점으로 순간이동하지 않도록 실제 착지한 XY를 기준점으로 갱신
			SpawnGroundLocation.X = NewLoc.X;
			SpawnGroundLocation.Y = NewLoc.Y;
		}

		SetActorLocation(NewLoc, true);
		AddActorLocalRotation(FRotator(0.0f, 540.0f * DeltaTime, 0.0f));
		return;
	}

	// 3. 바닥 안착 후 대기 상태: 둥둥 뜨기 & 제자리 회전 연출
	RunningTime += DeltaTime;
	AddActorLocalRotation(FRotator(0.0f, 100.0f * DeltaTime, 0.0f));

	const float BobbingOffset = FMath::Sin(RunningTime * 3.5f) * 6.0f;
	SetActorLocation(FVector(SpawnGroundLocation.X, SpawnGroundLocation.Y, RestZ + BobbingOffset));
}

void ARLItemDrop::InitializeItemDrop(URLItemDataAsset* InItemData, int32 InAmount)
{
	bIsGoldDrop = false;
	ItemData = InItemData;
	ItemAmount = FMath::Max(1, InAmount);
	ApplyDropColor();
}

void ARLItemDrop::InitializeGoldDrop(int32 InGoldAmount)
{
	bIsGoldDrop = true;
	GoldAmount = FMath::Max(1, InGoldAmount);
	ItemData = nullptr;
	ItemAmount = 0;
	ApplyDropColor();
}

void ARLItemDrop::LaunchDrop(FVector LaunchVelocity)
{
	CurrentVelocity = LaunchVelocity;
	bHasLanded = false;
	// 몬스터 위치(캡슐 중심)가 아니라 그 아래 실제 바닥을 착지 기준으로 사용
	SpawnGroundLocation = FindGroundBelow(GetActorLocation());
}

void ARLItemDrop::OnMagnetOverlapBegin(UPrimitiveComponent* OverlappedComp, AActor* OtherActor,
	UPrimitiveComponent* OtherComp, int32 OtherBodyIndex, bool bFromSweep, const FHitResult& SweepResult)
{
	TryStartAttraction(Cast<ARLCharacter>(OtherActor));
}

void ARLItemDrop::OnPickupOverlapBegin(UPrimitiveComponent* OverlappedComp, AActor* OtherActor,
	UPrimitiveComponent* OtherComp, int32 OtherBodyIndex, bool bFromSweep, const FHitResult& SweepResult)
{
	// 최소 노출 시간 전에는 즉시 획득하지 않음 (이후엔 Tick의 자석 흡수가 처리)
	if (Age < MinVisibleTime)
	{
		return;
	}

	if (ARLCharacter* PlayerChar = Cast<ARLCharacter>(OtherActor))
	{
		if (PlayerChar->TeamID == ERLTeam::Player || PlayerChar->IsPlayerControlled())
		{
			GrantTo(PlayerChar);
		}
	}
}

void ARLItemDrop::TryStartAttraction(ARLCharacter* PlayerChar)
{
	if (bIsBeingAttracted || !PlayerChar || Age < MinVisibleTime)
	{
		return;
	}

	// 플레이어 진영인 경우에만 자석 반응
	if (PlayerChar->TeamID == ERLTeam::Player || PlayerChar->IsPlayerControlled())
	{
		bIsBeingAttracted = true;
		TargetPlayer = PlayerChar;
	}
}

void ARLItemDrop::GrantTo(ARLCharacter* PlayerChar)
{
	if (IsActorBeingDestroyed() || !PlayerChar)
	{
		return;
	}

	if (URLInventoryComponent* InvComp = PlayerChar->GetInventoryComponent())
	{
		if (bIsGoldDrop)
		{
			InvComp->AddGold(GoldAmount);
		}
		else if (ItemData)
		{
			InvComp->AddItem(ItemData, ItemAmount);
		}
	}

	OnItemPickedUp(PlayerChar);
	Destroy();
}

void ARLItemDrop::ApplyDropColor()
{
	const FLinearColor DropColor = bIsGoldDrop ? GoldColor : ItemColor;

	if (MeshComp)
	{
		if (UStaticMesh* TargetMesh = bIsGoldDrop ? GoldMesh : ItemMesh)
		{
			MeshComp->SetStaticMesh(TargetMesh);
		}

		// 엔진 Cylinder(높이 100)를 세워 납작한 동전 모양으로, Sphere는 작은 구슬로
		if (bIsGoldDrop)
		{
			MeshComp->SetRelativeRotation(FRotator(0.0f, 0.0f, 90.0f));
			MeshComp->SetRelativeScale3D(FVector(0.55f, 0.55f, 0.08f)); // 눈에 잘 띄게 더 크게
		}
		else
		{
			MeshComp->SetRelativeRotation(FRotator::ZeroRotator);
			MeshComp->SetRelativeScale3D(FVector(0.28f));
		}

		if (!MeshMID)
		{
			MeshMID = DropMaterial
				? MeshComp->CreateDynamicMaterialInstance(0, DropMaterial)
				: MeshComp->CreateDynamicMaterialInstance(0);
		}
		if (MeshMID)
		{
			// M_DropGlow / 엔진 BasicShapeMaterial 공통 색 파라미터. 없는 머티리얼이면 무시됨
			MeshMID->SetVectorParameterValue(TEXT("Color"), DropColor);
		}
	}

	if (GlowLight)
	{
		GlowLight->SetLightColor(DropColor);
	}
}

FVector ARLItemDrop::FindGroundBelow(const FVector& From) const
{
	UWorld* World = GetWorld();
	if (!World)
	{
		return From;
	}

	// 바닥 메시가 Movable(WorldDynamic)이거나 단순 콜리전이 없는 경우도 있으므로 둘 다, 복합 콜리전으로 검사.
	// 몬스터/플레이어 캡슐(Pawn)은 대상에서 제외됨
	FCollisionObjectQueryParams ObjectParams;
	ObjectParams.AddObjectTypesToQuery(ECC_WorldStatic);
	ObjectParams.AddObjectTypesToQuery(ECC_WorldDynamic);

	FCollisionQueryParams Params(SCENE_QUERY_STAT(RLItemDropGround), true, this);

	TArray<FHitResult> Hits;
	const FVector Start = From + FVector(0.f, 0.f, 50.f);
	const FVector End = From - FVector(0.f, 0.f, 1000.f);
	World->LineTraceMultiByObjectType(Hits, Start, End, ObjectParams, Params);

	for (const FHitResult& Hit : Hits)
	{
		// 근처의 다른 드롭(오버랩 구체)은 바닥이 아님
		if (Hit.GetActor() && Hit.GetActor()->IsA<ARLItemDrop>())
		{
			continue;
		}
		return Hit.ImpactPoint;
	}
	return From;
}
