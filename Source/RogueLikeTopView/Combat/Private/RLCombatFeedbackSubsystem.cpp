#include "RLCombatFeedbackSubsystem.h"
#include "RLAudioSubsystem.h"
#include "RLCharacter.h"
#include "RLHealthComponent.h"
#include "RLSkillData.h"
#include "RLGameplayCueNotifies.h"
#include "Sound/SoundBase.h"
#include "Particles/ParticleSystem.h"
#include "NiagaraSystem.h"
#include "NiagaraFunctionLibrary.h"
#include "Materials/MaterialInterface.h"
#include "Components/SkeletalMeshComponent.h"
#include "Components/CapsuleComponent.h"
#include "Components/PointLightComponent.h"
#include "GameFramework/SpringArmComponent.h"
#include "GameFramework/PlayerController.h"
#include "Materials/MaterialInstanceDynamic.h"
#include "Kismet/GameplayStatics.h"
#include "EngineUtils.h"
#include "Engine/World.h"
#include "HAL/PlatformTime.h"
#include "HAL/IConsoleManager.h"

namespace
{
	static FAutoConsoleCommandWithWorldAndArgs GTestHitFeelCmd(
		TEXT("RL.TestHitFeel"),
		TEXT("타격감(카메라 펀치/쉐이크 + 캐릭터 탄성/플래시/이펙트) 즉시 테스트 명령어"),
		FConsoleCommandWithWorldAndArgsDelegate::CreateLambda([](const TArray<FString>& Args, UWorld* World)
		{
			if (!World)
			{
				return;
			}
			URLCombatFeedbackSubsystem* Sub = URLCombatFeedbackSubsystem::Get(World);
			if (!Sub)
			{
				return;
			}

			Sub->TriggerCameraFeedback(16.0f, 0.14f, 24.0f);
			if (APlayerController* PC = World->GetFirstPlayerController())
			{
				if (ARLCharacter* PlayerChar = Cast<ARLCharacter>(PC->GetPawn()))
				{
					Sub->NotifyCharacterDamaged(PlayerChar, 50.0f, PlayerChar);
				}
			}
			if (GEngine)
			{
				GEngine->AddOnScreenDebugMessage(-1, 3.0f, FColor::Cyan,
					TEXT("💥 [RL.TestHitFeel] 타격감 피드백(카메라 줌 펀치/쉐이크 + 메시 탄성/플래시/스파크) 테스트 실행!"));
			}
		})
	);

	static FAutoConsoleCommandWithWorldAndArgs GHitFeelIntensityCmd(
		TEXT("RL.HitFeelIntensity"),
		TEXT("전체 타격감 피드백 배율 설정 (예: RL.HitFeelIntensity 0.5)"),
		FConsoleCommandWithWorldAndArgsDelegate::CreateLambda([](const TArray<FString>& Args, UWorld* World)
		{
			if (!World || Args.Num() == 0)
			{
				return;
			}
			if (URLCombatFeedbackSubsystem* Sub = URLCombatFeedbackSubsystem::Get(World))
			{
				Sub->GlobalFeedbackIntensity = FMath::Clamp(FCString::Atof(*Args[0]), 0.0f, 5.0f);
				if (GEngine)
				{
					GEngine->AddOnScreenDebugMessage(-1, 3.0f, FColor::Green,
						FString::Printf(TEXT("⚡ [RL.HitFeelIntensity] 타격감 강도 배율: %.2f"), Sub->GlobalFeedbackIntensity));
				}
			}
		})
	);

	static FAutoConsoleCommandWithWorldAndArgs GAutoSkillVFXCmd(
		TEXT("RL.AutoSkillVFX"),
		TEXT("스킬 시전/속성 타격 이펙트 자동 재생 ON/OFF (예: RL.AutoSkillVFX 0 또는 1)"),
		FConsoleCommandWithWorldAndArgsDelegate::CreateLambda([](const TArray<FString>& Args, UWorld* World)
		{
			if (!World || Args.Num() == 0)
			{
				return;
			}
			if (URLCombatFeedbackSubsystem* Sub = URLCombatFeedbackSubsystem::Get(World))
			{
				Sub->bEnableAutoSkillVFX = FCString::Atoi(*Args[0]) != 0;
				if (GEngine)
				{
					GEngine->AddOnScreenDebugMessage(-1, 3.0f, FColor::Yellow,
						FString::Printf(TEXT("✨ [RL.AutoSkillVFX] 자동 스킬/타격 이펙트: %s"),
							Sub->bEnableAutoSkillVFX ? TEXT("ON (활성화)") : TEXT("OFF (기본 스파크만)")));
				}
			}
		})
	);
}

void URLCombatFeedbackListener::HandleTakeAnyDamage(AActor* DamagedActor, float Damage, const UDamageType* DamageType, AController* InstigatedBy, AActor* DamageCauser)
{
	if (OwnerSubsystem.IsValid() && WatchedCharacter.IsValid() && Damage > 0.0f)
	{
		AActor* EffectiveCauser = DamageCauser ? DamageCauser : (InstigatedBy ? InstigatedBy->GetPawn() : nullptr);
		OwnerSubsystem->NotifyCharacterDamaged(WatchedCharacter.Get(), Damage, EffectiveCauser);
	}
}

void URLCombatFeedbackListener::HandleCharacterDeath()
{
	if (OwnerSubsystem.IsValid() && WatchedCharacter.IsValid())
	{
		OwnerSubsystem->NotifyCharacterDied(WatchedCharacter.Get());
	}
}

URLCombatFeedbackSubsystem* URLCombatFeedbackSubsystem::Get(const UObject* WorldContextObject)
{
	if (!WorldContextObject)
	{
		return nullptr;
	}
	UWorld* World = WorldContextObject->GetWorld();
	return World ? World->GetSubsystem<URLCombatFeedbackSubsystem>() : nullptr;
}

bool URLCombatFeedbackSubsystem::DoesSupportWorldType(const EWorldType::Type WorldType) const
{
	return WorldType == EWorldType::Game || WorldType == EWorldType::PIE;
}

