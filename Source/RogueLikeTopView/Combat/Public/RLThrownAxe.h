#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "RLThrownAxe.generated.h"

class UArrowComponent;
class UPoseableMeshComponent;
class USkeletalMesh;
class USkeletalMeshComponent;
class USkinnedAsset;
class ARLThrownAxe;

DECLARE_MULTICAST_DELEGATE_OneParam(FOnThrownAxeLanded, ARLThrownAxe*);

/**
 * 보스 AxeLeap 패턴에서 던지는 도끼 (연출 전용, 판정은 던진 보스가 OnLanded를 받아서 처리).
 *
 * 카이메라 도끼는 별도 메시가 없고 몸 스켈레탈 메시에 붙어 있어서, 보스 메시를 PoseableMesh로 복사한 뒤
 * 무기 본(weapon_r)을 제외한 모든 본을 0.001배로 줄여 "도끼만 남은" 메시를 만듭니다. 무기 본은 액터 원점에
 * 놓이므로 액터를 움직이고 돌리면 도끼가 그대로 따라갑니다.
 *
 * 기준 자세: AxeRotationOffset을 적용했을 때 "손잡이가 위(+Z), 도끼날이 정면(+X, 화살표 방향)"으로 서 있어야
 * 합니다. 날아가는 동안 이 자세에서 앞으로 세로 공중제비(Pitch)를 돌다가 LandedPitch 각도로 박힙니다.
 * 레벨에 이 액터(또는 BP 자식)를 놓고 PreviewSourceMesh를 넣으면 에디터에서 도끼 모양을 보면서 맞출 수 있습니다.
 */
UCLASS()
class ROGUELIKETOPVIEW_API ARLThrownAxe : public AActor
{
	GENERATED_BODY()

public:
	ARLThrownAxe();

	// SourceMesh(보스 메시)를 복사해서 WeaponBoneName 본(과 그 자식 본)만 보이게 만듭니다. 본을 못 찾으면 false
	bool InitFromWeapon(USkeletalMeshComponent* SourceMesh, FName WeaponBoneName);

	// 현재 위치에서 TargetLocation(바닥 지점)까지 포물선으로 날아가 박힙니다
	void Launch(const FVector& TargetLocation, float FlightTime, float ArcHeight);

	// 땅에 박히는 순간 (던진 보스가 도끼 판정을 붙임)
	FOnThrownAxeLanded OnLanded;

	virtual void Tick(float DeltaTime) override;
	virtual void OnConstruction(const FTransform& Transform) override;

protected:
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Axe")
	TObjectPtr<UPoseableMeshComponent> AxeMesh;

#if WITH_EDITORONLY_DATA
	// 미리보기에서 정면(+X) 방향 표시
	UPROPERTY()
	TObjectPtr<UArrowComponent> ForwardArrow;
#endif

	// 무기 본 회전 보정. 손잡이가 위(+Z), 도끼날이 정면(+X)을 향하도록 맞춥니다
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Axe")
	FRotator AxeRotationOffset = FRotator::ZeroRotator;

	// 무기 본 위치 보정(cm, 스케일 적용 전). 회전 중심을 도끼 무게중심 근처로 옮길 때 사용
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Axe")
	FVector AxeLocationOffset = FVector::ZeroVector;

	// 초당 공중제비 바퀴 수 (실제 바퀴 수는 비행 시간에 맞춰 반올림해서 박힐 때 각도가 딱 맞게 함)
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Axe", meta = (ClampMin = "0.0"))
	float SpinTurnsPerSecond = 3.0f;

	// 박혔을 때의 Pitch(도). 기준 자세에서 앞으로 굴러간 각도로, -160이면 도끼날이 앞쪽 바닥에 70도로 꽂히고
	// 손잡이가 뒤쪽 위로 70도 들린 모양 (-90 = 손잡이가 수평)
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Axe")
	float LandedPitch = -160.0f;

	// 목표 지점보다 이만큼 아래로 박힘(cm)
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Axe", meta = (ClampMin = "0.0"))
	float EmbedDepth = 30.0f;

	// ★ 에디터 미리보기용: 여기에 보스 스켈레탈 메시(Khaimera)를 넣으면 레벨에 놓은 이 액터가 도끼만 보여줍니다.
	// 게임 중에는 쓰지 않습니다 (InitFromWeapon이 던진 보스 메시로 덮어씀)
	UPROPERTY(EditAnywhere, Category = "Axe|Preview")
	TObjectPtr<USkeletalMesh> PreviewSourceMesh;

	UPROPERTY(EditAnywhere, Category = "Axe|Preview")
	FName PreviewWeaponBoneName = FName("weapon_r");

	// 미리보기 스케일 (보스2 메시 스케일과 맞춤)
	UPROPERTY(EditAnywhere, Category = "Axe|Preview", meta = (ClampMin = "0.01"))
	float PreviewScale = 3.0f;

	// 미리보기에서 박힌 자세(LandedPitch)로 보여줄지 여부 (false면 기준 자세)
	UPROPERTY(EditAnywhere, Category = "Axe|Preview")
	bool bPreviewLandedPose = false;

	// ★ 땅에 박히는 순간 호출. BP에서 먼지/균열 VFX, 사운드를 붙이세요
	UFUNCTION(BlueprintImplementableEvent, Category = "Axe")
	void OnAxeLanded();

private:
	bool SetupWeaponPose(USkinnedAsset* SkinnedAsset, FName WeaponBoneName);

	FVector StartLocation = FVector::ZeroVector;
	FVector EndLocation = FVector::ZeroVector;
	float FlightYaw = 0.0f;
	float FlightDuration = 0.0f;
	float FlightArcHeight = 0.0f;
	float FlightElapsed = 0.0f;
	float TotalPitch = 0.0f;
	bool bFlying = false;
};
