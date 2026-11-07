#include "RLEnemyCharacter.h"
#include "RLHealthComponent.h"
#include "RLStatComponent.h"
#include "RLRewardChest.h"
#include "RLItemDrop.h"
#include "RLItemDataAsset.h"
#include "RLSaveSubsystem.h"
#include "RLPlayerController.h"
#include "RLBossTrigger.h"
#include "EngineUtils.h"
#include "AIController.h"
#include "BehaviorTree/BlackboardComponent.h"
#include "BrainComponent.h"
#include "Components/CapsuleComponent.h"
#include "Components/SkeletalMeshComponent.h"
#include "Engine/World.h"
#include "GameFramework/DamageType.h"
#include "TimerManager.h"
#include "Kismet/GameplayStatics.h"
#include "Animation/AnimInstance.h"
#include "Animation/AnimMontage.h"
#include "GameFramework/CharacterMovementComponent.h"
#include "RLDecalIndicator.h"
#include "RLThrownAxe.h"
#include "Components/StaticMeshComponent.h"
#include "Engine/StaticMesh.h"
#include "Materials/MaterialInterface.h"
#include "RLAbilitySystemComponent.h"
#include "RLGameplayEffects.h"
#include "RLGameplayTags.h"
#include "Materials/MaterialInstanceDynamic.h"
#include "NiagaraFunctionLibrary.h"
#include "NiagaraSystem.h"
#include "NiagaraComponent.h"
#include "Components/PostProcessComponent.h"

ARLEnemyCharacter::ARLEnemyCharacter()
{
	DropChestClass = ARLRewardChest::StaticClass();
	ItemDropClass = ARLItemDrop::StaticClass();
}

void ARLEnemyCharacter::BeginPlay()
{
	Super::BeginPlay();

	// ★ 레벨에 미리 배치된 보스 인스턴스는 BP 기본값을 나중에 바꿔도 균열 몽타주가 비어 있는 채로 남는 경우가 있음
	// (EditDefaultsOnly라 인스턴스에서 고칠 수도 없음) → 비어 있으면 클래스 기본값(BP에 넣은 값)으로 채움
	if (const ARLEnemyCharacter* Defaults = GetClass()->GetDefaultObject<ARLEnemyCharacter>())
	{
		if (!SoulCrackHitMontage)
		{
			SoulCrackHitMontage = Defaults->SoulCrackHitMontage;
		}
		if (!SoulShatterMontage)
		{
			SoulShatterMontage = Defaults->SoulShatterMontage;
		}
	}

	// EnrageHealthPercentThreshold가 음수(기본값)인 일반 몬스터도 델리게이트 바인딩 자체는 걸어두지만,
	// HandleEnemyHealthChanged 내부에서 threshold < 0이면 바로 return하므로 실질적인 비용은 거의 없습니다.
	if (HealthComp)
	{
		HealthComp->OnHealthChanged.AddDynamic(this, &ARLEnemyCharacter::HandleEnemyHealthChanged);
	}

	// ★ 영혼 보호막: 보스만 전투 시작부터 받는 피해 감소를 가집니다 (균열 카운터로 깎아내는 대상)
	if (IsBossEnemy() && SoulCrackShieldDamageReduction > 0.0f)
	{
		SetSoulCrackShield(SoulCrackShieldDamageReduction);
	}
}

void ARLEnemyCharacter::SetSoulCrackShield(float NewDamageReduction)
{
	if (!AbilitySystemComp)
	{
		return;
	}

	if (SoulCrackShieldHandle.IsValid())
	{
		AbilitySystemComp->RemoveActiveGameplayEffect(SoulCrackShieldHandle);
		SoulCrackShieldHandle.Invalidate();
	}

	CurrentSoulCrackShield = FMath::Max(0.0f, NewDamageReduction);
	if (CurrentSoulCrackShield <= 0.0f)
	{
		return;
	}

	const FGameplayEffectSpecHandle Spec = AbilitySystemComp->MakeOutgoingSpec(URLGE_SoulCrackShield::StaticClass(), 1.0f, AbilitySystemComp->MakeEffectContext());
	if (Spec.IsValid())
	{
		Spec.Data->SetSetByCallerMagnitude(RLGameplayTags::Data_Stat_DamageReduction, CurrentSoulCrackShield);
		SoulCrackShieldHandle = AbilitySystemComp->ApplyGameplayEffectSpecToSelf(*Spec.Data.Get());
	}
}

bool ARLEnemyCharacter::IsBossEnemy() const
{
	return bIsBoss || GetClass()->GetName().Contains(TEXT("Boss"), ESearchCase::IgnoreCase) || GetName().Contains(TEXT("Boss"), ESearchCase::IgnoreCase);
}

bool ARLEnemyCharacter::CanBeStaggered() const
{
	return !(bImmuneToStagger || bIsElite || IsBossEnemy());
}

float ARLEnemyCharacter::GetHealthPercent() const
{
	if (!HealthComp || FMath::IsNearlyZero(HealthComp->MaxHealth))
	{
		return 0.0f;
	}
	return HealthComp->CurrentHealth / HealthComp->MaxHealth;
}

bool ARLEnemyCharacter::DebugKill()
{
	if (!HealthComp || HealthComp->bIsDead)
	{
		return false;
	}

	// 방어력/보호막에 깎이지 않도록 트루 데미지로 남은 체력만큼 넣어 정상 사망 루트를 탑니다.
	HealthComp->ApplyIncomingDamage(HealthComp->CurrentHealth, nullptr, true);
	return true;
}

void ARLEnemyCharacter::HandleEnemyHealthChanged(float NewCurrentHealth, float NewMaxHealth, float HealthDelta)
{
	// ★ 이 몬스터가 분노 기능을 쓰지 않는 개체(일반 몬스터, threshold 기본값 -1)라면 여기서 끝냅니다.
	if (EnrageHealthPercentThreshold < 0.0f)
	{
		return;
	}

	const float Percent = FMath::IsNearlyZero(NewMaxHealth) ? 0.0f : (NewCurrentHealth / NewMaxHealth);

	if (AAIController* AICon = Cast<AAIController>(GetController()))
	{
		if (UBlackboardComponent* BB = AICon->GetBlackboardComponent())
		{
			BB->SetValueAsFloat(BlackboardKey_HealthPercent, Percent);
		}
	}

	if (!bIsEnraged && Percent <= EnrageHealthPercentThreshold)
	{
		EnterEnrage();
	}
	else if (bIsEnraged && !bEnrageIsOneWay && Percent > EnrageHealthPercentThreshold)
	{
		bIsEnraged = false;
		if (AAIController* AICon = Cast<AAIController>(GetController()))
		{
			if (UBlackboardComponent* BB = AICon->GetBlackboardComponent())
			{
				BB->SetValueAsBool(BlackboardKey_IsEnraged, false);
			}
		}
	}
}

void ARLEnemyCharacter::EnterEnrage()
{
	bIsEnraged = true;

	if (AAIController* AICon = Cast<AAIController>(GetController()))
	{
		if (UBlackboardComponent* BB = AICon->GetBlackboardComponent())
		{
			BB->SetValueAsBool(BlackboardKey_IsEnraged, true);
		}
	}

	OnEnraged();
}

void ARLEnemyCharacter::HandleDeath()
{
	Super::HandleDeath();

	bIsEnraged = false;

	// 균열 관련 타이머가 사망 후 BT를 재시작하지 않도록 정리
	EndSoulCrackPattern();
	StopAttackChase();
	ChainMontage = nullptr;
	GetWorldTimerManager().ClearTimer(ChainTimerHandle);
	StopPulse();
	EndAxeLeap();
	StopMirrorAnimation();
	bSoulCrackAIStopped = false;
	GetWorldTimerManager().ClearTimer(SoulCrackAITimerHandle);
	GetWorldTimerManager().ClearTimer(SoulShatterDebuffTimerHandle);

	if (AAIController* AICon = Cast<AAIController>(GetController()))
	{
		if (UBrainComponent* Brain = AICon->GetBrainComponent())
		{
			Brain->StopLogic(TEXT("EnemyDied"));
		}
	}

	// 사망 몽타주가 끝나갈 때쯤 페이드 + 가라앉기 시작 (끝나면 Destroy)
	if (bUseDeathFade)
	{
		if (DeathFadeDelay > 0.0f)
		{
			GetWorldTimerManager().SetTimer(DeathFadeTimerHandle, this, &ARLEnemyCharacter::StartDeathFade, DeathFadeDelay, false);
		}
		else
		{
			StartDeathFade();
		}
	}

	// 1. 플레이어에게 처치 경험치 지급
	if (ARLCharacter* PlayerChar = Cast<ARLCharacter>(UGameplayStatics::GetPlayerCharacter(this, 0)))
	{
		if (URLStatComponent* PlayerStats = PlayerChar->GetStatComponent())
		{
			PlayerStats->AddExp(ExpReward);
		}
	}

	// 몬스터 사망 델리게이트 방송 (방 관리자 / 웨이브 시스템 연동)
	OnEnemyDied.Broadcast(this);

	const FVector MyLoc = GetActorLocation();

	// 보스 여부 판정 (bIsBoss 체크 여부 또는 클래스/액터 이름에 "Boss"가 포함되어 있으면 자동 보스 판정)
	const bool bIsActualBoss = IsBossEnemy();

	// 1-1. 이번 런 임시 영혼(RunSouls) 적립 (일반 몬스터 기본 2, 보스 기본 35)
	int32 GrantedSouls = SoulReward;
	if (bIsActualBoss && GrantedSouls < 20)
	{
		GrantedSouls = 35;
	}

	if (GrantedSouls > 0)
	{
		if (URLSaveSubsystem* SaveSys = URLSaveSubsystem::Get(this))
		{
			// 영구 특성: 영혼 수확자 (SoulGain_Up) 적용 (레벨당 +15% 증폭)
			const int32 SoulGainLevel = SaveSys->GetUpgradeLevel(FName("SoulGain_Up"));
			if (SoulGainLevel > 0)
			{
				const float Multiplier = 1.0f + (SoulGainLevel * 0.15f);
				GrantedSouls = FMath::Max(GrantedSouls, FMath::RoundToInt(GrantedSouls * Multiplier));
			}

			SaveSys->AddRunSouls(GrantedSouls);
		}
	}

	// 2. 보스 처치: 100% 확정 보상 상자 + 대량 골드 드롭
	if (bIsActualBoss)
	{
		// 보스 처치 즉시 ARLBossTrigger에 보스 처치 태그 부여 (안개벽 해제 및 클리어 상태 마킹)
		for (TActorIterator<ARLBossTrigger> It(GetWorld()); It; ++It)
		{
			It->Tags.AddUnique(TEXT("BossDefeated"));
		}

		// 보스 처치 즉시 "STAGE CLEAR" 화면 (트리거 소환/맵 직접 배치 보스 모두 해당)
		if (ARLPlayerController* PC = Cast<ARLPlayerController>(UGameplayStatics::GetPlayerController(this, 0)))
		{
			PC->ShowStageClearScreen();
		}

		// 보상 상자 클래스 확보 (MyRLRewardChest 에셋이 있으면 메시/이펙트가 들어간 블루프린트로 스폰)
		TSubclassOf<ARLRewardChest> TargetChestClass = DropChestClass;
		if (!TargetChestClass || TargetChestClass == ARLRewardChest::StaticClass())
		{
			TargetChestClass = LoadClass<ARLRewardChest>(nullptr, TEXT("/Game/Blueprint/MyRLRewardChest.MyRLRewardChest_C"));
			if (!TargetChestClass)
			{
				TargetChestClass = ARLRewardChest::StaticClass();
			}
		}

		if (TargetChestClass)
		{
			FVector SpawnLoc = MyLoc + ChestSpawnOffset;
			if (UCapsuleComponent* Capsule = GetCapsuleComponent())
			{
				SpawnLoc.Z -= (Capsule->GetScaledCapsuleHalfHeight() - 10.0f);
			}

			FActorSpawnParameters SpawnParams;
			SpawnParams.SpawnCollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::AdjustIfPossibleButAlwaysSpawn;

			ARLRewardChest* BossChest = GetWorld()->SpawnActor<ARLRewardChest>(TargetChestClass, SpawnLoc, FRotator::ZeroRotator, SpawnParams);

			// 보스가 떨어뜨린 이 상자 하나만 추적 → 보상 수령이 끝나면 승리 화면
			if (BossChest)
			{
				if (ARLPlayerController* PC = Cast<ARLPlayerController>(UGameplayStatics::GetPlayerController(this, 0)))
				{
					PC->WatchBossRewardChest(BossChest);
				}
			}

			if (GEngine)
			{
				GEngine->AddOnScreenDebugMessage(-1, 5.0f, FColor::Yellow,
					FString::Printf(TEXT("🏆 [보스 처치!] %s 보상 상자가 생성되었습니다!"), *TargetChestClass->GetName()));
			}
		}

		// 보스 주변에 대량의 골드 드롭 3개 스폰
		if (ItemDropClass)
		{
			for (int32 i = 0; i < 3; ++i)
			{
				FActorSpawnParameters SpawnParams;
				SpawnParams.SpawnCollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::AlwaysSpawn;
				if (ARLItemDrop* GoldDrop = GetWorld()->SpawnActor<ARLItemDrop>(ItemDropClass, MyLoc, FRotator::ZeroRotator, SpawnParams))
				{
					GoldDrop->InitializeGoldDrop(FMath::RandRange(40, 70));
					const float Angle = (360.0f / 3.0f) * i;
					const FVector Dir = FRotator(0.f, Angle, 0.f).Vector();
					GoldDrop->LaunchDrop(Dir * 200.0f + FVector(0.f, 0.f, 400.f));
				}
			}
		}
	}
	// 3. 일반 몬스터: 보상 상자 X, 35% 확률로 바닥에 필드 아이템/골드만 드롭
	else if (ItemDropClass && FMath::FRand() <= ItemDropChance)
	{
		FActorSpawnParameters SpawnParams;
		SpawnParams.SpawnCollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::AlwaysSpawn;

		if (ARLItemDrop* DropActor = GetWorld()->SpawnActor<ARLItemDrop>(ItemDropClass, MyLoc, FRotator::ZeroRotator, SpawnParams))
		{
			// 아이템 후보 풀이 있고 50% 확률이면 아이템 드롭, 아니면 골드 드롭
			if (PossibleItemDrops.Num() > 0 && FMath::RandBool())
			{
				const int32 RandomIdx = FMath::RandRange(0, PossibleItemDrops.Num() - 1);
				DropActor->InitializeItemDrop(PossibleItemDrops[RandomIdx], 1);
			}
			else
			{
				const int32 RolledGold = FMath::RandRange(MinGoldDrop, MaxGoldDrop);
				DropActor->InitializeGoldDrop(RolledGold);
			}

			// 랜덤한 방향으로 퐁 튀어나오는 포물선 발사
			const FVector RandomDir = FVector(FMath::FRandRange(-1.f, 1.f), FMath::FRandRange(-1.f, 1.f), 0.f).GetSafeNormal();
			DropActor->LaunchDrop(RandomDir * FMath::FRandRange(100.f, 200.f) + FVector(0.f, 0.f, FMath::FRandRange(280.f, 380.f)));
		}
	}
}

