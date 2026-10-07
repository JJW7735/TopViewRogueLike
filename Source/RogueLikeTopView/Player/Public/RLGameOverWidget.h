#pragma once

#include "CoreMinimal.h"
#include "Blueprint/UserWidget.h"
#include "RLGameOverWidget.generated.h"

class ARLCharacter;

/**
 * 게임오버(사망) 화면 결과창 베이스 위젯
 * - 마우스 커서 표시 및 UI 입력 모드 자동 전환
 * - 정산된 영혼, 보유 영혼, 소멸 골드, 도달 스테이지 바인딩 제공
 * - 재시작(Retry) 및 메인메뉴 복귀 시 세이브 정리 및 입력 모드 안전 원복
 */
UCLASS()
class ROGUELIKETOPVIEW_API URLGameOverWidget : public UUserWidget
{
	GENERATED_BODY()

public:
	virtual void NativeConstruct() override;

	/**
	 * 사망한 플레이어 정보 및 이번 런 획득 영혼으로 게임오버 화면을 초기화합니다.
	 * InSoulsEarned: 이번 런 획득 영혼 (음수 전달 시 세이브 서브시스템의 LastSettledRunSouls를 자동 조회)
	 */
	UFUNCTION(BlueprintCallable, Category = "GameOver")
	void SetupGameOver(ARLCharacter* DeadPlayer = nullptr, int32 InSoulsEarned = -1);

	/**
	 * 다시하기 (Retry)
	 * 활성 런을 정리하고, UI 입력 모드를 게임 모드로 복원한 뒤 스테이지를 다시 시작합니다.
	 * RestartLevelName이 None이면 현재 레벨을 다시 로드합니다.
	 */
	UFUNCTION(BlueprintCallable, Category = "GameOver")
	void RetryRun(FName RestartLevelName = NAME_None);

	/**
	 * 메인 메뉴 / 타이틀로 돌아가기
	 * 활성 런을 정리하고, UI 입력 모드를 원복한 뒤 메인 메뉴 맵을 엽니다.
	 */
	UFUNCTION(BlueprintCallable, Category = "GameOver")
	void QuitToMainMenu(FName MainMenuLevelName = FName("L_MainMenu"));

	// ==========================================
	// UI 바인딩용 데이터
	// ==========================================

	// 이번 런에서 획득한 영혼 (정산 완료된 수량)
	UPROPERTY(BlueprintReadOnly, Category = "GameOver|Data")
	int32 SoulsEarnedThisRun = 0;

	// 현재 계정 총 보유 영혼 (이번 런 획득분 합산 완료된 총액)
	UPROPERTY(BlueprintReadOnly, Category = "GameOver|Data")
	int32 TotalSouls = 0;

	// 사망 전 소지했던 골드 (이번 런에서 소멸한 골드)
	UPROPERTY(BlueprintReadOnly, Category = "GameOver|Data")
	int32 GoldLost = 0;

	// 도달한 스테이지 번호
	UPROPERTY(BlueprintReadOnly, Category = "GameOver|Data")
	int32 StageReached = 1;

	// 사망한 플레이어 표시 이름
	UPROPERTY(BlueprintReadOnly, Category = "GameOver|Data")
	FText CharacterName;

protected:
	/**
	 * C++에서 수치 정산이 완료된 후 BP에서 텍스트 설정, 카운팅 애니메이션 연출 등을 할 수 있는 훅
	 */
	UFUNCTION(BlueprintImplementableEvent, Category = "GameOver")
	void OnGameOverDataReady(int32 InSoulsEarned, int32 InTotalSouls, int32 InGoldLost, int32 InStageReached);

private:
	void RestorePlayerInput();
};
