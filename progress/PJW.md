# PJW
> 최종 갱신: 2026-09-17

## 완료
- 포탈 상호작용 방식 개선: 접촉 즉시 텔레포트 → "근처 진입 시 프롬프트 표시 + 키 입력(F키) 시 텔레포트" 방식으로 변경
  - RLPortal: OnOverlapBegin/End로 근접 감지, ActivatePortal 함수로 실제 텔레포트 분리
  - RLCharacter: SetNearbyPortal/TryInteract/OnNearbyPortalChanged 추가
  - RLPlayerController: Enhanced Input으로 상호작용 키 바인딩 (IA_Interact, F키 — E는 기존 스킬 키와 충돌해서 F로 변경)
  - WBP_HUD에 상호작용 프롬프트 텍스트 UI 추가
- 엔진 5.7 → 5.8 전환 작업 (로컬 환경 세팅 및 엔진 버전 등록 이슈 해결)
- Unreal MCP 연동 설정 완료 — Claude Code로 언리얼 에디터 직접 제어 가능하도록 구축 (플러그인 활성화, 서버 자동 시작, 클라이언트 설정 연동까지 완료)

## 핵심 구조
- 포탈: 오버랩 시 SetNearbyPortal(this) 호출 → HUD 프롬프트 표시 → F키 입력 시 TryInteract() → ActivatePortal()
- 상호작용 키는 F (E는 스킬 슬롯과 충돌 방지 위해 변경)
- MCP 서버는 에디터의 "Auto Start Server" 설정 켜져 있어야 자동 동작