void URLCombatFeedbackSubsystem::EnsureDefaultAssetsLoaded()
{
	if (bTriedLoadingDefaultAssets)
	{
		return;
	}
	bTriedLoadingDefaultAssets = true;

	// 1. 기본 물리/참격 타격 이펙트
	DefaultNormalHitVFX = LoadObject<UFXSystemAsset>(nullptr,
		TEXT("/Game/ParagonKhaimera/FX/ParticleSystems/Abilities/Primary/FX/P_Khaimera_LMB_Impact.P_Khaimera_LMB_Impact"));
	if (!DefaultNormalHitVFX)
	{
		DefaultNormalHitVFX = LoadObject<UFXSystemAsset>(nullptr,
			TEXT("/Game/ParagonSevarog/FX/Particles/Abilities/Primary/FX/P_Sevarog_Melee_SucessfulImpact.P_Sevarog_Melee_SucessfulImpact"));
	}

	DefaultCritHitVFX = LoadObject<UFXSystemAsset>(nullptr,
		TEXT("/Game/ParagonKhaimera/FX/ParticleSystems/Abilities/Ultimate/FX/P_Ult_Impact_DMG.P_Ult_Impact_DMG"));
	if (!DefaultCritHitVFX)
	{
		DefaultCritHitVFX = DefaultNormalHitVFX;
	}

	// 2. 속성/공격별 고유 타격 이펙트 (Content/Fab 내장 팩 활용)
	FireHitVFX = LoadObject<UFXSystemAsset>(nullptr,
		TEXT("/Game/Fab/FXVarietyPack/Particles/P_ky_hit2.P_ky_hit2"));
	FireCritExplosionVFX = LoadObject<UFXSystemAsset>(nullptr,
		TEXT("/Game/Fab/FXVarietyPack/Particles/P_ky_explosion.P_ky_explosion"));
	if (!FireCritExplosionVFX)
	{
		FireCritExplosionVFX = LoadObject<UFXSystemAsset>(nullptr,
			TEXT("/Game/Fab/Free_Spells/VFX_Niagara/NS_Free_Spells_Explosion.NS_Free_Spells_Explosion"));
	}

	LightningHitVFX = LoadObject<UFXSystemAsset>(nullptr,
		TEXT("/Game/Fab/FXVarietyPack/Particles/P_ky_ThunderBallHit.P_ky_ThunderBallHit"));
	if (!LightningHitVFX)
	{
		LightningHitVFX = LoadObject<UFXSystemAsset>(nullptr,
			TEXT("/Game/Effect/Particle/P_Lightning_Small.P_Lightning_Small"));
	}

	ArcaneNormalHitVFX = LoadObject<UFXSystemAsset>(nullptr,
		TEXT("/Game/Fab/Free_Magic/VFX_Niagara/NS_Free_Magic_Hit1.NS_Free_Magic_Hit1"));
	ArcaneCritHitVFX = LoadObject<UFXSystemAsset>(nullptr,
		TEXT("/Game/Fab/Free_Magic/VFX_Niagara/NS_Free_Magic_Hit2.NS_Free_Magic_Hit2"));

	HeavyBluntHitVFX = LoadObject<UFXSystemAsset>(nullptr,
		TEXT("/Game/ParagonSevarog/FX/Particles/Abilities/Primary/FX/P_Sevarog_Melee_SucessfulImpact.P_Sevarog_Melee_SucessfulImpact"));
	HeavyShockwaveVFX = LoadObject<UFXSystemAsset>(nullptr,
		TEXT("/Game/Fab/FXVarietyPack/Particles/P_ky_shotShockwave.P_ky_shotShockwave"));
	if (!HeavyShockwaveVFX)
	{
		HeavyShockwaveVFX = LoadObject<UFXSystemAsset>(nullptr,
			TEXT("/Game/Fab/Free_Spells/VFX_Niagara/NS_Free_Spells_Shockwave.NS_Free_Spells_Shockwave"));
	}

	// 3. 스킬 시전/검기/대시 이펙트
	BladeSlashRedVFX = LoadObject<UFXSystemAsset>(nullptr,
		TEXT("/Game/Fab/Free_Magic/VFX_Niagara/NS_Blade_Q_Slash_Red.NS_Blade_Q_Slash_Red"));
	MagicSlash1VFX = LoadObject<UFXSystemAsset>(nullptr,
		TEXT("/Game/Fab/Free_Magic/VFX_Niagara/NS_Free_Magic_Slash.NS_Free_Magic_Slash"));
	MagicSlash2VFX = LoadObject<UFXSystemAsset>(nullptr,
		TEXT("/Game/Fab/Free_Magic/VFX_Niagara/NS_Free_Magic_Slash2.NS_Free_Magic_Slash2"));

	DashTrailVFX = LoadObject<UFXSystemAsset>(nullptr,
		TEXT("/Game/ParagonSevarog/FX/Particles/Abilities/Relentless/FX/P_SevarogTrailsDash.P_SevarogTrailsDash"));
	DashDustVFX = LoadObject<UFXSystemAsset>(nullptr,
		TEXT("/Game/ParagonSevarog/FX/Particles/Abilities/Relentless/FX/P_Sevarog_Relentless_Dustkick.P_Sevarog_Relentless_Dustkick"));

	CastCircleVFX = LoadObject<UFXSystemAsset>(nullptr,
		TEXT("/Game/Fab/Free_Magic/VFX_Niagara/NS_Free_Magic_Circle1.NS_Free_Magic_Circle1"));
	LightningAuraVFX = LoadObject<UFXSystemAsset>(nullptr,
		TEXT("/Game/Fab/Free_Spells/VFX_Niagara/NS_Free_Spells_Aura_Lightning.NS_Free_Spells_Aura_Lightning"));
	FireAuraVFX = LoadObject<UFXSystemAsset>(nullptr,
		TEXT("/Game/Fab/Free_Spells/VFX_Niagara/NS_Free_Spells_Aura_Fire.NS_Free_Spells_Aura_Fire"));

	// 4. 피격 오버레이 머티리얼
	DefaultHitOverlayMaterial = LoadObject<UMaterialInterface>(nullptr,
		TEXT("/Game/Effect/Material/M_DropGlow.M_DropGlow"));

	// 5. RPG_Fantasy_SFX 타격/시전 효과음 풀 로드
	auto LoadSFXGroup = [](const TCHAR* Folder, const TCHAR* Prefix, TArray<TObjectPtr<USoundBase>>& OutPool)
	{
		for (int32 i = 1; i <= 4; ++i)
		{
			const FString AssetName = FString::Printf(TEXT("%s__%d_"), Prefix, i);
			const FString FullPath = FString::Printf(TEXT("/Game/RPG_Fantasy_SFX/%s/%s.%s"), Folder, *AssetName, *AssetName);
			if (USoundBase* LoadedSound = LoadObject<USoundBase>(nullptr, *FullPath))
			{
				OutPool.Add(LoadedSound);
			}
		}
	};

	// ① 블레이드 / 참격 타격음 (Sword + Daggers)
	LoadSFXGroup(TEXT("Sword"), TEXT("12"), BladeHitSFXPool);
	LoadSFXGroup(TEXT("Daggers"), TEXT("6"), BladeHitSFXPool);

	// ② 디스트로이어 / 보스 둔기 강타 타격음 (Axes + Combat + Weapon)
	LoadSFXGroup(TEXT("Axes"), TEXT("11"), HeavyBluntHitSFXPool);
	LoadSFXGroup(TEXT("Combat"), TEXT("5"), HeavyBluntHitSFXPool);
	LoadSFXGroup(TEXT("Weapon"), TEXT("4"), HeavyBluntHitSFXPool);

	// ③ 화염 마법 폭발/타격음 (Elemental + Spell)
	LoadSFXGroup(TEXT("Elemental"), TEXT("2"), FireHitSFXPool);
	LoadSFXGroup(TEXT("Spell"), TEXT("8"), FireHitSFXPool);

	// ④ 번개 마법 감전/타격음 (Magical + Enchantment)
	LoadSFXGroup(TEXT("Magical"), TEXT("3"), LightningHitSFXPool);
	LoadSFXGroup(TEXT("Enchantment"), TEXT("1"), LightningHitSFXPool);

	// ⑤ 소서리스 평타 / 비전 마법 / 원거리 차크라 타격음 (Magical + Dark + Arrows)
	LoadSFXGroup(TEXT("Magical"), TEXT("3"), ArcaneHitSFXPool);
	LoadSFXGroup(TEXT("Dark"), TEXT("1"), ArcaneHitSFXPool);
	LoadSFXGroup(TEXT("Arrows"), TEXT("9"), ArcaneHitSFXPool);

	// ⑥ 플레이어 피격 방어구/몸통 타격음 (Shields + Combat)
	LoadSFXGroup(TEXT("Shields"), TEXT("10"), ArmorPlayerHitSFXPool);
	LoadSFXGroup(TEXT("Combat"), TEXT("5"), ArmorPlayerHitSFXPool);

	// ⑦ 무기 휘두르기 / 마법 시전 / 치명타 추가 레이어 / 처치 마무리음
	LoadSFXGroup(TEXT("Swing"), TEXT("1"), WeaponSwingSFXPool);
	LoadSFXGroup(TEXT("Spell"), TEXT("8"), SpellCastSFXPool);
	LoadSFXGroup(TEXT("Enchantment"), TEXT("1"), SpellCastSFXPool);
	LoadSFXGroup(TEXT("Weapon"), TEXT("4"), CritBonusSFXPool);
	LoadSFXGroup(TEXT("Shields"), TEXT("10"), CritBonusSFXPool);
	LoadSFXGroup(TEXT("Dark"), TEXT("1"), EnemyDeathSFXPool);
}

