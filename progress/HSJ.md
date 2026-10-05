# HSJ
> 최종 갱신: 2026-10-05

## 완료
- **2-Track 로그라이크 세이브 시스템 (영구 메타 + 런 세이브) 구축** (`Source/RogueLikeTopView/Save/`):
  - **세이브 데이터 구조체 및 세이브게임 정의 (`RLSaveGame.h / .cpp`)**:
    - `FRLHealthSaveData`, `FRLStatSaveData`, `FRLInventorySlotSaveData`, `FRLInventorySaveData`, `FRLSkillSlotSaveData`, `FRLSkillSaveData`
    - `FRLRunSaveData` ([Track 2] 런 세이브): 스테이지 번호, 맵 이름, 플레이어 위치/회전 좌표, 체력, 레벨/스탯/경험치, 인벤토리/소지 골드, 장착 스킬
    - `FRLMetaSaveData` ([Track 1] 영구 메타 세이브): 누적 영혼(`TotalSouls`), 최고 클리어 스테이지(`HighestStageCleared`), 영구 특성 업그레이드(`PermanentUpgrades`), 해금 ID 목록(`UnlockedIDs`)
    - `URLSaveGame : public USaveGame`
  - **세이브 총괄 서브시스템 (`RLSaveSubsystem.h / .cpp`)**:
    - `UGameInstanceSubsystem` 기반의 전역 세이브 매니저 (`URLSaveSubsystem::Get(WorldContext)`)
    - `SaveActiveRun`, `LoadActiveRun`, `ApplyRunDataToPlayer`, `ResumeActiveRun`, `ClearActiveRun`
    - 포탈 전이 자동 저장 (`AutoSaveOnStageTransition`) 및 영구 사망 정산 처리 (`HandlePlayerDeath`)
    - 영구 메타 세이브/로드 및 영혼 경제 시스템 (`AddTotalSouls`, `SpendTotalSouls`, `GetTotalSouls`, `AddRunSouls`)
    - 영구 특성/해금 관리 (`GetUpgradeLevel`, `SetUpgradeLevel`, `UnlockID`, `IsIDUnlocked`)
    - UI 연동용 이벤트 델리게이트 4종 (`OnSoulsChanged`, `OnRunSaveCompleted`, `OnRunLoaded`, `OnMetaSaved`)
  - **컴포넌트 복원 메서드 신설**:
    - `RLHealthComponent`: `RestoreHealthDirect`
    - `RLStatComponent`: `RestoreStatsDirect`
    - `RLInventoryComponent`: `RestoreInventoryDirect`
    - `RLSkillComponent`: `GetEquippedSkills`
  - **캐릭터 및 월드 연동 (`RLCharacter.h / .cpp`, `RLPortal.cpp`)**:
    - 스폰 시 자동 이어하기 (`bAutoRestoreRunSaveOnSpawn = true`): 저장된 런이 남아있으면 자동으로 저장 위치 텔레포트 및 스탯/아이템/스킬 복원
    - 포탈 통과 시 다음 스테이지 진입점 자동 저장 연동 (`AutoSaveOnStageTransition`)
    - 사망 시 런 세이브 영구 삭제 및 수집 영혼 메타 정산 연동 (`HandleDeath` -> `HandlePlayerDeath`)
    - 즉시 테스트용 콘솔 명령어 4종 탑재 (`SaveCurrentRun`, `LoadCurrentRun`, `ClearCurrentRun`, `AddSouls`)
  - **UI 담당자를 위한 상세 연동 가이드 완비**:
    - 본 문서 하단 `## 세이브 & 메타 진행 시스템 UI 연동 가이드`에 타이틀 메뉴, HUD, 일시정지, 게임오버, 영구 강화 상점 연동 노드 및 델리게이트 가이드 완비
