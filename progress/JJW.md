# JJW
> 최종 갱신: 2026-11-01

## 몬스터 제작 가이드 (일반 몬스터 추가 작업용 — 팀원/AI 참고)
> 2026-11-01 기준 실제 에셋/코드에서 확인한 값. 보스 작업(JJW)과 겹치지 않게 **일반 몬스터는 아래 구조만 따르면 C++ 수정 없이** 추가 가능.

### 1. 클래스 계층
```
ACharacter
 └─ ARLCharacter (C++)          체력/스탯/GAS, 데미지 수신(ReceiveCustomDamage), 판정 함수(Create*Hitbox), 예고 데칼
     └─ ARLEnemyCharacter (C++)  보상/드롭, 분노, 사망 페이드, 공격 패턴 진입점(BeginAttackPattern), 보스 기믹
         └─ BP_EnemyBase          패턴 선택 로직(PatternDataAsset, TrySelectPattern 등) ← BT가 이 BP로 캐스트함
             ├─ BP_Enemy_Ranged   일반 원거리 몬스터
             ├─ BP_Boss (세바로그) └─ BP_Boss_2 (카이메라)
             └─ (새 일반 몬스터는 여기 자식으로)
```
- **새 몬스터는 반드시 `BP_EnemyBase`의 자식 BP로** 만들 것. `BTT_SelectPattern`/`BTT_Attack`이 폰을 `BP_EnemyBase`로 캐스트하므로 다른 부모면 공격이 전부 안 됨.
- 경로: BP `/Game/EnemyAI/Blueprint/Enemy_Character/`, DA `/Game/DataAsset/Enemy/`, 몽타주 `/Game/Animation/AM/`

### 2. 새 일반 몬스터 만드는 순서
1. `BP_EnemyBase` 우클릭 → 자식 블루프린트 생성 (`BP_Enemy_이름`)
2. 클래스 디폴트 설정
   | 항목 | 일반 몬스터 기본값 | 비고 |
   |---|---|---|
   | AIControllerClass | `AIC_Enemy` | 보스만 `AIC_Boss` |
   | PatternDataAsset | 새 DA (`DA_Enemy_이름`) | 아래 3번 |
   | Mesh / AnimClass | `SKM_Manny` / `ABP_EnemyBase` (기존 몬스터) | 다른 메시면 그 스켈레톤용 AnimBP 필요 |
   | Mesh 위치/회전 | Z -90, Yaw -90 | 캡슐 반높이 88 / 반지름 34 기준 |
   | DeathMontage | `AM_Enemy_Death` | |
   | TeamID | Enemy (기본값) | |
   | bIsBoss / bIsElite / bImmuneToStagger | false | 엘리트·면역이면 경직 안 받음(슈퍼아머) |
   | ExpReward / SoulReward / ItemDropChance / Min·MaxGoldDrop | 30 / 2 / 0.35 / 5·20 | 보상 |
   | DisplayName | 몬스터 이름 | |
3. 패턴 DataAsset 생성: 기존 `DA_EnemyBase` 복제 → `Patterns` 배열 수정 (클래스 `URLMonsterPatternDataAsset`)
4. 공격 몽타주 만들기 (아래 5번 규칙 필수)
5. 레벨/아레나에 배치: `ARLArenaManager`의 Waves → `FRLMonsterSpawnEntry`(EnemyClass, Count, SpecificSpawnPointIndex)에 추가

