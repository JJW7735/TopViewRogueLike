#pragma once

#include "CoreMinimal.h"
#include "GameplayCueNotify_Static.h"
#include "GameplayCueNotify_Actor.h"
#include "RLGameplayCueNotifies.generated.h"

class UFXSystemAsset;
class UFXSystemComponent;
class URLSkillData;

// ★ 스킬 연출(VFX)은 GameplayCue로 분리합니다.
// GA/GE는 "GameplayCue.Skill.*" 태그만 보내고, 어떤 이펙트를 어디에 띄울지는 이 노티파이가 결정합니다.
// 이펙트 에셋은 지금처럼 스킬 데이터(DA_Skill_*)의 ChargeVFX/ReleaseVFX에 두고,
// 큐 파라미터의 SourceObject(= 스킬 데이터)에서 꺼내 씁니다 → 스킬마다 GC 에셋을 따로 만들 필요 없음.
//
// 에디터 쪽 연결: Content/GAS/GameplayCues 의 GC_* 블루프린트가 아래 클래스를 상속하고 GameplayCueTag만 지정합니다.

// 스킬 데이터의 어떤 이펙트 슬롯을 쓸지
UENUM(BlueprintType)
enum class ERLSkillCueVFX : uint8
{
	Charge,  // URLSkillData::ChargeVFX (캐스팅/차지 중)
	Release  // URLSkillData::ReleaseVFX (발동 순간, 버프 지속 중)
};

namespace RLGameplayCueUtils
{
	// 큐 파라미터에서 스킬 데이터 꺼내기 (SourceObject → 없으면 EffectContext의 SourceObject)
	ROGUELIKETOPVIEW_API const URLSkillData* GetSkillData(const FGameplayCueParameters& Parameters);

	ROGUELIKETOPVIEW_API UFXSystemAsset* GetSkillVFX(const URLSkillData* Skill, ERLSkillCueVFX Source);

	// 나이아가라/캐스케이드 공용 스폰. bAttach면 SocketName을 가진 메시(무기 등)에, 없으면 캐릭터 메시에 붙임
	ROGUELIKETOPVIEW_API UFXSystemComponent* SpawnVFX(AActor* Target, UFXSystemAsset* VFXAsset, const FVector& Location, bool bAttach, FName SocketName);
}

// 1회성 이펙트 (GameplayCue.Skill.Release): 발동 위치(Parameters.Location)에 ReleaseVFX를 띄움
UCLASS(Blueprintable)
class ROGUELIKETOPVIEW_API URLGameplayCueNotify_SkillBurst : public UGameplayCueNotify_Static
{
	GENERATED_BODY()

public:
	virtual bool OnExecute_Implementation(AActor* MyTarget, const FGameplayCueParameters& Parameters) const override;

protected:
	UPROPERTY(EditDefaultsOnly, Category = "Skill Cue")
	ERLSkillCueVFX VFXSource = ERLSkillCueVFX::Release;
};

// 지속 이펙트 (GameplayCue.Skill.Charge / GameplayCue.Skill.Buff): 큐가 붙어 있는 동안 유지, 제거되면 끔
// - Charge: 스킬 GA가 AddGameplayCue → 발동/취소/어빌리티 종료 시 자동 제거
// - Buff  : URLGE_SkillBuff의 GameplayCues에 등록 → 버프 GE가 사라질 때 GAS가 자동 제거
UCLASS(Blueprintable)
class ROGUELIKETOPVIEW_API ARLGameplayCueNotify_SkillLoop : public AGameplayCueNotify_Actor
{
	GENERATED_BODY()

public:
	ARLGameplayCueNotify_SkillLoop();

	virtual bool WhileActive_Implementation(AActor* MyTarget, const FGameplayCueParameters& Parameters) override;
	virtual bool OnRemove_Implementation(AActor* MyTarget, const FGameplayCueParameters& Parameters) override;

protected:
	UPROPERTY(EditDefaultsOnly, Category = "Skill Cue")
	ERLSkillCueVFX VFXSource = ERLSkillCueVFX::Charge;

	// true면 스킬 데이터의 bAttachToWeapon과 관계없이 캐릭터 메시에 붙임 (버프 오라)
	UPROPERTY(EditDefaultsOnly, Category = "Skill Cue")
	bool bForceAttachToMesh = false;

	// true면 제거 즉시 사라짐 (차지 이펙트 → 발동 이펙트로 바로 교체), false면 파티클이 자연스럽게 소멸
	UPROPERTY(EditDefaultsOnly, Category = "Skill Cue")
	bool bStopImmediatelyOnRemove = true;

private:
	UPROPERTY(Transient)
	TObjectPtr<UFXSystemComponent> ActiveVFX = nullptr;
};
