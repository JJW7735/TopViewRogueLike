# 🎮 RLTV (RogueLikeTopView) 개발 진행 상황 & 프로젝트 가이드

> **최종 갱신일:** 2026-11-05  
> **프로젝트 성격:** 3인 포트폴리오/공모전 탑다운 로그라이크 액션  
> **엔진 버전:** Unreal Engine 5.8  
> **AI 어시스턴트 협업:** Gemini (Antigravity CLI), Claude (Claude Code)

---

## 📌 1. 현재 구현 현황 요약 (Current Implementation)

### 🏟️ 아레나 & 웨이브 스폰 (Arena & Spawner)
- [x] **`ARLArenaManager`**: 방 영역 진입 감지(`TriggerVolume`), 자동 전투 개시(`bAutoStartOnOverlap`)
- [x] **웨이브 스폰 시스템**: 다중 웨이브 데이터(`FRLWaveData`), 개별 순차 스폰 간격(`SpawnInterval`), 웨이브 간 딜레이
- [x] **처치 카운팅 및 연동**: `ARLEnemyCharacter::OnEnemyDied` 실시간 감지 및 남은 적 카운트
- [x] **전투 구역 봉쇄/해제**: 자체 `EntranceBarrier` 박스(전투 시작 시 퇴로 차단, 클리어 시 개방) 및 외부 문 액터 연동
- [x] **클리어 보상 및 포탈 연동**: 모든 웨이브 완료 시 `ARLRewardChest` 확정 스폰 및 출구 `ARLPortal` 활성화/스폰
- [x] **테스트 치트**: `TestStartArena`, `TestClearCurrentWave`, `TestKillAllEnemies`

### 🧑‍🚀 캐릭터 & 조작 (Character & Player)
- [x] **`ARLCharacter`**: 탑다운 기본 이동, 대시/회피(`Dodge`, `StartDashMovement`)
- [x] **무적 및 피격 판정**: 무적 상태 관리(`IsInvulnerable`), 커스텀 데미지 수신(`ReceiveCustomDamage`, `ReceiveCustomDamageAtLocation`)
- [x] **팀/진영 시스템 (`ERLTeam`)**: 플레이어/적군 구분 및 아군 공격 무효화 처리
- [x] **상호작용 시스템**: `IA_Interact` 및 상호작용 키(E) 연동, 주변 포탈 감지(`SetNearbyPortal`, `OnNearbyPortalChanged`, `TryInteract`)
- [x] **히트박스 생성기**: 박스 형태(`CreateBoxHitbox`) 및 위치 기반 범위(`CreateLocationHitbox`)
- [x] **`ARLPlayerController`**: 탑다운 마우스 커서 조준, Enhanced Input 처리 및 상호작용 입력 바인딩

### 🧙‍♂️ 소서리스 (Sorceress) 플레이어블 캐릭터 & 스킬
- [x] **캐릭터 모델 & 애니메이션**: 마법사 메시(`SK_Wizard`), 애니메이션 블루프린트(`wizard_ABP_RLCharacter`), 이동/대기 애니메이션(Idle, Walk, Sprint), IK Rig(`IK_Wizard`, `RTG_Wizard`)
- [x] **전용 스킬 셋업 (DataAsset & BP)**:
  - **기본 공격**: 투사체 기반 평타 (`DA_Attack_Sorceress`, `soceress_AM_Sorceress_Attack`)
  - **Q 스킬 (파이어볼)**: 단일/폭발 투사체 (`DA_Skill_Sorceress_Fireball`, `BP_FireBall`, `soceress_AM_Sorceress_Q`)
  - **W 스킬 (인페르노)**: 장판형 범위 지속 피해 (`DA_Skill_Sorceress_Inferno`, `BP_Sorceress_W_Field`, `soceress_AM_Sorceress_E`)
  - **E 스킬 (라이트닝)**: 전방 집중 홀드/채널링 타격 (`DA_Skill_Sorceress_Lightning`, `soceress_AM_Sorceress_E`)
  - **R 스킬 (익스플로전/메테오)**: 원형 범위 지면 폭발 (`DA_Skill_Sorceress_Explosion`, `BP__Sorceress_MeteorArea`, `soceress_AM_Sorceress_R`)

### ⚔️ 전투 & 투사체 (Combat)
- [x] **`URLHealthComponent`**: 체력, 방어, 사망 처리 및 데미지 이벤트
- [x] **`ARLProjectile`**: 스킬/공격 투사체 로직, 충돌 처리 및 아군 충돌 시 관통 카운트 소모 없이 통과하는 보호 로직
- [x] **`ARLGroundAreaEffect`**: 장판형 스킬(인페르노/메테오) 공용 베이스 액터, 스탯 스케일링 데미지(`DamageData`) 전달
- [x] **`ARLDecalIndicator`**: 스킬 시전/몬스터 공격 예고 장판(Decal) 표시
- [x] **`ARLPortal`**: 상호작용(E키) 기반 맵 전환 및 레벨 스트리밍 비동기 로딩 연동 액터
- [ ] **보스 근접 공격이 항상 씹히는 버그 (진행중, 아래 작업 히스토리 참고)**