void URLCombatFeedbackSubsystem::SpawnFXAtTransform(UFXSystemAsset* FXAsset, const FVector& Location, const FRotator& Rotation, const FVector& Scale)
{
	UWorld* World = GetWorld();
	if (!World || !FXAsset)
	{
		return;
	}

	if (UNiagaraSystem* NiagaraSys = Cast<UNiagaraSystem>(FXAsset))
	{
		UNiagaraFunctionLibrary::SpawnSystemAtLocation(World, NiagaraSys, Location, Rotation, Scale, true, true);
	}
	else if (UParticleSystem* CascadeSys = Cast<UParticleSystem>(FXAsset))
	{
		UGameplayStatics::SpawnEmitterAtLocation(World, CascadeSys, FTransform(Rotation, Location, Scale), true);
	}
}

UFXSystemAsset* URLCombatFeedbackSubsystem::ResolveHitVFXAndColor(const URLSkillData* SkillData, bool bIsCritical, FLinearColor& OutLightColor) const
{
	OutLightColor = bIsCritical ? FLinearColor(1.0f, 0.55f, 0.1f) : FLinearColor(1.0f, 0.90f, 0.75f);

	if (SkillData && SkillData->HitVFX)
	{
		return SkillData->HitVFX.Get();
	}

	if (!bEnableAutoSkillVFX || !SkillData)
	{
		return bIsCritical ? DefaultCritHitVFX.Get() : DefaultNormalHitVFX.Get();
	}

	const FString AssetName = SkillData->GetName().ToLower();
	const FString SkillId = SkillData->SkillName.ToString().ToLower();
	const FString Combined = AssetName + TEXT(" ") + SkillId;

	// 1. 화염 스킬 (Fireball, Inferno, Explosion/Meteor)
	if (Combined.Contains(TEXT("fire")) || Combined.Contains(TEXT("inferno")) || Combined.Contains(TEXT("explosion")) || Combined.Contains(TEXT("meteor")))
	{
		OutLightColor = FLinearColor(1.0f, 0.32f, 0.05f);
		if (bIsCritical || Combined.Contains(TEXT("explosion")) || Combined.Contains(TEXT("meteor")))
		{
			return FireCritExplosionVFX ? FireCritExplosionVFX.Get() : (FireHitVFX ? FireHitVFX.Get() : DefaultCritHitVFX.Get());
		}
		return FireHitVFX ? FireHitVFX.Get() : DefaultNormalHitVFX.Get();
	}

	// 2. 번개/뇌전 스킬 (Lightning)
	if (Combined.Contains(TEXT("lightning")) || Combined.Contains(TEXT("thunder")) || Combined.Contains(TEXT("shock")))
	{
		OutLightColor = FLinearColor(0.20f, 0.78f, 1.0f);
		return LightningHitVFX ? LightningHitVFX.Get() : DefaultNormalHitVFX.Get();
	}

	// 3. 소서리스 평타 / 비전 마법 / 원거리 검기(차크라샷)
	if (Combined.Contains(TEXT("sorceress")) || Combined.Contains(TEXT("chakra")) || SkillData->APScaling > SkillData->ADScaling)
	{
		OutLightColor = FLinearColor(0.70f, 0.35f, 1.0f);
		if (bIsCritical && ArcaneCritHitVFX)
		{
			return ArcaneCritHitVFX.Get();
		}
		return ArcaneNormalHitVFX ? ArcaneNormalHitVFX.Get() : DefaultNormalHitVFX.Get();
	}

	// 4. 블레이드 / 참격 계열 (Blade)
	if (Combined.Contains(TEXT("blade")) || Combined.Contains(TEXT("slash")))
	{
		OutLightColor = FLinearColor(1.0f, 0.22f, 0.18f);
		if (bIsCritical && ArcaneCritHitVFX)
		{
			return ArcaneCritHitVFX.Get();
		}
		return DefaultNormalHitVFX.Get();
	}

	// 5. 디스트로이어 / 중전사 강타 계열 (Destroyer)
	if (Combined.Contains(TEXT("destroyer")) || SkillData->CastType == ESkillCastType::Charge || SkillData->CastType == ESkillCastType::ChargeDash)
	{
		OutLightColor = bIsCritical ? FLinearColor(1.0f, 0.65f, 0.15f) : FLinearColor(0.45f, 1.0f, 0.60f);
		if (bIsCritical && HeavyShockwaveVFX)
		{
			return HeavyShockwaveVFX.Get();
		}
		return HeavyBluntHitVFX ? HeavyBluntHitVFX.Get() : DefaultNormalHitVFX.Get();
	}

	return bIsCritical ? DefaultCritHitVFX.Get() : DefaultNormalHitVFX.Get();
}

void URLCombatFeedbackSubsystem::PlayRandomSFXFromPool(const TArray<TObjectPtr<USoundBase>>& Pool, const FVector& Location, float VolumeScale, float PitchMin, float PitchMax)
{
	if (Pool.Num() == 0)
	{
		return;
	}

	USoundBase* Chosen = Pool[FMath::RandHelper(Pool.Num())].Get();
	if (!Chosen)
	{
		return;
	}

	if (URLAudioSubsystem* AudioSys = URLAudioSubsystem::Get(this))
	{
		AudioSys->PlaySFXAtLocation(Chosen, Location, VolumeScale, PitchMin, PitchMax);
	}
	else if (UWorld* World = GetWorld())
	{
		UGameplayStatics::PlaySoundAtLocation(World, Chosen, Location, VolumeScale, FMath::FRandRange(PitchMin, PitchMax));
	}
}

void URLCombatFeedbackSubsystem::PlayAutoSkillCastSFX(ARLCharacter* Caster, const URLSkillData* SkillData, const FVector& TargetLocation)
{
	if (!Caster || !SkillData)
	{
		return;
	}

	// 스킬 데이터에 이미 CastSound가 지정되어 있으면 중복 재생하지 않음
	if (SkillData->CastSound)
	{
		return;
	}

	const double NowReal = FPlatformTime::Seconds();
	const double MinCooldown = (SkillData->CastType == ESkillCastType::Hold) ? 0.16 : 0.06;
	if (NowReal - LastCastSFXRealTime < MinCooldown)
	{
		return;
	}
	LastCastSFXRealTime = NowReal;

	const FVector CasterLoc = Caster->GetActorLocation();
	const FString Combined = (SkillData->GetName() + TEXT(" ") + SkillData->SkillName.ToString()).ToLower();

	// 1. 지면 타겟 화염/폭발 스킬 (메테오/익스플로전 등)
	if (SkillData->CastType == ESkillCastType::GroundTarget)
	{
		const FVector EffectLoc = TargetLocation.IsNearlyZero() ? CasterLoc : TargetLocation;
		PlayRandomSFXFromPool(FireHitSFXPool, EffectLoc, 0.14f, 0.86f, 0.96f);
		return;
	}

	// 2. 화염/번개/비전 마법 시전음
	if (Combined.Contains(TEXT("fire")) || Combined.Contains(TEXT("inferno")) || Combined.Contains(TEXT("meteor")))
	{
		PlayRandomSFXFromPool(FireHitSFXPool, CasterLoc, 0.11f, 0.94f, 1.05f);
		return;
	}
	if (Combined.Contains(TEXT("lightning")) || Combined.Contains(TEXT("thunder")))
	{
		PlayRandomSFXFromPool(LightningHitSFXPool, CasterLoc, 0.09f, 1.05f, 1.18f);
		return;
	}
	if (Combined.Contains(TEXT("sorceress")) || SkillData->APScaling > SkillData->ADScaling)
	{
		PlayRandomSFXFromPool(SpellCastSFXPool, CasterLoc, 0.10f, 0.96f, 1.08f);
		return;
	}

	// 3. 디스트로이어 묵직한 스윙 / 차지 해방음
	if (Combined.Contains(TEXT("destroyer")) || SkillData->CastType == ESkillCastType::Charge)
	{
		PlayRandomSFXFromPool(WeaponSwingSFXPool, CasterLoc, 0.11f, 0.80f, 0.92f);
		return;
	}

	// 4. 블레이드 및 기본 근접 무기 휘두르기 사운드
	PlayRandomSFXFromPool(WeaponSwingSFXPool, CasterLoc, 0.10f, 0.95f, 1.08f);
}