### 3. 패턴 데이터 (`FRLMonsterPatternData`, `Core/Public/RLTypes.h`)
일반 몬스터가 주로 쓰는 항목만 정리 (도끼/도약/균열 관련 항목은 보스 전용이라 무시).
| 항목 | 의미 |
|---|---|
| PatternID | 패턴 이름. **쿨타임이 이 이름 기준**이라 패턴마다 고유하게 |
| AttackMontage | 공격 몽타주 |
| AttackRange | 이 거리 안이어야 패턴 선택됨 |
| Cooldown / SelectionWeight | 재사용 대기(초) / 랜덤 선택 가중치 |
| DamageData | BaseDamage, KnockbackForce, HitStopDuration, bTrueDamage(방어 무시), LifestealRatio(흡혈 0~1) |
| AttackShape | 판정 형태 (아래 표) |
| bUseIndicator / IndicatorDecalMaterial | 예고 데칼 사용 여부 / 머티리얼 |
| IndicatorDecalSize | **판정 크기 겸 데칼 크기** (형태별 의미 아래 표) |
| IndicatorLeadTime | 타격 몇 초 전부터 데칼 표시 |
| SectorSafeZoneAngle / SectorInnerSafeRadius | 부채꼴: 뒤쪽 안전 각도 / 안쪽 안전 반경 |
| HitRangeScales / HitWidthScales | 연타의 N번째 타격 범위 배율 (예: [1, 1.2, 1.5]) |
| AttackChaseSpeed / AttackChaseStopDistance | 공격하면서 앞으로 전진하는 속도 / 멈추는 거리 |
| bIsRangedAttack / ProjectileClass / ProjectileSpeed / ProjectileSpawnForwardOffset | 투사체 패턴 (기본 투사체 `BP_RLProjectile_Base`) |
| bSummonsMinions / SummonClass / SummonCount / SummonRadius | 잔몹 소환 (`Notify_Summon`) |
| bMirrorAnimation | 몽타주 좌우 반전 (카이메라 AnimBP에만 Mirror 노드 있음 → 일반 몬스터 ABP엔 없음) |

**AttackShape별 동작** (`BTT_Attack`이 분기 — BP 배선은 건드리지 말 것)
| AttackShape | 판정 | IndicatorDecalSize 의미 | 몽타주 노티파이 |
|---|---|---|---|
| Box | 정면 직사각형 | X = 앞 길이 절반(=중심까지 거리), Y = 좌우 폭 절반 | `Notify_Hit` (연타면 여러 개, `Notify_Hit`로 시작하는 이름 모두 인식) |
| Sector | 자기 중심 원/부채꼴 | Y = 반지름 | `Notify_Hit` |
| Location | 플레이어 위치 추적 원 → 타격 0.5초 전 고정 | Y = 반지름 | `Notify_Hit` |
| Projectile | 정면 투사체 (bIsRangedAttack도 true) | 사용 안 함 | `Notify_Hit` |
| Dash | 정면 돌진, 몸통 충돌 판정 | X×2 = 돌진 거리, Y = 폭 | `Notify_DashStart` |
| AxeLeap / AxeReturn / AxeDoubleReturn | 보스2 전용 | — | — |
- 그 밖의 노티파이: `Notify_AttackEnd`(그 시점에 공격 종료 처리), `Notify_Summon`(소환), `Notify_JumpLaunch` / `Notify_LandImpact`(기존 일반 몬스터 점프 공격 `AM_Enemy_JumpSmash`용 — BP_EnemyBase의 `ReceivedNotify_SpawnTargetDecal` / `ReceivedNotify_LandImpact` 이벤트로 연결됨)
- 참고 예시 DA: `DA_EnemyBase`(근접 BasicAttack + JumpAttack), `DA_Enemy_Ranged`(투사체 Throw)

### 4. 예고 데칼 머티리얼 (`/Game/Effect/Material/`)
| 머티리얼 | 용도 |
|---|---|
| `M_Square_Red` | Box / Dash 직사각형 |
| `M_Indicator_Red` | Location / Sector 원형 |
| `M_Sector_Red` | Sector 부채꼴(안전각 있음) |
| `MI_*_Green` | 보스 균열(카운터) 강화판 전용 — 일반 몬스터는 빨강 사용 |
- 데칼과 판정은 같은 값(IndicatorDecalSize)을 쓰므로 **보이는 범위 = 맞는 범위**. 데칼은 캐릭터 발 위치 기준으로 깔리고, 박스/부채꼴 판정은 바닥~캐릭터 키 높이를 덮음 (2026-10-29 수정, 큰 몬스터도 OK)