- **아레나 매니저 진입 시 캐릭터 끼임 및 이동 버그 해결 (`RLArenaManager.h / .cpp`, `RLPlayerController.cpp`)**:
  - `TriggerVolume`의 `ECC_Ground` 충돌 해제로 마우스 커서 바닥 레이캐스트 간섭 제거
  - 자체 차단벽에 Pawn 전용 충돌 프로필 적용 및 진입 딜레이(1.5s), 근접 탈출 텔레포트 안전장치 추가
- **보상 상자 수령 라이프사이클 및 포탈 이동 잔여물 정리 구축** (`Source/RogueLikeTopView/`):
  - **보상 상자 수령 후 소멸 (`ARLRewardChest.h / .cpp`)**:
    - `FinishAndDestroyChest(DelaySeconds)`: 콜리전 즉시 차단(이동 방해 해소), 나이아가라 이펙트 정지, `OnChestClaimed` BP 연출 이벤트 방송 및 `SetLifeSpan` 지연 `Destroy()`
    - `OpenChest()` 시 `URLRewardSelectWidget`에 상자 인스턴스(`this`) 전달 및 위젯 없는 Fallback 환경에서도 자동 소멸 처리
  - **보상 선택 위젯 연동 (`URLRewardSelectWidget.h / .cpp`)**:
    - `SourceChest` 약참조(`TWeakObjectPtr<ARLRewardChest>`) 필드 추가
    - `SelectReward()`에서 카드 선택 완료 시 `SourceChest->FinishAndDestroyChest()` 호출 연동
  - **포탈 이동 및 아레나 잔여물 자동 정리 (`RLPortal.h / .cpp`, `ARLArenaManager.h / .cpp`)**:
    - `ARLPortal`: `LinkedChestToClean` 연결 및 `OnLevelLoaded` 시 혹시 남아있는 상자 자동 파괴, `bDestroyOnUse` 옵션으로 전이 완료 후 포탈 자신도 자동 파괴하여 다음 맵(보스방) 잔존 방지
    - `ARLArenaManager`: 클리어 시 스폰된 상자와 포탈 포인터(`SpawnedRewardChest`, `SpawnedPortal`) 추적, 포탈에 상자 링크 전달, `CleanupArenaSpawnedActors()` 신설 및 `ResetArena()`, `EndPlay()` 연동
