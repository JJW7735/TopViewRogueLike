# JJW
> 최종 갱신: 2026-09-16

## 완료
- 엔진 5.7.4 → 5.8 업그레이드 (`.uproject` EngineAssociation 변경, VS 프로젝트 파일 재생성으로 빌드 에러 해결)
- 팀/진영 시스템 추가 (`RLTypes.h`의 `ERLTeam` enum + `RLCharacter`의 `TeamID`) — 근접/장판/투사체 공격 전체에 아군 피해 방지 적용
- 투사체 관통 버그 수정 (아군 맞으면 데미지 없이 통과하도록, `ARLProjectile::OnHit`)
- Unreal MCP(에디터 내장, Experimental) 활성화 및 설정 (플러그인 켜고 Auto Start Server, `.mcp.json` 생성)
- Claude Code CLI 설치(Windows) 및 Unreal MCP 서버 연동 확인 (테스트 액터 스폰으로 검증 완료)
- 에디터 내장 "터미널" 패널(Experimental) 조사 — 한글 미지원(폰트 글리프 없음) 확인, 외부 터미널 사용으로 결론
- `CLAUDE.md` 엔진 버전 5.8로 수정
- 팀원 작성 `PROGRESS.md` 검토 (엔진 버전 표기가 아직 5.7.4로 안 고쳐져 있는 것 확인, 미수정 상태)
- `progress/` 폴더 + `/team-status` 커맨드로 팀 진행상황 온디맨드 공유 체계 구축

## 핵심 구조
- 모든 데미지는 `ARLCharacter::ReceiveCustomDamage` 한 곳을 거침 (근접/장판/투사체 공통) — 데미지 관련 로직은 여기만 고치면 셋 다 적용됨
- `ERLTeam` 기본값은 Enemy. 플레이어 BP는 클래스 디폴트에서 Player로 바꿔야 함
- 투사체는 데미지 무시와 별개로 `ARLProjectile::OnHit`에서 피격 처리(관통 카운트) 자체도 같은 팀이면 막아야 함
- Unreal MCP는 에디터에서 Auto Start Server 켜져 있어야 동작 (에디터 꺼져 있으면 Claude Code 쪽 MCP 연결 실패함)
- Claude Code는 `.claude/commands/`나 `.claude/agents/`에 파일을 추가해도 실행 중 세션엔 자동 반영 안 됨 — 새 커맨드 추가/수정하면 세션 재시작(`/exit` 후 재실행) 필요
- 에디터 내장 터미널(Experimental, Tools 메뉴)은 한글 폰트 미지원이라 한글 깨짐 — 한글로 Claude Code 쓸 땐 외부 PowerShell 터미널 사용
