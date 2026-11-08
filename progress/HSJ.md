# HSJ
> 최종 갱신: 2026-11-08

## 완료
- **BGM 페이드아웃 끊김 버그 및 재시작 입력 모드 수정** (`Source/RogueLikeTopView/`):
  - **BGM 페이드아웃 조기 중단 버그 수정 (`RLAudioSubsystem.cpp`)**:
    - `EvaluateWorldMusicState()`가 0.2초 주기로 `StopBGM()`을 재호출할 때 페이드아웃 중이던 `FadingOutBGMComp`를 즉시 파괴하던 문제를 수정하여, 보스 처치/사망 시 설정된 페이드아웃 시간(2.0~2.5초) 동안 부드럽게 감쇠되도록 개선
  - **수동/콘솔 BGM 트랙 온디맨드 로드 지원 (`RLAudioSubsystem.cpp`)**:
    - 시작 시에는 필수 트랙 3종만 로드하되, `RL.PlayBGM` 콘솔 명령어나 수동 호출로 나머지 트랙(2, 3, 5, 6, 7, 8, 10번) 재생 요청 시 온디맨드로 로드 및 캐싱하도록 보완
  - **사망 후 재시작 시 마우스 커서/입력 모드 일관성 수정 (`RLSaveSubsystem.cpp`, `RLGameOverWidget.cpp`)**:
    - `RestartRunAfterDeath` 및 `URLGameOverWidget::RestorePlayerInput`에서 탑뷰 기본 입력 모드(`bShowMouseCursor = true`, `FInputModeGameAndUI`)로 복원하도록 통일
  - **보스 승리 시 이번 런 획득 영혼(`RunSouls`) 영구 정산 연동 (`RLPlayerController.cpp`, `RLSaveSubsystem.cpp`)**:
    - `SaveBossVictoryRunProgress`에서 보스 보상 수령/승리 저장 시 누적된 `RunSouls`를 `TotalSouls`로 즉시 입금하고 `RunSouls = 0`으로 비워 승리 후 영혼 누락 및 중복 입금 방지
- **코드베이스 리팩토링 및 중복 호출 버그 수정** (`Source/RogueLikeTopView/`):
  - **피격/사망 연출 2중 호출 버그 수정 (`RLDamageNumberSubsystem.cpp`)**:
    - `URLDamageNumberListener::HandleHealthChanged`에서 `URLCombatFeedbackSubsystem`의 `NotifyCharacterDamaged` 및 `NotifyCharacterDied`를 중복 호출하던 문제를 제거하고, `URLCombatFeedbackListener`가 전담하도록 일원화
  - **오디오 서브시스템 불필요 로드 및 미사용 코드 정리 (`RLAudioSubsystem.h / .cpp`)**:
    - 미사용 함수 `SelectExplorationTrackForMap`, 미사용 사운드 배열(`KhaimeraLandSounds`, `SevarogLandSounds`) 및 실제로 재생되지 않던 BGM 큐 5종 사전 로드 제거
  - **중복 로직 헬퍼 통합 및 데드 코드 제거 (`RLCharacter`, `RLArenaManager`, `RLSaveSubsystem`, `RLRewardTypes`, `RLShopTypes`, `RLPortal`)**:
    - `ARLCharacter`: 3곳에 중복되던 `AttackIndicator` 지연 스폰 로직을 `EnsureAttackIndicator()`로 통합, 미사용 변수(`bIsHoldingRightClick`)·빈 오버라이드(`SetupPlayerInputComponent`)·불필요한 `HandleStatsChanged` 제거
    - `ARLArenaManager`: `CompleteArena`와 `CheckInitialPlayerOverlap`에 중복되던 포탈 활성화/스폰 코드를 `SpawnOrActivateExitPortal()`로 통합
    - `URLSaveSubsystem`: `GetAllMetaUpgradeInfos`와 `GetMetaUpgradeInfo`의 구조체 생성 로직을 `BuildMetaUpgradeInfo()`로 통합
    - `FRLRewardData` & `FRLShopItem`: 희귀도별 색상/표시명 switch 문을 `FRLRewardData::GetRarityColor(ERLRarity)` / `GetRarityDisplayName(ERLRarity)` 정적 함수로 일원화
    - `ARLPortal`: 주석 처리된 레거시 오버랩 텔레포트 코드 정리