// ==========================================
// ★ 영혼의 균열 (Soul Crack)
// ==========================================

void ARLEnemyCharacter::Tick(float DeltaTime)
{
	// ★ 연타 반복은 부모 Tick(예고 데칼 갱신)보다 먼저: 바퀴가 넘어간 프레임에 범위 배율을 먼저 올려야
	// 새 바퀴 1타 데칼이 이전 바퀴 크기로 잠깐 떴다가 커지는 현상이 없음
	UpdateComboLoop();

	Super::Tick(DeltaTime);

	UpdateAttackChase(DeltaTime);
	UpdateAttackStep();
	UpdateChainNextAttack();
	UpdatePulse();
	UpdateAxeLeap(DeltaTime);
	UpdateVanish(DeltaTime);
	UpdateVanishDarken(DeltaTime);
	UpdateMirrorAnimation();

	// ★ 균열 경직 중인데 BT가 다시 돌고 있으면(다른 경로에서 재시작된 경우) 원인 로그를 남기고 다시 멈춤
	if (bSoulCrackAIStopped)
	{
		if (AAIController* AICon = Cast<AAIController>(GetController()))
		{
			UBrainComponent* Brain = AICon->GetBrainComponent();
			if (Brain && Brain->IsRunning())
			{
				UE_LOG(LogTemp, Warning, TEXT("[SoulCrack] %s: 경직 중(%.1f초 남음)인데 BT가 다시 실행 중 → 재정지"),
					*GetName(), SoulCrackAIStopEndTime - GetWorld()->GetTimeSeconds());
				Brain->StopLogic(TEXT("SoulCrack"));
				AICon->StopMovement();
			}
		}
	}

	// 강화판 몽타주가 끝났거나 끊겼으면 자동 정리 (녹색 아우라 OFF)
	if (SoulCrackMontage)
	{
		// 잠행 강습처럼 몽타주를 잠깐 멈추는(Pause) 패턴도 있어서 IsPlaying(정지 중 false)이 아니라 IsActive로 확인
		UAnimInstance* AnimInstance = GetMesh() ? GetMesh()->GetAnimInstance() : nullptr;
		if (!AnimInstance || !AnimInstance->Montage_IsActive(SoulCrackMontage))
		{
			EndSoulCrackPattern();
		}
	}

	// 강화판 진행 중: 카운터 방향 안내 데칼이 보스를 따라가고, 균열 구간 동안 몸 발광
	if (SoulCrackMontage)
	{
		// 땅속으로 사라져 있는 동안은 안내 데칼도 숨김 (안 그러면 숨은 위치가 드러남)
		const bool bVanished = VanishStep == EVanishStep::FadingOut || VanishStep == EVanishStep::Hidden;
		if (bVanished)
		{
			if (!bSoulCrackZonesHiddenByVanish)
			{
				bSoulCrackZonesHiddenByVanish = true;
				HideSoulCrackZones();
			}
		}
		else
		{
			if (bSoulCrackZonesHiddenByVanish)
			{
				bSoulCrackZonesHiddenByVanish = false;
				ShowSoulCrackZones();
			}
			UpdateSoulCrackZones();
		}
		SetSoulCrackWindowVisual(IsSoulCrackWindowOpen());
	}
}

// ==========================================
// ★ 카운터 방향 안내 / 몸 발광
// ==========================================

ARLDecalIndicator* ARLEnemyCharacter::SpawnSoulCrackZoneIndicator()
{
	FActorSpawnParameters Params;
	Params.Owner = this;
	Params.SpawnCollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::AlwaysSpawn;
	return GetWorld()->SpawnActor<ARLDecalIndicator>(ARLDecalIndicator::StaticClass(), GetActorLocation(), FRotator::ZeroRotator, Params);
}

// ★ 공격 예고와 같은 부채꼴 셰이더(M_Sector_Red 계열)를 재사용. 셰이더는 "뒤쪽 SafeZoneAngle만큼 잘라낸 부채꼴"을
// 그리므로, SafeZone = 360 - 구역 각도로 넘기면 정면 쪽 구역 각도만큼의 쐐기만 남습니다. 후방 구역은 같은 쐐기를 180도 돌려 표시.
void ARLEnemyCharacter::ShowSoulCrackZones()
{
	if (!SoulCrackZoneMaterial)
	{
		SoulCrackZoneMaterial = LoadObject<UMaterialInterface>(nullptr, TEXT("/Game/Effect/Material/MI_SoulCrack_Zone.MI_SoulCrack_Zone"));
	}

	auto SetupZone = [this](TObjectPtr<ARLDecalIndicator>& Indicator, float ZoneAngle)
	{
		if (ZoneAngle <= 0.0f)
		{
			if (Indicator)
			{
				Indicator->SetIndicatorVisibility(false);
			}
			return;
		}
		if (!Indicator)
		{
			Indicator = SpawnSoulCrackZoneIndicator();
		}
		if (Indicator)
		{
			Indicator->SetupSectorIndicator(SoulCrackZoneRadius, FMath::Clamp(360.0f - ZoneAngle, 0.0f, 359.0f), SoulCrackZoneMaterial);
			Indicator->SetIndicatorVisibility(true);
		}
	};

	SetupZone(SoulCrackZoneFront, SoulCrackFrontAngle);
	SetupZone(SoulCrackZoneBack, SoulCrackBackAngle);
	UpdateSoulCrackZones();
}

void ARLEnemyCharacter::HideSoulCrackZones()
{
	if (SoulCrackZoneFront)
	{
		SoulCrackZoneFront->SetIndicatorVisibility(false);
	}
	if (SoulCrackZoneBack)
	{
		SoulCrackZoneBack->SetIndicatorVisibility(false);
	}
}

void ARLEnemyCharacter::UpdateSoulCrackZones()
{
	const FVector Location = GetIndicatorOrigin();
	const float Yaw = GetActorRotation().Yaw;
	if (SoulCrackZoneFront && SoulCrackFrontAngle > 0.0f)
	{
		SoulCrackZoneFront->UpdateSectorTransform(Location, Yaw);
	}
	if (SoulCrackZoneBack && SoulCrackBackAngle > 0.0f)
	{
		SoulCrackZoneBack->UpdateSectorTransform(Location, Yaw + 180.0f);
	}
}

void ARLEnemyCharacter::SetSoulCrackWindowVisual(bool bOpen)
{
	if (bOpen == bSoulCrackWindowVisualOn)
	{
		return;
	}
	bSoulCrackWindowVisualOn = bOpen;

	if (USkeletalMeshComponent* MeshComp = GetMesh())
	{
		if (bOpen)
		{
			if (!SoulCrackWindowOverlayMaterial)
			{
				SoulCrackWindowOverlayMaterial = LoadObject<UMaterialInterface>(nullptr, TEXT("/Game/Effect/Material/M_SoulCrack_Overlay.M_SoulCrack_Overlay"));
			}
			SavedOverlayMaterial = MeshComp->GetOverlayMaterial();
			MeshComp->SetOverlayMaterial(SoulCrackWindowOverlayMaterial);
		}
		else
		{
			MeshComp->SetOverlayMaterial(SavedOverlayMaterial);
			SavedOverlayMaterial = nullptr;
		}
	}

	OnSoulCrackWindowChanged(bOpen);
}

// ==========================================
// ★ 공격 중 전진 (Attack Chase)
// ==========================================

void ARLEnemyCharacter::BeginAttackPattern(const FRLMonsterPatternData& Pattern, FName HitNotifyName)
{
	SetAttackHitScales(Pattern.HitRangeScales);
	SetAttackHitWidthScales(Pattern.HitWidthScales);
	// BTT_Attack이 이 직후 StartBoxAttackTelegraph / StartDashAttack을 부르므로 여기서 미리 세팅
	DashForwardReach = Pattern.AttackShape == ERLAttackShape::Dash ? Pattern.DashForwardReach : 0.0f;
	++AttackPatternSerial;
	AttackLandedCount = 0;
	ChainMontage = Pattern.bChainNextAttack ? Pattern.AttackMontage : nullptr;
	ChainDelay = Pattern.ChainNextAttackDelay;
	bChainOnlyOnMiss = Pattern.bChainOnlyOnMiss;
	GetWorldTimerManager().ClearTimer(ChainTimerHandle);

	// 캔슬 시점: Notify_ChainCancel이 있으면 그 위치, 없으면 마지막 공격 노티파이(Notify_DashStart/Notify_Hit*) + 대기 시간
	ChainCancelTime = -1.0f;
	if (ChainMontage)
	{
		const TArray<FVector2D> CancelNotifies = ComputeTelegraphWindows(ChainMontage, FName("Notify_ChainCancel"), 0.0f);
		if (CancelNotifies.Num() > 0)
		{
			ChainCancelTime = CancelNotifies[0].Y;
		}
		else
		{
			float LastAttackTime = -1.0f;
			for (const TCHAR* NotifyName : { TEXT("Notify_DashStart"), TEXT("Notify_Hit") })
			{
				for (const FVector2D& Window : ComputeTelegraphWindows(ChainMontage, FName(NotifyName), 0.0f))
				{
					LastAttackTime = FMath::Max(LastAttackTime, Window.Y);
				}
			}
			if (LastAttackTime >= 0.0f)
			{
				ChainCancelTime = LastAttackTime + ChainDelay;
			}
		}
	}
	StartAttackChase(Pattern);
	StartPulse(Pattern);
	StartComboLoop(Pattern);
	StartVanish(Pattern);

	// 타격마다 한 걸음: 각 Notify_Hit보다 AttackStepLeadTime 앞선 시각들을 미리 계산
	StepMontage = nullptr;
	StepTimes.Reset();
	NextStepIndex = 0;
	if (Pattern.AttackStepDistance > 0.0f && Pattern.AttackMontage)
	{
		for (const FVector2D& Window : ComputeTelegraphWindows(Pattern.AttackMontage, FName("Notify_Hit"), 0.0f))
		{
			StepTimes.Add(FMath::Max(0.0f, Window.Y - Pattern.AttackStepLeadTime));
		}
		StepMontage = StepTimes.Num() > 0 ? Pattern.AttackMontage.Get() : nullptr;
		StepDistance = Pattern.AttackStepDistance;
		StepDuration = Pattern.AttackStepDuration;
		ChaseStopDistance = Pattern.AttackChaseStopDistance;
	}
	BeginSoulCrackPattern(Pattern, HitNotifyName);

	// 몽타주 전체 반전, 또는 Notify_MirrorStart ~ Notify_MirrorEnd 구간만 반전 (둘 다 아니면 반전 없음)
	if (Pattern.bMirrorAnimation)
	{
		StartMirrorAnimation(Pattern.AttackMontage);
	}
	else
	{
		StartMirrorWindows(Pattern.AttackMontage);
	}

	EndAxeLeap();
	if (Pattern.AttackShape == ERLAttackShape::AxeLeap || Pattern.AttackShape == ERLAttackShape::AxeReturn || Pattern.AttackShape == ERLAttackShape::AxeDoubleReturn)
	{
		BeginAxeLeap(Pattern);
	}
}

bool ARLEnemyCharacter::IsAttackHitNotify(FName NotifyName)
{
	return NotifyName.ToString().StartsWith(TEXT("Notify_Hit"));
}

void ARLEnemyCharacter::StartAttackChase(const FRLMonsterPatternData& Pattern)
{
	StopAttackChase();

	if (Pattern.AttackChaseSpeed <= 0.0f || !Pattern.AttackMontage || (HealthComp && HealthComp->bIsDead))
	{
		return;
	}

	ChaseMontage = Pattern.AttackMontage;
	ChaseSpeed = Pattern.AttackChaseSpeed;
	ChaseStopDistance = Pattern.AttackChaseStopDistance;
}

// ==========================================
// ★ 연타 반복 (ComboLoop)
// ==========================================

void ARLEnemyCharacter::StartComboLoop(const FRLMonsterPatternData& Pattern)
{
	ComboMontage = nullptr;
	if (Pattern.ComboLoopCount <= 0 || !Pattern.AttackMontage)
	{
		return;
	}

	UAnimInstance* AnimInstance = GetMesh() ? GetMesh()->GetAnimInstance() : nullptr;
	UAnimMontage* Montage = Pattern.AttackMontage;
	if (!AnimInstance || Montage->GetSectionIndex(Pattern.ComboLoopSection) == INDEX_NONE || Montage->GetSectionIndex(Pattern.ComboFinishSection) == INDEX_NONE)
	{
		UE_LOG(LogTemp, Warning, TEXT("[ComboLoop] %s: 몽타주 '%s'에 '%s' / '%s' 섹션이 있어야 합니다."), *GetName(), *Montage->GetName(),
			*Pattern.ComboLoopSection.ToString(), *Pattern.ComboFinishSection.ToString());
		return;
	}

	ComboMontage = Montage;
	ComboLoopSectionName = Pattern.ComboLoopSection;
	ComboFinishSectionName = Pattern.ComboFinishSection;
	ComboCyclesLeft = Pattern.ComboLoopCount - 1; // 첫 바퀴는 지금 재생 중
	ComboCycleIndex = 0;
	ComboRateStep = Pattern.ComboPlayRateStep;
	ComboMaxRate = Pattern.ComboMaxPlayRate;
	ComboFinishRate = Pattern.ComboFinishPlayRate;
	ComboRangeStep = Pattern.ComboRangeScaleStep;
	ComboRangeMax = FMath::Max(1.0f, Pattern.ComboRangeMaxScale);
	ComboLastPosition = 0.0f;
	bComboInFinish = false;

	// 바퀴마다 커지는 범위는 마지막 타격(마무리)을 뺀 나머지 전부에 적용. 반복 구간 타격(예: A·B)뿐 아니라
	// 마지막 바퀴에 Finish 섹션에서 이어지는 타격(예: C)도 그 바퀴 크기로 나감
	// (SetAttackHitScales가 먼저 호출되어 1.0/0으로 초기화된 뒤 여기서 설정)
	AttackHitGrowthScale = 1.0f;
	AttackHitGrowthHitCount = 0;
	if (ComboRangeStep > 0.0f)
	{
		AttackHitGrowthHitCount = FMath::Max(0, ComputeTelegraphWindows(Montage, FName("Notify_Hit"), 0.0f).Num() - 1);
	}

	// 반복이 남아 있으면 Loop 섹션이 끝날 때 다시 Loop로, 아니면 바로 Finish로
	AnimInstance->Montage_SetNextSection(ComboLoopSectionName, ComboCyclesLeft > 0 ? ComboLoopSectionName : ComboFinishSectionName, Montage);
}

