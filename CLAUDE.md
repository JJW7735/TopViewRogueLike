# RLTV (RogueLikeTopView) — Claude Code 프로젝트 가이드

3인 팀 포트폴리오/공모전용 탑뷰 로그라이크. 팀원 전원 클라이언트 프로그래머 지망. 아트/애니메이터 전담 인력 없음 (무료 에셋 + 팀 내 테크아트 담당으로 대체 중).

- 엔진: Unreal Engine 5.8
- 모듈: `RogueLikeTopView` (단일 게임 모듈)
- 기본 시작 맵: `Content/Level/Map_Persistent.umap`
- 기본 GameMode: `Content/Blueprint/BP_RLGameMode` (부모: `GameModeBase` 직접 상속, C++ 템플릿 기본 GameMode 아님)
- 기본 캐릭터: `Content/Blueprint/BP_RLCharacter` (부모: C++ `ARLCharacter`)

## Source 폴더 구조 (기능별, Public/Private 분리)

```
Source/RogueLikeTopView/
├─ RogueLikeTopView.Build.cs / .cpp / .h   ← 모듈 진입점, 이동 금지
├─ Character/Public,Private   RLCharacter
├─ Combat/Public,Private      RLPortal, RLProjectile, RLDecalIndicator, RLHealthComponent
├─ Skill/Public,Private       RLSkillComponent, RLSkillData
├─ Stats/Public,Private       RLStatComponent
├─ AI/Public                  RLMonsterPatternDataAsset
├─ Core/Public                RLTypes
└─ Player/Public,Private      RLPlayerController
```

**중요**: 언리얼 UBT는 모듈 루트 바로 아래의 `Public`/`Classes`만 자동으로 include 경로에 잡는다. 위처럼 중첩된 `Character/Public` 같은 폴더는 `RogueLikeTopView.Build.cs`의 `PublicIncludePaths`에 반드시 수동으로 등록해야 컴파일된다. **새 기능 폴더를 추가하면 Build.cs도 같이 수정할 것.**

`Variant_Strategy`, `Variant_TwinStick`, `RogueLikeTopViewCharacter/GameMode/PlayerController` (Epic Top Down 템플릿 기본/샘플 코드)는 실제 게임에서 미사용 확인 후 제거함. 다시 추가하지 말 것.

## 소스 파일 인코딩

한글 주석이 포함된 `.cpp`/`.h` 파일은 반드시 **UTF-8 (BOM 포함)** 으로 저장할 것. CP949로 저장하면 GitHub 등에서 한글이 깨져 보인다. Visual Studio에서 저장 시 "다른 이름으로 저장 → 인코딩하여 저장 → UTF-8 서명 포함"으로 확인.

## 네이밍 컨벤션 (Allar 스타일 가이드 기반)

`BP_`(블루프린트), `WBP_`(위젯 BP), `ABP_`(애니메이션 BP), `BT_`/`BB_`(비헤이비어 트리/블랙보드), `GA_`/`GE_`(게임플레이 어빌리티/이펙트), `IMC_`/`IA_`(Enhanced Input), `DA_`/`DT_`(데이터 애셋/테이블), `M_`/`MI_`(머티리얼/인스턴스), `SM_`/`SK_`(스태틱/스켈레탈 메시).

## C++ vs 블루프린트 판단 기준

기본 패턴: **C++로 뼈대(구조/알고리즘) + 블루프린트로 파생(콘텐츠/튜닝)**.
- C++: 코어 로직, 여러 클래스가 공유하는 알고리즘, Subsystem, 성능 민감 코드, GameMode/GameState 뼈대.
- Blueprint: 레벨/캐릭터별 튜닝 값, UI 이벤트 배선, 애니메이션 스테이트 머신, 빠른 프로토타이핑.
- 새 시스템을 만들 때는 C++ 클래스에 `UPROPERTY(EditDefaultsOnly/BlueprintReadWrite)`로 조정 가능한 값을 노출하고, `BlueprintImplementableEvent`/`BlueprintNativeEvent`로 BP가 끼어들 수 있는 훅을 만드는 방식을 기본으로 한다.

## 아트 에셋 방침

에셋 출처를 통일할 것 (현재 여러 무료 팩이 혼재되어 스타일 불일치 위험 있음). 신규 에셋 추가 시 Synty 계열 등 일관된 스타일 라인을 우선 검토. 애니메이션은 Mixamo → UE 마네킹 스켈레톤 리타겟팅 경로 사용. 자세한 내용은 claude.ai 프로젝트(RLTV)의 `아트팀-부재-대응-전략.md` 문서 참고.

## 빌드 확인 방법

Unreal은 npm/pytest 같은 커맨드 테스트가 없음. 변경 후 확인 절차:
1. Source 파일 추가/이동/삭제 시 `.uproject` 우클릭 → "Generate Visual Studio project files"
2. Visual Studio에서 Build → Rebuild Solution (Build 아님, 전체 재빌드)로 확인

## 커밋 메시지

Conventional Commits 스타일 권장: `fix:`, `feat:`, `refactor:`, `chore:`. 구조 변경과 기능 변경은 가능하면 커밋을 분리할 것.