### 5. 몽타주 규칙 (가장 자주 터지는 버그)
- **슬롯 이름은 그 몬스터 AnimBP에 있는 슬롯과 같아야 재생됨.** `ABP_EnemyBase`(일반 몬스터) = **DefaultSlot**, 카이메라/세바로그 보스 AnimBP = **FullBody**. 안 맞으면 몽타주가 재생 안 되고 멈춤.
- **노티파이는 반드시 Montage Notify**(Add Notify → Montage Notify, 디테일의 Notify Name에 `Notify_Hit` 등 입력). 스켈레톤 노티파이는 BTT_Attack의 PlayMontage 노드가 이벤트를 못 받음.
- Paragon 에셋의 `_MSA`, `Q_Intro` 등 **Additive 애니메이션은 몽타주에 일반 애니와 같이 못 넣음** → 애니메이션 복제 후 Additive Anim Type = None으로 바꾼 `*_Full` 복제본 사용
- 몽타주 세그먼트 추가/길이 변경은 에디터에서 직접 (MCP로 바꾸면 SequenceLength가 갱신 안 됨)

### 6. AI 흐름
- `AIC_Enemy` → `BT_Enemy` (블랙보드 `BB_Enemy`: TargetActor, IsInAttackRange, TargerAttackRange)
- `BTS_CheckAttackRange`가 거리 체크 → `BTT_SelectPattern`이 DA의 패턴 중 사거리·쿨타임 맞는 것을 가중치 랜덤 선택 → `BTT_Attack`이 몽타주 재생 + 노티파이마다 AttackShape에 맞는 판정 호출
- 데미지는 전부 `ARLCharacter::ReceiveCustomDamage` 한 곳으로 모임 (같은 TeamID끼리는 피해 없음)

### 7. 주의점
- 레벨에 **이미 배치된 몬스터 인스턴스**는 BP 클래스 디폴트를 바꿔도 반영 안 될 수 있음 → 인스턴스 값 확인하거나 재배치
- MCP로 DA `Patterns` 배열 수정 시 "크기 변경 + 기존 원소 변경"을 한 번에 하면 실패 → 원소 추가 후 따로 수정
- C++ 헤더(.h, 주석 포함)를 고쳤으면 **라이브 코딩 금지** → 에디터 끄고 빌드 (구조체가 LIVECODING 타입으로 꼬여서 BP 컴파일 다 깨짐)
- 보스 전용 시스템(도끼 패턴, 영혼의 균열, 미러링, 보호막)은 `IsBossEnemy()`/패턴 플래그로만 동작 → 일반 몬스터는 신경 안 써도 됨

## GAS(Gameplay Ability System) 구조 전환 — 2026-10-17 ~ 10-18 완료, main 머지됨
커스텀 스탯/체력/스킬 시스템을 언리얼 GAS 기반으로 단계적으로 옮김. 브랜치 `feat/gas-migration` → main 머지(98b5596e).

### 뭐가 바뀌었나 (단계별)
| 단계 | 내용 | 커밋 |
|---|---|---|
| 0 | GAS 플러그인/모듈 추가, `GAS/` 폴더 신설, `ARLCharacter`에 ASC 부착 (싱글플레이라 캐릭터가 Owner=Avatar) | fbc0fd0c |
| 1-체력 | `RLHealthComponent`를 GAS Facade로 전환. 피해/회복은 `URLGE_Damage`/`URLGE_Heal`(SetByCaller), 사망은 `State.Dead` 태그 | df0d06af |
| 1-스탯 | `RLStatComponent` 전투 스탯 8종을 AttributeSet으로 이전. 인벤토리/보상/상점/세이브에 흩어져 있던 스탯 switch문을 `AddStatBonus` 하나로 통합 | 0ddb62f3 |
| 2 | 스탯 변경을 지속시간별 GE로 분리: 보상·상점·메타강화=Instant(영구), 패시브 아이템=Infinite(해제 가능), 소울 셰터=Override, 스킬 속도 버프=Duration (기존 이동속도 600 고정 버그도 수정). 세이브는 아이템 효과 뺀 기본값만 저장 | b9b3859d |
| 3 | 데미지 파이프라인을 `URLDamageExecution`(ExecutionCalculation)으로 전환. 대상의 Defense/DamageReduction을 캡처해서 경감. 몬스터 `ApplyDamage` 공격에도 플레이어 방어력이 적용되도록 수정(예전엔 무시됐음) | 7380ffc2, 38d8accb |
| 4 | 스킬 시스템을 CastType별 GameplayAbility 8종으로 전환. 쿨타임=GE, 진행=AbilityTask | 6e002ff4 |
| 5 | 스킬 VFX(차지/릴리즈/버프 오라)를 GameplayCue로 전환 | cf9da869 |