- **오디오/BGM 서브시스템 구축 및 전투 SFX 자동 매칭 시스템 완성** (`Source/RogueLikeTopView/Combat/`, `Content/ARPGMusicPack/`, `Content/RPG_Fantasy_SFX/`):
  - **전역 오디오 서브시스템 (`URLAudioSubsystem.h / .cpp`)**:
    - `UGameInstanceSubsystem` 기반 BGM 및 환경/전투/UI 사운드 총괄 매니저
    - **상황별 BGM 크로스페이드 재생 (`ARPGMusicPack`)**: 메인 메뉴/탐험(`PlayExplorationMusic`), 일반 아레나 전투(`PlayArenaBattleMusic`), 중간보스(`PlayMiniBossMusic`), 최종 보스(`PlayBossMusic`), 승리/사망 징글 자동 전환 및 페이드 인/아웃 지원
    - **보스 애니메이션 몽타주 SFX 자동 감시 (`TickMontageAudioWatcher`)**: 보스1(Khaimera)·보스2(Sevarog)의 주요 공격/포효/투척/강습 몽타주 재생 구간을 실시간 감지하여 별도 애님 노티파이 없이도 타격·기합·발걸음 사운드 자동 재생
    - **아레나/보스 자동 연동**: `ARLArenaManager` 전투 시작/클리어 델리게이트 및 보스/중간보스 교전 상태에 맞춰 BGM 자동 전환
  - **전투 타격음/스킬 시전음 자동 매칭 및 볼륨 밸런싱 (`RLCombatFeedbackSubsystem.h / .cpp`, `RPG_Fantasy_SFX`)**:
    - 스킬 시전 타입(검기/대시/폭발/원소) 및 타격 속성(물리/화염/빙결/번개/암흑/신성/독)에 따라 `RPG_Fantasy_SFX` 사운드 풀에서 최적의 효과음을 자동 매칭 및 피치/볼륨 랜덤화 재생
    - 전체 BGM 및 전투/피격/스킬 SFX 간 오디오 볼륨 밸런스 튜닝
- **전투 타격감(Combat Feedback) 서브시스템 및 스킬/속성별 고유 VFX 자동 연동** (`Source/RogueLikeTopView/Combat/`, `Skill/`):
  - **전투 타격감 총괄 서브시스템 (`URLCombatFeedbackSubsystem.h / .cpp`)**:
    - `UWorldSubsystem` 기반으로 월드 내 모든 캐릭터(`ARLCharacter`, `ARLEnemyCharacter`)의 체력 변화(`URLCombatFeedbackListener`)를 자동 감지하여 코드 수정 없이 즉각적인 타격 피드백 제공
    - **히트스탑(Hit-Stop / Frame Freeze)**: 타격/치명타/처치 시 역동적인 순간 타임 딜레이션 연출
    - **절차적 카메라 쉐이크(Camera Shake)**: 피해량·치명타·보스 피격 여부에 비례하는 카메라 진동 적용
    - **피격 머티리얼 플래시 & 넉백/경직 연출**: 피격 시 순간 발광(Emissive Flash) 및 타격 방향 기반 피격 리액션
    - **치명타 데미지 폰트 연출 연동 (`RLDamageNumberSubsystem.h / .cpp`)**: 치명타 적중 시 데미지 팝업 폰트 크기·색상·강조 연출 차별화
  - **스킬 시전 및 속성별 고유 타격 이펙트(VFX/광원) 자동 연동 (`RLCombatFeedbackSubsystem`, `RLGameplayAbility_Skill`, `RLSkillAbilities`, `RLSkillData`)**:
    - 스킬 시전(검기 Slash / 대시 Dash / 폭발 Explosion) 시 이동 궤적·충격파 VFX 및 다이내믹 포인트 라이트(광원) 자동 생성
    - 공격/스킬의 속성(물리/화염/빙결/번개/비전 등)에 따라 고유 색상 광원 및 타격 파티클/나이아가라 이펙트 자동 스폰