void URLCombatFeedbackSubsystem::PlayAutoHitImpactSFX(ARLCharacter* Victim, ARLCharacter* Attacker, const URLSkillData* SkillData, bool bIsCritical, const FVector& ImpactLocation)
{
	if (!Victim)
	{
		return;
	}

	const double NowReal = FPlatformTime::Seconds();
	const bool bIsHoldSkill = SkillData && SkillData->CastType == ESkillCastType::Hold;
	const double MinHitInterval = bIsHoldSkill ? 0.11 : 0.075;

	// 광역기/다단히트로 여러 몬스터를 같은 프레임에 동시에 타격할 때 사운드가 중첩되어 귀가 아픈 현상 방지
	if (NowReal - LastHitSFXRealTime < MinHitInterval)
	{
		return;
	}
	LastHitSFXRealTime = NowReal;

	// 1. 플레이어가 적에게 피격당했을 때: 절제된 방어구/몸통 충격음
	if (Victim->TeamID == ERLTeam::Player)
	{
		PlayRandomSFXFromPool(ArmorPlayerHitSFXPool, ImpactLocation, 0.16f, 0.88f, 0.96f);
		return;
	}

	const FString SkillCombined = SkillData
		? (SkillData->GetName() + TEXT(" ") + SkillData->SkillName.ToString()).ToLower()
		: TEXT("");
	const FString AttackerName = Attacker
		? (Attacker->GetClass()->GetName() + TEXT(" ") + Attacker->GetName()).ToLower()
		: TEXT("");
	const FString Combined = SkillCombined + TEXT(" ") + AttackerName;

	// 2. 화염 계열 적중 (Fireball, Inferno, Explosion, Meteor)
	if (Combined.Contains(TEXT("fire")) || Combined.Contains(TEXT("inferno")) || Combined.Contains(TEXT("explosion")) || Combined.Contains(TEXT("meteor")))
	{
		PlayRandomSFXFromPool(FireHitSFXPool, ImpactLocation, bIsCritical ? 0.17f : 0.13f, 0.88f, 1.02f);
	}
	// 3. 번개/뇌전 계열 적중 (Lightning)
	else if (Combined.Contains(TEXT("lightning")) || Combined.Contains(TEXT("thunder")) || Combined.Contains(TEXT("shock")))
	{
		PlayRandomSFXFromPool(LightningHitSFXPool, ImpactLocation, bIsCritical ? 0.13f : 0.10f, 1.04f, 1.18f);
	}
	// 4. 소서리스 평타 / 비전 마법 / 차크라 원거리 적중
	else if (Combined.Contains(TEXT("sorceress")) || Combined.Contains(TEXT("chakra")) || (SkillData && SkillData->APScaling > SkillData->ADScaling))
	{
		PlayRandomSFXFromPool(ArcaneHitSFXPool, ImpactLocation, bIsCritical ? 0.15f : 0.12f, 0.95f, 1.08f);
	}
	// 5. 디스트로이어 / 둔기 강타 계열 적중
	else if (Combined.Contains(TEXT("destroyer")) || (SkillData && (SkillData->CastType == ESkillCastType::Charge || SkillData->CastType == ESkillCastType::ChargeDash)))
	{
		PlayRandomSFXFromPool(HeavyBluntHitSFXPool, ImpactLocation, bIsCritical ? 0.18f : 0.14f, 0.84f, 0.96f);
	}
	// 6. 블레이드 / 기본 참격 적중
	else
	{
		PlayRandomSFXFromPool(BladeHitSFXPool, ImpactLocation, bIsCritical ? 0.16f : 0.13f, 0.94f, 1.08f);
	}
}

void URLCombatFeedbackSubsystem::NotifySkillVFX(ARLCharacter* Caster, const URLSkillData* SkillData, const FVector& TargetLocation, int32 ComboIndex)
{
	if (!Caster || !SkillData)
	{
		return;
	}

	EnsureDefaultAssetsLoaded();

	// 스킬 시전/휘두르기 효과음 자동 재생 (ReleaseVFX 유무와 무관하게 작동)
	PlayAutoSkillCastSFX(Caster, SkillData, TargetLocation);

	if (!bEnableAutoSkillVFX)
	{
		return;
	}

	// 스킬 데이터에 이미 ReleaseVFX가 지정되어 있으면 중복 스폰하지 않음
	if (SkillData->ReleaseVFX)
	{
		return;
	}

	const FVector CasterLoc = Caster->GetActorLocation();
	const FRotator CasterRot = Caster->GetActorRotation();
	const FVector Forward = Caster->GetActorForwardVector();
	const FString Combined = (SkillData->GetName() + TEXT(" ") + SkillData->SkillName.ToString()).ToLower();

	switch (SkillData->CastType)
	{
	case ESkillCastType::Instant:
	{
		// 근접 베기/휘두르기 검기 이펙트 (콤보 타수에 따라 검기 궤적 교차)
		const FVector SlashLoc = CasterLoc + Forward * FMath::Clamp(SkillData->HitboxForwardOffset * 0.65f, 70.0f, 140.0f) + FVector(0.0f, 0.0f, 35.0f);
		FRotator SlashRot = CasterRot;
		if (ComboIndex % 2 == 1)
		{
			SlashRot.Roll += 180.0f;
		}

		if (Combined.Contains(TEXT("blade")))
		{
			UFXSystemAsset* SlashFX = (ComboIndex % 2 == 0 && BladeSlashRedVFX) ? BladeSlashRedVFX.Get() : (MagicSlash1VFX ? MagicSlash1VFX.Get() : BladeSlashRedVFX.Get());
			SpawnFXAtTransform(SlashFX, SlashLoc, SlashRot, FVector(1.05f));
		}
		else if (Combined.Contains(TEXT("destroyer")))
		{
			UFXSystemAsset* HeavySlashFX = (ComboIndex % 2 == 0 && MagicSlash2VFX) ? MagicSlash2VFX.Get() : MagicSlash1VFX.Get();
			SpawnFXAtTransform(HeavySlashFX, SlashLoc, SlashRot, FVector(1.25f));
		}
		else
		{
			SpawnFXAtTransform(MagicSlash1VFX.Get(), SlashLoc, SlashRot, FVector(1.0f));
		}
		break;
	}
	case ESkillCastType::Dash:
	case ESkillCastType::ChargeDash:
	{
		// 돌진/대시 시 발밑 먼지 폭발 + 잔상 트레일 + 전방 참격
		if (DashDustVFX)
		{
			SpawnFXAtTransform(DashDustVFX.Get(), CasterLoc - FVector(0.0f, 0.0f, 40.0f), CasterRot, FVector(1.0f));
		}
		if (DashTrailVFX)
		{
			RLGameplayCueUtils::SpawnVFX(Caster, DashTrailVFX.Get(), CasterLoc, true, NAME_None);
		}
		if (Combined.Contains(TEXT("blade")) && BladeSlashRedVFX)
		{
			SpawnFXAtTransform(BladeSlashRedVFX.Get(), CasterLoc + Forward * 110.0f + FVector(0.0f, 0.0f, 35.0f), CasterRot, FVector(1.15f));
		}
		break;
	}
	case ESkillCastType::Charge:
	{
		// 차지 해방 순간 강력한 회전/충격파 검기
		const FVector ReleaseLoc = CasterLoc + Forward * 120.0f + FVector(0.0f, 0.0f, 35.0f);
		if (Combined.Contains(TEXT("blade")) && BladeSlashRedVFX)
		{
			SpawnFXAtTransform(BladeSlashRedVFX.Get(), ReleaseLoc, CasterRot, FVector(1.4f));
		}
		else if (MagicSlash2VFX)
		{
			SpawnFXAtTransform(MagicSlash2VFX.Get(), ReleaseLoc, CasterRot, FVector(1.4f));
		}
		if (HeavyShockwaveVFX)
		{
			SpawnFXAtTransform(HeavyShockwaveVFX.Get(), ReleaseLoc, CasterRot, FVector(0.9f));
		}
		break;
	}
	case ESkillCastType::GroundTarget:
	{
		// 지면 타겟 스킬 폭발 순간 (메테오/익스플로전 등)
		const FVector EffectLoc = TargetLocation.IsNearlyZero() ? CasterLoc : TargetLocation;
		if (Combined.Contains(TEXT("explosion")) || Combined.Contains(TEXT("meteor")) || Combined.Contains(TEXT("fire")))
		{
			if (FireCritExplosionVFX)
			{
				SpawnFXAtTransform(FireCritExplosionVFX.Get(), EffectLoc + FVector(0.0f, 0.0f, 20.0f), FRotator::ZeroRotator, FVector(1.35f));
			}
		}
		else if (HeavyShockwaveVFX)
		{
			SpawnFXAtTransform(HeavyShockwaveVFX.Get(), EffectLoc + FVector(0.0f, 0.0f, 20.0f), FRotator::ZeroRotator, FVector(1.2f));
		}
		break;
	}
	case ESkillCastType::Hold:
	{
		// 홀드/채널링 시전 중 전방 이펙트 (라이트닝 / 휠윈드 등)
		if (Combined.Contains(TEXT("lightning")) || Combined.Contains(TEXT("thunder")))
		{
			if (LightningHitVFX)
			{
				const FVector StrikeLoc = CasterLoc + Forward * FMath::Max(160.0f, SkillData->HitboxForwardOffset) + FVector(0.0f, 0.0f, 40.0f);
				SpawnFXAtTransform(LightningHitVFX.Get(), StrikeLoc, CasterRot, FVector(1.1f));
			}
		}
		else if (MagicSlash2VFX)
		{
			SpawnFXAtTransform(MagicSlash2VFX.Get(), CasterLoc + FVector(0.0f, 0.0f, 35.0f), CasterRot + FRotator(0.0f, ComboIndex * 90.0f, 0.0f), FVector(1.15f));
		}
		break;
	}
	case ESkillCastType::Cast:
	{
		// 캐스팅 완료 해방 순간 전방 충격파/참격
		const FVector BlastLoc = CasterLoc + Forward * FMath::Max(120.0f, SkillData->HitboxForwardOffset);
		if (HeavyShockwaveVFX)
		{
			SpawnFXAtTransform(HeavyShockwaveVFX.Get(), BlastLoc, CasterRot, FVector(1.25f));
		}
		break;
	}
	default:
		break;
	}
}