void ARLEnemyCharacter::UpdateComboLoop()
{
	if (!ComboMontage)
	{
		return;
	}

	UAnimInstance* AnimInstance = GetMesh() ? GetMesh()->GetAnimInstance() : nullptr;
	if (!AnimInstance || !AnimInstance->Montage_IsPlaying(ComboMontage) || (HealthComp && HealthComp->bIsDead))
	{
		ComboMontage = nullptr;
		return;
	}

	const float Position = AnimInstance->Montage_GetPosition(ComboMontage);
	const FName Section = AnimInstance->Montage_GetCurrentSection(ComboMontage);

	// ★ Finish 섹션 안에 있는 타격(예: C)의 예고 데칼은 Finish 섹션에 들어간 뒤에만 뜨게 함. 안 그러면 표시 시작
	// 시각(히트 - IndicatorLeadTime)이 Loop 섹션 끝보다 앞이라, 반복 중 Loop 끝마다 C 데칼이 잠깐 떴다가 다음 바퀴
	// 1타 데칼로 바뀜. (예고 텔레그래프는 BTT_Attack이 이 패턴 시작 뒤에 걸어서 여기서 매 프레임 보정)
	if (TelegraphMontage == ComboMontage)
	{
		const float FinishStart = ComboMontage->GetAnimCompositeSection(ComboMontage->GetSectionIndex(ComboFinishSectionName)).GetTime();
		for (FVector2D& Window : TelegraphWindows)
		{
			if (Window.Y > FinishStart && Window.X < FinishStart)
			{
				Window.X = FinishStart;
			}
		}
	}

	if (Section == ComboFinishSectionName)
	{
		if (!bComboInFinish)
		{
			bComboInFinish = true;
			AnimInstance->Montage_SetPlayRate(ComboMontage, ComboFinishRate);
		}
	}
	else if (Section == ComboLoopSectionName && Position + KINDA_SMALL_NUMBER < ComboLastPosition)
	{
		// 위치가 뒤로 돌아감 = Loop 섹션 한 바퀴 완료 → 속도 올리고 남은 횟수 차감
		++ComboCycleIndex;
		--ComboCyclesLeft;
		AnimInstance->Montage_SetPlayRate(ComboMontage, FMath::Min(1.0f + ComboRateStep * ComboCycleIndex, ComboMaxRate));
		AnimInstance->Montage_SetNextSection(ComboLoopSectionName, ComboCyclesLeft > 0 ? ComboLoopSectionName : ComboFinishSectionName, ComboMontage);

		// 히트별 범위 배율(HitRangeScales)도 바퀴마다 처음부터 (예고 데칼은 몽타주 시간 기준 순번을 쓰므로 판정과 맞춤)
		AttackHitCounter = 0;
		AttackHitGrowthScale = FMath::Min(1.0f + ComboRangeStep * ComboCycleIndex, ComboRangeMax);

		// 타격마다 한 걸음: 새 바퀴의 타격에도 다시 적용
		NextStepIndex = 0;
		while (StepTimes.IsValidIndex(NextStepIndex) && StepTimes[NextStepIndex] < Position)
		{
			++NextStepIndex;
		}
	}

	ComboLastPosition = Position;
}

void ARLEnemyCharacter::UpdateAttackStep()
{
	if (!StepMontage)
	{
		return;
	}

	UAnimInstance* AnimInstance = GetMesh() ? GetMesh()->GetAnimInstance() : nullptr;
	if (!AnimInstance || !AnimInstance->Montage_IsPlaying(StepMontage) || (HealthComp && HealthComp->bIsDead))
	{
		StepMontage = nullptr;
		return;
	}

	// 내딛는 동안 떠 있는 Box 예고 데칼도 보스를 따라가게 해서 실제 판정 위치와 어긋나지 않게 함
	if (bTelegraphVisible && AttackIndicator && TelegraphShape == ETelegraphShape::Box)
	{
		AttackIndicator->UpdateBoxTransform(GetIndicatorOrigin(), TelegraphForwardOffset * TelegraphShownScale, GetActorRotation().Yaw);
	}

	if (!StepTimes.IsValidIndex(NextStepIndex) || AnimInstance->Montage_GetPosition(StepMontage) < StepTimes[NextStepIndex])
	{
		return;
	}
	++NextStepIndex;

	// 플레이어와 ChaseStopDistance보다 가까워지지 않게 걸음을 줄임 (이미 붙어 있으면 제자리)
	float Distance = StepDistance;
	if (const APawn* Player = UGameplayStatics::GetPlayerPawn(this, 0))
	{
		Distance = FMath::Min(Distance, FVector::Dist2D(Player->GetActorLocation(), GetActorLocation()) - ChaseStopDistance);
	}
	if (Distance > 1.0f)
	{
		// 방향은 현재 정면 (텔레그래프 회전 로직이 타격 사이에 플레이어 쪽으로 돌려놓음)
		const FVector StepDirection = GetActorForwardVector().GetSafeNormal2D();
		StartDashMovement(GetActorLocation() + StepDirection * Distance, StepDuration);

		// 내딛는 동안 AnimBP가 "이동 중"으로 보게 해서 하체는 이동 모션, 상체는 UpperBody 슬롯 공격으로 섞이게 함
		StepVelocity = StepDirection * (Distance / FMath::Max(StepDuration, KINDA_SMALL_NUMBER));
		StepEndWorldTime = GetWorld()->GetTimeSeconds() + StepDuration;
	}
}

void ARLEnemyCharacter::StopAttackChase()
{
	ChaseMontage = nullptr;
	ChaseSpeed = 0.0f;
	bChaseMovingThisFrame = false;
}

FVector ARLEnemyCharacter::GetVelocity() const
{
	if (bChaseMovingThisFrame)
	{
		return GetActorForwardVector().GetSafeNormal2D() * ChaseSpeed;
	}
	if (GetWorld() && GetWorld()->GetTimeSeconds() < StepEndWorldTime)
	{
		return StepVelocity;
	}
	return Super::GetVelocity();
}

// ARLCharacter::Tick은 몽타주 재생 중 CharacterMovement 속도를 매 프레임 0으로 만들기 때문에(공격 중 미끄러짐 방지),
// 여기서는 무브먼트 입력이 아니라 SetActorLocation(스윕)으로 직접 조금씩 옮깁니다. 방향은 현재 정면(텔레그래프
// 회전 로직이 히트 사이엔 대상 쪽으로 돌리고, 데칼이 뜨면 고정)을 그대로 따릅니다.
void ARLEnemyCharacter::UpdateAttackChase(float DeltaTime)
{
	if (!ChaseMontage)
	{
		return;
	}

	UAnimInstance* AnimInstance = GetMesh() ? GetMesh()->GetAnimInstance() : nullptr;
	if (!AnimInstance || !AnimInstance->Montage_IsPlaying(ChaseMontage) || (HealthComp && HealthComp->bIsDead))
	{
		StopAttackChase();
		return;
	}

	// 플레이어에게 충분히 붙었으면 더 전진하지 않음
	bChaseMovingThisFrame = false;
	if (const APawn* Player = UGameplayStatics::GetPlayerPawn(this, 0))
	{
		if (FVector::Dist2D(Player->GetActorLocation(), GetActorLocation()) <= ChaseStopDistance)
		{
			return;
		}
	}

	const FVector Forward = GetActorForwardVector().GetSafeNormal2D();
	SetActorLocation(GetActorLocation() + Forward * ChaseSpeed * DeltaTime, true);
	bChaseMovingThisFrame = true;

	// 보스가 움직였으니 떠 있는 Box 예고 데칼도 같이 옮겨서 실제 판정 위치와 어긋나지 않게 함
	if (bTelegraphVisible && AttackIndicator && TelegraphShape == ETelegraphShape::Box)
	{
		AttackIndicator->UpdateBoxTransform(GetIndicatorOrigin(), TelegraphForwardOffset * TelegraphShownScale, GetActorRotation().Yaw);
	}
}

// ==========================================
// ★ 다음 패턴 바로 잇기 (bChainNextAttack)
// ==========================================

void ARLEnemyCharacter::UpdateChainNextAttack()
{
	if (!ChainMontage)
	{
		return;
	}

	// "빗나갔을 때만 연결"인데 맞았으면 연결하지 않음 → 모션을 끝까지 재생하고 BT의 평소 대기로
	if (bChainOnlyOnMiss && AttackLandedCount > 0)
	{
		UE_LOG(LogTemp, Log, TEXT("[Chain] %s: '%s' 명중 → 연결 안 함 (모션 끝까지 재생)"), *GetName(), *GetNameSafe(ChainMontage));
		ChainMontage = nullptr;
		return;
	}

	UAnimInstance* AnimInstance = GetMesh() ? GetMesh()->GetAnimInstance() : nullptr;
	if (AnimInstance && AnimInstance->Montage_IsPlaying(ChainMontage))
	{
		// 캔슬 시점에 도달하면 남은 모션(착지 후 추스르기 등)을 끊고 곧바로 다음 패턴으로
		if (ChainCancelTime >= 0.0f && AnimInstance->Montage_GetPosition(ChainMontage) >= ChainCancelTime
			&& !(HealthComp && HealthComp->bIsDead) && !bSoulCrackAIStopped)
		{
			UE_LOG(LogTemp, Log, TEXT("[Chain] %s: '%s' %.2f초 지점에서 캔슬 → 다음 패턴으로 연결"), *GetName(), *GetNameSafe(ChainMontage), ChainCancelTime);
			UAnimMontage* CancelledMontage = ChainMontage;
			ChainMontage = nullptr;
			ChainPatternSerial = AttackPatternSerial;
			AnimInstance->Montage_Stop(0.25f, CancelledMontage);
			RestartAIForChain();
		}
		return;
	}

	UAnimMontage* FinishedMontage = ChainMontage;
	ChainMontage = nullptr;

	// 사망 / 균열 경직(BT 정지 중) / 다른 몽타주(경직 등)로 끊긴 경우는 잇지 않음
	// (끝난 몽타주가 블렌드 아웃 중이면 GetCurrentActiveMontage가 그 몽타주를 돌려주므로 정상 종료로 취급)
	const UAnimMontage* ActiveMontage = AnimInstance ? AnimInstance->GetCurrentActiveMontage() : nullptr;
	if ((HealthComp && HealthComp->bIsDead) || bSoulCrackAIStopped || (ActiveMontage && ActiveMontage != FinishedMontage))
	{
		return;
	}

	UE_LOG(LogTemp, Log, TEXT("[Chain] %s: '%s' 종료 → %.2f초 뒤 다음 패턴으로 연결"), *GetName(), *GetNameSafe(FinishedMontage), ChainDelay);
	ChainPatternSerial = AttackPatternSerial;
	if (ChainDelay > 0.0f)
	{
		GetWorldTimerManager().SetTimer(ChainTimerHandle, this, &ARLEnemyCharacter::RestartAIForChain, ChainDelay, false);
	}
	else
	{
		RestartAIForChain();
	}
}

void ARLEnemyCharacter::RestartAIForChain()
{
	// 기다리는 동안 죽었거나 균열 경직이 걸렸거나 이미 다음 공격을 시작했으면 취소
	// (몽타주 재생 여부로 판단하면 블렌드 아웃/다른 슬롯 몽타주에 걸려 오판할 수 있어서 공격 번호로 비교)
	if ((HealthComp && HealthComp->bIsDead) || bSoulCrackAIStopped || ChainPatternSerial != AttackPatternSerial)
	{
		UE_LOG(LogTemp, Log, TEXT("[Chain] %s: 연결 취소 (사망=%d, 균열경직=%d, 새 공격 시작=%d)"), *GetName(),
			HealthComp && HealthComp->bIsDead, bSoulCrackAIStopped, ChainPatternSerial != AttackPatternSerial);
		return;
	}

	// BT를 처음부터 다시 돌려 공격 후 Wait을 건너뛰고 곧바로 패턴 선택으로 감
	if (AAIController* AICon = Cast<AAIController>(GetController()))
	{
		if (UBrainComponent* Brain = AICon->GetBrainComponent())
		{
			Brain->RestartLogic();
			UE_LOG(LogTemp, Log, TEXT("[Chain] %s: BT 재시작 → 다음 패턴 선택"), *GetName());
		}
	}
}

// ==========================================
// ★ 반복 판정 (Pulse)
// ==========================================

void ARLEnemyCharacter::StartPulse(const FRLMonsterPatternData& Pattern)
{
	StopPulse();

	if (Pattern.PulseInterval <= 0.0f || !Pattern.AttackMontage || (HealthComp && HealthComp->bIsDead))
	{
		return;
	}

	const TArray<FVector2D> Starts = ComputeTelegraphWindows(Pattern.AttackMontage, FName("Notify_PulseStart"), 0.0f);
	const TArray<FVector2D> Ends = ComputeTelegraphWindows(Pattern.AttackMontage, FName("Notify_PulseEnd"), 0.0f);
	for (int32 Index = 0; Index < Starts.Num(); ++Index)
	{
		// 짝이 되는 End가 없으면 몽타주 끝까지
		const float EndTime = Ends.IsValidIndex(Index) ? Ends[Index].Y : Pattern.AttackMontage->GetPlayLength();
		PulseWindows.Add(FVector2D(Starts[Index].Y, EndTime));
	}

	if (PulseWindows.Num() == 0)
	{
		UE_LOG(LogTemp, Warning, TEXT("[Pulse] %s: 몽타주 '%s'에 Notify_PulseStart가 없어 반복 판정을 건너뜁니다."), *GetName(), *Pattern.AttackMontage->GetName());
		return;
	}

	PulseMontage = Pattern.AttackMontage;
	PulsePattern = Pattern;
	NextPulseTime = 0.0f;
}