- **신규 스테이지 맵 3종(`L_WarCrypt`, `L_OuterWard`, `L_VesselsHeart`) 및 신규 몬스터/중간보스 구축** (`Content/Maps/`, `Content/Blueprint/Enemy_Character/`):
  - **신규 던전/아레나 맵 3종 구축 및 내비게이션 설정**:
    - `L_WarCrypt` (`Content/Maps/L_WarCrypt.umap`, `Content/WarCrypt/`): 전쟁 납골당 스테이지 에셋, 스태틱 메시/머티리얼 세팅 및 NavMesh 구축
    - `L_OuterWard` (`Content/Maps/L_OuterWard.umap`, `Content/OuterWard/`): 외곽 성채 스테이지 에셋 배치 및 환경/콜리전 설정
    - `L_VesselsHeart` (`Content/Maps/L_VesselsHeart.umap`, `Content/VesselsHeart/`): 심장부 스테이지 맵 및 전용 배경/구조물 에셋 구축
  - **신규 일반 몬스터 돌진병 (`BP_Enemy_Charger`) 추가**:
    - 전용 데이터 에셋(`DA_Enemy_Charger`), 돌진 몽타주(`AM_Enemy_Charger_Dash`) 및 플레이어 조준 고속 돌진 패턴 구현
  - **중간보스 (`BP_Enemy_MiniBoss`) 추가 및 적 데미지 채널/체력바 갱신 수정**:
    - 중간보스 전용 블루프린트(`BP_Enemy_MiniBoss`), 데이터 에셋(`DA_Enemy_MiniBoss`), 전용 머티리얼 인스턴스(`MI_Manny_MiniBoss_01 / 02`) 구성
    - `DefaultEngine.ini` 콜리전/데미지 트레이스 채널 정비 및 `BP_EnemyBase` 피격 시 체력바 위젯 실시간 갱신 수정
- **주문력(AP) & 물리(AD) 빌드 아이템/축복/보상 카드 1:1 대칭 확장 및 세이브 복원 안전화** (`Source/RogueLikeTopView/Shop/`, `Reward/`, `Save/`, `Character/`):
  - **상점 패시브 유물 풀(`URLShopSubsystem::BuildDefaultPools`) — AD 3종 + AP 5종 추가 (총 AD 5 : AP 5 균형 완성)**:
    - **[일반]** `용병의 전투 단검` (`Item_MercenaryDagger`, 공격력 +12) / `견습 마도사의 고서` (`Item_ApprenticeTome`, 주문력 +12)
    - **[희귀]** `전사의 완력 반지` (공격력 +15, 치명타 +10%) / `비전 마력 반지` (`Item_MageRing`, 주문력 +18, 치명타 +10%)
    - **[희귀]** `추격자의 사냥 각반` (`Item_HunterGreaves`, 공격력 +15, 이속 +40) / `마력 증폭 수정구` (`Item_ManaCrystal`, 주문력 +15, 이속 +40)
    - **[영웅]** `광전사의 투혼 인장` (공격력 +20, 공속 +25%) / `대마법사의 비전 보주` (`Item_ArcaneOrb`, 주문력 +30, 치명타 피해 +25%)
    - **[전설]** `파멸의 군주 대검` (`Item_DoombringerBlade`, 공격력 +55, 치명타 +12%, 치명피해 +30%) / `공허의 마력 왕관` (`Item_ArchmageCrown`, 주문력 +55, 치명타 +12%, 치명피해 +30%)
  - **상점 스탯 축복 풀(`StatPool`) — AD 2종 + AP 3종 추가 (총 AD 4 : AP 4 균형 완성)**:
    - **[희귀]** `투사의 투지` (`Shop_Blessing_FighterSpirit`, 공격력 +18, 최대 체력 +30) / `현자의 깨달음` (`Shop_Blessing_SageWisdom`, 주문력 +18, 최대 체력 +30)
    - **[영웅]** `전장의 지배자` (`Shop_Blessing_WarlordMight`, 공격력 +30, 치명타 +10%) / `비전 폭주` (`Shop_Blessing_ArcaneSurge`, 주문력 +30, 치명타 +10%)
    - **[전설]** `전신의 가호` (기존) / `마법신의 가호` (`Shop_Blessing_ArchmageGrace`, 주문력 +45, 치명타 피해 +35%, 이동 속도 +50)
  - **3지선다 보상 카드 풀(`URLRewardSubsystem::BuildDefaultRewardPool`) — 고등급 AD 1종 + AP 2종 추가 (전 등급 AD:AP 1:1 균형 완성)**:
    - **[영웅]** `검성의 비기` (`SwordmasterSecret`, 공격력 +30, 공격 속도 +15%) / `비전 학자의 비전서` (`ArcaneGrimoire`, 주문력 +28, 치명타 확률 +10%)
    - **[전설]** `아카식 레코드` (`AkashicRecords`, 주문력 +50, 치명타 확률 +15%, 치명타 피해 +50%)
  - **아이템 세이브 복원 및 치트 아이템 GC 안전화 (`RLShopSubsystem`, `RLSaveGame`, `RLSaveSubsystem`, `RLCharacter`)**:
    - `URLShopSubsystem::FindItemByID(ItemID)` 조회 헬퍼 추가
    - `FRLInventorySlotSaveData`에 `ItemID` 필드 추가 및 세션 재시작/맵 이동 시 `TryLoad()` 실패 시에도 `FindItemByID`로 상점 풀에서 안전 복원
    - `ARLCharacter::TestGiveTestItems`가 `ShopSubsystem` 풀의 아이템을 사용하도록 개선하여 맵 이동 시 GC 증발 버그 해결 및 `비전 마력 반지`/`대마법사의 비전 보주` 지급 포함