void URLCombatFeedbackSubsystem::OnWorldBeginPlay(UWorld& InWorld)
{
	Super::OnWorldBeginPlay(InWorld);

	EnsureDefaultAssetsLoaded();

	for (TActorIterator<ARLCharacter> It(&InWorld); It; ++It)
	{
		TryRegisterCharacter(*It);
	}

	if (!ActorSpawnedHandle.IsValid())
	{
		ActorSpawnedHandle = InWorld.AddOnActorSpawnedHandler(
			FOnActorSpawned::FDelegate::CreateUObject(this, &URLCombatFeedbackSubsystem::HandleActorSpawned));
	}
}

void URLCombatFeedbackSubsystem::Deinitialize()
{
	if (UWorld* World = GetWorld())
	{
		World->RemoveOnActorSpawnedHandler(ActorSpawnedHandle);
	}
	ActorSpawnedHandle.Reset();

	// 남아있는 역경직(CustomTimeDilation) 안전 복구
	for (const FRLActiveHitStop& Entry : ActiveHitStops)
	{
		if (AActor* Actor = Entry.TargetActor.Get())
		{
			Actor->CustomTimeDilation = 1.0f;
		}
	}
	ActiveHitStops.Empty();

	// 남아있는 메시 스케일 및 오버레이 머티리얼 복구
	for (const FRLActiveMeshImpact& Impact : ActiveMeshImpacts)
	{
		if (ACharacter* Char = Impact.Character.Get())
		{
			if (USkeletalMeshComponent* Mesh = Char->GetMesh())
			{
				Mesh->SetRelativeScale3D(Impact.BaseScale);
				if (Impact.bAppliedOverlay)
				{
					Mesh->SetOverlayMaterial(nullptr);
				}
			}
		}
	}
	ActiveMeshImpacts.Empty();

	for (FRLActiveImpactFlashLight& LightEntry : ActiveFlashLights)
	{
		if (UPointLightComponent* Light = LightEntry.LightComp.Get())
		{
			Light->DestroyComponent();
		}
	}
	ActiveFlashLights.Empty();

	Listeners.Empty();
	PendingContexts.Empty();
	LastProcessedHitFrameMap.Empty();
	CachedBaseMeshScales.Empty();

	Super::Deinitialize();
}

void URLCombatFeedbackSubsystem::HandleActorSpawned(AActor* SpawnedActor)
{
	TryRegisterCharacter(SpawnedActor);
}

void URLCombatFeedbackSubsystem::TryRegisterCharacter(AActor* Actor)
{
	ARLCharacter* Character = Cast<ARLCharacter>(Actor);
	if (!Character)
	{
		return;
	}

	for (const URLCombatFeedbackListener* Existing : Listeners)
	{
		if (Existing && Existing->WatchedCharacter.Get() == Character)
		{
			return;
		}
	}

	if (USkeletalMeshComponent* Mesh = Character->GetMesh())
	{
		CachedBaseMeshScales.FindOrAdd(Character, Mesh->GetRelativeScale3D());
	}

	URLCombatFeedbackListener* Listener = NewObject<URLCombatFeedbackListener>(this);
	Listener->WatchedCharacter = Character;
	Listener->OwnerSubsystem = this;

	Character->OnTakeAnyDamage.AddDynamic(Listener, &URLCombatFeedbackListener::HandleTakeAnyDamage);

	if (URLHealthComponent* HealthComp = Character->FindComponentByClass<URLHealthComponent>())
	{
		HealthComp->OnDeath.AddDynamic(Listener, &URLCombatFeedbackListener::HandleCharacterDeath);
	}

	Listeners.Add(Listener);
}

void URLCombatFeedbackSubsystem::PushDamageContext(AActor* Attacker, const FRLDamageData& DamageData, const URLSkillData* SkillData)
{
	if (!Attacker)
	{
		return;
	}

	FRLPendingDamageContext Context;
	Context.Attacker = Attacker;
	Context.SkillData = SkillData;
	Context.EvaluatedDamage = DamageData.BaseDamage;
	Context.HitStopDuration = DamageData.HitStopDuration > 0.0f ? DamageData.HitStopDuration : 0.055f;
	Context.bIsCritical = DamageData.bIsCritical;
	Context.Timestamp = FPlatformTime::Seconds();

	PendingContexts.Add(Attacker, Context);
}

void URLCombatFeedbackSubsystem::RegisterDamageSourceActor(AActor* SourceActor, AActor* Attacker, const FRLDamageData& DamageData, const URLSkillData* SkillData)
{
	if (!SourceActor)
	{
		return;
	}

	FRLPendingDamageContext Context;
	Context.Attacker = Attacker ? Attacker : SourceActor;
	Context.SkillData = SkillData;
	Context.EvaluatedDamage = DamageData.BaseDamage;
	Context.HitStopDuration = DamageData.HitStopDuration > 0.0f ? DamageData.HitStopDuration : 0.055f;
	Context.bIsCritical = DamageData.bIsCritical;
	Context.Timestamp = FPlatformTime::Seconds();

	PendingContexts.Add(SourceActor, Context);
	if (Attacker)
	{
		PendingContexts.Add(Attacker, Context);
	}
}

