#pragma once

#include "CoreMinimal.h"
#include "Animation/AnimNotifies/AnimNotifyState.h"
#include "RLAnimNotifyState_MeleeDamage.generated.h"

/**
 * 근접 공격 몽타주에 붙여서 쓰는 데미지 판정용 NotifyState.
 *
 * 왜 Notify(순간)가 아니라 NotifyState(구간)로 만들었는가:
 * 단일 프레임 Notify는 정확히 그 프레임에서만 판정하기 때문에 몽타주 타이밍을 살짝만 조절해도
 * 판정이 씹히기 쉽습니다. NotifyState는 시작~끝 구간 동안 매 틱 스윕 트레이스를 하기 때문에
 * 훨씬 안정적으로 "무기가 스쳐 지나가는 구간"을 표현할 수 있어, 실무에서도 근접 무기 판정은
 * 보통 NotifyState(또는 별도 콜리전 컴포넌트 Enable/Disable) 방식을 많이 씁니다.
 *
 * 사용법: 몽타주 Notify 트랙에 이 NotifyState를 드래그해서 타격 구간(스윙이 실제로 닿는 프레임 구간)에
 * 맞춰 길이를 조절하고, 디테일 패널에서 Damage / TraceRadius / StartSocketName / EndSocketName 등을
 * 패턴별로 세팅하면 됩니다. SwingCombo / GroundSlam / SoulPull / Subjugation 각 몽타주에 이 NotifyState를
 * 독립적으로 배치하고 값만 다르게 주면 되므로, C++ 판정 로직은 공유하고 값만 에디터(BP/몽타주)에서
 * 파생시키는 구조입니다 — 프로젝트에서 원하시는 "C++ 뼈대 + BP/에디터 파생" 패턴에 맞춘 설계입니다.
 *
 * 주의: 아래 클래스의 API 매크로(ROGUELIKETOPVIEW_API)는 프로젝트 모듈 이름을 기준으로 추정한 값입니다.
 * 실제 프로젝트의 다른 헤더(RLHealthComponent.h 등)에 쓰여 있는 매크로 이름과 반드시 대조해서 맞춰주세요.
 */
UCLASS(meta = (DisplayName = "RL Melee Damage Window"))
class ROGUELIKETOPVIEW_API URLAnimNotifyState_MeleeDamage : public UAnimNotifyState
{
	GENERATED_BODY()

public:
	URLAnimNotifyState_MeleeDamage();

	virtual void NotifyBegin(USkeletalMeshComponent* MeshComp, UAnimSequenceBase* Animation, float TotalDuration, const FAnimNotifyEventReference& EventReference) override;
	virtual void NotifyTick(USkeletalMeshComponent* MeshComp, UAnimSequenceBase* Animation, float FrameDeltaTime, const FAnimNotifyEventReference& EventReference) override;
	virtual void NotifyEnd(USkeletalMeshComponent* MeshComp, UAnimSequenceBase* Animation, const FAnimNotifyEventReference& EventReference) override;

protected:
	/** 이 판정으로 줄 데미지량. 패턴마다 다르게 세팅 (예: SwingCombo는 약하게, GroundSlam은 강하게) */
	UPROPERTY(EditAnywhere, Category = "Damage")
	float Damage = 20.0f;

	/** 스윕 트레이스 반경 */
	UPROPERTY(EditAnywhere, Category = "Damage")
	float TraceRadius = 20.0f;

	/**
	 * 무기 시작점 소켓 이름. Sevarog 스켈레톤에 실제로 존재하는 소켓 이름으로 지정해야 합니다.
	 * (스켈레톤 에디터의 Skeleton Tree에서 무기 손잡이/날 쪽 소켓 이름을 직접 확인해서 채워주세요.)
	 */
	UPROPERTY(EditAnywhere, Category = "Damage")
	FName StartSocketName = TEXT("weapon_r");

	/** 무기 끝점 소켓 이름 */
	UPROPERTY(EditAnywhere, Category = "Damage")
	FName EndSocketName = TEXT("weapon_01");

	/** 한 번의 NotifyState 구간 동안 같은 대상을 여러 번 맞히지 않도록 막을지 여부 */
	UPROPERTY(EditAnywhere, Category = "Damage")
	bool bOnlyHitOnce = true;

private:
	/** 이번 NotifyState 구간에서 이미 맞힌 액터 목록 (중복 타격 방지용) */
	UPROPERTY(Transient)
	TSet<TWeakObjectPtr<AActor>> HitActorsThisWindow;

	void PerformSweepAndApplyDamage(USkeletalMeshComponent* MeshComp);
};