void ARLEnemyCharacter::UpdatePulse()
{
	if (!PulseMontage)
	{
		return;
	}

	UAnimInstance* AnimInstance = GetMesh() ? GetMesh()->GetAnimInstance() : nullptr;
	if (!AnimInstance || !AnimInstance->Montage_IsPlaying(PulseMontage) || (HealthComp && HealthComp->bIsDead))
	{
		StopPulse();
		return;
	}

	const float Position = AnimInstance->Montage_GetPosition(PulseMontage);
	const FVector DecalSize = PulsePattern.IndicatorDecalSize;
	const bool bSector = PulsePattern.AttackShape == ERLAttackShape::Sector;

	bool bShowIndicator = false;
	bool bInPulseWindow = false;
	for (const FVector2D& Window : PulseWindows)
	{
		bShowIndicator |= Position >= Window.X - PulsePattern.IndicatorLeadTime && Position < Window.Y;
		bInPulseWindow |= Position >= Window.X && Position < Window.Y;

		if (Position >= Window.X && Position < Window.Y && Position >= NextPulseTime)
		{
			NextPulseTime = Position + PulsePattern.PulseInterval;
			if (bSector)
			{
				CreateSectorHitbox(DecalSize.Y, PulsePattern.SectorSafeZoneAngle, PulsePattern.DamageData, true, PulsePattern.SectorInnerSafeRadius);
			}
			else
			{
				CreateBoxHitbox(DecalSize, DecalSize.X, PulsePattern.DamageData, true);
			}
		}
	}

	// 예고 데칼: 구간 시작 IndicatorLeadTime초 전부터 구간 끝까지 계속 (보스가 움직이거나 돌아도 따라감)
	if (bShowIndicator && PulsePattern.bUseIndicator)
	{
		if (bSector)
		{
			ShowSectorAttackIndicator(DecalSize.Y, PulsePattern.SectorSafeZoneAngle, PulsePattern.IndicatorDecalMaterial, PulsePattern.SectorInnerSafeRadius);
		}
		else
		{
			ShowBoxAttackIndicator(DecalSize, DecalSize.X, PulsePattern.IndicatorDecalMaterial);
		}
		bPulseIndicatorShown = true;
	}
	else if (bPulseIndicatorShown)
	{
		HideAttackIndicator();
		bPulseIndicatorShown = false;
	}

	UpdatePulseDome(bSector && bInPulseWindow && PulsePattern.PulseDomeMaterial != nullptr, Position);
}

void ARLEnemyCharacter::UpdatePulseDome(bool bActive, float Position)
{
	if (!bActive)
	{
		if (PulseDome)
		{
			PulseDome->SetVisibility(false);
		}
		return;
	}

	if (!PulseDome)
	{
		UStaticMesh* SphereMesh = LoadObject<UStaticMesh>(nullptr, TEXT("/Engine/BasicShapes/Sphere.Sphere"));
		if (!SphereMesh)
		{
			return;
		}

		PulseDome = NewObject<UStaticMeshComponent>(this, TEXT("PulseDome"));
		PulseDome->SetStaticMesh(SphereMesh);
		PulseDome->SetCollisionEnabled(ECollisionEnabled::NoCollision);
		PulseDome->SetCastShadow(false);
		PulseDome->SetUsingAbsoluteRotation(true);
		PulseDome->SetUsingAbsoluteScale(true);
		PulseDome->SetupAttachment(GetRootComponent());
		PulseDome->RegisterComponent();
	}

	if (PulseDome->GetMaterial(0) != PulsePattern.PulseDomeMaterial)
	{
		PulseDome->SetMaterial(0, PulsePattern.PulseDomeMaterial);
	}

	// 구 중심을 발 위치에 두면 아래 절반은 바닥에 가려져 돔(반구)으로 보임. 엔진 기본 구는 반지름 50
	// 판정할 때마다 6% 부풀었다가 다음 판정 전까지 원래 크기로 줄어들어 맥동하는 느낌
	const float SincePulse = FMath::Clamp(1.0f - (NextPulseTime - Position) / FMath::Max(PulsePattern.PulseInterval, KINDA_SMALL_NUMBER), 0.0f, 1.0f);
	const float Beat = 1.0f + 0.06f * (1.0f - SincePulse);
	PulseDome->SetWorldLocation(GetIndicatorOrigin());
	PulseDome->SetWorldScale3D(FVector(PulsePattern.IndicatorDecalSize.Y / 50.0f * Beat));
	PulseDome->SetVisibility(true);
}

void ARLEnemyCharacter::StopPulse()
{
	if (bPulseIndicatorShown)
	{
		HideAttackIndicator();
		bPulseIndicatorShown = false;
	}

	if (PulseDome)
	{
		PulseDome->SetVisibility(false);
	}

	PulseMontage = nullptr;
	PulseWindows.Reset();
}

// ==========================================
// ★ 도끼 투척 패턴 (AxeLeap / AxeReturn / AxeDoubleReturn)
// ==========================================

bool ARLEnemyCharacter::BeginAxeLeap(const FRLMonsterPatternData& Pattern)
{
	EndAxeLeap();

	if (!Pattern.AttackMontage || (HealthComp && HealthComp->bIsDead))
	{
		return false;
	}

	// 각 노티파이의 발동 시각들 (같은 이름이 여러 번 있으면 시간순)
	auto FindNotifyTimes = [this, &Pattern](const TCHAR* NotifyName) -> TArray<float>
	{
		TArray<float> Times;
		for (const FVector2D& Window : ComputeTelegraphWindows(Pattern.AttackMontage, FName(NotifyName), 0.0f))
		{
			Times.Add(Window.Y);
		}
		return Times;
	};
	auto FindNotifyTime = [&FindNotifyTimes](const TCHAR* NotifyName) -> float
	{
		const TArray<float> Times = FindNotifyTimes(NotifyName);
		return Times.Num() > 0 ? Times[0] : -1.0f;
	};

	const bool bDouble = Pattern.AttackShape == ERLAttackShape::AxeDoubleReturn;
	const TArray<float> ThrowTimes = FindNotifyTimes(TEXT("Notify_AxeThrow"));
	const int32 NumAxes = bDouble ? 2 : 1;
	const float LastThrowTime = ThrowTimes.Num() >= NumAxes ? ThrowTimes[NumAxes - 1] : -1.0f;

	if (Pattern.AttackShape == ERLAttackShape::AxeLeap)
	{
		AxeLeapUpTime = FindNotifyTime(TEXT("Notify_LeapUp"));
		AxeLeapFallTime = FindNotifyTime(TEXT("Notify_LeapFall"));
		AxeLeapHitTime = FindNotifyTime(TEXT("Notify_Hit"));

		if (LastThrowTime < 0.0f || AxeLeapUpTime < LastThrowTime || AxeLeapFallTime <= AxeLeapUpTime || AxeLeapHitTime <= AxeLeapFallTime)
		{
			UE_LOG(LogTemp, Warning, TEXT("[AxeLeap] %s: 몽타주 '%s'에 Notify_AxeThrow(%.2f) → Notify_LeapUp(%.2f) → Notify_LeapFall(%.2f) → Notify_Hit(%.2f)가 순서대로 있어야 합니다."),
				*GetName(), *Pattern.AttackMontage->GetName(), LastThrowTime, AxeLeapUpTime, AxeLeapFallTime, AxeLeapHitTime);
			return false;
		}
	}
	else
	{
		AxeRecallTime = FindNotifyTime(TEXT("Notify_AxeRecall"));
		AxeCatchTime = FindNotifyTime(TEXT("Notify_AxeCatch"));

		if (LastThrowTime < 0.0f || AxeRecallTime <= LastThrowTime || AxeCatchTime <= AxeRecallTime)
		{
			UE_LOG(LogTemp, Warning, TEXT("[AxeReturn] %s: 몽타주 '%s'에 Notify_AxeThrow %d개(%d개 있음) → Notify_AxeRecall(%.2f) → Notify_AxeCatch(%.2f)가 순서대로 있어야 합니다."),
				*GetName(), *Pattern.AttackMontage->GetName(), NumAxes, ThrowTimes.Num(), AxeRecallTime, AxeCatchTime);
			return false;
		}
	}

	UClass* DefaultAxeClass = Pattern.ThrownAxeClass ? Pattern.ThrownAxeClass.Get() : ARLThrownAxe::StaticClass();
	AxeSlots.SetNum(NumAxes);
	ThrownAxes.Init(nullptr, NumAxes);

	AxeSlots[0].BoneName = Pattern.AxeWeaponBoneName;
	AxeSlots[0].AxeClass = DefaultAxeClass;
	AxeSlots[0].ThrowTime = ThrowTimes[0];
	if (bDouble)
	{
		// 오른손 도끼는 보스 시점 왼쪽, 왼손 도끼는 오른쪽에 박아서 회수 경로가 엇갈리게
		AxeSlots[0].SideSign = -1.0f;
		AxeSlots[1].BoneName = Pattern.SecondAxeWeaponBoneName;
		AxeSlots[1].AxeClass = Pattern.SecondThrownAxeClass ? Pattern.SecondThrownAxeClass.Get() : DefaultAxeClass;
		AxeSlots[1].ThrowTime = ThrowTimes[1];
		AxeSlots[1].SideSign = 1.0f;
	}

	const FVector StartGround = FindGroundPoint(GetActorLocation());
	for (FAxeSlot& Slot : AxeSlots)
	{
		Slot.TargetGround = StartGround;
	}

	AxeLeapMontage = Pattern.AttackMontage;
	AxeLeapPattern = Pattern;
	AxeLeapStep = EAxeLeapStep::Tracking;
	return true;
}

void ARLEnemyCharacter::UpdateAxeLeap(float DeltaTime)
{
	if (AxeLeapStep == EAxeLeapStep::None)
	{
		return;
	}

	UAnimInstance* AnimInstance = GetMesh() ? GetMesh()->GetAnimInstance() : nullptr;
	if (!AnimInstance || !AnimInstance->Montage_IsPlaying(AxeLeapMontage) || (HealthComp && HealthComp->bIsDead))
	{
		// 균열 경직/사망 등으로 몽타주가 끊김
		EndAxeLeap();
		return;
	}

	const float Position = AnimInstance->Montage_GetPosition(AxeLeapMontage);

	if (AxeLeapStep == EAxeLeapStep::Tracking)
	{
		UpdateAxeTracking(Position, DeltaTime);
	}

	if (IsAxeReturnPattern())
	{
		if (AxeLeapStep == EAxeLeapStep::Thrown && Position >= AxeRecallTime)
		{
			AxeReturnStartRecall();
		}

		if (AxeLeapStep == EAxeLeapStep::Recalling)
		{
			UpdateAxeReturn(Position);
		}
		return;
	}

	// ---- AxeLeap 전용 (도끼 1자루 = 슬롯 0) ----
	const FVector LeapTarget = AxeSlots[0].TargetGround;

	// 착지(Notify_Hit) IndicatorLeadTime초 전부터 착지 범위 예고 데칼 표시
	if (AxeLeapStep != EAxeLeapStep::Tracking && AxeLeapPattern.bUseIndicator && Position >= AxeLeapHitTime - AxeLeapPattern.IndicatorLeadTime)
	{
		ShowAxeCircle(0, LeapTarget, AxeLeapPattern.IndicatorDecalSize.Y * AxeLeapPattern.LeapRadiusScale);
	}

	if (AxeLeapStep == EAxeLeapStep::Thrown && Position >= AxeLeapUpTime)
	{
		AxeLeapStartRise();
	}

	if (AxeLeapStep == EAxeLeapStep::Rising)
	{
		const float RiseEndTime = FMath::Lerp(AxeLeapUpTime, AxeLeapFallTime, AxeLeapPattern.LeapRiseRatio);
		const float Alpha = FMath::Clamp((Position - AxeLeapUpTime) / FMath::Max(RiseEndTime - AxeLeapUpTime, KINDA_SMALL_NUMBER), 0.0f, 1.0f);
		SetActorLocation(AxeLeapRiseStart + FVector(0.0f, 0.0f, AxeLeapPattern.LeapHeight * FMath::InterpEaseOut(0.0f, 1.0f, Alpha, 2.0f)));

		if (Alpha >= 1.0f && GetMesh()->IsVisible())
		{
			GetMesh()->SetVisibility(false);
			OnAxeLeapVanished();
		}

		if (Position >= AxeLeapFallTime)
		{
			AxeLeapStartFall();
		}
	}

	if (AxeLeapStep == EAxeLeapStep::Falling)
	{
		const FVector LandLocation = LeapTarget + FVector(0.0f, 0.0f, GetCapsuleComponent()->GetScaledCapsuleHalfHeight());
		const float Alpha = FMath::Clamp((Position - AxeLeapFallTime) / (AxeLeapHitTime - AxeLeapFallTime), 0.0f, 1.0f);
		SetActorLocation(LandLocation + FVector(0.0f, 0.0f, AxeLeapPattern.LeapHeight * (1.0f - FMath::InterpEaseIn(0.0f, 1.0f, Alpha, 2.0f))));

		if (Position >= AxeLeapHitTime)
		{
			AxeLeapImpact();
		}
	}
}

void ARLEnemyCharacter::UpdateAxeTracking(float Position, float DeltaTime)
{
	// 아직 안 던진 도끼의 데칼이 플레이어 발밑(쌍도끼는 좌우로 벌린 자리)을 따라다니고, 보스는 플레이어 쪽으로 회전
	if (const APawn* Player = UGameplayStatics::GetPlayerPawn(this, 0))
	{
		const FVector PlayerGround = FindGroundPoint(Player->GetActorLocation());
		const FVector ToPlayer = (PlayerGround - GetActorLocation()).GetSafeNormal2D();
		const FVector Right = FVector::CrossProduct(FVector::UpVector, ToPlayer);

		for (FAxeSlot& Slot : AxeSlots)
		{
			if (!Slot.bThrown)
			{
				const FVector Offset = Right * Slot.SideSign * AxeLeapPattern.AxeDoubleSideOffset;
				Slot.TargetGround = Slot.SideSign != 0.0f ? FindGroundPoint(PlayerGround + Offset) : PlayerGround;
			}
		}

		const FRotator NewRotation = FMath::RInterpConstantTo(GetActorRotation(), FRotator(0.0f, ToPlayer.Rotation().Yaw, 0.0f), DeltaTime, AxeLeapPattern.AxeLeapTurnSpeed);
		SetActorRotation(NewRotation);
	}

	bool bAllThrown = true;
	for (int32 SlotIndex = 0; SlotIndex < AxeSlots.Num(); ++SlotIndex)
	{
		FAxeSlot& Slot = AxeSlots[SlotIndex];
		if (!Slot.bThrown && Position >= Slot.ThrowTime)
		{
			ThrowAxeSlot(SlotIndex);
		}
		else if (!Slot.bThrown && AxeLeapPattern.bUseIndicator)
		{
			ShowAxeCircle(SlotIndex, Slot.TargetGround, AxeLeapPattern.IndicatorDecalSize.Y);
		}
		bAllThrown &= Slot.bThrown;
	}

	if (bAllThrown)
	{
		AxeLeapStep = EAxeLeapStep::Thrown;
	}
}