const FRLPendingDamageContext* URLCombatFeedbackSubsystem::FindMatchingContext(AActor* DamageCauser) const
{
	const double Now = FPlatformTime::Seconds();
	constexpr double MaxContextAge = 4.0;

	if (DamageCauser)
	{
		if (const FRLPendingDamageContext* Direct = PendingContexts.Find(DamageCauser))
		{
			if (Now - Direct->Timestamp <= MaxContextAge)
			{
				return Direct;
			}
		}
		if (AActor* Instigator = DamageCauser->GetInstigator())
		{
			if (const FRLPendingDamageContext* ByInstigator = PendingContexts.Find(Instigator))
			{
				if (Now - ByInstigator->Timestamp <= MaxContextAge)
				{
					return ByInstigator;
				}
			}
		}
		if (AActor* Owner = DamageCauser->GetOwner())
		{
			if (const FRLPendingDamageContext* ByOwner = PendingContexts.Find(Owner))
			{
				if (Now - ByOwner->Timestamp <= MaxContextAge)
				{
					return ByOwner;
				}
			}
		}
	}

	// 가장 최근(0.4초 이내) 시전된 컨텍스트 조회
	const FRLPendingDamageContext* NewestPlayerCtx = nullptr;
	double NewestTime = -1.0;
	for (const TPair<TWeakObjectPtr<AActor>, FRLPendingDamageContext>& Pair : PendingContexts)
	{
		if (Now - Pair.Value.Timestamp <= 0.40 && Pair.Value.Timestamp > NewestTime)
		{
			NewestTime = Pair.Value.Timestamp;
			NewestPlayerCtx = &Pair.Value;
		}
	}
	return NewestPlayerCtx;
}

bool URLCombatFeedbackSubsystem::IsCurrentHitCritical(AActor* DamageCauser) const
{
	if (const FRLPendingDamageContext* Ctx = FindMatchingContext(DamageCauser))
	{
		return Ctx->bIsCritical;
	}
	return false;
}

void URLCombatFeedbackSubsystem::NotifyCharacterDamaged(ARLCharacter* Victim, float Damage, AActor* DamageCauser)
{
	if (!Victim || Damage <= 0.0f || GlobalFeedbackIntensity <= 0.0f)
	{
		return;
	}

	// 같은 프레임 중복 호출 방지 (OnHealthChanged와 OnTakeAnyDamage가 동시에 불릴 때 1회만 실행)
	const uint64 CurrentFrame = GFrameCounter;
	if (const uint64* LastFrame = LastProcessedHitFrameMap.Find(Victim))
	{
		if (*LastFrame == CurrentFrame)
		{
			return;
		}
	}
	LastProcessedHitFrameMap.Add(Victim, CurrentFrame);

	EnsureDefaultAssetsLoaded();

	const FRLPendingDamageContext* Ctx = FindMatchingContext(DamageCauser);
	const bool bIsCritical = Ctx ? Ctx->bIsCritical : false;
	const URLSkillData* SkillData = Ctx ? Ctx->SkillData.Get() : nullptr;

	AActor* AttackerActor = (Ctx && Ctx->Attacker.IsValid()) ? Ctx->Attacker.Get() : DamageCauser;
	if (!AttackerActor && Victim->TeamID == ERLTeam::Enemy)
	{
		if (APlayerController* PC = GetWorld() ? GetWorld()->GetFirstPlayerController() : nullptr)
		{
			AttackerActor = PC->GetPawn();
		}
	}
	ARLCharacter* AttackerChar = Cast<ARLCharacter>(AttackerActor);

	// 1. 역경직 (Hit-Stop: 공격자와 피격자 모두 짧은 순간 정지하여 타격이 꽂히는 감각 부여)
	float HitStopDuration = Ctx ? FMath::Max(Ctx->HitStopDuration, 0.055f) : 0.055f;
	if (SkillData && SkillData->CastType == ESkillCastType::Hold)
	{
		// 홀드/채널링 스킬(라이트닝 등)은 연타 간격이 짧으므로 역경직을 짧게 줄여 답답함 방지
		HitStopDuration = 0.018f;
	}
	else if (bIsCritical)
	{
		HitStopDuration *= 1.45f;
	}
	HitStopDuration *= GlobalFeedbackIntensity;

	if (HitStopDuration > 0.005f)
	{
		ApplyHitStop(Victim, HitStopDuration, 0.03f);
		if (AttackerChar && AttackerChar != Victim)
		{
			ApplyHitStop(AttackerChar, HitStopDuration * 0.85f, 0.05f);
		}
	}

	// 2. 스킬 속성별 고유 타격 이펙트(HitVFX) 및 광원 색상 결정
	FLinearColor ElementLightColor;
	UFXSystemAsset* VFXToSpawn = ResolveHitVFXAndColor(SkillData, bIsCritical, ElementLightColor);

	// 3. 피격 메시 탄성 스케일 펀치(Squash & Stretch) + 속성별 오버레이 발광 플래시
	const FVector ImpactLoc = Victim->GetActorLocation();
	TriggerMeshImpactAndFlash(Victim, ImpactLoc, bIsCritical, ElementLightColor);

	// 4. 고유 타격 이펙트 스폰 (치명타일 때는 기본 치명타 스파크도 함께 겹쳐서 터뜨려 박력 강화)
	if (VFXToSpawn)
	{
		const FRotator HitRot = AttackerChar ? AttackerChar->GetActorRotation() : Victim->GetActorRotation();
		SpawnFXAtTransform(VFXToSpawn, ImpactLoc + FVector(0.0f, 0.0f, 50.0f), HitRot, FVector(bIsCritical ? 1.25f : 1.0f));
	}
	if (bIsCritical && DefaultCritHitVFX && VFXToSpawn != DefaultCritHitVFX.Get())
	{
		RLGameplayCueUtils::SpawnVFX(Victim, DefaultCritHitVFX.Get(), ImpactLoc + FVector(0.0f, 0.0f, 55.0f), false, NAME_None);
	}

	if (SkillData && SkillData->HitSound)
	{
		if (URLAudioSubsystem* AudioSys = URLAudioSubsystem::Get(Victim))
		{
			AudioSys->PlaySFXAtLocation(SkillData->HitSound, ImpactLoc, bIsCritical ? 1.18f : 1.0f, 0.93f, 1.07f);
		}
		else
		{
			UGameplayStatics::PlaySoundAtLocation(Victim, SkillData->HitSound, ImpactLoc);
		}
	}
	else
	{
		PlayAutoHitImpactSFX(Victim, AttackerChar, SkillData, bIsCritical, ImpactLoc);
	}

	// 5. 절제된 카메라 쉐이크 & 줌 펀치
	const bool bPlayerInvolved = (Victim->TeamID == ERLTeam::Player) || !AttackerChar || (AttackerChar->TeamID == ERLTeam::Player);
	if (bPlayerInvolved)
	{
		const float SkillShakeScale = SkillData ? SkillData->CameraShakeScale : 1.0f;
		float ShakeAmp = 7.5f * SkillShakeScale;
		float ShakeDur = 0.08f;
		float ZoomPunch = 9.0f * SkillShakeScale;

		if (Victim->TeamID == ERLTeam::Player)
		{
			ShakeAmp = 16.0f;
			ShakeDur = 0.14f;
			ZoomPunch = 22.0f;
		}
		else if (bIsCritical)
		{
			ShakeAmp = 14.0f * SkillShakeScale;
			ShakeDur = 0.11f;
			ZoomPunch = 20.0f * SkillShakeScale;
		}
		else if (SkillData && SkillData->CastType == ESkillCastType::Hold)
		{
			ShakeAmp = 2.5f * SkillShakeScale;
			ShakeDur = 0.05f;
			ZoomPunch = 0.0f;
		}

		TriggerCameraFeedback(ShakeAmp * GlobalFeedbackIntensity, ShakeDur, ZoomPunch * GlobalFeedbackIntensity);

		if (SkillData && SkillData->CameraShakeClass)
		{
			if (APlayerController* PC = GetWorld() ? GetWorld()->GetFirstPlayerController() : nullptr)
			{
				PC->ClientStartCameraShake(SkillData->CameraShakeClass, SkillShakeScale * GlobalFeedbackIntensity);
			}
		}
	}
}

