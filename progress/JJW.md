# JJW
> 최종 갱신: 2026-09-23

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