void ARLEnemyCharacter::ThrowAxeSlot(int32 SlotIndex)
{
	FAxeSlot& Slot = AxeSlots[SlotIndex];
	Slot.bThrown = true;

	// 데칼은 이 자리에 고정된 채로 도끼가 박힐 때까지 유지
	if (AxeLeapPattern.bUseIndicator)
	{
		ShowAxeCircle(SlotIndex, Slot.TargetGround, AxeLeapPattern.IndicatorDecalSize.Y);
	}

	if (GetMesh()->GetBoneIndex(Slot.BoneName) == INDEX_NONE)
	{
		UE_LOG(LogTemp, Warning, TEXT("[AxeLeap] %s: 도끼 본 '%s'을(를) 찾을 수 없어 도끼 없이 진행합니다."), *GetName(), *Slot.BoneName.ToString());
		OnAxeLeapThrown(Slot.TargetGround);
		return;
	}

	FActorSpawnParameters Params;
	Params.Owner = this;
	Params.SpawnCollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::AlwaysSpawn;
	const FVector SpawnLocation = GetMesh()->GetSocketLocation(Slot.BoneName);
	ARLThrownAxe* Axe = GetWorld()->SpawnActor<ARLThrownAxe>(Slot.AxeClass, SpawnLocation, FRotator::ZeroRotator, Params);
	if (Axe && Axe->InitFromWeapon(GetMesh(), Slot.BoneName))
	{
		// AxeFlightTime = 사거리(AttackRange) 끝까지 날아갈 때의 시간. 가까우면 그만큼 빨리 박힘 (일정한 속도)
		const float DistanceRatio = FVector::Dist2D(SpawnLocation, Slot.TargetGround) / FMath::Max(AxeLeapPattern.AttackRange, 1.0f);
		const float FlightTime = AxeLeapPattern.AxeFlightTime * FMath::Clamp(DistanceRatio, 0.0f, 1.0f);

		Axe->OnLanded.AddUObject(this, &ARLEnemyCharacter::HandleThrownAxeLanded);
		Axe->Launch(Slot.TargetGround, FlightTime, AxeLeapPattern.AxeArcHeight);
	}
	ThrownAxes[SlotIndex] = Axe;

	GetMesh()->HideBoneByName(Slot.BoneName, PBO_None);
	Slot.bBoneHidden = true;

	OnAxeLeapThrown(Slot.TargetGround);
}

void ARLEnemyCharacter::AxeLeapStartRise()
{
	AxeLeapStep = EAxeLeapStep::Rising;
	AxeLeapRiseStart = GetActorLocation();
	bAxeLeapInAir = true;

	// 공중에서는 중력/플레이어와의 충돌을 끄고 위치를 직접 제어
	GetCharacterMovement()->SetMovementMode(MOVE_Flying);
	SavedPawnCollisionResponse = GetCapsuleComponent()->GetCollisionResponseToChannel(ECC_Pawn);
	GetCapsuleComponent()->SetCollisionResponseToChannel(ECC_Pawn, ECR_Ignore);
}

void ARLEnemyCharacter::AxeLeapStartFall()
{
	AxeLeapStep = EAxeLeapStep::Falling;
	const FVector LeapTarget = AxeSlots[0].TargetGround;

	// 도약한 방향 그대로 데칼 위 상공으로 순간이동
	const FVector LeapDirection = (LeapTarget - AxeLeapRiseStart).GetSafeNormal2D();
	if (!LeapDirection.IsNearlyZero())
	{
		SetActorRotation(FRotator(0.0f, LeapDirection.Rotation().Yaw, 0.0f));
	}

	const FVector LandLocation = LeapTarget + FVector(0.0f, 0.0f, GetCapsuleComponent()->GetScaledCapsuleHalfHeight());
	SetActorLocation(LandLocation + FVector(0.0f, 0.0f, AxeLeapPattern.LeapHeight), false, nullptr, ETeleportType::TeleportPhysics);
	GetMesh()->SetVisibility(true);
}

void ARLEnemyCharacter::AxeLeapImpact()
{
	const FVector LeapTarget = AxeSlots[0].TargetGround;
	const FVector LandLocation = LeapTarget + FVector(0.0f, 0.0f, GetCapsuleComponent()->GetScaledCapsuleHalfHeight());
	SetActorLocation(LandLocation, false, nullptr, ETeleportType::TeleportPhysics);
	bAxeLeapInAir = false;

	CreateLocationHitbox(LeapTarget, AxeLeapPattern.IndicatorDecalSize.Y * AxeLeapPattern.LeapRadiusScale, AxeLeapPattern.DamageData, true);
	OnAxeLeapLanded(LeapTarget);

	EndAxeLeap();
}

void ARLEnemyCharacter::HandleThrownAxeLanded(ARLThrownAxe* Axe)
{
	const int32 SlotIndex = ThrownAxes.IndexOfByKey(Axe);
	if (SlotIndex == INDEX_NONE || AxeLeapStep == EAxeLeapStep::None)
	{
		return;
	}

	CreateLocationHitbox(AxeSlots[SlotIndex].TargetGround, AxeLeapPattern.IndicatorDecalSize.Y, AxeLeapPattern.AxeDamageData, true);

	// 회수 패턴: 도끼 범위 데칼을 끄고, 회수 예고 데칼은 Notify_AxeRecall에서 띄움
	if (IsAxeReturnPattern())
	{
		HideAxeIndicator(SlotIndex);
		return;
	}

	// AxeLeap: 도끼 범위 데칼은 여기서 끄고, 착지 범위 데칼은 UpdateAxeLeap이 착지 IndicatorLeadTime초 전에 띄움
	// (이미 착지 예고 구간에 들어왔으면 끄지 않음)
	UAnimInstance* AnimInstance = GetMesh() ? GetMesh()->GetAnimInstance() : nullptr;
	const float Position = AnimInstance ? AnimInstance->Montage_GetPosition(AxeLeapMontage) : 0.0f;
	if (Position < AxeLeapHitTime - AxeLeapPattern.IndicatorLeadTime)
	{
		HideAxeIndicator(SlotIndex);
	}
}

void ARLEnemyCharacter::EndAxeLeap()
{
	if (AxeLeapStep == EAxeLeapStep::None)
	{
		return;
	}

	// 공중에서 끊겼으면 땅으로 되돌림 (낙하 중이면 착지 지점, 상승 중이면 출발 지점)
	if (bAxeLeapInAir)
	{
		const FVector Ground = AxeLeapStep == EAxeLeapStep::Falling ? AxeSlots[0].TargetGround : FindGroundPoint(AxeLeapRiseStart);
		SetActorLocation(Ground + FVector(0.0f, 0.0f, GetCapsuleComponent()->GetScaledCapsuleHalfHeight()), false, nullptr, ETeleportType::TeleportPhysics);
		bAxeLeapInAir = false;
	}

	if (AxeLeapStep == EAxeLeapStep::Rising || AxeLeapStep == EAxeLeapStep::Falling)
	{
		GetCapsuleComponent()->SetCollisionResponseToChannel(ECC_Pawn, SavedPawnCollisionResponse);
		if (!(HealthComp && HealthComp->bIsDead))
		{
			GetCharacterMovement()->SetMovementMode(MOVE_Walking);
		}
	}

	GetMesh()->SetVisibility(true);

	for (const FAxeSlot& Slot : AxeSlots)
	{
		if (Slot.bBoneHidden)
		{
			GetMesh()->UnHideBoneByName(Slot.BoneName);
		}
	}

	for (ARLThrownAxe* Axe : ThrownAxes)
	{
		if (Axe)
		{
			Axe->Destroy();
		}
	}

	HideAxeIndicators();

	AxeSlots.Reset();
	ThrownAxes.Reset();
	AxeLeapStep = EAxeLeapStep::None;
	AxeLeapMontage = nullptr;
}

// ==========================================
// ★ 도끼 회수 (AxeReturn / AxeDoubleReturn)
// ==========================================

// 곡선 회수 경로 예고 데칼을 몇 조각의 직사각형으로 나눠 그릴지
static constexpr int32 AxeLaneSegments = 6;

void ARLEnemyCharacter::AxeReturnStartRecall()
{
	AxeLeapStep = EAxeLeapStep::Recalling;

	for (int32 SlotIndex = 0; SlotIndex < AxeSlots.Num(); ++SlotIndex)
	{
		FAxeSlot& Slot = AxeSlots[SlotIndex];
		ARLThrownAxe* Axe = ThrownAxes[SlotIndex];
		Slot.HitActors.Reset();

		const FVector AxeLocation = Axe ? Axe->GetActorLocation() : Slot.TargetGround;
		const FVector HandLocation = GetMesh()->GetSocketLocation(Slot.BoneName);

		// 플레이어가 도끼와 보스 사이(도끼→보스 선분의 양 끝 15% 안쪽 + 옆으로 AxeReturnCurveMaxOffset 이내)에 있으면
		// 지금 플레이어 위치를 거쳐 휘어서 돌아옴. 예고가 뜬 순간의 위치를 지나므로 보고 빠져나가면 피할 수 있음
		Slot.bCurved = false;
		const APawn* Player = AxeLeapPattern.bAxeReturnCurveThroughPlayer ? UGameplayStatics::GetPlayerPawn(this, 0) : nullptr;
		if (Player)
		{
			const FVector AxeToBoss = (GetActorLocation() - Slot.TargetGround) * FVector(1.0f, 1.0f, 0.0f);
			const FVector AxeToPlayer = (Player->GetActorLocation() - Slot.TargetGround) * FVector(1.0f, 1.0f, 0.0f);
			const float LengthSquared = AxeToBoss.SizeSquared();
			if (LengthSquared > 1.0f)
			{
				const float T = FVector::DotProduct(AxeToPlayer, AxeToBoss) / LengthSquared;
				const float Lateral = (AxeToPlayer - AxeToBoss * T).Size();
				if (T > 0.15f && T < 0.85f && Lateral <= AxeLeapPattern.AxeReturnCurveMaxOffset)
				{
					Slot.bCurved = true;
					Slot.CurvePassThrough = Player->GetActorLocation();
				}
			}
		}

		// 던질 때와 같은 속도로 돌아오도록 이동 시간을 구하고(곡선이면 꺾인 경로 길이), 받는 순간(Notify_AxeCatch)에 정확히
		// 손에 닿도록 출발 시각을 역산. 그 전까지는 박힌 채로 흔들리면서 예고 데칼만 보여줌
		const float PathLength = Slot.bCurved
			? FVector::Dist2D(AxeLocation, Slot.CurvePassThrough) + FVector::Dist2D(Slot.CurvePassThrough, HandLocation)
			: FVector::Dist2D(AxeLocation, HandLocation);
		const float MoveTime = AxeLeapPattern.AxeFlightTime * FMath::Clamp(PathLength / FMath::Max(AxeLeapPattern.AttackRange, 1.0f), 0.0f, 1.0f);
		Slot.MoveTime = FMath::Clamp(AxeCatchTime - MoveTime, AxeRecallTime, AxeCatchTime);

		if (Axe)
		{
			Axe->StartWobble();
		}

		// 회수 예고: 도끼 주변 원형(뽑힐 때 피해 범위) + 도끼 → 보스 경로(돌아오는 경로 피해 범위)
		if (AxeLeapPattern.bUseIndicator)
		{
			ShowAxeCircle(SlotIndex, Slot.TargetGround, AxeLeapPattern.IndicatorDecalSize.Y);
		}

		const int32 LaneBase = AxeSlots.Num() + SlotIndex * AxeLaneSegments;
		const FVector BossGround(GetActorLocation().X, GetActorLocation().Y, Slot.TargetGround.Z);
		if (Slot.bCurved)
		{
			// 곡선은 직사각형 여러 조각으로 그림. 꺾이는 바깥쪽에 틈이 생기지 않게 조각을 앞뒤로 조금씩 늘림
			const FVector PassGround(Slot.CurvePassThrough.X, Slot.CurvePassThrough.Y, Slot.TargetGround.Z);
			for (int32 Segment = 0; Segment < AxeLaneSegments; ++Segment)
			{
				FVector From = ARLThrownAxe::EvaluateCurve(Slot.TargetGround, PassGround, BossGround, static_cast<float>(Segment) / AxeLaneSegments);
				FVector To = ARLThrownAxe::EvaluateCurve(Slot.TargetGround, PassGround, BossGround, static_cast<float>(Segment + 1) / AxeLaneSegments);
				const FVector Overlap = (To - From).GetSafeNormal2D() * AxeLeapPattern.AxeReturnHitRadius * 0.5f;
				ShowAxeLane(LaneBase + Segment, From - Overlap, To + Overlap, AxeLeapPattern.AxeReturnHitRadius);
			}
		}
		else
		{
			ShowAxeLane(LaneBase, Slot.TargetGround, BossGround, AxeLeapPattern.AxeReturnHitRadius);
		}
	}

	OnAxeRecalled(AxeSlots[0].TargetGround);
}

void ARLEnemyCharacter::UpdateAxeReturn(float Position)
{
	for (int32 SlotIndex = 0; SlotIndex < AxeSlots.Num(); ++SlotIndex)
	{
		FAxeSlot& Slot = AxeSlots[SlotIndex];
		ARLThrownAxe* Axe = ThrownAxes[SlotIndex];
		if (!Axe || Position < Slot.MoveTime)
		{
			continue;
		}

		if (!Slot.bReturning)
		{
			// 흔들림을 멈추고 뽑혀 나오는 순간 도끼 주변 원형 범위 피해 (이후 경로 피해와 합쳐 이 도끼당 대상 1회)
			Slot.bReturning = true;
			Axe->BeginReturn();
			Slot.PrevLocation = Axe->GetActorLocation();
			ApplyAxeReturnDamage(Slot, Slot.TargetGround, Slot.TargetGround + FVector(0.0f, 0.0f, 1.0f), AxeLeapPattern.IndicatorDecalSize.Y);
		}

		const float Alpha = (Position - Slot.MoveTime) / FMath::Max(AxeCatchTime - Slot.MoveTime, KINDA_SMALL_NUMBER);
		Axe->UpdateReturn(Alpha, GetMesh()->GetSocketLocation(Slot.BoneName), Slot.bCurved, Slot.CurvePassThrough);

		// 지난 프레임 위치 → 현재 위치를 구체로 스윕해서 경로에 닿은 대상에게 피해
		const FVector CurrentLocation = Axe->GetActorLocation();
		ApplyAxeReturnDamage(Slot, Slot.PrevLocation, CurrentLocation, AxeLeapPattern.AxeReturnHitRadius);
		Slot.PrevLocation = CurrentLocation;
	}

	if (Position >= AxeCatchTime)
	{
		OnAxeCaught();
		EndAxeLeap();
	}
}

