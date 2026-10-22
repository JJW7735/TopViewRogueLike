#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "RLThrownAxe.generated.h"

class UPoseableMeshComponent;
class USkeletalMeshComponent;

/**
 * 보스 AxeLeap 패턴에서 던지는 도끼 (연출 전용, 판정 없음).
 *
 * 카이메라 도끼는 별도 메시가 없고 몸 스켈레탈 메시에 붙어 있어서, 보스 메시를 PoseableMesh로 복사한 뒤
 * 무기 본(weapon_r)을 제외한 모든 본을 0.001배로 줄여 "도끼만 남은" 메시를 만듭니다. 무기 본은 액터 원점에
 * 놓이므로 액터를 움직이고 돌리면 도끼가 그대로 따라갑니다.
 *
 * 날아가는 동안 앞으로 공중제비를 돌다가 LandedPitch 각도로 땅에 박힙니다. 도끼 본의 축 방향은 메시마다 달라서
 * 박힌 모양이 어색하면 BP 자식에서 AxeRotationOffset / LandedPitch를 조절하세요.
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

	virtual void Tick(float DeltaTime) override;

protected:
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Axe")
	TObjectPtr<UPoseableMeshComponent> AxeMesh;

	// 무기 본에 줄 회전 보정. 도끼날이 액터 정면(X)을 향하고 손잡이가 아래를 향하도록 맞추는 용도
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Axe")
	FRotator AxeRotationOffset = FRotator::ZeroRotator;

	// 초당 공중제비 바퀴 수 (실제 바퀴 수는 비행 시간에 맞춰 반올림해서 박힐 때 각도가 딱 맞게 함)
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Axe", meta = (ClampMin = "0.0"))
	float SpinTurnsPerSecond = 3.0f;

	// 땅에 박혔을 때 앞으로 숙인 각도(도). 음수 = 날이 앞으로 숙여져 땅에 꽂힘
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Axe")
	float LandedPitch = -35.0f;

	// 목표 지점보다 이만큼 아래로 박힘(cm)
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Axe", meta = (ClampMin = "0.0"))
	float EmbedDepth = 20.0f;

	// ★ 땅에 박히는 순간 호출. BP에서 먼지/균열 VFX, 사운드를 붙이세요
	UFUNCTION(BlueprintImplementableEvent, Category = "Axe")
	void OnAxeLanded();

private:
	FVector StartLocation = FVector::ZeroVector;
	FVector EndLocation = FVector::ZeroVector;
	float FlightYaw = 0.0f;
	float FlightDuration = 0.0f;
	float FlightArcHeight = 0.0f;
	float FlightElapsed = 0.0f;
	float TotalPitch = 0.0f;
	bool bFlying = false;
};
