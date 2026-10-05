#include "RLArenaMoodController.h"
#include "Components/LightComponent.h"
#include "Components/SkyLightComponent.h"
#include "Components/ExponentialHeightFogComponent.h"
#include "Components/MeshComponent.h"
#include "Engine/PostProcessVolume.h"
#include "Engine/ExponentialHeightFog.h"
#include "Materials/MaterialInstanceDynamic.h"
#include "NiagaraComponent.h"
#include "Components/InputComponent.h"
#include "GameFramework/PlayerController.h"
#include "EngineUtils.h"

ARLArenaMoodController::ARLArenaMoodController()
{
	PrimaryActorTick.bCanEverTick = true;
	PrimaryActorTick.bStartWithTickEnabled = false; // 전환 중에만 틱
}

void ARLArenaMoodController::BeginPlay()
{
	Super::BeginPlay();

	GatherTargets();
	ApplyAlpha(0.f);
	SetFXActive(false);
	BindDebugKey();
}

bool ARLArenaMoodController::IsInMyLevel(const AActor* Actor) const
{
	return IsValid(Actor) && Actor->GetLevel() == GetLevel();
}

void ARLArenaMoodController::GatherTargets()
{
	UWorld* World = GetWorld();
	if (!World)
	{
		return;
	}

	for (TActorIterator<AActor> It(World); It; ++It)
	{
		AActor* Actor = *It;
		if (!IsInMyLevel(Actor) || Actor == this)
		{
			continue;
		}

		// 1. 라이트 그룹
		for (const FRLMoodLightGroup& Group : LightGroups)
		{
			if (Group.ActorTag.IsNone() || !Actor->ActorHasTag(Group.ActorTag))
			{
				continue;
			}

			TInlineComponentArray<ULightComponentBase*> Lights(Actor);
			for (ULightComponentBase* Light : Lights)
			{
				FLightEntry& Entry = LightEntries.AddDefaulted_GetRef();
				Entry.Light = Light;
				Entry.CalmColor = Light->GetLightColor();
				Entry.CalmIntensity = Light->Intensity;
				Entry.BossColor = Group.BossColor;
				Entry.BossIntensity = Light->Intensity * Group.BossIntensityScale;
			}
		}

		// 2. 머티리얼 파라미터 (슬롯별 MID 생성)
		for (const FRLMoodMaterialParam& Param : MaterialParams)
		{
			if (Param.ActorTag.IsNone() || Param.ParameterName.IsNone() || !Actor->ActorHasTag(Param.ActorTag))
			{
				continue;
			}

			TInlineComponentArray<UMeshComponent*> Meshes(Actor);
			for (UMeshComponent* Mesh : Meshes)
			{
				for (int32 Slot = 0; Slot < Mesh->GetNumMaterials(); ++Slot)
				{
					UMaterialInterface* Material = Mesh->GetMaterial(Slot);
					FLinearColor CalmValue;
					if (!Material || !Material->GetVectorParameterValue(FHashedMaterialParameterInfo(Param.ParameterName), CalmValue))
					{
						continue;
					}

					UMaterialInstanceDynamic* MID = Mesh->CreateAndSetMaterialInstanceDynamic(Slot);
					if (MID)
					{
						FMaterialEntry& Entry = MaterialEntries.AddDefaulted_GetRef();
						Entry.MID = MID;
						Entry.ParameterName = Param.ParameterName;
						Entry.CalmValue = CalmValue;
						Entry.BossValue = Param.BossValue;
					}
				}
			}
		}

		// 3. 보스 전용 포스트프로세스 볼륨
		if (APostProcessVolume* PPV = Cast<APostProcessVolume>(Actor))
		{
			if (!BossPostProcessTag.IsNone() && PPV->ActorHasTag(BossPostProcessTag))
			{
				BossPostProcessVolumes.Add(PPV);
			}
		}

		// 4. FX
		const bool bCalmFX = !CalmFXTag.IsNone() && Actor->ActorHasTag(CalmFXTag);
		const bool bBossFX = !BossFXTag.IsNone() && Actor->ActorHasTag(BossFXTag);
		if (bCalmFX || bBossFX)
		{
			TInlineComponentArray<UNiagaraComponent*> FXComps(Actor);
			for (UNiagaraComponent* FX : FXComps)
			{
				(bBossFX ? BossFX : CalmFX).Add(FX);
			}
		}

		// 5. 포그 (레벨의 첫 번째 ExponentialHeightFog)
		if (bAffectFog && !Fog.IsValid())
		{
			if (AExponentialHeightFog* FogActor = Cast<AExponentialHeightFog>(Actor))
			{
				Fog = FogActor->GetComponent();
				if (Fog.IsValid())
				{
					CalmFogInscatteringColor = Fog->FogInscatteringLuminance;
					CalmVolumetricFogAlbedo = FLinearColor(Fog->VolumetricFogAlbedo);
				}
			}
		}
	}
}