### 전체 구조
```
ARLCharacter
 ├─ URLAbilitySystemComponent (ASC)      ← 실제 값/효과/어빌리티가 전부 여기
 │   ├─ URLAttributeSet                   Health, MaxHealth, AttackPower, AbilityPower,
 │   │                                    CritChance, CritDamageMultiplier, AttackSpeed,
 │   │                                    Defense, DamageReduction, MoveSpeed
 │   │                                    + IncomingDamage(메타 어트리뷰트, 피해 계산용)
 │   ├─ GameplayEffect                     (GAS/RLGameplayEffects)
 │   └─ GameplayAbility                    (Skill/RLSkillAbilities)
 │
 ├─ RLHealthComponent  ─┐
 ├─ RLStatComponent    ─┼─ Facade: 기존 API/델리게이트 그대로 유지, 내부에서 ASC 호출
 └─ RLSkillComponent   ─┘          → UI, BP_EnemyBase, 보스 BTT 등 BP는 수정 없이 동작
```

**GameplayEffect 종류 (`GAS/RLGameplayEffects`)**
- `URLGE_Damage` / `URLGE_Heal` — SetByCaller(`Data.Damage`/`Data.Heal`)로 양 전달
- `URLGE_StatBonus_Instant` — 런 영구 스탯 증가 (보상, 상점, 메타강화)
- `URLGE_StatBonus_Infinite` — 해제 가능한 스탯 (패시브 아이템, 슬롯 기준으로 재적용)
- `URLGE_StatBonus_Duration` / `URLGE_SkillBuff` — 시간제 버프 (버프 스킬, 오라 큐 포함)
- `URLGE_SkillCooldown` — 스킬 쿨타임 (`Data.Cooldown`)
- `URLGE_SoulCrackShield` — 보스 소울 크랙 기믹 보호막

**데미지 흐름**
1. 공격자 쪽: 스킬 시전 시 `CalculateFinalDamage`에서 AD/AP 스케일링 + 치명타 계산
2. `RLHealthComponent::ApplyIncomingDamage` → 공격자 ASC가 대상에게 `URLGE_Damage` 적용
3. `URLDamageExecution`이 대상 Defense/DamageReduction으로 경감 → `IncomingDamage` → Health 감소
4. `State.IgnoreDefense`(대상) 또는 `Data.TrueDamage`면 경감 생략, `State.Invulnerable`이면 무적
5. BP `Event AnyDamage`는 경감 후 값으로 계속 알림 (기존 BP 호환)

**스킬 (`Skill/RLGameplayAbility_Skill`, `Skill/RLSkillAbilities`)**
- 베이스 `URLGameplayAbility_Skill` + CastType별 파생 8종: `Instant`, `Projectile`, `Dash`, `Buff`, `Cast`, `GroundTarget`, `Charge`, `Hold`
- 스킬 DataAsset(`RLSkillData`)은 AbilitySpec의 SourceObject로 넘기고, 슬롯(Q/W/E/R/LMB)은 InputID로 구분
- 차지/홀드 등 진행 중 처리는 `URLAbilityTask_Tick`
- `RLSkillComponent`는 Facade로 남아서 입력 처리, ABP 노티파이(`TriggerActiveSkillHit`, Dash), `OnSkillCooldownChanged`, 세이브 API를 그대로 제공

