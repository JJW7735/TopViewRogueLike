#pragma once

#include "CoreMinimal.h"
#include "Animation/AnimNotifies/AnimNotifyState.h"
#include "RLTypes.h"
#include "RLAnimNotifyState_Spin.generated.h"

/**
 * 몽타주 구간 동안 메시를 Yaw 방향으로 계속 회전시키는 NotifyState (회전 공격 프로토타입용).
 *
 * 액터가 아니라 "메시 컴포넌트"를 돌리는 이유: AI 컨트롤러가 포커스/이동 방향으로 액터 회전을 매 틱
 * 덮어쓰기 때문에 액터를 돌리면 서로 싸웁니다. 메시만 돌리면 AI 로직과 충돌 없이 "보이는 회전"만 만들 수
 * 있고, NotifyEnd에서 원래 상대 회전으로 되돌립니다. 몽타주 에디터 미리보기에서도 그대로 돌아가서
 * 게임을 켜지 않고도 느낌을 확인할 수 있습니다.
 *
 * bDealDamage를 켜면 HitInterval마다 ARLCharacter::CreateSectorHitbox(안전각 0 = 360도 원형)를 호출해
 * 회전하는 동안 주변을 주기적으로 타격합니다.
 */
UCLASS(meta = (DisplayName = "RL Spin Window"))
class ROGUELIKETOPVIEW_API URLAnimNotifyState_Spin : public UAnimNotifyState
{
	GENERATED_BODY()

public:
	URLAnimNotifyState_Spin();

	virtual void NotifyBegin(USkeletalMeshComponent* MeshComp, UAnimSequenceBase* Animation, float TotalDuration, const FAnimNotifyEventReference& EventReference) override;
	virtual void NotifyTick(USkeletalMeshComponent* MeshComp, UAnimSequenceBase* Animation, float FrameDeltaTime, const FAnimNotifyEventReference& EventReference) override;
	virtual void NotifyEnd(USkeletalMeshComponent* MeshComp, UAnimSequenceBase* Animation, const FAnimNotifyEventReference& EventReference) override;

protected:
	/** 초당 회전 각도(도). 양수 = 시계 방향(위에서 볼 때). 720이면 1초에 두 바퀴 */
	UPROPERTY(EditAnywhere, Category = "Spin")
	float YawSpeed = 720.0f;

	/** 구간 시작/끝에서 회전 속도를 부드럽게 올리고 내리는 시간(초). 0이면 즉시 최고 속도 */
	UPROPERTY(EditAnywhere, Category = "Spin", meta = (ClampMin = "0.0"))
	float RampTime = 0.15f;

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
	// NotifyState 인스턴스는 몽타주 에셋에 하나만 존재하므로(여러 보스가 공유), 메시별로 상태를 따로 저장합니다.
	struct FSpinState
	{
		FRotator OriginalRelativeRotation;
		float Elapsed = 0.0f;
		float TotalDuration = 0.0f;
		float HitTimer = 0.0f;
	};

	TMap<TWeakObjectPtr<USkeletalMeshComponent>, FSpinState> SpinStates;
};
