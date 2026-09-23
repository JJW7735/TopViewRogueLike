# HSJ
> 최종 갱신: 2026-09-23

## 완료
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