**GameplayCue (`GAS/RLGameplayCueNotifies`)**
- `URLGameplayCueNotify_SkillBurst`(Static, 1회성) / `ARLGameplayCueNotify_SkillLoop`(Actor, 지속형)
- 큐의 SourceObject(스킬 DA)에 있는 Charge/Release VFX를 띄우는 공용 구조 → 스킬마다 큐 BP를 따로 안 만들어도 됨
- 태그 `GameplayCue.Skill.Release` / `.Charge` / `.Buff`, 에셋은 `Content/GAS/GameplayCues/GC_Skill_*` (`DefaultGame.ini`의 GameplayCueNotifyPaths에 등록)

**네이티브 태그 (`GAS/RLGameplayTags`)**
- `State.*`: Dead, Invulnerable, IgnoreDefense, Stunned, SuperArmor, Casting, Skill.Aiming/Charging/Holding
- `Data.*`: Damage, Heal, Cooldown, Duration, TrueDamage, Stat.(스탯 9종)
- `InputTag.*`: Skill.LMB/Q/W/E/R, Dodge (정의만 해둠, 바인딩은 아직 슬롯 핸들 맵 사용)
- `GameplayCue.Skill.*`: Release, Charge, Buff

### 작업할 때 규칙 / 주의점
- 스탯은 변수에 직접 쓰지 말 것 → `AddStatBonus`(영구) / `ApplyRemovableStatBonuses`(해제 가능) / `IncreaseMaxHealth` / `Heal` 사용
- 컴포넌트의 미러 변수는 BlueprintReadOnly(표시용). BP 클래스 디폴트 값은 컴포넌트 BeginPlay에서 AttributeSet 시작값으로 들어가고, 세이브 복원/메타강화는 그 뒤에 적용됨
- 세이브 복원 순서: 스탯 → 인벤토리 → 체력 (`StatSaveVersion=1`, 아이템 효과는 저장값에 미포함 → 이중 적용 방지)
- 캐릭터 BP에서 스킬을 `SkillComponent.EquippedSkills` 맵 디폴트로 지정 → BeginPlay에서 GiveAbility됨
- C++ 태그 추가는 `RLGameplayTags`에 네이티브 태그로
- 런타임 `NewObject` 아이템은 UPROPERTY 참조 없으면 맵 이동 중 GC되어 세이브 복원 실패 (치트 아이템은 AddToRoot로 해결, `TestGiveTestItems` 반지/장화는 아직 같은 문제)
- 미검증: Cast·Hold 타입 스킬(현재 쓰는 스킬 없음), InputTag 기반 입력 바인딩 미도입

## 완료
- 엔진 5.7.4 → 5.8 업그레이드 (`.uproject` EngineAssociation 변경, VS 프로젝트 파일 재생성으로 빌드 에러 해결)
- 팀/진영 시스템 추가 (`RLTypes.h`의 `ERLTeam` enum + `RLCharacter`의 `TeamID`) — 근접/장판/투사체 공격 전체에 아군 피해 방지 적용
- 투사체 관통 버그 수정 (아군 맞으면 데미지 없이 통과하도록, `ARLProjectile::OnHit`)
- 소서리스(SK_Wizard) 캐릭터 리타게팅, IK Rig/Retargeter 구축 및 애니메이션 블루프린트(`wizard_ABP_RLCharacter`) 생성
- 소서리스 스킬 몽타주 및 DataAsset 구성 (평타 투사체, Q 파이어볼, W 인페르노, E 라이트닝, R 메테오)
- 장판형 스킬 공용 베이스 액터(`ARLGroundAreaEffect`) 구현 및 `RLSkillComponent`에서 W, R 스킬 AD/AP 스케일링 데미지 누락 버그 수정
- `ARLCharacter::ReceiveCustomDamageAtLocation` 편의 함수 추가 (BP에서 FHitResult 조립 없이 위치 기반 데미지 전달)
- 상호작용 시스템 구축 (`IA_Interact`, `ARLPlayerController` E키 바인딩, `WBP_HUD` 상호작용 UI 연동)
- 포탈(`ARLPortal`) 진입 시 단순 접촉 자동 이동 방식에서 E키 상호작용 시 맵 비동기 로딩/이동하도록 로직 개선
- Unreal MCP(에디터 내장, Experimental) 활성화 및 설정 (플러그인 켜고 Auto Start Server, `.mcp.json` 생성)
- Claude Code CLI 설치(Windows) 및 Unreal MCP 서버 연동 확인
- 에디터 내장 "터미널" 패널(Experimental) 조사 — 한글 미지원(폰트 글리프 없음) 확인, 외부 터미널 사용으로 결론
- `CLAUDE.md` 엔진 버전 5.8로 수정
- `progress/` 폴더 + `/team-status` 커맨드로 팀 진행상황 온디맨드 공유 체계 구축

