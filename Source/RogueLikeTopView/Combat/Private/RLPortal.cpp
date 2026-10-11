#include "RLPortal.h"
#include "Components/BoxComponent.h"
#include "RLCharacter.h"
#include "RLSaveSubsystem.h"
#include "Kismet/GameplayStatics.h"
#include "Misc/PackageName.h"

ARLPortal::ARLPortal()
{
    PrimaryActorTick.bCanEverTick = false;

    // 1. 충돌 박스(히트박스)를 만들고 포탈의 기본 뼈대로 설정합니다.
    PortalCollision = CreateDefaultSubobject<UBoxComponent>(TEXT("PortalCollision"));
    RootComponent = PortalCollision;

    // 포탈의 크기를 캐릭터가 충분히 들어갈 만큼 넉넉하게 잡아줍니다.
    PortalCollision->InitBoxExtent(FVector(100.f, 100.f, 150.f));
    PortalCollision->SetCollisionProfileName(TEXT("Trigger")); // 겹침 감지용 프로필
}

void ARLPortal::BeginPlay()
{
    Super::BeginPlay();

    // 2. 게임이 시작되면, 누군가 포탈에 닿았을 때 OnOverlapBegin 함수가 실행되도록 연결(바인딩)합니다.
    if (PortalCollision)
    {
        PortalCollision->OnComponentBeginOverlap.AddUniqueDynamic(this, &ARLPortal::OnOverlapBegin);
        PortalCollision->OnComponentEndOverlap.AddUniqueDynamic(this, &ARLPortal::OnOverlapEnd);
    }
}

void ARLPortal::OnOverlapBegin(UPrimitiveComponent* OverlappedComp, AActor* OtherActor,
    UPrimitiveComponent* OtherComp, int32 OtherBodyIndex,
    bool bFromSweep, const FHitResult& SweepResult)
{
    // 3. 겹친 대상이 몬스터나 투사체가 아니라 개발자님의 캐릭터(ARLCharacter)인지 확실히 검사합니다.
    ARLCharacter* PlayerChar = Cast<ARLCharacter>(OtherActor);
    if (PlayerChar && HasDestination())
    {
        // ▼ 기존 로직: 닿자마자 바로 텔레포트했었음. 이제 E키 누를 때(TryInteract → ActivatePortal)만 실행되도록 변경.
        // // 중복해서 포탈을 타는 버그를 막기 위해, 한 번 닿으면 포탈의 충돌을 바로 꺼버립니다.
        // PortalCollision->SetCollisionEnabled(ECollisionEnabled::NoCollision);
        //
        // OverlappedPlayer = PlayerChar;
        //
        // // 4. 새로운 맵(Map_B) 로딩을 시작합니다! (이때 게임이 멈추지 않고 비동기로 로딩됩니다.)
        // FLatentActionInfo LatentInfo;
        // LatentInfo.CallbackTarget = this;
        // LatentInfo.ExecutionFunction = FName("OnLevelLoaded"); // 로딩 끝나면 이 함수를 실행해라!
        // LatentInfo.Linkage = 0;
        // LatentInfo.UUID = GetUniqueID(); // 안전한 고유 ID 배정
        //
        // UGameplayStatics::LoadStreamLevel(this, LevelToLoad, true, false, LatentInfo);

        // ▼ 새 로직: 포탈 범위에 들어왔다는 것만 캐릭터에게 알려줌
        PlayerChar->SetNearbyPortal(this);
    }
}

void ARLPortal::OnLevelLoaded()
{
    // 5. 맵 로딩이 완벽하게 끝난 직후에 엔진이 이 안으로 진입합니다.
    if (OverlappedPlayer)
    {
        // 캐릭터를 새 맵에 지정해 둔 텔레포트 위치(도착점)로 부드럽게 순간이동시킵니다.
        // GetActorTransform()을 더해서 포탈 기준의 상대 좌표를 절대 좌표로 변환해 줍니다.
        FVector FinalLocation = GetActorLocation() + TeleportLocation;
        OverlappedPlayer->SetActorLocation(FinalLocation);

        // ★ 로그라이크 표준: 다음 맵 진입 시점 자동 저장
        if (URLSaveSubsystem* SaveSys = URLSaveSubsystem::Get(this))
        {
            SaveSys->AutoSaveOnStageTransition(OverlappedPlayer, LevelToLoad);
        }
    }

    // 6. 무사히 도착했으니, 등 뒤에 남겨진 예전 맵(Map_A)을 메모리에서 흔적도 없이 싹 지워버립니다!
    if (!LevelToUnload.IsNone())
    {
        FLatentActionInfo UnloadInfo;
        UGameplayStatics::UnloadStreamLevel(this, LevelToUnload, UnloadInfo, false);
    }

    // 7. 이전 아레나에서 혹시 남아있던 보상 상자가 있다면 안전하게 소멸
    if (LinkedChestToClean.IsValid())
    {
        LinkedChestToClean->Destroy();
    }

    // 8. 일회성 전이 포탈인 경우, 다음 맵에 남지 않도록 포탈 자신도 파괴
    if (bDestroyOnUse)
    {
        Destroy();
    }
}


void ARLPortal::OnOverlapEnd(UPrimitiveComponent* OverlappedComp, AActor* OtherActor,
    UPrimitiveComponent* OtherComp, int32 OtherBodyIndex)
{
    ARLCharacter* PlayerChar = Cast<ARLCharacter>(OtherActor);
    if (PlayerChar)
    {
        PlayerChar->SetNearbyPortal(nullptr);
    }
}

void ARLPortal::ActivatePortal(ARLCharacter* PlayerChar)
{
    if (!PlayerChar || !HasDestination())
    {
        return;
    }

    PortalCollision->SetCollisionEnabled(ECollisionEnabled::NoCollision);

    if (!TravelMap.IsNull())
    {
        TravelToMap(PlayerChar);
        return;
    }

    OverlappedPlayer = PlayerChar;

    FLatentActionInfo LatentInfo;
    LatentInfo.CallbackTarget = this;
    LatentInfo.ExecutionFunction = FName("OnLevelLoaded");
    LatentInfo.Linkage = 0;
    LatentInfo.UUID = GetUniqueID();
    UGameplayStatics::LoadStreamLevel(this, LevelToLoad, true, false, LatentInfo);
}

void ARLPortal::TravelToMap(ARLCharacter* PlayerChar)
{
    const FString MapPackage = TravelMap.GetLongPackageName();
    const FName MapShortName(*FPackageName::GetShortName(MapPackage));

    // 맵이 통째로 바뀌므로 캐릭터 상태를 런 세이브에 담아두고, 새 맵에서 스폰될 때 BeginPlay가 복원하게 한다.
    // 위치는 새 맵의 PlayerStart를 쓰도록 비워서 저장.
    if (URLSaveSubsystem* SaveSys = URLSaveSubsystem::Get(this))
    {
        if (SaveSys->AutoSaveOnStageTransition(PlayerChar, MapShortName, true))
        {
            SaveSys->bPendingRunApply = true;
        }
    }

    UGameplayStatics::OpenLevel(this, FName(*MapPackage));
}