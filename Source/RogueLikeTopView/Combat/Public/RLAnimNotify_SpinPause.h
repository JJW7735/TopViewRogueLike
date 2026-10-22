#pragma once

#include "CoreMinimal.h"
#include "Animation/AnimNotifies/AnimNotify.h"
#include "RLTypes.h"
#include "RLAnimNotify_SpinPause.generated.h"

class UAnimMontage;

/**
 * 이 노티파이 지점부터 메시를 회전시키면서 몽타주 재생 속도를 서서히 0까지 줄이고(SlowDownTime),
 * 회전이 끝나갈 때 다시 원래 속도로 되돌리는(SpeedUpTime) 노티파이. 뚝 멈췄다 재개되는 대신 느려지며 멈추고 풀립니다.
 *
 * 몽타주가 멈춰 있는 동안에도 진행해야 하므로 회전은 NotifyTick 대신 월드 타이머(SetTimerForNextTick)로 매 프레임 진행합니다.
 * 회전량을 "속도"가 아니라 SpinTurns(바퀴 수)로 받아서 EaseInOut으로 보간하므로, 바퀴 수를 정수로 두면
 * 끝날 때 정확히 원래 방향으로 돌아와 몽타주가 끊김 없이 이어집니다.
 *
 * 액터가 아니라 메시 컴포넌트를 돌리는 이유는 RL Spin Window와 같습니다 (AI 회전 로직과 충돌 방지).
 */
UCLASS(meta = (DisplayName = "RL Spin Pause"))
class ROGUELIKETOPVIEW_API URLAnimNotify_SpinPause : public UAnimNotify
{
	GENERATED_BODY()

public:
	URLAnimNotify_SpinPause();

	virtual void Notify(USkeletalMeshComponent* MeshComp, UAnimSequenceBase* Animation, const FAnimNotifyEventReference& EventReference) override;

protected:
	/** 일시정지 상태로 회전하는 시간(초) */
	UPROPERTY(EditAnywhere, Category = "Spin", meta = (ClampMin = "0.05"))
	float SpinDuration = 1.2f;

	/** 회전 바퀴 수. 정수로 두면 끝날 때 원래 방향과 정확히 맞아떨어집니다 */
	UPROPERTY(EditAnywhere, Category = "Spin")
	float SpinTurns = 2.0f;

	/** true면 반시계 방향(위에서 볼 때)으로 회전 */
	UPROPERTY(EditAnywhere, Category = "Spin")
	bool bCounterClockwise = false;

	/** 가속/감속 곡선 강도. 1이면 등속, 클수록 시작/끝이 느리고 중간이 빠름 */
	UPROPERTY(EditAnywhere, Category = "Spin", meta = (ClampMin = "1.0"))
	float EaseExponent = 2.0f;

	/** 회전 시작과 함께 몽타주 재생 속도를 1 → 0으로 줄이는 시간(초). 이 동안 몽타주가 조금 더 진행되므로
	 *  노티파이는 멈추고 싶은 프레임보다 살짝 앞에 두세요. 0이면 즉시 정지 */
	UPROPERTY(EditAnywhere, Category = "Spin", meta = (ClampMin = "0.0"))
	float SlowDownTime = 0.25f;

	/** 회전이 끝나기 전 이 시간(초) 동안 재생 속도를 0 → 원래 속도로 되돌림. 0이면 회전 종료 시 즉시 재개 */
	UPROPERTY(EditAnywhere, Category = "Spin", meta = (ClampMin = "0.0"))
	float SpeedUpTime = 0.25f;

	/** 회전 중 주기적으로 원형 판정을 줄지 여부 */
	UPROPERTY(EditAnywhere, Category = "Spin|Damage")
	bool bDealDamage = false;

	/** 판정 간격(초) */
	UPROPERTY(EditAnywhere, Category = "Spin|Damage", meta = (EditCondition = "bDealDamage", ClampMin = "0.05"))
	float HitInterval = 0.35f;

	/** 원형 판정 반지름 */
	UPROPERTY(EditAnywhere, Category = "Spin|Damage", meta = (EditCondition = "bDealDamage"))
	float HitRadius = 450.0f;

	UPROPERTY(EditAnywhere, Category = "Spin|Damage", meta = (EditCondition = "bDealDamage"))
	FRLDamageData DamageData;

	/** 판정 범위 디버그 표시 */
	UPROPERTY(EditAnywhere, Category = "Spin|Damage", meta = (EditCondition = "bDealDamage"))
	bool bDrawDebug = true;

private:
	// 노티파이 인스턴스는 몽타주 에셋에 하나만 존재하므로(여러 보스가 공유), 메시별로 상태를 따로 저장합니다.
	struct FSpinState
	{
		TWeakObjectPtr<UAnimMontage> Montage;
		FRotator OriginalRelativeRotation;
		float OriginalPlayRate = 1.0f;
		float Elapsed = 0.0f;
		float HitTimer = 0.0f;
	};

	TMap<TWeakObjectPtr<USkeletalMeshComponent>, FSpinState> SpinStates;

	void TickSpin(TWeakObjectPtr<USkeletalMeshComponent> WeakMesh);
	void FinishSpin(USkeletalMeshComponent* MeshComp, bool bRestorePlayRate);
	float GetPlayRateScale(float Elapsed) const;
};