### ⚡ 스킬 & 스탯 (Skill & Stats)
- [x] **`URLSkillComponent` & `URLSkillData`**: 데이터 기반 스킬 쿨다운, 시전 타입, 투사체/근접/장판 발동
- [x] **AD/AP 스탯 스케일링 연동 버그 수정**: 대시 타격, 홀드 틱, 장판 스킬 등 모든 스킬 판정에 최종 계산 데미지(`CalculateFinalDamage`) 적용
- [x] **`URLStatComponent`**: 공격력, 이동속도, 쿨다운 감소 등 런타임 스탯 관리

### 🎒 인벤토리 & 아이템 (Inventory & Item)
- [x] **`URLInventoryComponent`**: 슬롯 기반(기본 20칸) 아이템 보관, 스택 병합, 골드 관리
- [x] **`URLItemDataAsset`**: 5종 아이템 분류(패시브 유물, 소비, 장비, 재료, 재화), 체력 회복 및 패시브 스탯 자동 연동
- [x] **경험치 및 레벨업 시스템 (`URLStatComponent`)**: 처치 경험치 획득, 레벨업 시 체력 100% 회복 및 3지선다 보상 창 자동 팝업
- [x] **필드 드롭 액터 (`ARLItemDrop`)**: 포물선 점프, 바닥 안착 후 둥둥 떠서 회전, 자석(Magnet) 플레이어 흡수 루팅
- [x] **몬스터 처치 보상 차등화 (`ARLEnemyCharacter`)**: 일반 몬스터 확률 드롭(35%) vs 보스 100% 확정 보상 상자 + 대량 골드 드롭
- [x] **UI 연동 델리게이트 4종**: `OnInventoryUpdated`, `OnSlotUpdated`, `OnGoldChanged`, `OnItemUsed`
- [x] **인벤토리 UI 연동 가이드 문서화**: [`progress/UI_INVENTORY_GUIDE.md`](progress/UI_INVENTORY_GUIDE.md)

### 🖥️ UI & 입력 (UI / UX)
- [x] **`WBP_HUD`**: 기본 HUD 구성 및 상호작용(포탈) 안내 UI 연동
- [x] **`IA_Interact` / `IMC_Default`**: 상호작용 키(E) 매핑

### 👾 AI & 몬스터 (AI)
- [x] **`URLMonsterPatternDataAsset`**: 몬스터 패턴 정의용 DataAsset 기초
- [x] **`BP_TrainingDummy`**: 훈련용 더미 및 피격 테스트 환경 구성

---

## 📋 2. 다음 개발 우선순위 (Next Milestones)

- [ ] **보스 근접 공격 버그 수정** (최우선 — 아래 작업 히스토리 2026-09-23 참고)
- [ ] **절차적 맵 생성 (Procedural Dungeon / Room Generation)**
  - 방(시작방, 전투방, 상점방, 보스방) 템플릿 연결 시스템 또는 레벨 스트리밍 결합
  - 방 입장 시 문 닫힘 및 몬스터 처치 시 포탈/문 개방 연동
- [ ] **몬스터 AI 및 스폰 시스템 (AI & Spawner)**
  - Behavior Tree / Blackboard를 활용한 적 추적 및 패턴 실행
  - 방 입장 시 웨이브/위치별 몬스터 동적 스폰
- [ ] **로그라이크 보상 & 퍽(Perk) 시스템**
  - 방 클리어 후 3지선다 보상 선택 UI 및 스탯/스킬 강화 로직
- [ ] **UI/UX (UMG) 고도화**
  - 플레이어 체력바, 스킬 쿨다운 슬롯, 보스 체력바, 게임 오버/클리어 화면

---

## 👥 3. 3인 팀 협업 수칙 (Gemini & Claude 공통)

1. **바이너리 에셋(`.uasset`, `.umap`) 동시 작업 금지**
   - C++은 머지가 되지만 블루프린트/맵은 Git 충돌 해결이 불가능합니다.
   - 작업 시작 전 **누가 어떤 BP/레벨을 만질지 반드시 공유**하고 작업하세요.
2. **C++ 뼈대 + 블루프린트 데이터 주입 패턴 유지**
   - 로직, 수치 계산, 상태 처리는 C++에 작성
   - 외형 메시, 이펙트(VFX), 사운드(SFX), 기본 튜닝 값은 블루프린트 디테일 패널에서 설정
3. **새 C++ 폴더 추가 시 `Build.cs` 필수 수정**
   - 언리얼 UBT는 중첩 폴더를 자동 인클루드하지 않으므로 `RogueLikeTopView.Build.cs`의 `PublicIncludePaths`에 등록해야 컴파일됩니다.
