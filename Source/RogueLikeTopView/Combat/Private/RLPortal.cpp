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

    // ★ 스테이지 진행 순서 고정 오버라이드:
    //   2-1_Graveyard -> 2-2_WallPassage 로 이동
    //   2-2_WallPassage -> 아직 2-3은 연결하지 않음 (포탈 비활성화/숨김)
    if (UWorld* World = GetWorld())
    {
        const FString CurrentMap = UWorld::RemovePIEPrefix(World->GetMapName());
        if (CurrentMap.Equals(TEXT("2-1_Graveyard"), ESearchCase::IgnoreCase) ||
            CurrentMap.Equals(TEXT("L_S2Sub_Graveyard"), ESearchCase::IgnoreCase))
        {
            SetTravelMapByPath(TEXT("/Game/Maps/2-2_WallPassage"));
        }
        else if (CurrentMap.Equals(TEXT("2-2_WallPassage"), ESearchCase::IgnoreCase) ||
                 CurrentMap.Equals(TEXT("L_S2Sub_WallPassage"), ESearchCase::IgnoreCase))
        {
            ClearDestination();
            SetActorHiddenInGame(true);
            SetActorEnableCollision(false);
            if (PortalCollision)
            {
                PortalCollision->SetCollisionEnabled(ECollisionEnabled::NoCollision);
            }
            return;
        }
    }

    // 2. 게임이 시작되면, 누군가 포탈에 닿았을 때 OnOverlapBegin 함수가 실행되도록 연결(바인딩)합니다.
    if (PortalCollision)
    {
        PortalCollision->OnComponentBeginOverlap.AddUniqueDynamic(this, &ARLPortal::OnOverlapBegin);
        PortalCollision->OnComponentEndOverlap.AddUniqueDynamic(this, &ARLPortal::OnOverlapEnd);
    }
}

void ARLPortal::SetTravelMapByPath(const FString& InMapPackagePath)
{
    if (InMapPackagePath.IsEmpty())
    {
        TravelMap.Reset();
        return;
    }

    const FString ShortName = FPackageName::GetShortName(InMapPackagePath);
    const FString FullObjectPath = InMapPackagePath.Contains(TEXT("."))
        ? InMapPackagePath
        : FString::Printf(TEXT("%s.%s"), *InMapPackagePath, *ShortName);

    TravelMap = TSoftObjectPtr<UWorld>(FSoftObjectPath(FullObjectPath));
}

void ARLPortal::ClearDestination()
{
    TravelMap.Reset();
    LevelToLoad = NAME_None;
    LevelToUnload = NAME_None;
}

void ARLPortal::OnOverlapBegin(UPrimitiveComponent* OverlappedComp, AActor* OtherActor,
    UPrimitiveComponent* OtherComp, int32 OtherBodyIndex,
    bool bFromSweep, const FHitResult& SweepResult)
{
    // 3. 겹친 대상이 몬스터나 투사체가 아니라 개발자님의 캐릭터(ARLCharacter)인지 확실히 검사합니다.
    ARLCharacter* PlayerChar = Cast<ARLCharacter>(OtherActor);
    if (PlayerChar && HasDestination())
    {
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

    // 상점/일시정지 메뉴 등으로 게임이 멈춰 있는 동안에는 F키가 눌려도 포탈이 작동하지 않도록 방어
    // (IA_Interact가 Trigger When Paused로 바뀌면서, 메뉴가 열려 있어도 F 입력이 여기까지 도달할 수 있게 됨)
    if (UGameplayStatics::IsGamePaused(GetWorld()))
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