#include "RLGameplayCueNotifies.h"
#include "RLSkillData.h"
#include "GameFramework/Character.h"
#include "Components/SkeletalMeshComponent.h"
#include "Kismet/GameplayStatics.h"
#include "Particles/ParticleSystem.h"
#include "Particles/ParticleSystemComponent.h"
#include "NiagaraSystem.h"
#include "NiagaraFunctionLibrary.h"
#include "NiagaraComponent.h"

namespace RLGameplayCueUtils
{
	const URLSkillData* GetSkillData(const FGameplayCueParameters& Parameters)
	{
		return Cast<URLSkillData>(Parameters.GetSourceObject());
	}

	UFXSystemAsset* GetSkillVFX(const URLSkillData* Skill, ERLSkillCueVFX Source)
	{
		if (!Skill)
		{
			return nullptr;
		}
		return Source == ERLSkillCueVFX::Charge ? Skill->ChargeVFX : Skill->ReleaseVFX;
	}

	UFXSystemComponent* SpawnVFX(AActor* Target, UFXSystemAsset* VFXAsset, const FVector& Location, bool bAttach, FName SocketName)
	{
		if (!VFXAsset || !Target)
		{
			return nullptr;
		}

		const ACharacter* TargetCharacter = Cast<ACharacter>(Target);
		USceneComponent* TargetAttachComp = TargetCharacter ? TargetCharacter->GetMesh() : Target->GetRootComponent();
		if (bAttach && SocketName != NAME_None)
		{
			TArray<UMeshComponent*> AllMeshes;
			Target->GetComponents<UMeshComponent>(AllMeshes);
			for (UMeshComponent* MeshComp : AllMeshes)
			{
				if (MeshComp->DoesSocketExist(SocketName))
				{
					TargetAttachComp = MeshComp;
					break;
				}
			}
		}

		if (UNiagaraSystem* NiagaraSys = Cast<UNiagaraSystem>(VFXAsset))
		{
			if (bAttach && TargetAttachComp)
			{
				return UNiagaraFunctionLibrary::SpawnSystemAttached(NiagaraSys, TargetAttachComp, SocketName, FVector::ZeroVector, FRotator::ZeroRotator, EAttachLocation::SnapToTarget, true);
			}
			return UNiagaraFunctionLibrary::SpawnSystemAtLocation(Target->GetWorld(), NiagaraSys, Location, FRotator::ZeroRotator);
		}
		if (UParticleSystem* CascadeSys = Cast<UParticleSystem>(VFXAsset))
		{
			if (bAttach && TargetAttachComp)
			{
				return UGameplayStatics::SpawnEmitterAttached(CascadeSys, TargetAttachComp, SocketName, FVector::ZeroVector, FRotator::ZeroRotator, EAttachLocation::SnapToTarget, true);
			}
			return UGameplayStatics::SpawnEmitterAtLocation(Target->GetWorld(), CascadeSys, Location, FRotator::ZeroRotator);
		}
		return nullptr;
	}
}

// =====================================================================
// 1회성 (GameplayCue.Skill.Release)
// =====================================================================
bool URLGameplayCueNotify_SkillBurst::OnExecute_Implementation(AActor* MyTarget, const FGameplayCueParameters& Parameters) const
{
	const URLSkillData* Skill = RLGameplayCueUtils::GetSkillData(Parameters);
	UFXSystemAsset* VFX = RLGameplayCueUtils::GetSkillVFX(Skill, VFXSource);
	if (!VFX || !MyTarget)
	{
		return false;
	}

	RLGameplayCueUtils::SpawnVFX(MyTarget, VFX, Parameters.Location, Skill->bAttachToWeapon, Skill->WeaponSocketName);
	return true;
}

// =====================================================================
// 지속 (GameplayCue.Skill.Charge / GameplayCue.Skill.Buff)
// =====================================================================
ARLGameplayCueNotify_SkillLoop::ARLGameplayCueNotify_SkillLoop()
{
	// 제거되면 큐 액터는 바로 반납 (이펙트 컴포넌트는 대상 메시에 붙어 있으므로 OnRemove에서 직접 끔)
	bAutoDestroyOnRemove = true;
	AutoDestroyDelay = 0.0f;
}

bool ARLGameplayCueNotify_SkillLoop::WhileActive_Implementation(AActor* MyTarget, const FGameplayCueParameters& Parameters)
{
	if (ActiveVFX)
	{
		return true;
	}

	const URLSkillData* Skill = RLGameplayCueUtils::GetSkillData(Parameters);
	UFXSystemAsset* VFX = RLGameplayCueUtils::GetSkillVFX(Skill, VFXSource);
	if (!VFX || !MyTarget)
	{
		return false;
	}

	const bool bAttach = bForceAttachToMesh || Skill->bAttachToWeapon;
	const FName SocketName = bForceAttachToMesh ? NAME_None : Skill->WeaponSocketName;
	ActiveVFX = RLGameplayCueUtils::SpawnVFX(MyTarget, VFX, Parameters.Location, bAttach, SocketName);
	return ActiveVFX != nullptr;
}

bool ARLGameplayCueNotify_SkillLoop::OnRemove_Implementation(AActor* MyTarget, const FGameplayCueParameters& Parameters)
{
	if (ActiveVFX)
	{
		if (bStopImmediatelyOnRemove)
		{
			ActiveVFX->DestroyComponent();
		}
		else
		{
			ActiveVFX->Deactivate();
		}
		ActiveVFX = nullptr;
	}
	return true;
}
