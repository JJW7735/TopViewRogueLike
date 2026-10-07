#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "RLPortal.generated.h"

class UBoxComponent;
class ARLCharacter;

UCLASS()
class ROGUELIKETOPVIEW_API ARLPortal : public AActor
{
    GENERATED_BODY()

public:
    ARLPortal();

protected:
    virtual void BeginPlay() override;

public:
    // 포탈과 캐릭터가 겹쳤을 때 감지하는 함수
    UFUNCTION()
    void OnOverlapBegin(UPrimitiveComponent* OverlappedComp, AActor* OtherActor,
        UPrimitiveComponent* OtherComp, int32 OtherBodyIndex,
        bool bFromSweep, const FHitResult& SweepResult);

    // 새로운 맵이 100% 로딩 완료되었을 때 엔진이 자동으로 불러줄 콜백 함수
    UFUNCTION()
    void OnLevelLoaded();

    UFUNCTION()
    void OnOverlapEnd(UPrimitiveComponent* OverlappedComp, AActor* OtherActor,
        UPrimitiveComponent* OtherComp, int32 OtherBodyIndex);

    UFUNCTION(BlueprintCallable, Category = "Portal")
    void ActivatePortal(ARLCharacter* PlayerChar);

    UFUNCTION(BlueprintCallable, Category = "Portal")
    void SetLinkedChestToClean(AActor* InChest) { LinkedChestToClean = InChest; }

    // 플레이어가 범위에 들어왔을 때 HUD에 띄울 상호작용 안내 문구 (인스턴스별로 에디터에서 지정)
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Interaction")
    FText InteractPromptText;

private:
    // 이동 완료 후 포탈 자신을 파괴할지 여부 (일회성 전이 포탈 정리)
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Portal Setup", meta = (AllowPrivateAccess = "true"))
    bool bDestroyOnUse = true;

    // 포탈 이동 시 함께 정리할 보상 상자 (플레이어가 상자를 열지 않고 넘어갔을 때 대비)
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Portal Setup", meta = (AllowPrivateAccess = "true"))
    TWeakObjectPtr<AActor> LinkedChestToClean;

    // 포탈의 충돌 영역 (이 범위에 닿으면 작동)
    UPROPERTY(VisibleAnywhere, Category = "Portal")
    UBoxComponent* PortalCollision;

    // 새로 불러올 다음 맵의 이름 (예: Map_B)
    UPROPERTY(EditAnywhere, Category = "Portal Setup")
    FName LevelToLoad;

    // 이제 떠날 이전 맵의 이름 (예: Map_A)
    UPROPERTY(EditAnywhere, Category = "Portal Setup")
    FName LevelToUnload;

    // ★ 핵심 팁: MakeEditWidget을 켜두면, 에디터 화면에서 마우스로 드래그해서 도착 위치를 직관적으로 찍을 수 있습니다!
    UPROPERTY(EditAnywhere, Category = "Portal Setup", meta = (MakeEditWidget = true))
    FVector TeleportLocation;

    // 독립 맵(.umap)으로 넘어갈 때 지정. 설정되면 스트리밍(LevelToLoad/Unload) 대신 OpenLevel로 맵 자체를 전환하고,
    // 체력/스탯/인벤토리/스킬은 런 세이브로 넘겨서 새 맵의 PlayerStart에서 복원한다.
    UPROPERTY(EditAnywhere, Category = "Portal Setup")
    TSoftObjectPtr<UWorld> TravelMap;

    bool HasDestination() const { return !TravelMap.IsNull() || !LevelToLoad.IsNone(); }

    // TravelMap으로 맵 전환 (런 자동저장 → OpenLevel)
    void TravelToMap(ARLCharacter* PlayerChar);

    // 로딩하는 동안 포탈을 탄 플레이어를 안전하게 기억해 둘 변수
    UPROPERTY()
    ARLCharacter* OverlappedPlayer;
};

