# HSJ
> 최종 갱신: 2026-09-17

## 완료
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
- **데미지 파이프라인 (공격자 -> 피격자)**:
  1. 공격자: `URLSkillComponent::CalculateFinalDamage(Skill)` -> AD/AP 계수 합산 -> `EvaluateDamage`로 치명타 판정 및 최종 `FRLDamageData` 생성
  2. 피격자: `ARLCharacter::ReceiveCustomDamage` 진입 -> 팀 및 무적 체크 -> `!bTrueDamage`일 때 피격자의 `StatComp->CalculateDefendedDamage()`로 방어력/피해감소율 적용 -> `UGameplayStatics::ApplyDamage` -> `HealthComp` 체력 차감
- **방어력 공식**:
  - `Defense >= 0`: 피해 배율 = `100.0f / (100.0f + Defense)`
  - `Defense < 0`: 피해 배율 = `2.0f - (100.0f / (100.0f - Defense))` (방어력 감소 디버프 시 데미지 증폭)
  - `최종 피해 = 원래 피해 * 방어 배율 * (1.0 - 피해감소율 / 100.0)`
- **에디터 튜닝**:
  - 캐릭터 BP(플레이어, 몬스터)의 `StatComp` 디폴트값에서 `Defense`와 `DamageReduction` 수치를 자유롭게 조절 가능
