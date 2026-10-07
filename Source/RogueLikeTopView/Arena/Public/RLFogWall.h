#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "RLFogWall.generated.h"

class UBoxComponent;
class UStaticMeshComponent;
class UMaterialInterface;
class UMaterialInstanceDynamic;

/**
 * 길목을 막는 반투명 안개벽
 * - BlockVolume: Pawn만 막음 (마우스 바닥 클릭/카메라/투사체는 통과)
 * - 안개 평면 2장을 살짝 앞뒤로 겹쳐 깊이감 연출 (M_FogWall, 노이즈가 위로 흘러가는 반투명 머티리얼)
 * - ARLArenaManager의 BarrierActors에 넣으면 아레나 클리어 시 Dissolve()로 서서히 걷힘
 */
UCLASS()
class ROGUELIKETOPVIEW_API ARLFogWall : public AActor
{
	GENERATED_BODY()

public:
	ARLFogWall();

	virtual void OnConstruction(const FTransform& Transform) override;

protected:
	virtual void BeginPlay() override;

public:
	// 안개를 서서히 걷어내고, 다 걷히면 충돌 해제 + 숨김
	UFUNCTION(BlueprintCallable, Category = "FogWall")
	void Dissolve();

	// 안개를 즉시 다시 세우고 길을 막음
	UFUNCTION(BlueprintCallable, Category = "FogWall")
	void Restore();

	UFUNCTION(BlueprintPure, Category = "FogWall")
	bool IsBlocking() const { return bBlocking; }

	// 안개가 걷히기 시작할 때 (사운드/파티클 연출용)
	UFUNCTION(BlueprintImplementableEvent, Category = "FogWall")
	void BP_OnDissolveStarted();

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Components")
	TObjectPtr<USceneComponent> SceneRoot;

	// 실제로 길을 막는 박스. 크기(BoxExtent)를 조절하면 안개 평면도 자동으로 맞춰짐 (X: 두께, Y: 폭, Z: 높이의 절반)
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Components")
	TObjectPtr<UBoxComponent> BlockVolume;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Components")
	TObjectPtr<UStaticMeshComponent> FogLayerFront;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Components")
	TObjectPtr<UStaticMeshComponent> FogLayerBack;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "FogWall")
	TObjectPtr<UMaterialInterface> FogMaterial;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "FogWall")
	FLinearColor FogColor = FLinearColor(0.22f, 0.18f, 0.32f, 1.0f);

	// 안개 최대 불투명도 (0~1)
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "FogWall", meta = (ClampMin = "0.0", ClampMax = "1.0"))
	float FogDensity = 0.85f;

	// 안개 평면을 바닥 아래로 얼마나 묻을지 (아래쪽 가장자리 페이드가 바닥에 가려지도록)
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "FogWall", meta = (ClampMin = "0.0"))
	float SinkBelowFloor = 60.0f;

	// 앞뒤 안개 평면 사이 간격
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "FogWall", meta = (ClampMin = "0.0"))
	float LayerSpacing = 40.0f;

	// 걷히는 데 걸리는 시간 (초)
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "FogWall", meta = (ClampMin = "0.05"))
	float DissolveDuration = 1.5f;

private:
	void UpdateLayerLayout();
	void ApplyFade(float Alpha);
	void TickDissolve();

	UPROPERTY(Transient)
	TArray<TObjectPtr<UMaterialInstanceDynamic>> FogMIDs;

	bool bBlocking = true;
	float DissolveElapsed = 0.0f;
	FTimerHandle DissolveTimerHandle;
};
