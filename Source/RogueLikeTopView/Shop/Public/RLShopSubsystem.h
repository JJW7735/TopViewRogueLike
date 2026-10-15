#pragma once

#include "CoreMinimal.h"
#include "Subsystems/GameInstanceSubsystem.h"
#include "RLShopTypes.h"
#include "RLRewardTypes.h"
#include "RLShopSubsystem.generated.h"

class ARLCharacter;
class URLItemDataAsset;

/**
 * 인게임 상점 시스템을 총괄하는 GameInstanceSubsystem
 * - 무작위 상품 진열 (소비 아이템, 패시브 유물, 스탯 강화, 즉시 치료)
 * - 골드 결제, 인벤토리 아이템 지급 및 스탯 보너스 즉각 반영
 * - 리롤(새로고침) 및 비용 누적 관리
 * - UI 바인딩용 델리게이트 제공
 */
UCLASS()
class ROGUELIKETOPVIEW_API URLShopSubsystem : public UGameInstanceSubsystem
{
	GENERATED_BODY()

public:
	virtual void Initialize(FSubsystemCollectionBase& Collection) override;

	/**
	 * 월드 컨텍스트 객체로부터 서브시스템을 쉽게 가져오는 정적 헬퍼 함수
	 */
	UFUNCTION(BlueprintPure, Category = "Shop", meta = (WorldContext = "WorldContextObject"))
	static URLShopSubsystem* Get(const UObject* WorldContextObject);

	// ==========================================
	// 상점 조작 함수
	// ==========================================

	/**
	 * 상점 상품 목록을 무작위로 생성하여 진열합니다.
	 * @param Count 진열할 상품 개수 (기본 4칸)
	 * @param bResetReroll 리롤 카운트를 0으로 초기화할지 여부
	 */
	UFUNCTION(BlueprintCallable, Category = "Shop")
	void GenerateShopItems(int32 Count = 4, bool bResetReroll = false);

	/**
	 * 특정 슬롯의 상품을 구매합니다.
	 * 플레이어의 골드를 차감하고 아이템/스탯/회복을 지급합니다.
	 * @param Buyer 구매하는 플레이어 캐릭터
	 * @param SlotIndex 구매할 상점 슬롯 인덱스 (0~3)
	 * @param OutFailReason 구매 실패 시 사유 텍스트
	 * @return 구매 성공 여부
	 */
	UFUNCTION(BlueprintCallable, Category = "Shop")
	bool BuyShopItem(ARLCharacter* Buyer, int32 SlotIndex, FString& OutFailReason);

	/**
	 * 상점 상품을 골드를 소모하여 새로고침(리롤)합니다.
	 * @param Buyer 리롤을 요청하는 플레이어 캐릭터
	 * @param OutFailReason 리롤 실패 시 사유 텍스트
	 * @return 리롤 성공 여부
	 */
	UFUNCTION(BlueprintCallable, Category = "Shop")
	bool RerollShop(ARLCharacter* Buyer, FString& OutFailReason);

	/**
	 * 다음 리롤에 필요한 골드 비용을 반환합니다.
	 */
	UFUNCTION(BlueprintPure, Category = "Shop")
	int32 GetRerollCost() const;

	/**
	 * 현재 상점에 진열된 전체 상품 목록을 반환합니다.
	 */
	UFUNCTION(BlueprintPure, Category = "Shop")
	const TArray<FRLShopItem>& GetCurrentShopItems() const { return CurrentShopItems; }

	/**
	 * 특정 슬롯의 상품 정보를 조회합니다.
	 */
	UFUNCTION(BlueprintPure, Category = "Shop")
	bool GetShopItemAt(int32 SlotIndex, FRLShopItem& OutItem) const;

	/**
	 * 새로운 런 시작 시 상점 상태를 완전 초기화합니다.
	 */
	UFUNCTION(BlueprintCallable, Category = "Shop")
	void ResetShop();

	/**
	 * 외부 아이템 에셋을 상점 판매 풀에 추가 등록합니다.
	 */
	UFUNCTION(BlueprintCallable, Category = "Shop")
	void RegisterItemToPool(URLItemDataAsset* ItemAsset);

	// ==========================================
	// 델리게이트 (UI 연동용)
	// ==========================================

	/** 상점 상품 목록이 갱신(생성 또는 리롤)되었을 때 호출 */
	UPROPERTY(BlueprintAssignable, Category = "Shop|Event")
	FOnShopRefreshedSignature OnShopRefreshed;

	/** 상품 구매가 성공했을 때 호출 (슬롯 번호, 구매된 상품 정보) */
	UPROPERTY(BlueprintAssignable, Category = "Shop|Event")
	FOnShopItemPurchasedSignature OnShopItemPurchased;

	/** 리롤 비용이 변경되었을 때 호출 */
	UPROPERTY(BlueprintAssignable, Category = "Shop|Event")
	FOnShopRerollCostChangedSignature OnShopRerollCostChanged;

protected:
	// 현재 진열 중인 상점 상품 (기본 4칸)
	UPROPERTY(VisibleInstanceOnly, BlueprintReadOnly, Category = "Shop")
	TArray<FRLShopItem> CurrentShopItems;

	// 현재 런에서 실행한 리롤 횟수
	UPROPERTY(VisibleInstanceOnly, BlueprintReadOnly, Category = "Shop")
	int32 RerollCount = 0;

	// 기본 리롤 비용 (골드)
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Shop|Config")
	int32 BaseRerollCost = 25;

	// 리롤할 때마다 추가로 증가하는 골드 비용
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Shop|Config")
	int32 RerollCostIncrement = 15;

	// 기본 아이템 풀 (DataAsset 없이도 작동하도록 내장 생성)
	UPROPERTY()
	TArray<TObjectPtr<URLItemDataAsset>> ItemPool;

	// 기본 스탯/축복 상품 풀
	UPROPERTY()
	TArray<FRLRewardData> StatPool;

	/** 기본 상품 풀 구축 */
	void BuildDefaultPools();

	/** 단일 상품 무작위 추첨 헬퍼 */
	FRLShopItem RollRandomShopItem(int32 SlotIndex, const TArray<FName>& ExcludedIDs);
};
