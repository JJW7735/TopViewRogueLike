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

    // 로딩하는 동안 포탈을 탄 플레이어를 안전하게 기억해 둘 변수
    UPROPERTY()
    ARLCharacter* OverlappedPlayer;
};