void ARLArenaMoodController::ApplyAlpha(float Alpha)
{
	CurrentAlpha = Alpha;

	for (const FLightEntry& Entry : LightEntries)
	{
		ULightComponentBase* Light = Entry.Light.Get();
		if (!Light)
		{
			continue;
		}

		const FLinearColor Color = FLinearColor::LerpUsingHSV(Entry.CalmColor, Entry.BossColor, Alpha);
		const float Intensity = FMath::Lerp(Entry.CalmIntensity, Entry.BossIntensity, Alpha);

		if (ULightComponent* LocalLight = Cast<ULightComponent>(Light))
		{
			LocalLight->SetLightColor(Color);
			LocalLight->SetIntensity(Intensity);
		}
		else if (USkyLightComponent* SkyLight = Cast<USkyLightComponent>(Light))
		{
			SkyLight->SetLightColor(Color);
			SkyLight->SetIntensity(Intensity);
		}
	}

	for (const FMaterialEntry& Entry : MaterialEntries)
	{
		if (UMaterialInstanceDynamic* MID = Entry.MID.Get())
		{
			MID->SetVectorParameterValue(Entry.ParameterName, FMath::Lerp(Entry.CalmValue, Entry.BossValue, Alpha));
		}
	}

	for (const TWeakObjectPtr<APostProcessVolume>& PPV : BossPostProcessVolumes)
	{
		if (PPV.IsValid())
		{
			PPV->BlendWeight = Alpha;
		}
	}

	if (UExponentialHeightFogComponent* FogComp = Fog.Get())
	{
		FogComp->SetFogInscatteringColor(FMath::Lerp(CalmFogInscatteringColor, BossFogInscatteringColor, Alpha));
		FogComp->SetVolumetricFogAlbedo(FMath::Lerp(CalmVolumetricFogAlbedo, BossVolumetricFogAlbedo, Alpha).ToFColor(true));
	}
}

void ARLArenaMoodController::SetFXActive(bool bBoss)
{
	for (const TWeakObjectPtr<UNiagaraComponent>& FX : CalmFX)
	{
		if (FX.IsValid())
		{
			bBoss ? FX->Deactivate() : FX->Activate(true);
		}
	}
	for (const TWeakObjectPtr<UNiagaraComponent>& FX : BossFX)
	{
		if (FX.IsValid())
		{
			bBoss ? FX->Activate(true) : FX->Deactivate();
		}
	}
}

void ARLArenaMoodController::SetBossPhase(bool bBoss, bool bInstant)
{
	if (bBossPhase == bBoss && !bInstant)
	{
		return;
	}

	bBossPhase = bBoss;
	SetFXActive(bBoss);
	OnBossPhaseChanged(bBoss);

	if (bInstant || TransitionDuration <= KINDA_SMALL_NUMBER)
	{
		ApplyAlpha(bBoss ? 1.f : 0.f);
		SetActorTickEnabled(false);
	}
	else
	{
		SetActorTickEnabled(true);
	}
}

void ARLArenaMoodController::ToggleBossPhase()
{
	SetBossPhase(!bBossPhase);
}

void ARLArenaMoodController::Tick(float DeltaSeconds)
{
	Super::Tick(DeltaSeconds);

	const float Target = bBossPhase ? 1.f : 0.f;
	const float Step = DeltaSeconds / FMath::Max(TransitionDuration, KINDA_SMALL_NUMBER);
	const float NewAlpha = FMath::FInterpConstantTo(CurrentAlpha, Target, 1.f, Step);
	ApplyAlpha(NewAlpha);

	if (FMath::IsNearlyEqual(NewAlpha, Target))
	{
		SetActorTickEnabled(false);
	}
}

void ARLArenaMoodController::BindDebugKey()
{
#if !UE_BUILD_SHIPPING
	// ★ 테스트용: 보스가 아직 없으므로 키로 페이즈 전환 확인
	if (!bEnableDebugToggleKey || !DebugToggleKey.IsValid())
	{
		return;
	}

	APlayerController* PC = GetWorld() ? GetWorld()->GetFirstPlayerController() : nullptr;
	if (!PC)
	{
		return;
	}

	EnableInput(PC);
	if (InputComponent)
	{
		InputComponent->BindKey(DebugToggleKey, IE_Pressed, this, &ARLArenaMoodController::ToggleBossPhase);
	}
#endif
}
