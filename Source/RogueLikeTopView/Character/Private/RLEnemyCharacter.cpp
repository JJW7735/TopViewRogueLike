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
#include "Materials/MaterialInterface.h"

ARLEnemyCharacter::ARLEnemyCharacter()
{
	DropChestClass = ARLRewardChest::StaticClass();
	ItemDropClass = ARLItemDrop::StaticClass();
}

void ARLEnemyCharacter::BeginPlay()
{
	Super::BeginPlay();

	// EnrageHealthPercentThreshold가 음수(기본값)인 일반 몬스터도 델리게이트 바인딩 자체는 걸어두지만,
	// HandleEnemyHealthChanged 내부에서 threshold < 0이면 바로 return하므로 실질적인 비용은 거의 없습니다.
	if (HealthComp)
	{
		HealthComp->OnHealthChanged.AddDynamic(this, &ARLEnemyCharacter::HandleEnemyHealthChanged);
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

	UGameplayStatics::ApplyDamage(this, HealthComp->CurrentHealth, nullptr, nullptr, UDamageType::StaticClass());
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
	Super::Tick(DeltaTime);

	UpdateAttackChase(DeltaTime);

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
		UAnimInstance* AnimInstance = GetMesh() ? GetMesh()->GetAnimInstance() : nullptr;
		if (!AnimInstance || !AnimInstance->Montage_IsPlaying(SoulCrackMontage))
		{
			EndSoulCrackPattern();
		}
	}

	// 강화판 진행 중: 카운터 방향 안내 데칼이 보스를 따라가고, 균열 구간 동안 몸 발광
	if (SoulCrackMontage)
	{
		UpdateSoulCrackZones();
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
	const FVector Location = GetActorLocation();
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
	StartAttackChase(Pattern);
	BeginSoulCrackPattern(Pattern, HitNotifyName);
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

void ARLEnemyCharacter::StopAttackChase()
{
	ChaseMontage = nullptr;
	ChaseSpeed = 0.0f;
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
	if (const APawn* Player = UGameplayStatics::GetPlayerPawn(this, 0))
	{
		if (FVector::Dist2D(Player->GetActorLocation(), GetActorLocation()) <= ChaseStopDistance)
		{
			return;
		}
	}

	const FVector Forward = GetActorForwardVector().GetSafeNormal2D();
	SetActorLocation(GetActorLocation() + Forward * ChaseSpeed * DeltaTime, true);

	// 보스가 움직였으니 떠 있는 Box 예고 데칼도 같이 옮겨서 실제 판정 위치와 어긋나지 않게 함
	if (bTelegraphVisible && AttackIndicator && TelegraphShape == ETelegraphShape::Box)
	{
		AttackIndicator->UpdateBoxTransform(GetActorLocation(), TelegraphForwardOffset * TelegraphShownScale, GetActorRotation().Yaw);
	}
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
	if (bIsInvulnerable && !DamageData.bIgnoreInvincibility)
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
	}

	if (GEngine)
	{
		GEngine->AddOnScreenDebugMessage(-1, 2.5f, FColor::Green,
			FString::Printf(TEXT("💚 [영혼의 균열] %s 균열 성공! (%d/%d)"), *GetName(), SoulCrackStackCount, SoulCrackMaxStack));
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

	// 피해감소 디버프: 기존 StatComp->DamageReduction을 저장해두고 덮어쓴 뒤, 지속 시간 후 복구
	if (StatComp && !bSoulShatterDebuffActive)
	{
		SavedDamageReduction = StatComp->DamageReduction;
		StatComp->DamageReduction = SoulShatterDamageReduction;
		bSoulShatterDebuffActive = true;

		if (SoulShatterDebuffDuration > 0.0f)
		{
			GetWorldTimerManager().SetTimer(SoulShatterDebuffTimerHandle, this, &ARLEnemyCharacter::EndSoulShatterDebuff, SoulShatterDebuffDuration, false);
		}
		else
		{
			EndSoulShatterDebuff();
		}
	}

	if (GEngine)
	{
		GEngine->AddOnScreenDebugMessage(-1, 4.0f, FColor::Emerald,
			FString::Printf(TEXT("💥 [영혼의 균열] %s 보호막 붕괴! %.1f초 경직, %.1f초간 피해감소 %.0f%% → %.0f%%"),
				*GetName(), SoulShatterStunDuration, SoulShatterDebuffDuration, SavedDamageReduction, SoulShatterDamageReduction));
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

	if (StatComp)
	{
		StatComp->DamageReduction = SavedDamageReduction;
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
