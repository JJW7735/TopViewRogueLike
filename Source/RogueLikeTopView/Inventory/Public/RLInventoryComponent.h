#pragma once

#include "CoreMinimal.h"
#include "Components/ActorComponent.h"
#include "GameplayEffectTypes.h"
#include "RLItemTypes.h"
#include "RLInventoryComponent.generated.h"

class URLItemDataAsset;
class ARLCharacter;

DECLARE_DYNAMIC_MULTICAST_DELEGATE(FOnInventoryUpdatedSignature);
DECLARE_DYNAMIC_MULTICAST_DELEGATE_TwoParams(FOnSlotUpdatedSignature, int32, SlotIndex, const FRLInventorySlot&, SlotData);
DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(FOnGoldChangedSignature, int32, NewGold);
DECLARE_DYNAMIC_MULTICAST_DELEGATE_TwoParams(FOnItemUsedSignature, URLItemDataAsset*, ItemData, int32, SlotIndex);

/**
 * 캐릭터의 아이템, 유물, 골드 재화를 관리하는 인벤토리 컴포넌트
 */
UCLASS(ClassGroup = (Custom), meta = (BlueprintSpawnableComponent))
class ROGUELIKETOPVIEW_API URLInventoryComponent : public UActorComponent
{
	GENERATED_BODY()

public:
	URLInventoryComponent();

	virtual void BeginPlay() override;

	// ==========================================
	// 델리게이트 (UI 연동용)
	// ==========================================

	// 인벤토리 전체 갱신 시 호출
	UPROPERTY(BlueprintAssignable, Category = "Inventory|Event")
	FOnInventoryUpdatedSignature OnInventoryUpdated;

	// 특정 슬롯이 갱신되었을 때 호출 (슬롯 인덱스, 변경된 슬롯 데이터)
	UPROPERTY(BlueprintAssignable, Category = "Inventory|Event")
	FOnSlotUpdatedSignature OnSlotUpdated;

	// 골드 수량이 변경되었을 때 호출
	UPROPERTY(BlueprintAssignable, Category = "Inventory|Event")
	FOnGoldChangedSignature OnGoldChanged;

	// 아이템을 사용했을 때 호출
	UPROPERTY(BlueprintAssignable, Category = "Inventory|Event")
	FOnItemUsedSignature OnItemUsed;

	// ==========================================
	// 아이템 조작 함수
	// ==========================================

	/**
	 * 아이템을 인벤토리에 추가합니다.
	 * 겹칠 수 있는 아이템은 기존 슬롯에 합치고, 남은 수량은 빈 슬롯에 배치합니다.
	 * @return 실제로 인벤토리에 추가된 수량
	 */
	UFUNCTION(BlueprintCallable, Category = "Inventory")
	int32 AddItem(URLItemDataAsset* ItemData, int32 Amount = 1);

	/**
	 * 특정 슬롯의 아이템을 수량만큼 제거합니다.
	 */
	UFUNCTION(BlueprintCallable, Category = "Inventory")
	bool RemoveItemAtIndex(int32 SlotIndex, int32 Amount = 1);

	/**
	 * 아이템 ID로 인벤토리 전체에서 해당 아이템을 수량만큼 찾아 제거합니다.
	 */
	UFUNCTION(BlueprintCallable, Category = "Inventory")
	bool RemoveItemByID(FName ItemID, int32 Amount = 1);

	/**
	 * 특정 슬롯의 아이템을 사용합니다 (소비 아이템 등).
	 */
	UFUNCTION(BlueprintCallable, Category = "Inventory")
	bool UseItemAtIndex(int32 SlotIndex);

	/**
	 * 두 슬롯의 위치를 교환합니다 (UI 드래그 앤 드롭 지원).
	 */
	UFUNCTION(BlueprintCallable, Category = "Inventory")
	bool SwapSlots(int32 FromIndex, int32 ToIndex);

	// ==========================================
	// 조회 및 재화 함수
	// ==========================================

	/** 해당 ID를 가진 아이템의 총 보유 수량 반환 */
	UFUNCTION(BlueprintPure, Category = "Inventory")
	int32 GetItemTotalCount(FName ItemID) const;

	/** 해당 ID의 아이템을 필요한 수량 이상 가지고 있는지 확인 */
	UFUNCTION(BlueprintPure, Category = "Inventory")
	bool HasItem(FName ItemID, int32 RequiredAmount = 1) const;

