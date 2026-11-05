#include "RLCombatFeedbackSubsystem.h"
#include "RLCharacter.h"
#include "RLHealthComponent.h"
#include "RLSkillData.h"
#include "RLGameplayCueNotifies.h"
#include "Particles/ParticleSystem.h"
#include "NiagaraSystem.h"
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

	// 스킬 데이터에 HitVFX가 비어있어도 기본 타격 파티클이 터지도록 프로젝트 내 에셋 로드
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

	DefaultHitOverlayMaterial = LoadObject<UMaterialInterface>(nullptr,
		TEXT("/Game/Effect/Material/M_DropGlow.M_DropGlow"));
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

	// 2. 피격 메시 탄성 스케일 펀치(Squash & Stretch) + 오버레이 발광 플래시
	const FVector ImpactLoc = Victim->GetActorLocation();
	TriggerMeshImpactAndFlash(Victim, ImpactLoc, bIsCritical);

	// 3. 피격 이펙트(HitVFX: 미지정 시 기본 타격 스파크 자동 재생) 및 타격음(HitSound) 재생
	UFXSystemAsset* VFXToSpawn = (SkillData && SkillData->HitVFX)
		? SkillData->HitVFX.Get()
		: (bIsCritical ? DefaultCritHitVFX.Get() : DefaultNormalHitVFX.Get());

	if (VFXToSpawn)
	{
		RLGameplayCueUtils::SpawnVFX(Victim, VFXToSpawn, ImpactLoc + FVector(0.0f, 0.0f, 50.0f), false, NAME_None);
	}

	if (SkillData && SkillData->HitSound)
	{
		UGameplayStatics::PlaySoundAtLocation(Victim, SkillData->HitSound, ImpactLoc);
	}

	// 4. 절제된 카메라 쉐이크 & 줌 펀치 (눈이 피로하지 않도록 진폭과 줌인 폭을 대폭 완화)
	const bool bPlayerInvolved = (Victim->TeamID == ERLTeam::Player) || !AttackerChar || (AttackerChar->TeamID == ERLTeam::Player);
	if (bPlayerInvolved)
	{
		const float SkillShakeScale = SkillData ? SkillData->CameraShakeScale : 1.0f;
		float ShakeAmp = 7.5f * SkillShakeScale;
		float ShakeDur = 0.08f;
		float ZoomPunch = 9.0f * SkillShakeScale;

		if (Victim->TeamID == ERLTeam::Player)
		{
			// 플레이어 피격 시에만 살짝 더 흔들림
			ShakeAmp = 16.0f;
			ShakeDur = 0.14f;
			ZoomPunch = 22.0f;
		}
		else if (bIsCritical)
		{
			// 치명타 적중 시 절제된 임팩트 펀치
			ShakeAmp = 14.0f * SkillShakeScale;
			ShakeDur = 0.11f;
			ZoomPunch = 20.0f * SkillShakeScale;
		}
		else if (SkillData && SkillData->CastType == ESkillCastType::Hold)
		{
			// 채널링 스킬은 거의 흔들리지 않게 미세 진동만 유지
			ShakeAmp = 2.5f * SkillShakeScale;
			ShakeDur = 0.05f;
			ZoomPunch = 0.0f;
		}

		TriggerCameraFeedback(ShakeAmp * GlobalFeedbackIntensity, ShakeDur, ZoomPunch * GlobalFeedbackIntensity);

		// 선택적 블루프린트 카메라 쉐이크 에셋이 지정되어 있으면 함께 실행
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

	// 사망한 캐릭터의 역경직 즉시 해제 (사망 모션/랙돌이 멈추지 않도록)
	DeadCharacter->CustomTimeDilation = 1.0f;

	// 적 처치 시 깔끔한 마무리 임팩트 이펙트 및 가벼운 줌 펀치
	if (DeadCharacter->TeamID == ERLTeam::Enemy)
	{
		EnsureDefaultAssetsLoaded();
		if (DefaultCritHitVFX)
		{
			RLGameplayCueUtils::SpawnVFX(DeadCharacter, DefaultCritHitVFX, DeadCharacter->GetActorLocation() + FVector(0.0f, 0.0f, 50.0f), false, NAME_None);
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

void URLCombatFeedbackSubsystem::TriggerMeshImpactAndFlash(ACharacter* Victim, const FVector& ImpactLocation, bool bIsCritical)
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

		// 머티리얼 다이나믹 인스턴스 캐싱
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
		: FLinearColor(3.0f, 3.0f, 3.0f, 1.0f);

	for (const TWeakObjectPtr<UMaterialInstanceDynamic>& MIDPtr : ExistingImpact->DynamicMaterials)
	{
		if (UMaterialInstanceDynamic* MID = MIDPtr.Get())
		{
			MID->SetScalarParameterValue(TEXT("HitFlash"), 1.0f);
			MID->SetScalarParameterValue(TEXT("EmissiveBoost"), bIsCritical ? 5.0f : 2.5f);
			MID->SetVectorParameterValue(TEXT("HitFlashColor"), FlashColor);
		}
	}

	// 타격 순간 포인트 라이트 스파크 생성
	SpawnImpactFlashLight(ImpactLocation + FVector(0.0f, 0.0f, 75.0f), bIsCritical);
}

void URLCombatFeedbackSubsystem::SpawnImpactFlashLight(const FVector& Location, bool bIsCritical)
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
	FlashLight->SetLightColor(bIsCritical ? FLinearColor(1.0f, 0.55f, 0.1f) : FLinearColor(1.0f, 0.90f, 0.75f));

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
		// 다중 타격 시 진폭이 무한정 중첩되지 않도록 상한(25.0f) 적용
		CurrentShakeAmplitude = FMath::Clamp(FMath::Max(CurrentShakeAmplitude, ShakeAmplitude), 0.0f, 25.0f);
		ShakeTimeRemaining = FMath::Clamp(FMath::Max(ShakeTimeRemaining, ShakeDuration), 0.0f, 0.18f);
		ShakeTotalDuration = FMath::Max(ShakeTotalDuration, ShakeTimeRemaining);
	}

	if (ZoomPunchAmount > 0.0f)
	{
		// 다중 타격(광역기 등) 시 줌인이 중첩되어 멀미를 유발하지 않도록 합산(+) 대신 최대값(Max)만 반영하고 상한(32.0f) 제한
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

		// 피격 후 초반 40% 지점에서 오버레이 발광 해제 (짧고 강렬한 번쩍임)
		if (Impact.bAppliedOverlay && Alpha >= 0.40f)
		{
			Mesh->SetOverlayMaterial(nullptr);
			Impact.bAppliedOverlay = false;
		}

		// 탄성 복원 (EaseOut으로 빠르게 원래 스케일로 복귀)
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

	// 1. 카메라 오프셋 진동(Shake) 업데이트 (부드러운 2차 감쇠 적용)
	FVector ShakeOffset = FVector::ZeroVector;
	if (ShakeTimeRemaining > 0.0f)
	{
		ShakeTimeRemaining = FMath::Max(0.0f, ShakeTimeRemaining - DeltaTime);
		const float Damping = FMath::Clamp(ShakeTimeRemaining / FMath::Max(ShakeTotalDuration, KINDA_SMALL_NUMBER), 0.0f, 1.0f);
		const float Amp = CurrentShakeAmplitude * Damping * Damping;

		ShakePhase += DeltaTime * 50.0f;
		ShakeOffset.X = FMath::Sin(ShakePhase * 1.13f) * Amp * 0.45f;
		ShakeOffset.Y = FMath::Cos(ShakePhase * 1.71f) * Amp;
		ShakeOffset.Z = FMath::Sin(ShakePhase * 2.37f) * Amp * 0.55f;

		if (ShakeTimeRemaining <= 0.0f)
		{
			CurrentShakeAmplitude = 0.0f;
			ShakeTotalDuration = 0.0f;
		}
	}
	Boom->SocketOffset = BaseSocketOffset + ShakeOffset;

	// 2. 카메라 줌 펀치(TargetArmLength 순간 당김 후 부드러운 복귀) 업데이트
	if (!FMath::IsNearlyZero(CurrentZoomOffset, 0.1f) || !FMath::IsNearlyZero(TargetZoomPunch, 0.1f))
	{
		CurrentZoomOffset = FMath::FInterpTo(CurrentZoomOffset, 0.0f, DeltaTime, 16.0f);
		TargetZoomPunch = CurrentZoomOffset;
		Boom->TargetArmLength = FMath::Max(300.0f, BaseTargetArmLength - CurrentZoomOffset);
	}
	else
	{
		CurrentZoomOffset = 0.0f;
		TargetZoomPunch = 0.0f;
		Boom->TargetArmLength = BaseTargetArmLength;
	}
}

TStatId URLCombatFeedbackSubsystem::GetStatId() const
{
	RETURN_QUICK_DECLARE_CYCLE_STAT(URLCombatFeedbackSubsystem, STATGROUP_Tickables);
}