void ARLEnemyCharacter::ApplyAxeReturnDamage(FAxeSlot& Slot, const FVector& Start, const FVector& End, float Radius)
{
	TArray<FHitResult> Hits;
	FCollisionQueryParams Params(SCENE_QUERY_STAT(AxeReturnSweep), false, this);
	GetWorld()->SweepMultiByObjectType(Hits, Start, End, FQuat::Identity,
		FCollisionObjectQueryParams(ECC_Pawn), FCollisionShape::MakeSphere(Radius), Params);

	for (const FHitResult& Hit : Hits)
	{
		ARLCharacter* Target = Cast<ARLCharacter>(Hit.GetActor());
		if (!Target || Slot.HitActors.Contains(Target))
		{
			continue;
		}
		Slot.HitActors.Add(Target);

		FRLDamageData FinalDamage = AxeLeapPattern.AxeReturnDamageData;
		FinalDamage.DamageCauser = this;
		Target->ReceiveCustomDamage(FinalDamage, Hit);
	}
}

// ==========================================
// ★ 도끼 패턴 예고 데칼 풀
// ==========================================

ARLDecalIndicator* ARLEnemyCharacter::GetAxeIndicator(int32 Index)
{
	if (!AxeIndicators.IsValidIndex(Index))
	{
		AxeIndicators.SetNum(Index + 1);
	}

	if (!AxeIndicators[Index])
	{
		FActorSpawnParameters Params;
		Params.Owner = this;
		Params.SpawnCollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::AlwaysSpawn;
		AxeIndicators[Index] = GetWorld()->SpawnActor<ARLDecalIndicator>(ARLDecalIndicator::StaticClass(), FVector::ZeroVector, FRotator::ZeroRotator, Params);
	}
	return AxeIndicators[Index];
}

void ARLEnemyCharacter::ShowAxeCircle(int32 Index, const FVector& Location, float Radius)
{
	if (ARLDecalIndicator* Indicator = GetAxeIndicator(Index))
	{
		Indicator->SetupIndicator(EIndicatorType::Circle, Radius, 0.0f, AxeLeapPattern.IndicatorDecalMaterial);
		Indicator->UpdateIndicatorTransform(Location, Location, 0.0f);
		Indicator->SetIndicatorVisibility(true);
	}
}

void ARLEnemyCharacter::ShowAxeLane(int32 Index, const FVector& From, const FVector& To, float HalfWidth)
{
	const FVector Delta = (To - From) * FVector(1.0f, 1.0f, 0.0f);
	const float HalfLength = Delta.Size() * 0.5f;
	if (!AxeLeapPattern.AxeReturnLaneMaterial || HalfLength < 1.0f)
	{
		return;
	}

	if (ARLDecalIndicator* Indicator = GetAxeIndicator(Index))
	{
		Indicator->SetupBoxIndicator(FVector(HalfLength, HalfWidth, 0.0f), AxeLeapPattern.AxeReturnLaneMaterial);
		// 데칼 투사 높이를 바닥 기준으로 (캡슐 중심 기준이면 큰 보스는 바닥까지 안 닿을 수 있음)
		Indicator->UpdateBoxTransform(From, HalfLength, Delta.Rotation().Yaw);
		Indicator->SetIndicatorVisibility(true);
	}
}

void ARLEnemyCharacter::HideAxeIndicator(int32 Index)
{
	if (AxeIndicators.IsValidIndex(Index) && AxeIndicators[Index])
	{
		AxeIndicators[Index]->SetIndicatorVisibility(false);
	}
}

void ARLEnemyCharacter::HideAxeIndicators()
{
	for (int32 Index = 0; Index < AxeIndicators.Num(); ++Index)
	{
		HideAxeIndicator(Index);
	}
}

void ARLEnemyCharacter::EndPlay(const EEndPlayReason::Type EndPlayReason)
{
	for (ARLDecalIndicator* Indicator : AxeIndicators)
	{
		if (Indicator)
		{
			Indicator->Destroy();
		}
	}
	AxeIndicators.Reset();
	EndVanish();

	Super::EndPlay(EndPlayReason);
}

// ==========================================
// ★ 몽타주 좌우 반전 (bMirrorAnimation / Notify_MirrorStart ~ Notify_MirrorEnd)
// ==========================================

void ARLEnemyCharacter::StartMirrorAnimation(UAnimMontage* Montage)
{
	StopMirrorAnimation();
	if (!Montage || !SetMirrorPoseVariable(true))
	{
		return;
	}

	MirrorMontage = Montage;
	bMirrorWholeMontage = true;
	bMeshMirrored = true;
}

bool ARLEnemyCharacter::StartMirrorWindows(UAnimMontage* Montage)
{
	StopMirrorAnimation();
	if (!Montage)
	{
		return false;
	}

	const TArray<FVector2D> Starts = ComputeTelegraphWindows(Montage, FName("Notify_MirrorStart"), 0.0f);
	const TArray<FVector2D> Ends = ComputeTelegraphWindows(Montage, FName("Notify_MirrorEnd"), 0.0f);
	for (int32 Index = 0; Index < Starts.Num(); ++Index)
	{
		// 짝이 되는 End가 없으면 몽타주 끝까지 반전
		const float StartTime = Starts[Index].Y;
		const float EndTime = Ends.IsValidIndex(Index) ? Ends[Index].Y : Montage->GetPlayLength();
		MirrorWindows.Add(FVector2D(StartTime, EndTime));
	}

	if (MirrorWindows.Num() == 0)
	{
		return false;
	}

	MirrorMontage = Montage;
	bMirrorWholeMontage = false;
	return true;
}

void ARLEnemyCharacter::StopMirrorAnimation()
{
	if (bMeshMirrored)
	{
		SetMirrorPoseVariable(false);
	}

	MirrorMontage = nullptr;
	bMirrorWholeMontage = false;
	MirrorWindows.Reset();
	bMeshMirrored = false;
}

bool ARLEnemyCharacter::SetMirrorPoseVariable(bool bMirror)
{
	// AnimBP마다 변수 이름만 맞추면 되도록 C++ 쪽에서는 이름으로 찾아서 씀 (AnimBP에 캐스트/이벤트 그래프 불필요)
	UAnimInstance* AnimInstance = GetMesh() ? GetMesh()->GetAnimInstance() : nullptr;
	FBoolProperty* MirrorProperty = AnimInstance ? FindFProperty<FBoolProperty>(AnimInstance->GetClass(), MirrorPoseVariableName) : nullptr;
	if (!MirrorProperty)
	{
		UE_LOG(LogTemp, Warning, TEXT("[Mirror] %s: AnimBP에 bool 변수 '%s'가 없어 좌우 반전을 건너뜁니다."), *GetName(), *MirrorPoseVariableName.ToString());
		return false;
	}

	MirrorProperty->SetPropertyValue_InContainer(AnimInstance, bMirror);
	return true;
}

void ARLEnemyCharacter::UpdateMirrorAnimation()
{
	if (!MirrorMontage)
	{
		return;
	}

	UAnimInstance* AnimInstance = GetMesh() ? GetMesh()->GetAnimInstance() : nullptr;
	if (!AnimInstance || !AnimInstance->Montage_IsPlaying(MirrorMontage))
	{
		StopMirrorAnimation();
		return;
	}

	if (bMirrorWholeMontage)
	{
		return;
	}

	// 반전 구간 안에 들어오거나 나갈 때만 AnimBP 변수를 바꿈 (Mirror 노드의 Blend Time으로 부드럽게 전환)
	const float Position = AnimInstance->Montage_GetPosition(MirrorMontage);
	bool bShouldMirror = false;
	for (const FVector2D& Window : MirrorWindows)
	{
		bShouldMirror |= Position >= Window.X && Position < Window.Y;
	}

	if (bShouldMirror != bMeshMirrored && SetMirrorPoseVariable(bShouldMirror))
	{
		bMeshMirrored = bShouldMirror;
	}
}

FVector ARLEnemyCharacter::FindGroundPoint(const FVector& Location) const
{
	// ECC_Ground는 Pawn이 무시하는 바닥 전용 채널이라 플레이어/보스 캡슐에 막히지 않음
	FHitResult Hit;
	FCollisionQueryParams Params(SCENE_QUERY_STAT(AxeLeapGround), false, this);
	if (GetWorld()->LineTraceSingleByChannel(Hit, Location + FVector(0.0f, 0.0f, 100.0f), Location - FVector(0.0f, 0.0f, 3000.0f), ECC_Ground, Params))
	{
		return Hit.ImpactPoint;
	}
	return Location;
}

bool ARLEnemyCharacter::BeginSoulCrackPattern(const FRLMonsterPatternData& Pattern, FName HitNotifyName)
{
	if (!Pattern.bSoulCrackVariant || !Pattern.AttackMontage || (HealthComp && HealthComp->bIsDead))
	{
		return false;
	}

	// 텔레그래프와 같은 노티파이 스캔을 재사용해서 각 히트의 [히트 - LeadTime, 히트) 구간을 구하고, 그중 하나만 균열 구간으로 씀
	const TArray<FVector2D> Windows = ComputeTelegraphWindows(Pattern.AttackMontage, HitNotifyName, Pattern.SoulCrackWindowLeadTime);
	if (Windows.Num() == 0)
	{
		UE_LOG(LogTemp, Warning, TEXT("[SoulCrack] %s: 몽타주 %s에서 히트 노티파이 '%s'를 찾지 못해 균열 구간을 열 수 없습니다."),
			*GetName(), *Pattern.AttackMontage->GetName(), *HitNotifyName.ToString());
		return false;
	}

	EndSoulCrackPattern();

	const int32 Index = Pattern.SoulCrackHitIndex < 0 ? Windows.Num() - 1 : FMath::Min(Pattern.SoulCrackHitIndex, Windows.Num() - 1);
	SoulCrackMontage = Pattern.AttackMontage;
	SoulCrackWindow = Windows[Index];

	// ★ 잠행 강습 강화판: 사라지기 전/숨어 있는 동안/솟아오르는(무적) 동안에는 균열 불가 → 다 나타난 뒤부터 타격 전까지만 열림
	// (StartVanish가 이 함수보다 먼저 호출되어 VanishNotifyTime이 정해져 있음. 몽타주는 사라짐 지점에서 멈췄다가 이어지므로
	// 나타난 뒤 VanishFadeInTime만큼 진행된 몽타주 시각부터)
	if (VanishStep != EVanishStep::None && VanishMontage == Pattern.AttackMontage && SoulCrackWindow.Y > VanishNotifyTime)
	{
		SoulCrackWindow.X = FMath::Max(SoulCrackWindow.X, VanishNotifyTime + VanishPattern.VanishFadeInTime + 0.02f);
	}

	SoulCrackFrontAngle = Pattern.SoulCrackFrontAngle;
	SoulCrackBackAngle = Pattern.SoulCrackBackAngle;
	bSoulCrackConsumed = false;

	ShowSoulCrackZones();
	OnSoulCrackVariantStarted();
	return true;
}

void ARLEnemyCharacter::EndSoulCrackPattern()
{
	if (!SoulCrackMontage)
	{
		return;
	}

	SoulCrackMontage = nullptr;
	SoulCrackWindow = FVector2D::ZeroVector;
	bSoulCrackConsumed = false;
	bSoulCrackZonesHiddenByVanish = false;

	HideSoulCrackZones();
	SetSoulCrackWindowVisual(false);
	OnSoulCrackVariantEnded();
}

bool ARLEnemyCharacter::IsSoulCrackWindowOpen() const
{
	if (!SoulCrackMontage || bSoulCrackConsumed)
	{
		return false;
	}

	const UAnimInstance* AnimInstance = GetMesh() ? GetMesh()->GetAnimInstance() : nullptr;
	if (!AnimInstance || !AnimInstance->Montage_IsPlaying(SoulCrackMontage))
	{
		return false;
	}

	const float Position = AnimInstance->Montage_GetPosition(SoulCrackMontage);
	return Position >= SoulCrackWindow.X && Position < SoulCrackWindow.Y;
}

// ★ CreateSectorHitbox와 같은 각도 계산 (보스 forward vs 공격자 방향). 0도 = 정면, 180도 = 정후방
bool ARLEnemyCharacter::IsInSoulCrackZone(const AActor* Attacker) const
{
	if (!Attacker)
	{
		return false;
	}

	const FVector ToAttacker2D = (Attacker->GetActorLocation() - GetActorLocation()).GetSafeNormal2D();
	if (ToAttacker2D.IsNearlyZero())
	{
		return true;
	}

	const FVector ForwardDir = GetActorForwardVector().GetSafeNormal2D();
	const float AngleFromForward = FMath::RadiansToDegrees(FMath::Acos(FMath::Clamp(FVector::DotProduct(ForwardDir, ToAttacker2D), -1.0f, 1.0f)));

	const bool bInFront = SoulCrackFrontAngle > 0.0f && AngleFromForward <= SoulCrackFrontAngle * 0.5f;
	const bool bInBack = SoulCrackBackAngle > 0.0f && (180.0f - AngleFromForward) <= SoulCrackBackAngle * 0.5f;
	return bInFront || bInBack;
}

void ARLEnemyCharacter::ReceiveCustomDamage(const FRLDamageData& DamageData, const FHitResult& HitInfo)
{
	const bool bWasDead = HealthComp && HealthComp->bIsDead;

	Super::ReceiveCustomDamage(DamageData, HitInfo);

	if (bWasDead || !DamageData.bCanTriggerSoulCrack || (HealthComp && HealthComp->bIsDead))
	{
		return;
	}

	// Super에서 무시된 피격(같은 팀 / 무적)은 균열도 무시
	if (const ARLCharacter* CauserCharacter = Cast<ARLCharacter>(DamageData.DamageCauser))
	{
		if (CauserCharacter->TeamID == TeamID)
		{
			return;
		}
	}
	if (IsInvulnerable() && !DamageData.bIgnoreInvincibility)
	{
		return;
	}

	if (IsSoulCrackWindowOpen() && IsInSoulCrackZone(DamageData.DamageCauser))
	{
		TriggerSoulCrackHit();
	}
}