- **영구 메타 강화(Permanent Upgrades) 백엔드 및 실시간 캐릭터 주입 완성** (`Source/RogueLikeTopView/Save/`, `Character/`):
  - **영구 메타 특성 구조체 (`RLSaveGame.h`)**:
    - `FRLMetaUpgradeInfo`: 특성 키, 이름, 설명, 현재 레벨(0~5), 최대 레벨, 다음 레벨 필요 소울 비용, 현재/다음 보너스 수치, 구매 가능 여부
  - **공식 영구 특성 8종 카탈로그 구축 (`RLSaveSubsystem.cpp`)**:
    - `MaxHealth_Up`(거인의 체질, HP +20), `Atk_Up`(전사의 완력, 공격력 +3), `AP_Up`(비전의 마력, 주문력 +3), `MoveSpeed_Up`(바람의 발걸음, 이속 +15), `Crit_Up`(암살자의 예리함, 치명타 +2%), `Defense_Up`(강철 피부, 방어력 +3), `StartGold_Up`(부유한 시작, 시작 골드 +50G), `SoulGain_Up`(영혼 수확자, 소울 획득량 +15% 증폭)
  - **메타 특성 강화 & 환불 & 조회 API (`RLSaveSubsystem.h / .cpp`)**:
    - `GetAllMetaUpgradeInfos`: 8종 전체 특성의 UI 표시용 데이터 일괄 반환
    - `UpgradeMetaTrait(UpgradeKey)`: 소울 검사/차감, 단계 증가, 디스크 자동 저장 및 `OnMetaUpgradePurchased` 방송
    - `ResetAllMetaUpgrades(bRefundSouls)`: 모든 특성 0단계 초기화 및 소모된 소울 전액 환불
  - **신규 런 시작 시 캐릭터 스탯 자동 주입 (`RLCharacter.cpp`, `RLSaveSubsystem.cpp`)**:
    - `ApplyMetaUpgradesToPlayer`: 8종 특성 레벨을 읽어 `StatComp`, `HealthComp`, `InventoryComp`에 스탯 즉각 반영
    - `ARLCharacter::BeginPlay`: 저장된 런 이어하기가 아닌 신규 런 스폰 시 메타 강화 스탯 자동 반영
    - `RLEnemyCharacter::HandleDeath`: 적 처치 시 `SoulGain_Up` 레벨에 따라 획득 소울 수치 자동 증폭 연동
  - **치트 명령어 3종 (`RLCharacter.h / .cpp`)**:
    - `UpgradeMetaTrait [Key]`: 특정 특성 즉시 강화
    - `ResetMetaUpgrades`: 특성 초기화 및 소울 환불
    - `ShowMetaUpgrades`: 전체 특성 단계 및 보유 소울 화면 출력
