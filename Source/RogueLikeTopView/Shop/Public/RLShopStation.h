#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "RLShopStation.generated.h"

class USphereComponent;
class UStaticMeshComponent;
class ARLCharacter;
class URLShopWidget;

/**
 * 월드에 배치하는 상점 액터
 * - 포탈/보상 상자/강화소와 같은 근접 상호작용 패턴: InteractionSphere 진입 시 ARLCharacter::SetNearbyShop 등록
 * - F키(TryInteract) → Interact() → WBP_ShopTest 위젯 생성 및 표시
 * - 메시/연출은 BP_ShopStation에서 자유롭게 지정 (지금은 임시 박스, 추후 NPC 메시로 교체 가능)
 * - Interact()는 BlueprintNativeEvent라서, 나중에 NPC 대사 연출이 필요해지면 BP에서 오버라이드해서
 *   "대사 먼저 보여주고 끝나면 상점 열기"로 바꿀 수 있음 (C++ 구조 변경 불필요)
 */
UCLASS()
class ROGUELIKETOPVIEW_API ARLShopStation : public AActor
{
	GENERATED_BODY()

public:
	ARLShopStation();

	UFUNCTION()
	void OnOverlapBegin(UPrimitiveComponent* OverlappedComp, AActor* OtherActor,
		UPrimitiveComponent* OtherComp, int32 OtherBodyIndex,
		bool bFromSweep, const FHitResult& SweepResult);

	UFUNCTION()
	void OnOverlapEnd(UPrimitiveComponent* OverlappedComp, AActor* OtherActor,
		UPrimitiveComponent* OtherComp, int32 OtherBodyIndex);

	/** 상점 UI를 엽니다 (F키 상호작용 시 호출). BP에서 오버라이드해서 NPC 대사 연출 등을 끼워넣을 수 있음 */
	UFUNCTION(BlueprintNativeEvent, Category = "Shop|Station")
	void Interact(ARLCharacter* Interactor);
	virtual void Interact_Implementation(ARLCharacter* Interactor);

	/** 상점 UI가 현재 열려 있는지 여부 */
	UFUNCTION(BlueprintPure, Category = "Shop|Station")
	bool IsWidgetOpen() const;

	/** 상점을 이용했을 때 BP에서 연출(VFX, SFX)할 수 있는 이벤트 */
	UFUNCTION(BlueprintImplementableEvent, Category = "Shop|Station")
	void OnStationUsed(ARLCharacter* Interactor);

	/** 플레이어가 범위에 들어왔을 때 표시할 상호작용 안내 문구 (인스턴스별로 에디터에서 지정) */
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

	/** 화면에 띄울 상점 위젯 클래스 (비어 있으면 /Game/Blueprint/UI/WBP_ShopTest 로드) */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Shop|Station")
	TSubclassOf<URLShopWidget> ShopWidgetClass;

	/** 상호작용 유효 반경 */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Shop|Station")
	float InteractionRadius = 220.0f;

	UPROPERTY()
	TWeakObjectPtr<ARLCharacter> CurrentOverlappedPlayer;

	UPROPERTY()
	TWeakObjectPtr<URLShopWidget> ActiveWidget;
};
