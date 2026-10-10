#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "RLBossTrigger.generated.h"

class UBoxComponent;
class UArrowComponent;
class ARLArenaMoodController;

/**
 * 보스 등장 트리거.
 * 플레이어(ARLCharacter)가 박스를 밟으면 BossSpawnPoint 위치에 BossClass를 소환하고,
 * MoodController->SetBossPhase(true)로 보스룸 분위기를 전환한다.
 */
UCLASS()
class ROGUELIKETOPVIEW_API ARLBossTrigger : public AActor
{
	GENERATED_BODY()

public:
	ARLBossTrigger();

	// 트리거 없이 직접 보스전을 시작할 때도 사용 가능
	UFUNCTION(BlueprintCallable, Category = "Boss")
	void TriggerBossEncounter();

	UFUNCTION(BlueprintPure, Category = "Boss")
	APawn* GetSpawnedBoss() const { return SpawnedBoss; }

	// 보스 소환 직후 호출 (BP에서 보스 체력바/카메라 연출 등 추가용)
	UFUNCTION(BlueprintImplementableEvent, Category = "Boss")
	void OnBossSpawned(APawn* Boss);

protected:
	virtual void BeginPlay() override;

	UFUNCTION()
	void OnTriggerBeginOverlap(UPrimitiveComponent* OverlappedComp, AActor* OtherActor,
		UPrimitiveComponent* OtherComp, int32 OtherBodyIndex,
		bool bFromSweep, const FHitResult& SweepResult);

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Components")
	TObjectPtr<UBoxComponent> TriggerBox;

	// 보스가 소환될 위치/방향 (뷰포트에서 이동 가능)
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Components")
	TObjectPtr<UArrowComponent> BossSpawnPoint;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Boss")
	TSubclassOf<APawn> BossClass;

	// 비워두면 같은 레벨의 ARLArenaMoodController를 자동으로 찾음
	UPROPERTY(EditInstanceOnly, BlueprintReadWrite, Category = "Boss")
	TObjectPtr<ARLArenaMoodController> MoodController;

	// true면 한 번만 발동
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Boss")
	bool bTriggerOnce = true;

	// 보스 소환 시 닫고, 보스가 사라지면 열어줄 길목 액터 (ARLFogWall은 bStartOpen=true로 두면 처음엔 열려 있음)
	UPROPERTY(EditInstanceOnly, BlueprintReadWrite, Category = "Boss|Barriers")
	TArray<TObjectPtr<AActor>> BarrierActors;

	// 보스 소환 후 길목이 닫히기까지의 딜레이 (초)
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Boss|Barriers", meta = (ClampMin = "0.0"))
	float BarrierCloseDelay = 0.5f;

private:
	void LockBarriers();
	void UnlockBarriers();

	UFUNCTION()
	void HandleBossDestroyed(AActor* DestroyedActor);

	UPROPERTY()
	TObjectPtr<APawn> SpawnedBoss;

	bool bTriggered = false;
	FTimerHandle BarrierCloseTimerHandle;
};