	/** 특정 슬롯 정보 조회 */
	UFUNCTION(BlueprintPure, Category = "Inventory")
	bool GetSlotAt(int32 SlotIndex, FRLInventorySlot& OutSlot) const;

	/** 전체 슬롯 목록 조회 (읽기 전용) */
	UFUNCTION(BlueprintPure, Category = "Inventory")
	const TArray<FRLInventorySlot>& GetSlots() const { return Slots; }

	/** 세이브 로드 시 골드 및 슬롯 아이템 목록을 안전하게 복원 */
	UFUNCTION(BlueprintCallable, Category = "Inventory")
	void RestoreInventoryDirect(int32 InGold, const TArray<FRLInventorySlot>& InSlots);

	/** 골드 획득 */
	UFUNCTION(BlueprintCallable, Category = "Inventory|Economy")
	void AddGold(int32 Amount);

	/** 골드 소비 (보유 골드가 부족하면 실패하고 false 반환) */
	UFUNCTION(BlueprintCallable, Category = "Inventory|Economy")
	bool SpendGold(int32 Amount);

	/** 현재 보유 골드 반환 */
	UFUNCTION(BlueprintPure, Category = "Inventory|Economy")
	int32 GetGold() const { return Gold; }

	/** 최대 슬롯 개수 반환 */
	UFUNCTION(BlueprintPure, Category = "Inventory")
	int32 GetMaxSlots() const { return MaxSlots; }

	/** 첫 번째 빈 슬롯 인덱스 반환 (없으면 INDEX_NONE) */
	UFUNCTION(BlueprintPure, Category = "Inventory")
	int32 FindFirstEmptySlotIndex() const;

	// ==========================================
	// 퀵슬롯 (HUD 상시 표시용, 소비 아이템 자동 모음)
	// ==========================================

	/**
	 * 소비 아이템을 종류(ItemData)별로 하나씩 묶어, 인벤토리에서 처음 등장한 순서대로 최대 MaxCount개 반환합니다.
	 * Quantity는 해당 아이템이 여러 슬롯에 나뉘어 있어도 합계입니다.
	 * 별도 저장 상태가 없으므로 세이브 구조와 무관하며, OnInventoryUpdated 때 다시 조회하면 됩니다.
	 */
	UFUNCTION(BlueprintPure, Category = "Inventory|QuickSlot")
	TArray<FRLInventorySlot> GetQuickSlotEntries(int32 MaxCount = 3) const;

	/**
	 * QuickIndex번째 퀵슬롯 아이템을 사용합니다.
	 * 해당 아이템이 들어있는 실제 슬롯을 찾아 UseItemAtIndex를 호출합니다 (뒤쪽 슬롯부터 소모해 덜 찬 스택을 먼저 비움).
	 * @return 사용 성공 여부 (빈 퀵슬롯이거나 사용 불가면 false)
	 */
	UFUNCTION(BlueprintCallable, Category = "Inventory|QuickSlot")
	bool UseQuickSlot(int32 QuickIndex);

protected:
	// 기본 슬롯 개수 (기본 20칸)
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Inventory|Config", meta = (ClampMin = "1"))
	int32 MaxSlots = 20;

	// 실제 슬롯 배열
	UPROPERTY(VisibleInstanceOnly, BlueprintReadOnly, Category = "Inventory")
	TArray<FRLInventorySlot> Slots;

	// 현재 보유 골드
	UPROPERTY(VisibleInstanceOnly, BlueprintReadOnly, Category = "Inventory")
	int32 Gold = 0;

	// 시작 시 기본 지급 아이템 목록
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Inventory|Config")
	TArray<FRLInventorySlot> DefaultStartingItems;

	// ★ 패시브 아이템 스탯 반영: 현재 슬롯 내용을 기준으로 아이템 GE(Infinite)를 다시 겁니다.
	// 획득/제거/세이브 복원 등 슬롯이 바뀔 때마다 호출합니다. (슬롯 최대 20칸이라 매번 재계산해도 저렴하고,
	// 기존 방식처럼 +=/-= 짝을 정확히 맞출 필요가 없어 어긋날 일이 없습니다)
	void RefreshPassiveItemEffects();

	// 현재 걸려 있는 패시브 아이템 GE 핸들 (슬롯당 하나)
	TArray<FActiveGameplayEffectHandle> PassiveItemEffectHandles;
};
