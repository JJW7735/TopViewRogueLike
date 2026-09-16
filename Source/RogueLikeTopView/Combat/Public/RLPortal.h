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

private:
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

