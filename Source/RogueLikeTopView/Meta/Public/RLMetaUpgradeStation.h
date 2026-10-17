#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "RLMetaUpgradeStation.generated.h"

class USphereComponent;
class UStaticMeshComponent;
class ARLCharacter;
class URLMetaUpgradeWidget;

/**
 * 월드에 배치하는 영구 강화소 (메타 상점) 액터
 * - 포탈/보상 상자와 같은 근접 상호작용 패턴: InteractionSphere 진입 시 ARLCharacter::SetNearbyMetaStation 등록
 * - F키(TryInteract) → Interact() → WBP_MetaUpgrade 위젯 생성 및 표시
 * - 메시/연출은 BP_MetaUpgradeStation에서 자유롭게 지정
 */
UCLASS()
class ROGUELIKETOPVIEW_API ARLMetaUpgradeStation : public AActor
{
	GENERATED_BODY()

public:
	ARLMetaUpgradeStation();

	UFUNCTION()
	void OnOverlapBegin(UPrimitiveComponent* OverlappedComp, AActor* OtherActor,
		UPrimitiveComponent* OtherComp, int32 OtherBodyIndex,
		bool bFromSweep, const FHitResult& SweepResult);

	UFUNCTION()
	void OnOverlapEnd(UPrimitiveComponent* OverlappedComp, AActor* OtherActor,
		UPrimitiveComponent* OtherComp, int32 OtherBodyIndex);

	/** 강화 상점 UI를 엽니다 (F키 상호작용 시 호출). 이미 열려 있으면 무시 */
	UFUNCTION(BlueprintCallable, Category = "Meta|Station")
	void Interact(ARLCharacter* Interactor);

	/** 강화 상점 UI가 현재 열려 있는지 여부 */
	UFUNCTION(BlueprintPure, Category = "Meta|Station")
	bool IsWidgetOpen() const;

	/** 강화소를 사용했을 때 BP에서 연출(VFX, SFX)할 수 있는 이벤트 */
	UFUNCTION(BlueprintImplementableEvent, Category = "Meta|Station")
	void OnStationUsed(ARLCharacter* Interactor);

	/** 플레이어가 범위에 들어왔을 때 HUD에 띄울 상호작용 안내 문구 (인스턴스별로 에디터에서 지정) */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Interaction")
	FText InteractPromptText;

protected:
	virtual void BeginPlay() override;
	virtual void EndPlay(const EEndPlayReason::Type EndPlayReason) override;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Components")
	TObjectPtr<USceneComponent> SceneRoot;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Components")
	TObjectPtr<UStaticMeshComponent> StationMesh;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Components")
	TObjectPtr<USphereComponent> InteractionSphere;

	/** 화면에 띄울 영구 강화 위젯 클래스 (비어 있으면 /Game/Blueprint/UI/WBP_MetaUpgrade 로드) */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Meta|Station")
	TSubclassOf<URLMetaUpgradeWidget> MetaWidgetClass;

	/** 상호작용 유효 반경 */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Meta|Station")
	float InteractionRadius = 220.0f;

	UPROPERTY()
	TWeakObjectPtr<ARLCharacter> CurrentOverlappedPlayer;

	UPROPERTY()
	TWeakObjectPtr<URLMetaUpgradeWidget> ActiveWidget;
};