## 진행 중 (다음에 이어서 — 2026-09-23)
- **증상**: 보스가 스폰/추격까지는 정상인데, 플레이어가 공격 사거리에 들어오면 `SelectPattern`이 패턴 번호를 계속 재시도(`3 2 1 0`)만 하고 공격 없이 등장모션이 다시 재생됨
- **확인된 진짜 원인**: `AM_Boss_Swing` 몽타주가 재생 시작 후 약 1초 만에 `Montage.Stop`으로 강제 종료됨 (`Notify_Hit`가 걸린 스윙 후반부 타이밍에 도달하기 전에 끊김). BT의 `OnCompleted`가 노티파이 발동 여부와 무관하게 바로 `FinishExecute(true)`로 연결돼있어서, BT 입장에서는 "정상 완료"로 착각하고 다음(Wait)으로 넘어감 — 그래서 겉보기엔 정상 흐름처럼 보였던 것
- **유력 용의자**: BT observer 데코레이터(`BTDecorator_Blackboard_0/1/2`)가 `IsInAttackRange`/`bIsEnraged` 등 블랙보드 값을 감시하다가, 재평가 시점에 실행 중인 Attack 시퀀스를 abort시키는 구조로 추정
- **다음에 확인할 것**: 공격 애니메이션 재생 "중"에 라이브 BT 디버거에서 Sequence/Attack 노드가 빨갛게 abort되는 순간이 있는지 직접 확인 (abort 확인되면 해당 데코레이터의 Observer Aborts 설정/조건을 고칠 것)
- **추가 발견**: `BP_Boss`에 `AttackCollision_L`/`AttackCollision_R`(BoxComponent) + `AlreadyHitActors` + `AttackDamage` 기반의 완전히 별도인 무기 콜리전 데미지 시스템이 이미 존재함. `Notify_Hit` + `BTT_Attack`의 `CreateBoxHitbox` 경로와는 다른, 보스 전용 독자 메커니즘일 가능성 — 어느 쪽이 실제로 쓰이는 경로인지 정리 필요

## 핵심 구조
- 모든 데미지는 `ARLCharacter::ReceiveCustomDamage` 한 곳을 거침 (근접/장판/투사체 공통) — 데미지 관련 로직은 여기만 고치면 셋 다 적용됨
- 블루프린트 장판형 액터는 `ARLCharacter::ReceiveCustomDamageAtLocation` 또는 `ARLGroundAreaEffect` 상속 후 `DamageData` 참조
- `ERLTeam` 기본값은 Enemy. 플레이어 BP는 클래스 디폴트에서 Player로 바꿔야 함
- 투사체는 데미지 무시와 별개로 `ARLProjectile::OnHit`에서 피격 처리(관통 카운트) 자체도 같은 팀이면 막아야 함
- 포탈은 오버랩 시 `ARLCharacter::SetNearbyPortal`로 등록하고, 플레이어가 E키(`TryInteract`)를 누를 때 `ActivatePortal`을 호출하여 이동
- Unreal MCP는 에디터에서 Auto Start Server 켜져 있어야 동작 (에디터 꺼져 있으면 Claude Code 쪽 MCP 연결 실패함)
- Claude Code는 `.claude/commands/`나 `.claude/agents/`에 파일을 추가해도 실행 중 세션엔 자동 반영 안 됨 — 새 커맨드 추가/수정하면 세션 재시작(`/exit` 후 재실행) 필요
- 에디터 내장 터미널(Experimental, Tools 메뉴)은 한글 폰트 미지원이라 한글 깨짐 — 한글로 Claude Code 쓸 땐 외부 PowerShell 터미널 사용