void ARLEnemyCharacter::TriggerSoulCrackHit()
{
	if (HealthComp && HealthComp->bIsDead)
	{
		return;
	}

	EndSoulCrackPattern();
	StopAttackTelegraph();

	if (!bHasShatteredThisFight)
	{
		SoulCrackStackCount = FMath::Min(SoulCrackStackCount + 1, SoulCrackMaxStack);
		OnSoulCrackStackChanged.Broadcast(SoulCrackStackCount, SoulCrackMaxStack);

		// 균열 성공마다 보호막이 깎임 (30 → 20 → 10 → 대균열 시 0)
		if (CurrentSoulCrackShield > 0.0f)
		{
			SetSoulCrackShield(SoulCrackShieldDamageReduction - SoulCrackShieldReductionPerHit * SoulCrackStackCount);
		}
	}

	if (GEngine)
	{
		GEngine->AddOnScreenDebugMessage(-1, 2.5f, FColor::Green,
			FString::Printf(TEXT("💚 [영혼의 균열] %s 균열 성공! (%d/%d) 남은 보호막 %.0f%%"), *GetName(), SoulCrackStackCount, SoulCrackMaxStack, CurrentSoulCrackShield));
	}

	OnSoulCrackHit(SoulCrackStackCount);

	if (!bHasShatteredThisFight && SoulCrackStackCount >= SoulCrackMaxStack)
	{
		TriggerSoulCrackShatter();
		return;
	}

	// ★ CanBeStaggered()(보스=false)를 거치지 않는 강제 경직: 진행 중인 공격 몽타주를 끊고(Notify_Hit 발동 안 함) 피격 모션
	if (UAnimInstance* AnimInstance = GetMesh() ? GetMesh()->GetAnimInstance() : nullptr)
	{
		AnimInstance->StopAllMontages(0.15f);
	}
	const float HitPlayLength = SoulCrackHitMontage ? PlayAnimMontage(SoulCrackHitMontage) : 0.0f;
	UE_LOG(LogTemp, Warning, TEXT("[SoulCrack] Hit %s: Stack=%d/%d Montage=%s PlayLength=%.2f"),
		*GetName(), SoulCrackStackCount, SoulCrackMaxStack, *GetNameSafe(SoulCrackHitMontage), HitPlayLength);

	PauseAIForSoulCrack(SoulCrackStaggerDuration);
}

void ARLEnemyCharacter::TriggerSoulCrackShatter()
{
	// 한 전투 1회 한정
	if (bHasShatteredThisFight || (HealthComp && HealthComp->bIsDead))
	{
		return;
	}

	bHasShatteredThisFight = true;
	EndSoulCrackPattern();
	StopAttackTelegraph();

	if (UAnimInstance* AnimInstance = GetMesh() ? GetMesh()->GetAnimInstance() : nullptr)
	{
		AnimInstance->StopAllMontages(0.15f);
	}
	const float ShatterPlayLength = SoulShatterMontage ? PlayAnimMontage(SoulShatterMontage) : 0.0f;
	UE_LOG(LogTemp, Warning, TEXT("[SoulCrack] Shatter %s: Montage=%s PlayLength=%.2f (0이면 재생 실패)"),
		*GetName(), *GetNameSafe(SoulShatterMontage), ShatterPlayLength);

	PauseAIForSoulCrack(SoulShatterStunDuration);

	// 보호막 완전 소멸 (이번 전투 동안 복구 안 됨)
	SetSoulCrackShield(0.0f);

	// 대경직 동안 방어 무시(트루뎀): State.IgnoreDefense 태그가 있으면 ARLCharacter::ReceiveCustomDamage가
	// 방어력/피해감소 계산을 건너뜁니다. 경직이 끝나면 태그만 떼고, 방어력은 원래 값 그대로 다시 적용됩니다.
	if (AbilitySystemComp && !bSoulShatterDebuffActive)
	{
		AbilitySystemComp->AddLooseGameplayTag(RLGameplayTags::State_IgnoreDefense);
		bSoulShatterDebuffActive = true;

		if (SoulShatterStunDuration > 0.0f)
		{
			GetWorldTimerManager().SetTimer(SoulShatterDebuffTimerHandle, this, &ARLEnemyCharacter::EndSoulShatterDebuff, SoulShatterStunDuration, false);
		}
		else
		{
			EndSoulShatterDebuff();
		}
	}

	if (GEngine)
	{
		GEngine->AddOnScreenDebugMessage(-1, 4.0f, FColor::Emerald,
			FString::Printf(TEXT("💥 [영혼의 균열] %s 보호막 붕괴! %.1f초 대경직 동안 방어 무시"), *GetName(), SoulShatterStunDuration));
	}

	OnSoulCrackShattered();
}

void ARLEnemyCharacter::PauseAIForSoulCrack(float Duration)
{
	if (UCharacterMovementComponent* MoveComp = GetCharacterMovement())
	{
		MoveComp->StopMovementImmediately();
	}

	// ★ BT를 완전히 멈췄다가 RestartLogic으로 루트부터 다시 시작 — PauseLogic은 진행 중이던 BTT_Attack의
	// 몽타주 대기 콜백과 꼬일 수 있어서, 사망 처리(HandleDeath)와 같은 StopLogic 방식을 씁니다.
	AAIController* AICon = Cast<AAIController>(GetController());
	UBrainComponent* Brain = AICon ? AICon->GetBrainComponent() : nullptr;
	if (AICon)
	{
		AICon->StopMovement();
	}
	if (Brain)
	{
		const bool bWasRunning = Brain->IsRunning();
		Brain->StopLogic(TEXT("SoulCrack"));
		UE_LOG(LogTemp, Warning, TEXT("[SoulCrack] PauseAI %s: Controller=%s Brain=%s Running %d -> %d, Duration=%.1f"),
			*GetName(), *GetNameSafe(AICon), *GetNameSafe(Brain), bWasRunning, Brain->IsRunning(), Duration);
	}
	else
	{
		UE_LOG(LogTemp, Warning, TEXT("[SoulCrack] PauseAI %s: BrainComponent 없음 (Controller=%s)"), *GetName(), *GetNameSafe(GetController()));
	}

	bSoulCrackAIStopped = true;
	SoulCrackAIStopEndTime = GetWorld()->GetTimeSeconds() + Duration;

	GetWorldTimerManager().ClearTimer(SoulCrackAITimerHandle);
	if (Duration > 0.0f)
	{
		GetWorldTimerManager().SetTimer(SoulCrackAITimerHandle, this, &ARLEnemyCharacter::ResumeAIAfterSoulCrack, Duration, false);
	}
	else
	{
		ResumeAIAfterSoulCrack();
	}
}

void ARLEnemyCharacter::ResumeAIAfterSoulCrack()
{
	bSoulCrackAIStopped = false;

	if (HealthComp && HealthComp->bIsDead)
	{
		return;
	}

	UE_LOG(LogTemp, Warning, TEXT("[SoulCrack] ResumeAI %s"), *GetName());

	if (AAIController* AICon = Cast<AAIController>(GetController()))
	{
		if (UBrainComponent* Brain = AICon->GetBrainComponent())
		{
			Brain->RestartLogic();
		}
	}
}

void ARLEnemyCharacter::EndSoulShatterDebuff()
{
	if (!bSoulShatterDebuffActive)
	{
		return;
	}

	if (AbilitySystemComp)
	{
		AbilitySystemComp->RemoveLooseGameplayTag(RLGameplayTags::State_IgnoreDefense);
	}
	bSoulShatterDebuffActive = false;

	OnSoulShatterDebuffEnded();
}

void ARLEnemyCharacter::StartDeathFade()
{
	USkeletalMeshComponent* MeshComp = GetMesh();
	if (!MeshComp)
	{
		Destroy();
		return;
	}

	// BP OnDeath의 SetLifeSpan이 페이드 도중 액터를 지우지 않도록 해제 (페이드 종료 시 직접 Destroy)
	SetLifeSpan(0.0f);

	DeathFadeElapsed = 0.0f;
	DeathFadeMeshStartLocation = MeshComp->GetRelativeLocation();

	// 사망 직후 머티리얼이 페이드 파라미터를 받을 수 있게 초기값 0 세팅 (다이나믹 머티리얼 인스턴스 자동 생성)
	MeshComp->SetScalarParameterValueOnMaterials(DeathFadeParamName, 0.0f);

	OnDeathFadeStarted();

	GetWorldTimerManager().SetTimer(DeathFadeTimerHandle, this, &ARLEnemyCharacter::TickDeathFade, 1.0f / 60.0f, true);
}

void ARLEnemyCharacter::TickDeathFade()
{
	USkeletalMeshComponent* MeshComp = GetMesh();
	if (!MeshComp)
	{
		GetWorldTimerManager().ClearTimer(DeathFadeTimerHandle);
		Destroy();
		return;
	}

	DeathFadeElapsed += GetWorldTimerManager().GetTimerRate(DeathFadeTimerHandle);
	const float Alpha = FMath::Clamp(DeathFadeElapsed / DeathFadeDuration, 0.0f, 1.0f);

	MeshComp->SetScalarParameterValueOnMaterials(DeathFadeParamName, Alpha);

	// 처음엔 천천히, 끝으로 갈수록 빠르게 가라앉도록 EaseIn
	const float SinkAlpha = FMath::InterpEaseIn(0.0f, 1.0f, Alpha, 2.0f);
	MeshComp->SetRelativeLocation(DeathFadeMeshStartLocation - FVector(0.0f, 0.0f, DeathSinkDistance * SinkAlpha));

	if (Alpha >= 1.0f)
	{
		GetWorldTimerManager().ClearTimer(DeathFadeTimerHandle);
		Destroy();
	}
}

// ==========================================
// ★ 잠행 강습 (FRLMonsterPatternData::bVanishStrike)
// Notify_Vanish에서 몽타주 정지 → 가라앉으며 사라짐 → 플레이어 머리 위 표식 → 플레이어 등 뒤에서 솟아오르며 몽타주 재개
// ==========================================

// 사라지기/나타나기 이펙트: 반복 재생형(연기처럼 계속 뿜는) 시스템도 쓸 수 있게 Duration초 뒤 Deactivate
// (새 파티클 생성만 멈추고 남은 연기는 자연스럽게 사라진 뒤 컴포넌트 자동 파괴)
static UNiagaraComponent* SpawnVanishFX(AActor* Owner, UNiagaraSystem* System, const FVector& Location, float Duration, float Scale)
{
	if (!Owner || !System)
	{
		return nullptr;
	}

	UNiagaraComponent* FX = UNiagaraFunctionLibrary::SpawnSystemAtLocation(Owner, System, Location);
	if (!FX)
	{
		return nullptr;
	}
	FX->SetVariableFloat(FName("User.Scale_All"), Scale);

	TWeakObjectPtr<UNiagaraComponent> WeakFX = FX;
	FTimerHandle Handle;
	Owner->GetWorldTimerManager().SetTimer(Handle, FTimerDelegate::CreateWeakLambda(FX, [WeakFX]()
	{
		if (WeakFX.IsValid())
		{
			WeakFX->Deactivate();
		}
	}), FMath::Max(Duration, 0.05f), false);
	return FX;
}

void ARLEnemyCharacter::StartVanish(const FRLMonsterPatternData& Pattern)
{
	EndVanish();

	if (!Pattern.bVanishStrike || !Pattern.AttackMontage || (HealthComp && HealthComp->bIsDead))
	{
		return;
	}

	const TArray<FVector2D> Notifies = ComputeTelegraphWindows(Pattern.AttackMontage, FName("Notify_Vanish"), 0.0f);
	if (Notifies.Num() == 0)
	{
		UE_LOG(LogTemp, Warning, TEXT("[Vanish] %s: 몽타주 '%s'에 Notify_Vanish 노티파이가 없습니다."), *GetName(), *Pattern.AttackMontage->GetName());
		return;
	}

	VanishMontage = Pattern.AttackMontage;
	VanishPattern = Pattern;
	VanishNotifyTime = Notifies[0].Y;
	VanishStepElapsed = 0.0f;
	bVanishOutFXSpawned = false;
	bVanishInPrepared = false;
	VanishStep = EVanishStep::Waiting;
}