- **아레나/전투 구역 총괄 매니저 & 웨이브 스포너 시스템 구축** (`Source/RogueLikeTopView/Arena/`):
  - **C++ 코어 매니저 (`ARLArenaManager.h / .cpp`)**:
    - **전투 영역 감지**: `TriggerVolume`(BoxComponent)에 플레이어 진입 시 자동 전투 개시 (`bAutoStartOnOverlap`)
    - **웨이브 & 무작위 스폰 (`FRLWaveData`, `FRLMonsterSpawnEntry`, `ERLSpawnLocationMode`)**:
      - 다중 웨이브 순차 진행, 개별 몬스터 순차 스폰 간격(`SpawnInterval`), 웨이브 간 대기 딜레이(`DelayBeforeWave`) 지원
      - 전투 구역(`TriggerVolume`) 내 완전 무작위 스폰, 네비메시(NavMesh) 안전 바닥 안착, 플레이어 조준 스폰
    - **처치 카운팅 및 이벤트 연동**:
      - 몬스터 스폰 시 `ARLEnemyCharacter::OnEnemyDied` 델리게이트 자동 바인딩으로 잔여 몬스터 실시간 카운팅
    - **자체 입구 차단벽 & 문(Barrier) 제어**:
      - 자체 내장 `EntranceBarrier`(BoxComponent) 지원: 전투 전엔 통과 가능, 전투 시작 시 콜리전 ON(퇴로 차단), 클리어 시 콜리전 OFF(문 개방)
      - 외부 문 액터 연동(`BarrierActors`) 및 BP 연출 훅(`BP_OnBarriersLocked / Unlocked`) 제공
    - **클리어 보상 및 포탈 연동**:
      - 모든 웨이브 클리어 시 [`ARLRewardChest`](file:///C:/Unreal%20Projects/TopViewRogueLike/Source/RogueLikeTopView/Reward/Public/RLRewardChest.h) 자동 스폰
      - 출구 [`ARLPortal`](file:///C:/Unreal%20Projects/TopViewRogueLike/Source/RogueLikeTopView/Combat/Public/RLPortal.h) 스폰 또는 기존 포탈 활성화
    - **델리게이트 5종**: `OnArenaStateChanged`, `OnWaveStarted`, `OnEnemyCountChanged`, `OnWaveCleared`, `OnArenaCleared`
    - **테스트 치트 콘솔 명령어 3종**: `TestStartArena`, `TestClearCurrentWave`, `TestKillAllEnemies`
  - **빌드 설정 (`RogueLikeTopView.Build.cs`)**:
    - `Arena/Public` 인클루드 경로 등록 및 VS 프로젝트 파일 갱신 (`GenerateProjectFiles`) 완료
- **경험치/레벨업 & 필드 드롭 & 몬스터 차등 파밍 시스템 구축** (`Source/RogueLikeTopView/`):
  - **경험치 및 레벨업 시스템 (`URLStatComponent.h / .cpp`)**:
    - `CurrentLevel`, `CurrentExp`, `MaxExp`, 점진적 요구 경험치 배율(`ExpGrowthMultiplier` = 1.25)
    - `AddExp(Amount)`: 경험치 누적 및 다중 레벨업 루프 판정
    - 델리게이트 2종: `OnExpChanged(Current, Max, Percent)`, `OnLevelUp(NewLevel)`
    - 캐릭터 연동(`ARLCharacter::HandleLevelUp`): 레벨업 시 체력 100% 회복 + 3지선다 보상 카드 팝업(`TestOpenRewardUI`) + 연출 훅(`OnCharacterLevelUp`)
    - 즉시 테스트 치트: `TestAddExp [Amount]`
  - **필드 드롭 및 자석 루팅 액터 (`ARLItemDrop.h / .cpp`)**:
    - 몬스터 사망/상자 개봉 시 퐁 튀어나오는 포물선 점프 연출(`LaunchDrop`)
    - 바닥 안착 후 둥둥 떠서 제자리 회전 연출
    - 자석 시스템(`MagnetSphere`): 플레이어 접근 시 가속하여 날아와 인벤토리에 자동 흡수(`AddGold` / `AddItem`)
  - **몬스터 처치 보상 및 드롭 차등화 (`ARLEnemyCharacter.h / .cpp`)**:
    - 처치 시 플레이어에게 경험치(`ExpReward`) 자동 지급 (일반몹: 30, 보스: 350)
    - **보스 몬스터(`bIsBoss = true`)**: 100% 확정 보상 상자(`ARLRewardChest`) 스폰 + 대량 골드 드롭 확정 분출
    - **일반 몬스터**: `ItemDropChance`(기본 35%) 확률 주사위 기반 필드 골드/아이템(`ARLItemDrop`) 스폰
- **인벤토리 및 아이템 시스템 백엔드(Inventory & Item Backend) 구축** (`Source/RogueLikeTopView/Inventory`):
  - **아이템 타입 및 슬롯 구조 정의 (`RLItemTypes.h`)**:
    - 5종 아이템 분류(`ERLItemType`): 패시브 유물(Passive), 소비 아이템(Consumable), 장비(Equipment), 재료/열쇠(Material), 재화(Currency)
    - 인벤토리 슬롯 구조체(`FRLInventorySlot`): `ItemData`, `Quantity`, 유효성 검사(`IsValid`), 초기화(`Clear`)
  - **아이템 데이터 에셋 (`RLItemDataAsset.h / .cpp`)**:
    - `UPrimaryDataAsset` 기반 아이템 정의 (아이디, 이름, 설명, 아이콘, 희귀도, 최대 스택 수, 골드 가치)
    - 소비 아이템 효과(즉시 체력 회복 `InstantHealAmount`, 퍼센트 회복 `PercentHealAmount`, 사용 시 1개 소모 `bConsumeOnUse`)
    - 패시브 스탯 보너스(`TArray<FRLStatBonus>`) 지원
    - 가상 함수 및 블루프린트 확장 지원 (`UseItem` BlueprintNativeEvent)
  - **인벤토리 컴포넌트 (`RLInventoryComponent.h / .cpp`)**:
    - `UActorComponent` 기반 인벤토리 & 재화 매니저
    - 아이템 추가(`AddItem`): 최대 스택(`MaxStackSize`) 기반 기존 슬롯 자동 병합 + 빈 슬롯 자동 배치
    - 아이템 제거(`RemoveItemAtIndex`, `RemoveItemByID`)
    - 아이템 사용(`UseItemAtIndex`): 효과 발동 및 소모 처리
    - 슬롯 위치 교환(`SwapSlots`): UI 드래그 앤 드롭 지원
    - 재화 관리(`AddGold`, `SpendGold`, `GetGold`)
    - 패시브 아이템 자동 연동: 패시브 아이템 획득/제거 시 `URLStatComponent` 및 `URLHealthComponent`에 스탯 즉시 가감 및 이동속도 동기화
    - UI 연동용 델리게이트 4종: `OnInventoryUpdated`, `OnSlotUpdated`, `OnGoldChanged`, `OnItemUsed`
  - **캐릭터 연동 및 즉시 테스트 치트 (`RLCharacter.h / .cpp`)**:
    - `ARLCharacter` 기본 컴포넌트로 `InventoryComp` 부착 및 `GetInventoryComponent()` 게터 추가
    - 즉시 테스트용 콘솔 치트 3종 탑재:
      - `TestGiveTestItems`: 소비 물약(5개), 공격력 반지(1개), 신속 장화(1개), 루비(3개), 300 골드 즉시 지급
      - `TestUseItemSlot [SlotIndex]`: 특정 슬롯 아이템 사용 테스트
      - `TestAddGold [Amount]`: 골드 증감 테스트
  - **빌드 설정 (`RogueLikeTopView.Build.cs`)**:
    - `Inventory/Public` 인클루드 경로 등록 및 미사용 템플릿 경로 정리
- **로그라이크 3지선다 보상 시스템(Reward System) 구축** (`Source/RogueLikeTopView/Reward`):
  - **보상 데이터 구조 및 희귀도 시스템 (`RLRewardTypes.h`)**:
    - 4단계 희귀도: 일반(Common), 희귀(Rare), 영웅(Epic), 전설(Legendary) 및 희귀도별 고유 컬러(Slate, Blue, Purple, Gold) 지원
    - 10종 강화 타겟(`ERLTargetStat`): 공격력, 주문력, 치명타 확률, 치명타 피해, 이동 속도, 공격 속도, 방어력, 피해 감소율, 최대 체력, 체력 회복
    - 복합 스탯 보너스(`TArray<FRLStatBonus>`) 및 아이콘, 설명, 가중치 지원
  - **보상 데이터 에셋 (`RLRewardDataAsset.h`)**: 에디터에서 보상 풀을 생성하고 튜닝할 수 있는 PrimaryDataAsset 지원
  - **보상 총괄 서브시스템 (`RLRewardSubsystem.h / .cpp`)**:
    - `UGameInstanceSubsystem` 기반으로 레벨 이동 시에도 유지되는 보상 매니저
    - 기본 16종 이상의 완성도 높은 보상 카드 풀 내장 (DataAsset 없이도 즉시 작동)
    - 희귀도 확률 가중치(일반 55%, 희귀 28%, 영웅 13%, 전설 4%) 기반 중복 없는 N개(기본 3개) 무작위 추첨(`RollRewards`)
    - 보상 선택 시 `URLStatComponent` 및 `URLHealthComponent` 즉시 반영, `CharacterMovement->MaxWalkSpeed` 자동 동기화, 피드백 온스크린 메시지 출력
  - **필드 보상 상자 액터 (`RLRewardChest.h / .cpp`)**:
    - 필드/웨이브 클리어 지점에 스폰되는 상호작용 가능 상자
    - 플레이어 접근 시 감지하여 HUD 상호작용 프롬프트 표시
    - F키 상호작용(`OpenChest`) 시 뚜껑 오픈 연출과 함께 게임 일시정지 및 3지선다 카드 선택창(`WBP_RewardSelect`) 팝업
  - **보상 선택 위젯 C++ 베이스 (`RLRewardSelectWidget.h / .cpp`)**:
    - 안전한 보상 선택을 위한 게임 일시정지(`SetGamePaused`) 및 마우스 커서/UI 입력 모드 자동 전환
    - 카드 클릭 시 서브시스템을 통해 보상 적용 후 게임 자동 재개 및 위젯 소멸
  - **기존 시스템 연동 및 개선**:
    - `URLHealthComponent`: `Heal(HealAmount)`, `IncreaseMaxHealth(Amount, bHealCurrent)` 추가
    - `URLStatComponent`: 스탯 변경 이벤트 `OnStatsChanged` 델리게이트 및 `NotifyStatsChanged()` 추가
    - `ARLCharacter`:
      - `SetNearbyChest`, `GetNearbyChest` 추가
      - `TryInteract()`에서 상자 우선 열기 / 포탈 이동의 유기적 분기 처리
      - `OnStatsChanged` 수신 시 `MaxWalkSpeed = StatComp->MoveSpeed` 자동 갱신
      - 즉시 테스트용 치트 함수 `TestOpenRewardUI()`(콘솔 실행 가능) 탑재
    - `ARLEnemyCharacter`:
      - `bDropChestOnDeath`, `DropChestClass`, `DropChance`, `ChestSpawnOffset` 추가
      - 몬스터 사망(`HandleDeath`) 시 발밑에 보상 상자 확정/확률 스폰 연동
      - 웨이브 매니저 연동용 `OnEnemyDied` 델리게이트 방송 추가
- 스탯 컴포넌트(`URLStatComponent`) 신설 (`Source/RogueLikeTopView/Stats`)
  - 핵심 스탯 8종 정의: 공격력(`AttackPower`), 주문력(`AbilityPower`), 치명타 확률(`CritChance`), 치명타 피해 배율(`CritDamageMultiplier`), 이동 속도(`MoveSpeed`), 공격 속도(`AttackSpeed`), 방어력(`Defense`), 피해 감소율(`DamageReduction`)
- 치명타(Critical Hit) 판정 및 피해 증폭 공식 구현 (`URLStatComponent::EvaluateDamage`)
  - 확률 주사위 기반 치명타 판정 및 치명타 피해 배율(`CritDamageMultiplier`) 적용
  - `URLSkillComponent::CalculateFinalDamage`에서 AD/AP 스케일링 기본 데미지에 치명타 반영
  - 치명타 발동 시 넉백 강도 1.25배 증폭 (`ARLCharacter::ReceiveCustomDamage`)
  - `FRLDamageData`에 `bIsCritical` 플래그 추가 (UI/이펙트 분기용)
- 방어력(Defense) 및 받는 피해 감소(DamageReduction) 시스템 구현
  - `URLStatComponent::CalculateDefendedDamage`: 점근적 방어 공식 `100 / (100 + Defense)` 적용 (유효 체력 선형 증가, 무적 버그 방지)
  - 퍼센트 피해 감소율(`DamageReduction`, 0%~80% 클램프) 추가 감쇄 적용
  - 최소 1.0f 데미지 보장 및 감쇄량(`OutReducedDamage`) 계산
  - `FRLDamageData`에 `bTrueDamage`(방어력 무시 고정 피해) 플래그 추가
  - `ARLCharacter::ReceiveCustomDamage`에서 피격자의 `StatComp`를 거쳐 방어력 적용 및 디버그 메시지 가시화 (`(방어 감쇄: -X.X)`)
- `WBP_HUD` 스탯 연동 및 온스크린 디버그 출력

## 핵심 구조
- **보상 파이프라인 (상자 상호작용 -> 3지선다 선택 -> 스탯 반영)**:
  1. 플레이어가 보상 상자(`ARLRewardChest`) 반경 진입 시 `SetNearbyChest` 등록 -> HUD 프롬프트 활성화
  2. 플레이어가 F키 입력(`TryInteract`) -> `OpenChest` 호출 -> 상자 뚜껑 오픈
  3. `URLRewardSubsystem::RollRewards(3)`로 희귀도 가중치 기반 중복 없는 카드 3장 추첨
  4. `RLRewardSelectWidget` 생성 및 화면 표시 -> 게임 일시정지(`SetGamePaused(true)`) 및 마우스 커서 활성화
  5. 카드 선택 시 `URLRewardSubsystem::ApplyReward` 실행 -> `StatComp` / `HealthComp` 스탯 갱신 -> `OnStatsChanged` 방송으로 이동속도/스탯창 자동 동기화 -> 게임 재개
- **에디터 및 테스트**:
  - `TestOpenRewardUI` 콘솔 명령어 또는 함수 호출로 상자 없이도 언제든 3지선다 팝업 즉시 테스트 가능
  - `Content/Blueprint/UI/WBP_RewardSelect` 위젯 생성 시 자동으로 상자와 연동

---

## 💾 세이브 & 메타 진행 시스템 UI 연동 가이드 (for UI 담당자)

> **핵심 C++ 클래스:** `URLSaveSubsystem` (GameInstanceSubsystem), `URLSaveGame`  
> **접근 방법:** 위젯 블루프린트 우클릭 후 `Get RLSaveSubsystem` 노드 하나로 모든 기능에 접근 가능

### 1. 시스템 핵심 요약
- **2-Track 저장 구조:**
  1. **[Track 1] 영구 메타 세이브 (`RL_MetaSave.sav`)**: 사망해도 보존되는 계정 누적 영혼(`Souls`), 최고 클리어 스테이지, 영구 해금/특성 레벨.
  2. **[Track 2] 런 세이브 (`RL_RunSave.sav`)**: 1회차 런 진행 데이터(현재 스테이지, 플레이어 위치, HP, 스탯, 인벤토리, 스킬). **포탈 탑승 시 자동 저장**되며, **사망 시 영구 삭제(초기화)**.
- **자동 이어하기 지원:** 저장된 런이 남아있으면 게임 시작 시 **저장 좌표 및 스탯/아이템 상태로 자동 복원**.

### 2. UI 화면별 연동 가이드

#### ① 타이틀 / 메인 메뉴 (`WBP_TitleMenu`)
- **[이어하기 (Continue)] 버튼:**
  - **버튼 활성화 판정:** `Get RLSaveSubsystem` → **`HasActiveRun`** 호출 (True면 활성화, False면 비활성화 `SetIsEnabled`)
  - **이어하기 정보 표시 (선택):** `Get Active Run Data` 노드를 풀어 `StageNumber`(현재 스테이지), `SaveTimestamp`(저장 일시) 텍스트 바인딩
  - **버튼 클릭 시:** `Get RLSaveSubsystem` → **`ResumeActiveRun`** 호출! (저장된 맵 자동 오픈 및 플레이어 좌표/상태 복원)
- **[새 게임 (New Game)] 버튼:**
  - **버튼 클릭 시:** `Get RLSaveSubsystem` → **`ClearActiveRun(true)`** 호출 (이전 런 세이브 삭제) 후 1스테이지 레벨 오픈 (`OpenLevel`).

#### ② 인게임 HUD (`WBP_HUD`)
- **영구 영혼(Souls) 재화 실시간 표시:**
  - 초기값: `Get RLSaveSubsystem` → **`GetTotalSouls`** 호출 후 텍스트 설정
  - 실시간 변화 감지: `Get RLSaveSubsystem` → **`Assign OnSoulsChanged`** 바인딩 (`NewTotal`, `DeltaAmount` 수신하여 애니메이션/숫자 갱신)
- **현재 스테이지 표시:**
  - `Get RLSaveSubsystem` → `Get Active Run Data` → `StageNumber` 조회

#### ③ 일시정지(Pause / ESC) 메뉴 (`WBP_PauseMenu`)
- **[저장하고 타이틀로 나가기] 버튼:**
  1. `Get Player Character` (ARLCharacter) 가져오기
  2. 현재 맵 이름 가져오기 (`Get Current Level Name`)
  3. `Get RLSaveSubsystem` → **`SaveActiveRun(PlayerCharacter, CurrentLevelName, StageNumber)`** 호출
  4. 타이틀 맵(`Map_Title`)으로 이동

#### ④ 게임오버 / 사망 팝업 (`WBP_GameOver`)
- 플레이어 사망 시 C++ 내부에서 `HandlePlayerDeath`가 자동 실행되어 이번 런 세이브가 삭제되고, 이번 런에서 얻은 영혼은 메타로 입금됨.
- **결과창 텍스트:** `Get Active Run Data`의 `RunSouls`를 읽어 *"이번 런에서 수집한 영혼: +X"* 출력 가능.
- **[타이틀 / 로비로 돌아가기] 버튼:** 타이틀 맵 오픈. 다음 시작 시 자동으로 1스테이지 시작 지점에서 1레벨로 시작.

#### ⑤ 영구 강화 / 메타 상점 UI (`WBP_MetaUpgrade`)
- **`GetTotalSouls`**: 영구 영혼 보유 잔액 반환 (`int32`)
- **`SpendTotalSouls(Amount)`**: 영혼 소비 (`bool`). 잔액 부족 시 false(구매 실패), 충분 시 차감 후 자동 저장 및 true 반환
- **`AddTotalSouls(Amount)`**: 영구 영혼 지급
- **`GetUpgradeLevel(UpgradeKey)`**: 해당 특성(예: `"MaxHealth_Up"`, `"Atk_Up"`)의 현재 강화 단계 반환 (`int32`, 기본 0)
- **`SetUpgradeLevel(UpgradeKey, Level)`**: 해당 특성 강화 단계 저장 (자동 디스크 저장)
- **`IsIDUnlocked(InID)`** / **`UnlockID(InID)`**: 특정 캐릭터, 무기, 유물 ID 해금 여부 확인 및 해금 처리

### 3. UI 갱신용 이벤트 델리게이트 목록
| 델리게이트 이름 | 파라미터 | 발생 시점 & UI 활용 추천 |
| :--- | :--- | :--- |
| **`OnSoulsChanged`** | `NewTotal` (int32)<br>`DeltaAmount` (int32) | 영구 영혼 획득 또는 소비 시 즉시 발생. HUD 상단 영혼 카운터 숫자 갱신 및 획득 애니메이션 연출 |
| **`OnRunSaveCompleted`**| `bSuccess` (bool) | 런 세이브 완료 시 (화면 구석에 *"자동 저장 완료..."* 아이콘 팝업 연출) |
| **`OnRunLoaded`** | `bSuccess` (bool) | 런 로드 완료 시 발생 |
| **`OnMetaSaved`** | `bSuccess` (bool) | 영구 메타 데이터 저장 완료 시 발생 |

### 4. 테스트 콘솔 치트 명령어
- **`SaveCurrentRun`**: 현재 플레이어 위치, HP, 인벤토리, 스탯을 즉시 저장
- **`LoadCurrentRun`**: 저장된 런을 불러와 현재 위치로 텔레포트 및 스탯 복원
- **`ClearCurrentRun`**: 런 세이브를 즉시 삭제 (초기화 테스트용)
- **`AddSouls 500`**: 영구 영혼 재화 500개 즉시 지급 (상점 구매 UI 테스트용)
- **`TestGiveTestItems`**: 물약, 장신구, 장화, 루비, 300골드 즉시 지급

