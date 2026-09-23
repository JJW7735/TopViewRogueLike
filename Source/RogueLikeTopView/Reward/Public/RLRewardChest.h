#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "RLRewardTypes.h"
#include "RLRewardChest.generated.h"

class USphereComponent;
class UStaticMeshComponent;
class UNiagaraComponent;
class ARLCharacter;
class URLRewardSelectWidget;

/**
 * 필드에 배치되거나 몬스터/보스 처치 시 스폰되는 상호작용 가능한 보상 상자 액터
 */
UCLASS()
class ROGUELIKETOPVIEW_API ARLRewardChest : public AActor
{
	GENERATED_BODY()

public:
	ARLRewardChest();

	UFUNCTION()
	void OnOverlapBegin(UPrimitiveComponent* OverlappedComp, AActor* OtherActor,
		UPrimitiveComponent* OtherComp, int32 OtherBodyIndex,
		bool bFromSweep, const FHitResult& SweepResult);

	UFUNCTION()
	void OnOverlapEnd(UPrimitiveComponent* OverlappedComp, AActor* OtherActor,
		UPrimitiveComponent* OtherComp, int32 OtherBodyIndex);

	/** 상자를 열고 보상 UI를 엽니다 (F키 상호작용 시 호출) */
	UFUNCTION(BlueprintCallable, Category = "Reward|Chest")
	void OpenChest(ARLCharacter* Interactor);

	/** 상자 상태를 닫힘으로 초기화 (테스트용) */
	UFUNCTION(BlueprintCallable, Category = "Reward|Chest")
	void ResetChest();

	UFUNCTION(BlueprintPure, Category = "Reward|Chest")
	bool IsOpen() const { return bIsOpen; }

	/** 상자가 열렸을 때 BP에서 연출(VFX, SFX, 애니메이션)할 수 있는 이벤트 */
	UFUNCTION(BlueprintImplementableEvent, Category = "Reward|Chest")
	void OnChestOpened(ARLCharacter* Interactor);

protected:
	virtual void BeginPlay() override;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Components")
	TObjectPtr<USceneComponent> SceneRoot;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Components")
	TObjectPtr<UStaticMeshComponent> ChestBaseMesh;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Components")
	TObjectPtr<UStaticMeshComponent> ChestLidMesh;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Components")
	TObjectPtr<USphereComponent> InteractionSphere;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Components")
	TObjectPtr<UNiagaraComponent> ChestFX;

	/** 화면에 띄울 3지선다 보상 선택 위젯 클래스 */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Reward|Chest")
	TSubclassOf<URLRewardSelectWidget> RewardWidgetClass;

	/** 추첨할 카드 개수 (기본 3개) */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Reward|Chest")
	int32 CardCount = 3;

	/** 상호작용 유효 반경 */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Reward|Chest")
	float InteractionRadius = 220.0f;

	UPROPERTY(VisibleInstanceOnly, BlueprintReadOnly, Category = "Reward|Chest")
	bool bIsOpen = false;

	UPROPERTY()
	TWeakObjectPtr<ARLCharacter> CurrentOverlappedPlayer;
};
