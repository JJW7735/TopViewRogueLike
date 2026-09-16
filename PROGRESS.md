# 🎮 RLTV (RogueLikeTopView) 개발 진행 상황 & 프로젝트 가이드

> **최종 갱신일:** 2026-09-16  
> **프로젝트 성격:** 3인 포트폴리오/공모전 탑다운 로그라이크 액션  
> **엔진 버전:** Unreal Engine 5.7.4  
> **AI 어시스턴트 협업:** Gemini (Antigravity CLI), Claude (Claude Code)

---

## 📌 1. 현재 구현 현황 요약 (Current Implementation)

### 🧑‍🚀 캐릭터 & 조작 (Character & Player)
- [x] **`ARLCharacter`**: 탑다운 기본 이동, 대시/회피(`Dodge`, `StartDashMovement`)
- [x] **무적 및 피격 판정**: 무적 상태 관리(`IsInvulnerable`), 커스텀 데미지 수신(`ReceiveCustomDamage`)
- [x] **히트박스 생성기**: 박스 형태(`CreateBoxHitbox`) 및 위치 기반 범위(`CreateLocationHitbox`)
- [x] **`ARLPlayerController`**: 탑다운 마우스 커서 조준 및 Enhanced Input 처리

### ⚔️ 전투 & 투사체 (Combat)
- [x] **`URLHealthComponent`**: 체력, 방어, 사망 처리 및 데미지 이벤트
- [x] **`ARLProjectile`**: 스킬/공격 시 발사되는 투사체 로직 및 충돌 처리
- [x] **`ARLDecalIndicator`**: 스킬 시전/몬스터 공격 예고 장판(Decal) 표시
- [x] **`ARLPortal`**: 맵 전환 및 레벨 스트리밍 로딩 연동 액터

### ⚡ 스킬 & 스탯 (Skill & Stats)
- [x] **`URLSkillComponent` & `URLSkillData`**: 데이터 기반 스킬 쿨다운, 시전 타입, 투사체/근접 발동
- [x] **`URLStatComponent`**: 공격력, 이동속도, 쿨다운 감소 등 런타임 스탯 관리

### 👾 AI & 몬스터 (AI)
- [x] **`URLMonsterPatternDataAsset`**: 몬스터 패턴 정의용 DataAsset 기초

---

## 📋 2. 다음 개발 우선순위 (Next Milestones)

- [ ] **절차적 맵 생성 (Procedural Dungeon / Room Generation)**
  - 방(시작방, 전투방, 상점방, 보스방) 템플릿 연결 시스템 또는 레벨 스트리밍 결합
  - 방 입장 시 문 닫힘 및 몬스터 처치 시 포탈/문 개방 연동
- [ ] **몬스터 AI 및 스폰 시스템 (AI & Spawner)**
  - Behavior Tree / Blackboard를 활용한 적 추적 및 패턴 실행
  - 방 입장 시 웨이브/위치별 몬스터 동적 스폰
- [ ] **로그라이크 보상 & 퍽(Perk) 시스템**
  - 방 클리어 후 3지선다 보상 선택 UI 및 스탯/스킬 강화 로직
- [ ] **UI/UX (UMG)**
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

| 일자 | 작업자 (도구) | 작업 내용 |
| :--- | :--- | :--- |
| 2026-09-16 | 전체 팀 | 프로젝트 아키텍처 정리 및 AI 협업 가이드(`CLAUDE.md`, `PROGRESS.md`) 구축 |
| - | - | 캐릭터 기본 이동/회피, 스킬/스탯 컴포넌트, 포탈 기초 구현 완료 |

> 💡 **Tip:** 새로운 작업을 시작하거나 세션을 마칠 때 이 파일의 체크리스트와 작업 히스토리를 업데이트해 두면 언제든 매끄럽게 이어서 개발할 수 있습니다.