void ARLEnemyCharacter::UpdateVanish(float DeltaTime)
{
	if (VanishStep == EVanishStep::None)
	{
		return;
	}

	UAnimInstance* AnimInstance = GetMesh() ? GetMesh()->GetAnimInstance() : nullptr;
	// 정지(Pause)된 몽타주는 Montage_IsPlaying이 false라서 IsActive로 확인
	if (!AnimInstance || !AnimInstance->Montage_IsActive(VanishMontage) || (HealthComp && HealthComp->bIsDead))
	{
		EndVanish();
		return;
	}

	VanishStepElapsed += DeltaTime;

	// ★ 사라지기 전에 걸쳐 있는 예고 데칼(표시 시작이 Notify_Vanish 앞)은 다시 나타난 뒤에 뜨게 미룸.
	// 안 그러면 원래 자리에 데칼이 뜬 채로 사라지고, 나타난 뒤에도 엉뚱한 곳을 가리킴
	if (TelegraphMontage == VanishMontage)
	{
		for (FVector2D& Window : TelegraphWindows)
		{
			if (Window.Y > VanishNotifyTime && Window.X <= VanishNotifyTime)
			{
				Window.X = VanishNotifyTime + 0.01f;
			}
		}
	}

	switch (VanishStep)
	{
	case EVanishStep::Waiting:
	{
		const float Position = AnimInstance->Montage_GetPosition(VanishMontage);

		// 연기는 가라앉기 시작보다 먼저 피어오름 (그 연기 속으로 가라앉는 느낌)
		if (!bVanishOutFXSpawned && Position >= VanishNotifyTime - VanishPattern.VanishFXLeadTime)
		{
			bVanishOutFXSpawned = true;
			VanishOutFXComp = SpawnVanishFX(this, VanishPattern.VanishFX, GetIndicatorOrigin(),
				VanishPattern.VanishFXLeadTime + VanishPattern.VanishFadeOutTime + 0.2f, VanishPattern.VanishFXScale);
		}

		if (Position >= VanishNotifyTime)
		{
			AnimInstance->Montage_Pause(VanishMontage);

			SetInvulnerable(true);
			if (UCapsuleComponent* Capsule = GetCapsuleComponent())
			{
				VanishSavedPawnResponse = Capsule->GetCollisionResponseToChannel(ECC_Pawn);
				Capsule->SetCollisionResponseToChannel(ECC_Pawn, ECR_Ignore);
			}
			if (UCharacterMovementComponent* Move = GetCharacterMovement())
			{
				Move->StopMovementImmediately();
			}

			VanishMeshBaseLocation = GetMesh()->GetRelativeLocation();
			VanishSinkDepthApplied = VanishPattern.VanishSinkDepth > 0.0f ? VanishPattern.VanishSinkDepth
				: (GetCapsuleComponent() ? GetCapsuleComponent()->GetScaledCapsuleHalfHeight() * 2.0f : 200.0f);

			if (!bVanishOutFXSpawned)
			{
				bVanishOutFXSpawned = true;
				VanishOutFXComp = SpawnVanishFX(this, VanishPattern.VanishFX, GetIndicatorOrigin(), VanishPattern.VanishFadeOutTime + 0.2f, VanishPattern.VanishFXScale);
			}

			VanishStep = EVanishStep::FadingOut;
			VanishStepElapsed = 0.0f;
			SetVanishVisual(0.0f);
		}
		break;
	}

	case EVanishStep::FadingOut:
	{
		const float Alpha = VanishPattern.VanishFadeOutTime > 0.0f ? FMath::Clamp(VanishStepElapsed / VanishPattern.VanishFadeOutTime, 0.0f, 1.0f) : 1.0f;
		SetVanishVisual(Alpha);

		// 다 가라앉을 즈음 연기가 퍼지듯 흩어지며 사라짐: 70%부터 새 연기 생성을 멈추고, 다 가라앉으면 남은 연기를
		// 빠르게 재생(시간 배속)해서 크기 커짐 + 투명해짐 커브가 금방 끝나게 함 (즉시 제거하면 뚝 끊겨 보임)
		if (VanishOutFXComp.IsValid())
		{
			if (Alpha >= 0.7f && VanishOutFXComp->IsActive())
			{
				VanishOutFXComp->Deactivate();
			}
			if (Alpha >= 1.0f)
			{
				VanishOutFXComp->SetCustomTimeDilation(2.5f);
				VanishOutFXComp.Reset();
			}
		}

		if (Alpha >= 1.0f)
		{
			GetMesh()->SetVisibility(false);
			ShowVanishMark(true);
			VanishStep = EVanishStep::Hidden;
			VanishStepElapsed = 0.0f;
		}
		break;
	}

	case EVanishStep::Hidden:
		ShowVanishMark(true); // 플레이어를 따라다니며 회전

		// 나타나기 VanishFXLeadTime 전에 나타날 자리(플레이어 등 뒤)로 미리 이동해 두고 연기부터 피어오르게 함
		if (!bVanishInPrepared && VanishStepElapsed >= VanishPattern.VanishHiddenTime - VanishPattern.VanishFXLeadTime)
		{
			bVanishInPrepared = true;
			TeleportBehindPlayer();
			SpawnVanishFX(this, VanishPattern.VanishFX, GetIndicatorOrigin(),
				VanishPattern.VanishFXLeadTime + VanishPattern.VanishFadeInTime + 0.2f, VanishPattern.VanishFXScale);
		}

		if (VanishStepElapsed >= VanishPattern.VanishHiddenTime)
		{
			ShowVanishMark(false);
			if (!bVanishInPrepared)
			{
				bVanishInPrepared = true;
				TeleportBehindPlayer();
				SpawnVanishFX(this, VanishPattern.VanishFX, GetIndicatorOrigin(), VanishPattern.VanishFadeInTime + 0.2f, VanishPattern.VanishFXScale);
			}

			// 이동해 있던 동안 플레이어가 움직였을 수 있으니 나타나는 순간 다시 플레이어를 바라봄
			if (const APawn* Player = UGameplayStatics::GetPlayerPawn(this, 0))
			{
				SetActorRotation(FRotator(0.0f, (Player->GetActorLocation() - GetActorLocation()).GetSafeNormal2D().Rotation().Yaw, 0.0f));
			}

			GetMesh()->SetVisibility(true);
			SetVanishVisual(1.0f);
			AnimInstance->Montage_Resume(VanishMontage);

			VanishStep = EVanishStep::FadingIn;
			VanishStepElapsed = 0.0f;
		}
		break;

	case EVanishStep::FadingIn:
	{
		const float Alpha = VanishPattern.VanishFadeInTime > 0.0f ? 1.0f - FMath::Clamp(VanishStepElapsed / VanishPattern.VanishFadeInTime, 0.0f, 1.0f) : 0.0f;
		SetVanishVisual(Alpha);
		if (Alpha <= 0.0f)
		{
			EndVanish();
		}
		break;
	}

	default:
		break;
	}
}

void ARLEnemyCharacter::SetVanishVisual(float Alpha)
{
	USkeletalMeshComponent* MeshComp = GetMesh();
	if (!MeshComp)
	{
		return;
	}

	const float Eased = FMath::SmoothStep(0.0f, 1.0f, Alpha);
	MeshComp->SetRelativeLocation(VanishMeshBaseLocation - FVector(0.0f, 0.0f, VanishSinkDepthApplied * Eased));

	if (!VanishPattern.VanishOverlayMaterial)
	{
		return;
	}

	if (!bVanishOverlayApplied)
	{
		VanishSavedOverlay = MeshComp->GetOverlayMaterial();
		VanishOverlayMID = UMaterialInstanceDynamic::Create(VanishPattern.VanishOverlayMaterial, this);
		MeshComp->SetOverlayMaterial(VanishOverlayMID);
		bVanishOverlayApplied = true;
	}

	// 어둠이 먼저 몸을 덮고(앞 40%) 그다음 가라앉는 느낌: 오버레이 불투명도를 빠르게 끝값까지 올림
	if (VanishOverlayMID)
	{
		static const FName RimOpacityName(TEXT("RimOpacity"));
		float TargetOpacity = 1.0f;
		VanishPattern.VanishOverlayMaterial->GetScalarParameterValue(FMaterialParameterInfo(RimOpacityName), TargetOpacity);
		VanishOverlayMID->SetScalarParameterValue(RimOpacityName, TargetOpacity * FMath::Clamp(Alpha / 0.4f, 0.0f, 1.0f));
	}
}

void ARLEnemyCharacter::ShowVanishMark(bool bShow)
{
	if (!bShow)
	{
		if (VanishMark)
		{
			VanishMark->SetVisibility(false);
		}
		return;
	}

	const ACharacter* Player = Cast<ACharacter>(UGameplayStatics::GetPlayerPawn(this, 0));
	if (!Player)
	{
		return;
	}

	if (!VanishMark)
	{
		VanishMark = NewObject<UStaticMeshComponent>(this, TEXT("VanishMark"));
		VanishMark->SetCollisionEnabled(ECollisionEnabled::NoCollision);
		VanishMark->SetCastShadow(false);
		VanishMark->SetUsingAbsoluteLocation(true);
		VanishMark->SetUsingAbsoluteRotation(true);
		VanishMark->SetUsingAbsoluteScale(true);
		VanishMark->SetupAttachment(GetRootComponent());
		VanishMark->RegisterComponent();
	}

	UStaticMesh* MarkMesh = VanishPattern.VanishMarkMesh ? VanishPattern.VanishMarkMesh.Get()
		: LoadObject<UStaticMesh>(nullptr, TEXT("/Engine/BasicShapes/Cone.Cone"));
	if (VanishMark->GetStaticMesh() != MarkMesh)
	{
		VanishMark->SetStaticMesh(MarkMesh);
	}
	if (VanishPattern.VanishMarkMaterial && VanishMark->GetMaterial(0) != VanishPattern.VanishMarkMaterial)
	{
		VanishMark->SetMaterial(0, VanishPattern.VanishMarkMaterial);
	}

	// 머리 위에 거꾸로 뒤집어 띄우고(원뿔 끝이 아래 = 머리를 가리킴) 천천히 회전 + 위아래로 살짝 흔들림
	const float HalfHeight = Player->GetCapsuleComponent() ? Player->GetCapsuleComponent()->GetScaledCapsuleHalfHeight() : 90.0f;
	const float Time = GetWorld()->GetTimeSeconds();
	const float Bob = FMath::Sin(Time * 6.0f) * 10.0f;
	VanishMark->SetWorldLocation(Player->GetActorLocation() + FVector(0.0f, 0.0f, HalfHeight + VanishPattern.VanishMarkHeight + Bob));
	VanishMark->SetWorldRotation(FRotator(180.0f, FMath::Fmod(Time * 180.0f, 360.0f), 0.0f));
	VanishMark->SetWorldScale3D(FVector(VanishPattern.VanishMarkScale));
	VanishMark->SetVisibility(true);
}

void ARLEnemyCharacter::TeleportBehindPlayer()
{
	const ACharacter* Player = Cast<ACharacter>(UGameplayStatics::GetPlayerPawn(this, 0));
	UCapsuleComponent* MyCapsule = GetCapsuleComponent();
	if (!Player || !MyCapsule)
	{
		return;
	}

	const FVector PlayerLoc = Player->GetActorLocation();
	FVector Back = -Player->GetActorForwardVector().GetSafeNormal2D();
	if (Back.IsNearlyZero())
	{
		Back = (GetActorLocation() - PlayerLoc).GetSafeNormal2D();
	}

	const float PlayerRadius = Player->GetCapsuleComponent() ? Player->GetCapsuleComponent()->GetScaledCapsuleRadius() : 40.0f;
	const float PlayerHalfHeight = Player->GetCapsuleComponent() ? Player->GetCapsuleComponent()->GetScaledCapsuleHalfHeight() : 90.0f;
	const float MyRadius = MyCapsule->GetScaledCapsuleRadius();
	const float Distance = PlayerRadius + MyRadius + VanishPattern.VanishAppearDistance;

	// 벽 너머로 나타나지 않게: 플레이어 → 등 뒤 방향으로 벽(WorldStatic)이 있으면 그 앞에서 멈춤
	FVector Target = PlayerLoc + Back * Distance;
	FHitResult Hit;
	FCollisionQueryParams Params(SCENE_QUERY_STAT(VanishTeleport), false, this);
	Params.AddIgnoredActor(Player);
	if (GetWorld()->SweepSingleByChannel(Hit, PlayerLoc, Target, FQuat::Identity, ECC_WorldStatic, FCollisionShape::MakeSphere(MyRadius * 0.5f), Params))
	{
		Target = Hit.Location;
	}

	// 발 높이를 플레이어와 맞춤 (보스 캡슐이 더 커서 중심 높이가 다름)
	Target.Z = PlayerLoc.Z - PlayerHalfHeight + MyCapsule->GetScaledCapsuleHalfHeight();

	const FRotator FaceRot(0.0f, (PlayerLoc - Target).GetSafeNormal2D().Rotation().Yaw, 0.0f);
	GetWorld()->FindTeleportSpot(this, Target, FaceRot);
	TeleportTo(Target, FaceRot, false, true);
	SetActorRotation(FaceRot);
}

void ARLEnemyCharacter::EndVanish()
{
	if (VanishStep == EVanishStep::None)
	{
		return;
	}

	const bool bStarted = VanishStep != EVanishStep::Waiting;
	const bool bWasPaused = VanishStep == EVanishStep::FadingOut || VanishStep == EVanishStep::Hidden;
	VanishStep = EVanishStep::None;

	ShowVanishMark(false);
	if (VanishOutFXComp.IsValid())
	{
		VanishOutFXComp->DeactivateImmediate();
	}
	VanishOutFXComp.Reset();

	if (bStarted)
	{
		if (USkeletalMeshComponent* MeshComp = GetMesh())
		{
			MeshComp->SetRelativeLocation(VanishMeshBaseLocation);
			MeshComp->SetVisibility(true);
			if (bVanishOverlayApplied)
			{
				MeshComp->SetOverlayMaterial(VanishSavedOverlay);
			}

			if (bWasPaused)
			{
				if (UAnimInstance* AnimInstance = MeshComp->GetAnimInstance())
				{
					if (AnimInstance->Montage_IsActive(VanishMontage))
					{
						AnimInstance->Montage_Resume(VanishMontage);
					}
				}
			}
		}

		if (UCapsuleComponent* Capsule = GetCapsuleComponent())
		{
			Capsule->SetCollisionResponseToChannel(ECC_Pawn, VanishSavedPawnResponse);
		}
		SetInvulnerable(false);
	}

	bVanishOverlayApplied = false;
	VanishSavedOverlay = nullptr;
	VanishOverlayMID = nullptr;
	VanishMontage = nullptr;
}

void ARLEnemyCharacter::UpdateVanishDarken(float DeltaTime)
{
	const bool bWantDark = VanishPattern.bVanishStrike && VanishPattern.VanishDarkenExposure < 0.0f
		&& ((VanishStep == EVanishStep::Waiting && bVanishOutFXSpawned)
			|| VanishStep == EVanishStep::FadingOut
			|| VanishStep == EVanishStep::Hidden);

	if (!bWantDark && VanishDarkenWeight <= 0.0f)
	{
		return;
	}

	if (!VanishDarkenPP)
	{
		VanishDarkenPP = NewObject<UPostProcessComponent>(this, TEXT("VanishDarkenPP"));
		VanishDarkenPP->bUnbound = true;   // 위치와 상관없이 화면 전체
		VanishDarkenPP->Priority = 100.0f; // 맵의 분위기 PPV보다 위에서 섞임
		VanishDarkenPP->BlendWeight = 0.0f;
		VanishDarkenPP->SetupAttachment(GetRootComponent());
		VanishDarkenPP->RegisterComponent();
	}

	FPostProcessSettings& Settings = VanishDarkenPP->Settings;
	Settings.bOverride_AutoExposureBias = true;
	Settings.AutoExposureBias = VanishPattern.VanishDarkenExposure;
	Settings.bOverride_VignetteIntensity = true;
	Settings.VignetteIntensity = VanishPattern.VanishDarkenVignette;

	const float Rate = 1.0f / FMath::Max(VanishPattern.VanishDarkenFadeTime, 0.01f);
	VanishDarkenWeight = FMath::FInterpConstantTo(VanishDarkenWeight, bWantDark ? 1.0f : 0.0f, DeltaTime, Rate);
	VanishDarkenPP->BlendWeight = VanishDarkenWeight;
}