- **인게임 상점(Shop) 백엔드 시스템 구축** (`Source/RogueLikeTopView/Shop/`):
  - **상점 데이터 구조 및 델리게이트 (`RLShopTypes.h`)**:
    - `ERLShopItemCategory`: 인벤토리 아이템(`Item`), 즉시 스탯 강화(`StatUpgrade`), 응급 치료(`Heal`)
    - `FRLShopItem`: 슬롯 번호(`SlotIndex`), 상품 분류, `URLItemDataAsset`, 스탯 보너스(`StatBonuses`), 가격(`Price`), 품절 여부(`bIsSoldOut`), 희귀도 테마 색상/이름 반환
    - UI 연동용 델리게이트 3종: `OnShopRefreshed`, `OnShopItemPurchased`, `OnShopRerollCostChanged`
  - **상점 총괄 서브시스템 (`RLShopSubsystem.h / .cpp`)**:
    - `UGameInstanceSubsystem` 기반의 전역 상점 매니저 (`URLShopSubsystem::Get(WorldContext)`)
    - **무작위 4칸 상품 진열 (`GenerateShopItems`)**:
      - 슬롯 0: 소비 아이템(물약/회복제 등 생존 수단 확정)
      - 슬롯 1~2: 패시브 유물 또는 스탯 강화 (희귀도 주사위 기반 무작위 추첨)
      - 슬롯 3: 고급 유물 또는 강력한 스탯/특수 아이템 (희귀/영웅/전설 가중치 우대)
    - **골드 결제 및 지급 (`BuyShopItem`)**:
      - 플레이어 인벤토리 골드(`SpendGold`) 차감 검사
      - 인벤토리 공간/중첩 검사 (아이템인 경우 가득 찼으면 친절한 사유와 함께 실패 반환)
      - 아이템 인벤토리 추가(`AddItem`) 또는 스탯/체력 즉시 반영 + `NotifyStatsChanged()`
      - 품절 처리(`bIsSoldOut = true`) 및 `OnShopItemPurchased` 브로드캐스트
    - **리롤(새로고침) 시스템 (`RerollShop`, `GetRerollCost`)**:
      - 기본 25G에서 리롤 1회당 +15G씩 점진적 증가
      - 골드 차감 후 4칸 신규 상품 재추첨
    - **기본 내장 상품 풀 구축 (`BuildDefaultPools`)**:
      - 7종의 아이템(체력 물약, 상급 비약, 완력 반지, 신속 장화, 수호자 방패, 광전사 인장, 루비) 및 6종의 스탯 강화 풀 탑재
  - **상점 UI 베이스 위젯 (`RLShopWidget.h / .cpp`)**:
    - `WBP_ShopTest`의 위젯들을 `meta = (BindWidgetOptional)`로 자동 바인딩
    - `Txt_Gold` 플레이어 잔액 실시간 동기화 (`OnGoldChanged` 연동)
    - `Btn_Reroll`, `Btn_Exit` 클릭 이벤트 C++ 자동 바인딩
    - 슬롯 1~4번의 이름, 설명, 희귀도 색상, 가격, 품절 상태 자동 갱신
    - 마우스 커서 활성화 및 UI 입력 모드 자동 전환, 닫을 때 안전 원복
    - 블루프린트 연출 훅 5종 제공 (`BP_OnShopOpened`, `BP_OnPurchaseSuccess`, `BP_OnPurchaseFailed`, `BP_OnRerollSuccess`, `BP_OnRerollFailed`)
  - **캐릭터 연동 및 즉시 테스트 치트 (`RLCharacter.h / .cpp`)**:
    - 콘솔 치트 3종 탑재: `TestOpenShop`, `TestBuyShopItem [SlotIndex]`, `TestRerollShop`
  - **세이브 연동 (`RLSaveSubsystem.cpp`)**:
    - `ClearActiveRun` 시 런 종료와 함께 상점 상태 리셋(`ResetShop`) 자동 연동
- **보스 처치 완료 세이브 및 이어하기 스폰 안정화** (`Source/RogueLikeTopView/`):
  - 보스 처치 및 보상 수령 시 활성 런에 클리어 상태(`bIsStageCleared`) 저장
  - 이어하기 시 보스 재소환 차단 및 보스룸 분위기(Mood) 유지
  - 이어하기 시 클리어된 아레나 보상 상자 중복 스폰 버그 방지 및 델리게이트 안전화
- **영혼(Souls) 획득 및 게임오버/재시작 시스템 구축** (`Source/RogueLikeTopView/`):
  - **몬스터 처치 시 영혼 적립 (`RLEnemyCharacter.h / .cpp`)**:
    - `SoulReward` 속성 신설 (일반 몬스터 2, 보스 몬스터 35)
    - 처치 시 `URLSaveSubsystem::AddRunSouls` 호출로 이번 런 임시 영혼 실시간 적립
  - **게임오버 및 재도전 편의 기능 (`RLSaveSubsystem.h / .cpp`)**:
    - `GetLastSettledRunSouls`: 사망 후 정산된 영혼 수치 결과창 조회용 메서드
    - `RestartRunAfterDeath`: 런 세이브 완전 초기화 + 마우스 커서 숨김 / GameOnly 입력 복원 + 레벨 재시작
    - `QuitToMainMenuAfterDeath`: 런 세이브 정리 + 메인 메뉴 오픈
  - **게임오버 결과창 C++ 베이스 위젯 신설 (`RLGameOverWidget.h / .cpp`)**:
    - 생성 시 마우스 커서 활성화 및 UI 입력 모드 자동 전환
    - 정산 영혼, 누적 영혼, 소멸 골드, 도달 스테이지 데이터 바인딩 제공
    - `RetryRun`, `QuitToMainMenu` 기본 탑재
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