void URLCombatFeedbackSubsystem::NotifyCharacterDied(ARLCharacter* DeadCharacter)
{
	if (!DeadCharacter || GlobalFeedbackIntensity <= 0.0f)
	{
		return;
	}

	DeadCharacter->CustomTimeDilation = 1.0f;

	// 적 처치 시 마무리 임팩트 이펙트, 사운드 및 가벼운 줌 펀치
	if (DeadCharacter->TeamID == ERLTeam::Enemy)
	{
		EnsureDefaultAssetsLoaded();
		const FVector DeathLoc = DeadCharacter->GetActorLocation() + FVector(0.0f, 0.0f, 50.0f);
		if (DefaultCritHitVFX)
		{
			RLGameplayCueUtils::SpawnVFX(DeadCharacter, DefaultCritHitVFX, DeathLoc, false, NAME_None);
		}
		if (bEnableAutoSkillVFX && HeavyShockwaveVFX)
		{
			SpawnFXAtTransform(HeavyShockwaveVFX.Get(), DeathLoc, FRotator::ZeroRotator, FVector(0.85f));
		}

		const double NowReal = FPlatformTime::Seconds();
		if (NowReal - LastDeathSFXRealTime >= 0.16)
		{
			LastDeathSFXRealTime = NowReal;
			PlayRandomSFXFromPool(EnemyDeathSFXPool, DeathLoc, 0.11f, 0.86f, 0.96f);
		}

		TriggerCameraFeedback(15.0f * GlobalFeedbackIntensity, 0.12f, 24.0f * GlobalFeedbackIntensity);
	}
}

void URLCombatFeedbackSubsystem::ApplyHitStop(AActor* TargetActor, float Duration, float TimeDilation)
{
	if (!TargetActor || Duration <= 0.0f)
	{
		return;
	}

	const double EndTime = FPlatformTime::Seconds() + static_cast<double>(Duration);
	TargetActor->CustomTimeDilation = FMath::Clamp(TimeDilation, 0.01f, 1.0f);

	for (FRLActiveHitStop& Existing : ActiveHitStops)
	{
		if (Existing.TargetActor.Get() == TargetActor)
		{
			Existing.EndRealTime = FMath::Max(Existing.EndRealTime, EndTime);
			return;
		}
	}

	FRLActiveHitStop NewHitStop;
	NewHitStop.TargetActor = TargetActor;
	NewHitStop.EndRealTime = EndTime;
	ActiveHitStops.Add(NewHitStop);
}

void URLCombatFeedbackSubsystem::TriggerMeshImpactAndFlash(ACharacter* Victim, const FVector& ImpactLocation, bool bIsCritical, const FLinearColor& ElementLightColor)
{
	if (!Victim)
	{
		return;
	}

	USkeletalMeshComponent* Mesh = Victim->GetMesh();
	if (!Mesh)
	{
		return;
	}

	FVector BaseScale = CachedBaseMeshScales.FindRef(Victim);
	if (BaseScale.IsNearlyZero())
	{
		BaseScale = Mesh->GetRelativeScale3D();
		CachedBaseMeshScales.Add(Victim, BaseScale);
	}

	// 타격 순간 옆으로 살짝 퍼지고 위아래로 눌리는 탄성(Squash & Stretch) 배율
	const float RadialScale = bIsCritical ? 1.24f : 1.15f;
	const float VerticalScale = bIsCritical ? 0.80f : 0.86f;
	const FVector TargetPunchScale(BaseScale.X * RadialScale, BaseScale.Y * RadialScale, BaseScale.Z * VerticalScale);

	FRLActiveMeshImpact* ExistingImpact = nullptr;
	for (FRLActiveMeshImpact& Entry : ActiveMeshImpacts)
	{
		if (Entry.Character.Get() == Victim)
		{
			ExistingImpact = &Entry;
			break;
		}
	}

	if (!ExistingImpact)
	{
		FRLActiveMeshImpact NewImpact;
		NewImpact.Character = Victim;
		NewImpact.BaseScale = BaseScale;

		const int32 NumMats = Mesh->GetNumMaterials();
		for (int32 i = 0; i < NumMats; ++i)
		{
			if (UMaterialInstanceDynamic* MID = Mesh->CreateAndSetMaterialInstanceDynamic(i))
			{
				NewImpact.DynamicMaterials.Add(MID);
			}
		}

		ActiveMeshImpacts.Add(NewImpact);
		ExistingImpact = &ActiveMeshImpacts.Last();
	}

	ExistingImpact->PunchScale = TargetPunchScale;
	ExistingImpact->Elapsed = 0.0f;
	ExistingImpact->Duration = bIsCritical ? 0.16f : 0.12f;
	ExistingImpact->bIsCritical = bIsCritical;

	Mesh->SetRelativeScale3D(TargetPunchScale);

	// UE5 Overlay Material로 캐릭터 전신 피격 발광 플래시 적용
	if (DefaultHitOverlayMaterial && !Mesh->GetOverlayMaterial())
	{
		Mesh->SetOverlayMaterial(DefaultHitOverlayMaterial);
		ExistingImpact->bAppliedOverlay = true;
	}

	const FLinearColor FlashColor = bIsCritical
		? FLinearColor(5.0f, 2.2f, 0.2f, 1.0f)
		: (ElementLightColor * 3.5f);

	for (const TWeakObjectPtr<UMaterialInstanceDynamic>& MIDPtr : ExistingImpact->DynamicMaterials)
	{
		if (UMaterialInstanceDynamic* MID = MIDPtr.Get())
		{
			MID->SetScalarParameterValue(TEXT("HitFlash"), 1.0f);
			MID->SetScalarParameterValue(TEXT("EmissiveBoost"), bIsCritical ? 5.0f : 2.5f);
			MID->SetVectorParameterValue(TEXT("HitFlashColor"), FlashColor);
		}
	}

	// 타격 순간 속성 색상에 맞는 포인트 라이트 스파크 생성
	SpawnImpactFlashLight(ImpactLocation + FVector(0.0f, 0.0f, 75.0f), bIsCritical, ElementLightColor);
}

void URLCombatFeedbackSubsystem::SpawnImpactFlashLight(const FVector& Location, bool bIsCritical, const FLinearColor& ElementLightColor)
{
	UWorld* World = GetWorld();
	if (!World)
	{
		return;
	}

	APlayerController* PC = World->GetFirstPlayerController();
	AActor* HostActor = PC ? PC->GetPawn() : nullptr;
	if (!HostActor)
	{
		return;
	}

	UPointLightComponent* FlashLight = NewObject<UPointLightComponent>(HostActor);
	if (!FlashLight)
	{
		return;
	}

	FlashLight->RegisterComponent();
	FlashLight->SetWorldLocation(Location);
	FlashLight->SetCastShadows(false);
	FlashLight->SetAttenuationRadius(bIsCritical ? 420.0f : 300.0f);
	FlashLight->SetLightColor(ElementLightColor);

	const float Intensity = (bIsCritical ? 60000.0f : 30000.0f) * GlobalFeedbackIntensity;
	FlashLight->SetIntensity(Intensity);

	FRLActiveImpactFlashLight Entry;
	Entry.LightComp = FlashLight;
	Entry.InitialIntensity = Intensity;
	Entry.Elapsed = 0.0f;
	Entry.Duration = bIsCritical ? 0.09f : 0.07f;
	ActiveFlashLights.Add(Entry);
}

USpringArmComponent* URLCombatFeedbackSubsystem::FindPlayerCameraBoom()
{
	if (CachedCameraBoom.IsValid())
	{
		return CachedCameraBoom.Get();
	}

	UWorld* World = GetWorld();
	APlayerController* PC = World ? World->GetFirstPlayerController() : nullptr;
	APawn* PlayerPawn = PC ? PC->GetPawn() : nullptr;
	if (!PlayerPawn)
	{
		return nullptr;
	}

	if (USpringArmComponent* Boom = PlayerPawn->FindComponentByClass<USpringArmComponent>())
	{
		CachedCameraBoom = Boom;
		if (!bHasCachedCameraDefaults)
		{
			BaseTargetArmLength = Boom->TargetArmLength;
			BaseSocketOffset = Boom->SocketOffset;
			bHasCachedCameraDefaults = true;
		}
		return Boom;
	}
	return nullptr;
}

