#pragma once

#include "CoreMinimal.h"
#include "Blueprint/UserWidget.h"
#include "RLMetaUpgradeWidget.generated.h"

class UTextBlock;
class UButton;
class UPanelWidget;
class ARLCharacter;
class URLMetaUpgradeRowWidget;

/**
 * 영구 강화 상점 UI (WBP_MetaUpgrade)의 C++ 베이스 위젯
 * - 보유 소울 표시 및 OnSoulsChanged 실시간 갱신
 * - GetAllMetaUpgradeInfos 결과로 WBP_MetaUpgradeRow를 목록(ScrollBox)에 생성
 * - 행 클릭 → UpgradeMetaTrait → 성공 시 해당 행 갱신 / 실패 시 사유 잠깐 표시
 * - 초기화(전액 환불) / 나가기, 마우스 커서 및 일시정지 안전 관리 (보상 선택 위젯과 같은 방식)
 */
UCLASS()
class ROGUELIKETOPVIEW_API URLMetaUpgradeWidget : public UUserWidget
{
	GENERATED_BODY()

public:
	virtual void NativeConstruct() override;
	virtual void NativeDestruct() override;
	virtual void NativeTick(const FGeometry& MyGeometry, float InDeltaTime) override;

	/**
	 * 상점을 열고 입력 모드를 전환합니다 (AddToViewport 직후 호출)
	 * - InPlayer가 nullptr이면 메인 메뉴 모드: 일시정지 없이 UI Only 입력
	 * - InPlayer가 있으면 인게임 모드: bPauseGameWhenOpen에 따라 일시정지, Game And UI 입력
	 * 입력 모드/커서는 위젯의 OwningPlayer 기준으로 처리합니다
	 */
	UFUNCTION(BlueprintCallable, Category = "Meta|UI")
	void OpenMetaUpgrade(ARLCharacter* InPlayer = nullptr);

	/** 상점을 닫고 일시정지 및 입력 모드를 원복합니다. ReturnWidgetClass가 있으면 그 위젯을 다시 띄움 */
	UFUNCTION(BlueprintCallable, Category = "Meta|UI")
	void CloseMetaUpgrade();

	/** 닫을 때 다시 띄울 위젯 클래스 (메인 메뉴에서 열 때 WBP_StartScreen 지정). 비어 있으면 닫기만 함 */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Meta|Config", meta = (ExposeOnSpawn = true))
	TSubclassOf<UUserWidget> ReturnWidgetClass;

	/** 특성 1단계 강화 요청. 성공 여부 반환 */
	UFUNCTION(BlueprintCallable, Category = "Meta|UI")
	bool RequestUpgrade(FName UpgradeKey);

	/** 모든 특성 초기화 (소울 전액 환불) */
	UFUNCTION(BlueprintCallable, Category = "Meta|UI")
	void RequestResetAll();

	/** 행 목록을 비우고 GetAllMetaUpgradeInfos 기준으로 다시 생성 */
	UFUNCTION(BlueprintCallable, Category = "Meta|UI")
	void RebuildRows();

protected:
	/** 강화 성공 시 BP 연출 훅 */
	UFUNCTION(BlueprintImplementableEvent, Category = "Meta|UI")
	void BP_OnUpgradeSucceeded(FName UpgradeKey, int32 NewLevel);

	/** 강화 실패 시 BP 연출 훅 (Txt_FailReason 표시와 별개로 사운드 등) */
	UFUNCTION(BlueprintImplementableEvent, Category = "Meta|UI")
	void BP_OnUpgradeFailed(FName UpgradeKey, const FString& FailReason);

	UFUNCTION()
	void HandleSoulsChanged(int32 TotalSouls, int32 Delta);

	UFUNCTION()
	void HandleRowUpgradeClicked(FName UpgradeKey);

	UFUNCTION()
	void HandleResetClicked();

	UFUNCTION()
	void HandleExitClicked();

	void UpdateSoulsText(int32 TotalSouls);
	void RefreshExistingRows();
	void ShowFailReason(const FString& Reason);

	/** 목록에 생성할 행 위젯 클래스 (비어 있으면 /Game/Blueprint/UI/WBP_MetaUpgradeRow 로드) */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Meta|Config")
	TSubclassOf<URLMetaUpgradeRowWidget> RowWidgetClass;

	/** 상점 오픈 시 게임을 일시정지할지 여부 */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Meta|Config")
	bool bPauseGameWhenOpen = true;

	/** 실패 사유 표시 시간 (초, 일시정지 중에도 흐름) */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Meta|Config")
	float FailReasonDisplayTime = 2.0f;

	UPROPERTY(BlueprintReadOnly, Category = "Meta|UI")
	TWeakObjectPtr<ARLCharacter> TargetPlayer;

	UPROPERTY(Transient)
	TArray<TObjectPtr<URLMetaUpgradeRowWidget>> RowWidgets;

	float FailReasonRemaining = 0.0f;

	/** OpenMetaUpgrade에서 실제로 일시정지를 걸었는지 (닫을 때 그 경우에만 해제) */
	bool bPausedByWidget = false;

	// ==========================================
	// WBP_MetaUpgrade 자동 바인딩 위젯 (Optional)
	// ==========================================

	UPROPERTY(meta = (BindWidgetOptional))
	TObjectPtr<UTextBlock> Txt_Souls;

	/** 행이 추가될 컨테이너 (ScrollBox / VerticalBox 등) */
	UPROPERTY(meta = (BindWidgetOptional))
	TObjectPtr<UPanelWidget> Box_Upgrades;

	UPROPERTY(meta = (BindWidgetOptional))
	TObjectPtr<UTextBlock> Txt_FailReason;

	UPROPERTY(meta = (BindWidgetOptional))
	TObjectPtr<UButton> Btn_Reset;

	UPROPERTY(meta = (BindWidgetOptional))
	TObjectPtr<UButton> Btn_Exit;
};