4. **한글 주석 인코딩**
   - `.cpp`/`.h` 파일에 한글 주석 작성 시 반드시 **UTF-8 (BOM 포함)**으로 저장할 것.

---

## 📝 4. 작업 히스토리 (Work Log)

| 2026-11-05 | HSJ (Gemini) | 전투 타격감(Combat Feedback) 서브시스템 구축 (URLCombatFeedbackSubsystem: 역경직 Hit-Stop, 프로시저럴 카메라 셰이크 & 줌 펀치, 피격 메시 Squash & Stretch 탄성 및 오버레이/광원 플래시, URLSkillData 타격 VFX/시전·타격 SFX 확장 및 기본 파티클 폴백 연동, URLDamageNumberSubsystem 치명타 전용 골드/오렌지 대형 팝업 연출 연동) |
| 2026-10-01 | HSJ (Gemini) | 보상 상자 수령 시 소멸 라이프사이클 및 포탈 전이 시 잔여 액터 자동 정리 구현 (카드 선택 시 `ARLRewardChest` 콜리전/FX 즉시 차단 및 지연 Destroy, 포탈 이동 시 이전 아레나 상자 및 포탈 자체 파괴 처리, `ARLArenaManager` 생성 액터 추적 및 정리 함수 추가) |
| 2026-09-29 | HSJ (Gemini) | 아레나/전투 구역 총괄 매니저 & 웨이브 스포너 구축 (`ARLArenaManager`: TriggerVolume 영역 감지, 다중 웨이브 및 순차 스폰 `FRLWaveData`, 문 폐쇄/개방 `BarrierActors`, 처치 카운팅 `OnEnemyDied` 연동, 클리어 시 `ARLRewardChest` & `ARLPortal` 자동 스폰, 테스트 치트 3종 탑재) |
| 2026-09-28 | HSJ (Gemini) | 인벤토리 & 아이템 시스템 백엔드 구축 (`URLInventoryComponent`, `URLItemDataAsset`, 5종 아이템 분류, 골드 관리, 패시브 스탯 자동 연동, 테스트 치트 3종 탑재, UI 연동 가이드 문서화: `progress/UI_INVENTORY_GUIDE.md`) |
| 2026-09-23 | JJW (Claude) | 보스 근접 공격이 항상 씹히는 버그 원인 추적: `AM_Boss_Swing` 몽타주가 재생 시작 ~1초 만에 `Montage.Stop`으로 강제 종료됨을 로그로 확인 (`Notify_Hit` 도달 전에 끊김). BT의 `OnCompleted`가 노티파이 여부와 무관하게 `FinishExecute(true)`로 연결돼있어 겉보기엔 정상 흐름처럼 보였던 것도 확인. BT observer 데코레이터(`BTDecorator_Blackboard`) 재평가로 인한 abort가 유력 용의자 — 라이브 BT 디버거로 공격 애니메이션 재생 "중" abort 여부 확인 필요 (진행중, 내일 이어서). 추가로 `BP_Boss`에 `AttackCollision_L`/`AttackCollision_R` + `AlreadyHitActors` 기반의 별도 무기 콜리전 데미지 시스템이 이미 존재함을 발견, `Notify_Hit`/`CreateBoxHitbox` 경로와의 관계 확인 필요 |
| 2026-09-17 | JJW | 포탈 상호작용 시스템 구축 (`IA_Interact`, `ARLPlayerController` E키 바인딩, `ARLPortal` 수정, `WBP_HUD` 안내 연동) |
| 2026-09-17 | JJW | 소서리스 캐릭터 리타게팅(`SK_Wizard`), 애니메이션 블루프린트(`wizard_ABP_RLCharacter`) 및 전용 스킬셋(Q, W, E, R) DataAsset/몽타주 연동 |
| 2026-09-16 | JJW | 장판형 스킬 베이스 `ARLGroundAreaEffect` 구현, 소서리스 W/R 스킬 AD/AP 계수 누락 버그 수정 및 `ReceiveCustomDamageAtLocation` 추가 |
| 2026-09-16 | JJW | 엔진 5.8 업그레이드, 팀/진영 시스템(`ERLTeam`) 및 아군 오사 방지/투사체 관통 버그 수정, Unreal MCP 연동 |
| 2026-09-16 | 전체 팀 | 프로젝트 아키텍처 정리 및 AI 협업 가이드(`CLAUDE.md`, `PROGRESS.md`) 구축 |
| - | - | 캐릭터 기본 이동/회피, 스킬/스탯 컴포넌트, 포탈 기초 구현 완료 |

> 💡 **Tip:** 새로운 작업을 시작하거나 세션을 마칠 때 이 파일의 체크리스트와 작업 히스토리를 업데이트해 두면 언제든 매끄럽게 이어서 개발할 수 있습니다.