#### ④ 게임오버 / 사망 팝업 (`WBP_GameOver` or `URLGameOverWidget`)
- **자동 정산**: 플레이어 사망 시 C++ 내부에서 `HandlePlayerDeath`가 자동 실행되어 이번 런 세이브가 삭제되고, 이번 런에서 얻은 영혼은 메타(`TotalSouls`)로 자동 입금됩니다.
- **결과창 텍스트 바인딩**:
  - 이번 런 획득 영혼: `Get RLSaveSubsystem` → **`GetLastSettledRunSouls`** (사망 직전 런에서 정산된 영혼 수치 반환) 또는 `OnCharacterDeath(SoulsEarnedThisRun)` 핀 사용.
  - 현재 누적 영혼: `Get RLSaveSubsystem` → **`GetTotalSouls`** (합산된 전체 영혼).
- **[다시하기 (Retry)] 버튼**:
  - `Get RLSaveSubsystem` → **`RestartRunAfterDeath`** 호출! (런 세이브 완전 초기화 + 마우스 커서/입력 모드 GameOnly 원복 + 현재 맵 즉시 재시작).
- **[타이틀 / 로비로 (QuitToMain)] 버튼**:
  - `Get RLSaveSubsystem` → **`QuitToMainMenuAfterDeath`** 호출! (런 세이브 완전 초기화 + 메인 메뉴 오픈).
- **C++ 베이스 위젯 지원 (`URLGameOverWidget`)**:
  - 블루프린트에서 `File -> Reparent Blueprint`로 `RLGameOverWidget`을 상속받으면, 마우스 커서 표시 / 입력 모드 자동 전환 및 위의 수치 바인딩/버튼 로직이 C++에서 일괄 지원됩니다.

#### ⑤ 영구 강화 / 메타 상점 UI (`WBP_MetaUpgrade`)
- **`GetAllMetaUpgradeInfos`**: 8종 전체 특성의 구조체 배열(`TArray<FRLMetaUpgradeInfo>`)을 반환합니다. UI에서 `For Each Loop`로 손쉽게 버튼 목록을 생성할 수 있습니다:
  - `DisplayName`: 특성 이름 (예: "거인의 체질", "전사의 완력")
  - `Description`: 특성 설명 (예: "최대 체력이 단계당 +20 증가합니다.")
  - `CurrentLevel`: 현재 강화 단계 (0~5)
  - `MaxLevel`: 최대 단계 (5)
  - `NextLevelCost`: 다음 단계에 필요한 소울 비용
  - `bIsMaxLevel`: 최대 레벨 도달 여부
  - `bCanAfford`: 현재 보유 소울로 구매 가능한지 여부 (`TotalSouls >= NextLevelCost`)
