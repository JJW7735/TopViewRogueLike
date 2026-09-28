#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "RLItemDrop.generated.h"

class USphereComponent;
class UStaticMeshComponent;
class URLItemDataAsset;
class ARLCharacter;

/**
 * 필드에 떨어져 바닥에서 회전하며, 플레이어가 접근하면 자석처럼 흡수되는 드롭 아이템 액터
 */
UCLASS()
class ROGUELIKETOPVIEW_API ARLItemDrop : public AActor
{
	GENERATED_BODY()

public:
	ARLItemDrop();

	virtual void Tick(float DeltaTime) override;

	/** 일반 아이템 드롭으로 초기화 */
	UFUNCTION(BlueprintCallable, Category = "Drop")
	void InitializeItemDrop(URLItemDataAsset* InItemData, int32 InAmount = 1);

	/** 골드 드롭으로 초기화 */
	UFUNCTION(BlueprintCallable, Category = "Drop")
	void InitializeGoldDrop(int32 InGoldAmount);

	/** 드롭 시 바닥으로 퐁 튀어나오는 포물선 연출 */
	UFUNCTION(BlueprintCallable, Category = "Drop")
	void LaunchDrop(FVector LaunchVelocity);

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Components")
	TObjectPtr<USphereComponent> CollisionSphere;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Components")
	TObjectPtr<UStaticMeshComponent> MeshComp;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Components")
	TObjectPtr<USphereComponent> MagnetSphere;

	// 골드 드롭인지 여부
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Drop")
	bool bIsGoldDrop = false;

	// 골드 수량
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Drop", meta = (EditCondition = "bIsGoldDrop"))
	int32 GoldAmount = 10;

	// 아이템 데이터
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Drop", meta = (EditCondition = "!bIsGoldDrop"))
	TObjectPtr<URLItemDataAsset> ItemData;

	// 아이템 수량
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Drop", meta = (EditCondition = "!bIsGoldDrop"))
	int32 ItemAmount = 1;

	// 자석 반경 (플레이어가 이 범위 안에 들어오면 플레이어에게 빨려 들어감)
	UPROPERTY(EditDefaultsOnly, BlueprintReadWrite, Category = "Drop|Magnet")
	float MagnetRadius = 350.0f;

	// 자석 흡수 이동 속도
	UPROPERTY(EditDefaultsOnly, BlueprintReadWrite, Category = "Drop|Magnet")
	float MagnetSpeed = 1200.0f;

protected:
	virtual void BeginPlay() override;

	UFUNCTION()
	void OnMagnetOverlapBegin(UPrimitiveComponent* OverlappedComp, AActor* OtherActor,
		UPrimitiveComponent* OtherComp, int32 OtherBodyIndex, bool bFromSweep, const FHitResult& SweepResult);

	UFUNCTION()
	void OnPickupOverlapBegin(UPrimitiveComponent* OverlappedComp, AActor* OtherActor,
		UPrimitiveComponent* OtherComp, int32 OtherBodyIndex, bool bFromSweep, const FHitResult& SweepResult);

	UFUNCTION(BlueprintImplementableEvent, Category = "Drop")
	void OnItemPickedUp(ARLCharacter* Collector);

private:
	bool bIsBeingAttracted = false;
	TWeakObjectPtr<ARLCharacter> TargetPlayer;

	float RunningTime = 0.0f;
	FVector SpawnGroundLocation = FVector::ZeroVector;
	bool bHasLanded = false;

	// 포물선 점프 시뮬레이션
	FVector CurrentVelocity = FVector::ZeroVector;
};