void URLCombatFeedbackSubsystem::TriggerCameraFeedback(float ShakeAmplitude, float ShakeDuration, float ZoomPunchAmount)
{
	if (!FindPlayerCameraBoom())
	{
		return;
	}

	if (ShakeDuration > 0.0f && ShakeAmplitude > 0.0f)
	{
		CurrentShakeAmplitude = FMath::Clamp(FMath::Max(CurrentShakeAmplitude, ShakeAmplitude), 0.0f, 25.0f);
		ShakeTimeRemaining = FMath::Clamp(FMath::Max(ShakeTimeRemaining, ShakeDuration), 0.0f, 0.18f);
		ShakeTotalDuration = FMath::Max(ShakeTotalDuration, ShakeTimeRemaining);
	}

	if (ZoomPunchAmount > 0.0f)
	{
		TargetZoomPunch = FMath::Clamp(FMath::Max(TargetZoomPunch, ZoomPunchAmount), 0.0f, 32.0f);
		CurrentZoomOffset = TargetZoomPunch;
	}
}

void URLCombatFeedbackSubsystem::Tick(float DeltaTime)
{
	Super::Tick(DeltaTime);

	Listeners.RemoveAll([](const URLCombatFeedbackListener* Listener)
	{
		return !Listener || !Listener->WatchedCharacter.IsValid();
	});

	const double CurrentRealTime = FPlatformTime::Seconds();
	UpdateHitStops(CurrentRealTime);
	UpdateMeshImpacts(DeltaTime);
	UpdateImpactFlashLights(DeltaTime);
	UpdateCameraFeedback(DeltaTime);
}

void URLCombatFeedbackSubsystem::UpdateHitStops(double CurrentRealTime)
{
	for (int32 i = ActiveHitStops.Num() - 1; i >= 0; --i)
	{
		FRLActiveHitStop& Entry = ActiveHitStops[i];
		AActor* Actor = Entry.TargetActor.Get();
		if (!Actor)
		{
			ActiveHitStops.RemoveAtSwap(i);
			continue;
		}

		if (CurrentRealTime >= Entry.EndRealTime)
		{
			Actor->CustomTimeDilation = 1.0f;
			ActiveHitStops.RemoveAtSwap(i);
		}
	}
}

void URLCombatFeedbackSubsystem::UpdateMeshImpacts(float DeltaTime)
{
	for (int32 i = ActiveMeshImpacts.Num() - 1; i >= 0; --i)
	{
		FRLActiveMeshImpact& Impact = ActiveMeshImpacts[i];
		ACharacter* Char = Impact.Character.Get();
		USkeletalMeshComponent* Mesh = Char ? Char->GetMesh() : nullptr;
		if (!Mesh)
		{
			ActiveMeshImpacts.RemoveAtSwap(i);
			continue;
		}

		Impact.Elapsed += DeltaTime;
		const float Alpha = FMath::Clamp(Impact.Elapsed / FMath::Max(Impact.Duration, KINDA_SMALL_NUMBER), 0.0f, 1.0f);

		if (Impact.bAppliedOverlay && Alpha >= 0.40f)
		{
			Mesh->SetOverlayMaterial(nullptr);
			Impact.bAppliedOverlay = false;
		}

		const float EaseAlpha = FMath::InterpEaseOut(0.0f, 1.0f, Alpha, 2.5f);
		const FVector CurrentScale = FMath::Lerp(Impact.PunchScale, Impact.BaseScale, EaseAlpha);
		Mesh->SetRelativeScale3D(CurrentScale);

		const float FlashStrength = 1.0f - Alpha;
		for (const TWeakObjectPtr<UMaterialInstanceDynamic>& MIDPtr : Impact.DynamicMaterials)
		{
			if (UMaterialInstanceDynamic* MID = MIDPtr.Get())
			{
				MID->SetScalarParameterValue(TEXT("HitFlash"), FlashStrength);
				MID->SetScalarParameterValue(TEXT("EmissiveBoost"), FlashStrength * (Impact.bIsCritical ? 5.0f : 2.5f));
			}
		}

		if (Alpha >= 1.0f)
		{
			Mesh->SetRelativeScale3D(Impact.BaseScale);
			if (Impact.bAppliedOverlay)
			{
				Mesh->SetOverlayMaterial(nullptr);
				Impact.bAppliedOverlay = false;
			}
			for (const TWeakObjectPtr<UMaterialInstanceDynamic>& MIDPtr : Impact.DynamicMaterials)
			{
				if (UMaterialInstanceDynamic* MID = MIDPtr.Get())
				{
					MID->SetScalarParameterValue(TEXT("HitFlash"), 0.0f);
					MID->SetScalarParameterValue(TEXT("EmissiveBoost"), 0.0f);
				}
			}
			ActiveMeshImpacts.RemoveAtSwap(i);
		}
	}
}

void URLCombatFeedbackSubsystem::UpdateImpactFlashLights(float DeltaTime)
{
	for (int32 i = ActiveFlashLights.Num() - 1; i >= 0; --i)
	{
		FRLActiveImpactFlashLight& Entry = ActiveFlashLights[i];
		UPointLightComponent* Light = Entry.LightComp.Get();
		if (!Light)
		{
			ActiveFlashLights.RemoveAtSwap(i);
			continue;
		}

		Entry.Elapsed += DeltaTime;
		const float Alpha = FMath::Clamp(Entry.Elapsed / FMath::Max(Entry.Duration, KINDA_SMALL_NUMBER), 0.0f, 1.0f);
		Light->SetIntensity(Entry.InitialIntensity * (1.0f - Alpha));

		if (Alpha >= 1.0f)
		{
			Light->DestroyComponent();
			ActiveFlashLights.RemoveAtSwap(i);
		}
	}
}

void URLCombatFeedbackSubsystem::UpdateCameraFeedback(float DeltaTime)
{
	USpringArmComponent* Boom = FindPlayerCameraBoom();
	if (!Boom)
	{
		return;
	}

	FVector ShakeOffset = FVector::ZeroVector;
	if (ShakeTimeRemaining > 0.0f)
	{
		ShakeTimeRemaining = FMath::Max(0.0f, ShakeTimeRemaining - DeltaTime);
		ShakePhase += DeltaTime * 58.0f;

		const float Decay = ShakeTotalDuration > KINDA_SMALL_NUMBER
			? FMath::Clamp(ShakeTimeRemaining / ShakeTotalDuration, 0.0f, 1.0f)
			: 0.0f;
		const float Amp = CurrentShakeAmplitude * Decay;

		ShakeOffset.Y = FMath::Sin(ShakePhase * 1.13f) * Amp;
		ShakeOffset.Z = FMath::Cos(ShakePhase * 1.79f) * Amp * 0.7f;
		ShakeOffset.X = FMath::Sin(ShakePhase * 2.41f) * Amp * 0.35f;

		if (ShakeTimeRemaining <= 0.0f)
		{
			CurrentShakeAmplitude = 0.0f;
			ShakeTotalDuration = 0.0f;
		}
	}

	if (!FMath::IsNearlyZero(CurrentZoomOffset, 0.1f))
	{
		CurrentZoomOffset = FMath::FInterpTo(CurrentZoomOffset, 0.0f, DeltaTime, 16.0f);
		TargetZoomPunch = CurrentZoomOffset;
	}
	else
	{
		CurrentZoomOffset = 0.0f;
		TargetZoomPunch = 0.0f;
	}

	Boom->SocketOffset = BaseSocketOffset + ShakeOffset;
	Boom->TargetArmLength = FMath::Max(250.0f, BaseTargetArmLength - CurrentZoomOffset);
}

TStatId URLCombatFeedbackSubsystem::GetStatId() const
{
	RETURN_QUICK_DECLARE_CYCLE_STAT(URLCombatFeedbackSubsystem, STATGROUP_Tickables);
}
