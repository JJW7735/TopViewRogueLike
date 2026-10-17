#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "RLItemDrop.generated.h"

class USphereComponent;
class UStaticMeshComponent;
class UPointLightComponent;
class UStaticMesh;
class UMaterialInstanceDynamic;
class UMaterialInterface;
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

	// 어두운 맵에서도 드롭이 보이도록 하는 작은 발광 라이트
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Components")
	TObjectPtr<UPointLightComponent> GlowLight;

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

	// 자석 흡수 이동 속도 (낮출수록 플레이어에게 천천히 빨려들어가서 눈에 더 잘 띔)
	UPROPERTY(EditDefaultsOnly, BlueprintReadWrite, Category = "Drop|Magnet")
	float MagnetSpeed = 500.0f;

	// 스폰 후 이 시간 동안은 흡수/획득되지 않음 (튀어나오는 연출을 최소한 이만큼은 보여줌)
	UPROPERTY(EditDefaultsOnly, BlueprintReadWrite, Category = "Drop|Feel", meta = (ClampMin = "0.0"))
	float MinVisibleTime = 0.45f;

	// 바닥 안착 후 바닥에서 떠 있는 높이 (둥둥 연출의 중심)
	UPROPERTY(EditDefaultsOnly, BlueprintReadWrite, Category = "Drop|Feel")
	float HoverHeight = 35.0f;

	// 골드 드롭 메시/라이트 색 (선명한 노란색)
	UPROPERTY(EditDefaultsOnly, BlueprintReadWrite, Category = "Drop|Visual")
	FLinearColor GoldColor = FLinearColor(1.0f, 0.9f, 0.1f);

	// 아이템 드롭 메시/라이트 색
	UPROPERTY(EditDefaultsOnly, BlueprintReadWrite, Category = "Drop|Visual")
	FLinearColor ItemColor = FLinearColor(0.35f, 0.75f, 1.0f);

	// 골드 드롭 메시 (기본: 엔진 Cylinder를 납작하게 세운 동전). BP에서 전용 에셋으로 교체 가능
	UPROPERTY(EditDefaultsOnly, BlueprintReadWrite, Category = "Drop|Visual")
	TObjectPtr<UStaticMesh> GoldMesh;

	// 아이템 드롭 메시 (기본: 엔진 Sphere)
	UPROPERTY(EditDefaultsOnly, BlueprintReadWrite, Category = "Drop|Visual")
	TObjectPtr<UStaticMesh> ItemMesh;

	// 드롭 메시 머티리얼 (기본: M_DropGlow, "Color" 파라미터로 베이스/발광색 지정). 어두운 맵에서도 보이도록 발광
	UPROPERTY(EditDefaultsOnly, BlueprintReadWrite, Category = "Drop|Visual")
	TObjectPtr<UMaterialInterface> DropMaterial;

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
	/** MinVisibleTime이 지났고 플레이어 진영이면 자석 흡수 시작 */
	void TryStartAttraction(ARLCharacter* PlayerChar);

	/** 골드/아이템을 플레이어에게 지급하고 드롭 제거 */
	void GrantTo(ARLCharacter* PlayerChar);

	/** 드롭 종류에 맞게 메시/라이트 색 갱신 */
	void ApplyDropColor();

	/** 위치 아래의 바닥 지점. 못 찾으면 입력 위치 그대로 */
	FVector FindGroundBelow(const FVector& From) const;

	bool bIsBeingAttracted = false;
	TWeakObjectPtr<ARLCharacter> TargetPlayer;

	UPROPERTY(Transient)
	TObjectPtr<UMaterialInstanceDynamic> MeshMID;

	// 스폰 후 경과 시간 (MinVisibleTime 판정용)
	float Age = 0.0f;

	float RunningTime = 0.0f;
	// 착지 지점의 바닥 위치 (둥둥 연출은 여기서 HoverHeight 위)
	FVector SpawnGroundLocation = FVector::ZeroVector;
	bool bHasLanded = false;

	// 포물선 점프 시뮬레이션
	FVector CurrentVelocity = FVector::ZeroVector;
};
