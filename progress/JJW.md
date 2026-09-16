# JJW
> 최종 갱신: 2026-09-16

## 완료
- 엔진 5.7.4 → 5.8 업그레이드 (`.uproject` EngineAssociation 변경, VS 프로젝트 파일 재생성)
- 팀/진영 시스템 추가 (`RLTypes.h`의 `ERLTeam` enum + `RLCharacter`의 `TeamID`) — 근접/장판/투사체 공격 전체에 아군 피해 방지 적용
- 투사체 관통 버그 수정 (아군 맞으면 데미지 없이 통과하도록)
- Unreal MCP(에디터 내장) + Claude Code CLI 연동
- `progress/` 폴더 + `/team-status` 커맨드로 팀 진행상황 공유 체계 구축
- `CLAUDE.md` 엔진 버전 5.8로 수정

## 핵심 구조
- 모든 데미지는 `ARLCharacter::ReceiveCustomDamage` 한 곳을 거침 (근접/장판/투사체 공통) — 데미지 관련 로직은 여기만 고치면 셋 다 적용됨
- `ERLTeam` 기본값은 Enemy. 플레이어 BP는 클래스 디폴트에서 Player로 바꿔야 함
- 투사체는 데미지 무시와 별개로 `ARLProjectile::OnHit`에서 피격 처리(관통 카운트) 자체도 같은 팀이면 막아야 함