- **`UpgradeMetaTrait(UpgradeKey)`**: 버튼 클릭 시 이 함수 하나만 호출하면 끝! 소울 충분 여부 자동 검사 → 소울 차감(`SpendTotalSouls`) → 단계 상승 및 디스크 자동 저장 → `OnMetaUpgradePurchased` 방송까지 원스톱 처리 (`bool` 반환).
- **`ResetAllMetaUpgrades(bRefundSouls = true)`**: 모든 특성을 0단계로 리셋하고 소모된 소울을 전액 환불합니다.
- **`ApplyMetaUpgradesToPlayer(Player)`**: 런 시작 시 플레이어에게 영구 특성 스탯을 자동 주입합니다 (C++ `BeginPlay`에서 자동 연동 완료).
- **공식 영구 특성 8종 목록**:
  | Key | 이름 | 단계당 효과 | 최대 레벨 | 기본 비용 | 비용 증가량 |
  | :--- | :--- | :--- | :--- | :--- | :--- |
  | **`MaxHealth_Up`** | 거인의 체질 | 최대 체력 +20 | 5 | 30 Souls | +20 / Lv |
  | **`Atk_Up`** | 전사의 완력 | 기본 공격력 +3 | 5 | 40 Souls | +25 / Lv |
  | **`AP_Up`** | 비전의 마력 | 기본 주문력 +3 | 5 | 40 Souls | +25 / Lv |
  | **`MoveSpeed_Up`** | 바람의 발걸음 | 이동 속도 +15 | 5 | 35 Souls | +20 / Lv |
  | **`Crit_Up`** | 암살자의 예리함 | 치명타 확률 +2% | 5 | 50 Souls | +30 / Lv |
  | **`Defense_Up`** | 강철 피부 | 기본 방어력 +3 | 5 | 35 Souls | +20 / Lv |
  | **`StartGold_Up`** | 부유한 시작 | 던전 시작 소지 골드 +50G | 5 | 45 Souls | +25 / Lv |
  | **`SoulGain_Up`** | 영혼 수확자 | 적 처치 시 영혼 획득량 +15% 증폭 | 5 | 60 Souls | +40 / Lv |


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
- **`TestOpenShop`**: 상점 UI 즉시 팝업
- **`TestBuyShopItem 0`**: 0번 슬롯 상품 즉시 구매
- **`TestRerollShop`**: 상점 상품 즉시 새로고침 (골드 소모)

---

## 🛒 상점 시스템(Shop) UI 연동 가이드 (for UI 담당자)

> **핵심 C++ 클래스:** [`URLShopSubsystem`](file:///C:/Unreal%20Projects/TopViewRogueLike/Source/RogueLikeTopView/Shop/Public/RLShopSubsystem.h) (GameInstanceSubsystem), [`URLShopWidget`](file:///C:/Unreal%20Projects/TopViewRogueLike/Source/RogueLikeTopView/Shop/Public/RLShopWidget.h) (UUserWidget)  
> **대상 위젯:** `WBP_ShopTest`

### 1. 초간단 연동 방법 (추천: Reparent Blueprint)
1. `Content/Blueprint/UI/WBP_ShopTest`를 엽니다.
2. 상단 메뉴 `File` -> `Reparent Blueprint`를 클릭합니다.
3. 부모 클래스를 **`RLShopWidget`**으로 선택합니다.
4. **끝!** 아래 기능들이 C++에서 완전 자동으로 작동합니다:
   - `Txt_Gold`: 현재 플레이어 골드 자동 표시 및 실시간 갱신
   - `Btn_Reroll` / `Txt_Reroll`: 클릭 시 골드 차감 및 신규 상품 재추첨, 비용 자동 표시
   - `Btn_Exit`: 클릭 시 상점 닫기 및 마우스 커서/GameOnly 입력 자동 복원
   - `Border_Card1~4`, `Txt_Name1~4`, `Txt_Desc1~4`, `Txt_Rarity1~4`, `Txt_Price1~4`: 4칸의 상품 정보, 희귀도 테두리/텍스트 색상, 품절(SOLD OUT) 상태 자동 갱신
   - 슬롯에 버튼(`Btn_Card1~4`)이 있거나 클릭 시 `BuyItem(슬롯번호)`만 호출하면 구매 완료!

### 2. 블루프린트에서 직접 연동할 때의 노드 (자유 커스텀 시)
- **서브시스템 접근:**
  - `Get RLShopSubsystem` (또는 `Get Shop Subsystem`) 노드 호출
- **상품 목록 가져오기:**
  - `Get Current Shop Items` → `For Each Loop`로 각 슬롯 UI 바인딩
- **상품 구매:**
  - `Buy Shop Item(Buyer, SlotIndex)`
  - Return: 성공(`bool`), 실패 사유(`OutFailReason` - "골드가 부족합니다", "인벤토리가 가득 찼습니다" 등)
- **새로고침(리롤):**
  - `Reroll Shop(Buyer)`
  - 현재 리롤 비용 확인: `Get Reroll Cost`
- **실시간 이벤트 바인딩:**
  - `OnShopRefreshed`: 상품 새로고침 시 UI 갱신
  - `OnShopItemPurchased`: 특정 슬롯 구매 완료 시 품절 연출
  - `OnShopRerollCostChanged`: 리롤 비용 변경 시 버튼 텍스트 갱신


