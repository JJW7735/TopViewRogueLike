#pragma once

#include "CoreMinimal.h"
#include "Blueprint/UserWidget.h"
#include "RLSaveGame.h"
#include "RLMetaUpgradeRowWidget.generated.h"

class UTextBlock;
class UButton;

DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(FOnMetaRowUpgradeClicked, FName, UpgradeKey);

/**
 * 영구 강화 상점의 특성 1줄 (WBP_MetaUpgradeRow)의 C++ 베이스 위젯
 * - SetUpgradeInfo(FRLMetaUpgradeInfo) 한 번으로 이름/설명/단계/효과/비용/버튼 상태 갱신
 * - "강화" 버튼 클릭 시 OnUpgradeClicked(UpgradeKey) 방송 → 메인 위젯이 실제 구매 처리
 */
UCLASS()
class ROGUELIKETOPVIEW_API URLMetaUpgradeRowWidget : public UUserWidget
{
	GENERATED_BODY()

public:
	virtual void NativeConstruct() override;

	/** 행 전체를 특성 정보로 갱신합니다 */
	UFUNCTION(BlueprintCallable, Category = "Meta|UI")
	void SetUpgradeInfo(const FRLMetaUpgradeInfo& InInfo);

	/** 진행 중인 런이 있어 강화가 잠겼는지 설정 (버튼을 흐리게, 클릭은 허용해 안내를 띄움) */
	UFUNCTION(BlueprintCallable, Category = "Meta|UI")
	void SetRunLocked(bool bInLocked);

	UFUNCTION(BlueprintPure, Category = "Meta|UI")
	FName GetUpgradeKey() const { return CachedInfo.UpgradeKey; }

	UFUNCTION(BlueprintPure, Category = "Meta|UI")
	const FRLMetaUpgradeInfo& GetUpgradeInfo() const { return CachedInfo; }

	/** "강화" 버튼 클릭 시 방송 (UpgradeKey 전달) */
	UPROPERTY(BlueprintAssignable, Category = "Meta|UI")
	FOnMetaRowUpgradeClicked OnUpgradeClicked;

protected:
	/** 보너스 수치를 화면 표기 문자열로 변환 (예: +40, +4%, +100G). BP에서 오버라이드 가능 */
	UFUNCTION(BlueprintNativeEvent, Category = "Meta|UI")
	FText FormatBonusValue(FName UpgradeKey, float Value) const;

	/** SetUpgradeInfo 직후 호출되는 BP 연출 훅 (MAX 반짝임 등) */
	UFUNCTION(BlueprintImplementableEvent, Category = "Meta|UI")
	void BP_OnUpgradeInfoUpdated(const FRLMetaUpgradeInfo& InInfo);

	UFUNCTION()
	void HandleUpgradeButtonClicked();

	/** 소울 부족 시 버튼에 적용할 불투명도 */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Meta|Config")
	float CannotAffordOpacity = 0.4f;

	UPROPERTY(BlueprintReadOnly, Category = "Meta|UI")
	FRLMetaUpgradeInfo CachedInfo;

	UPROPERTY(BlueprintReadOnly, Category = "Meta|UI")
	bool bRunLocked = false;

	void UpdateUpgradeButtonState();

	// ==========================================
	// WBP_MetaUpgradeRow 자동 바인딩 위젯 (Optional)
	// ==========================================

	UPROPERTY(meta = (BindWidgetOptional))
	TObjectPtr<UTextBlock> Txt_Name;

	UPROPERTY(meta = (BindWidgetOptional))
	TObjectPtr<UTextBlock> Txt_Desc;

	/** 현재/최대 단계 ("3/5") */
	UPROPERTY(meta = (BindWidgetOptional))
	TObjectPtr<UTextBlock> Txt_Level;

	/** 현재 효과 → 다음 효과 ("+40 → +60"). MAX면 현재 효과만 */
	UPROPERTY(meta = (BindWidgetOptional))
	TObjectPtr<UTextBlock> Txt_Effect;

	/** 다음 비용. MAX면 숨김 */
	UPROPERTY(meta = (BindWidgetOptional))
	TObjectPtr<UTextBlock> Txt_Cost;

	UPROPERTY(meta = (BindWidgetOptional))
	TObjectPtr<UButton> Btn_Upgrade;

	/** 버튼 라벨 ("강화" / "MAX") */
	UPROPERTY(meta = (BindWidgetOptional))
	TObjectPtr<UTextBlock> Txt_UpgradeLabel;
};
