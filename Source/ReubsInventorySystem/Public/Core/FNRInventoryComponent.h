// Copyright Vinipi Studios 2024. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Components/ActorComponent.h"
#include "FNRInventoryItem.h"
#include "Utils/RbsTypes.h"
#include "FNRInventoryComponent.generated.h"

DECLARE_DYNAMIC_MULTICAST_DELEGATE(FOnInventoryUpdated);

UCLASS(ClassGroup=(Custom), meta=(BlueprintSpawnableComponent))
class REUBSINVENTORYSYSTEM_API UFNRInventoryComponent : public UActorComponent
{
	GENERATED_BODY()

public:
	UFNRInventoryComponent();

	friend UFNRInventoryItem;

////////////////////////////////////////////// Variables ///////////////////////////////////////////////////////////////
	
/*
 * info
 */

protected:
	UPROPERTY(ReplicatedUsing = OnReplicated_Items, VisibleAnywhere, Category = "Inventory")
	TArray<TObjectPtr<UFNRInventoryItem>> Items;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Inventory")
	float WeightCapacity;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Inventory", meta = (ClampMin=0, ClampMax=500))
	int32 Capacity;

/*
 * Behaviour
 */

	UFUNCTION(Client, Reliable)
	void ClientRefreshInventory();
	
	UPROPERTY(BlueprintAssignable, Category = "Inventory")
	FOnInventoryUpdated OnInventoryUpdated;
	

/*
 * Replication
 */

private:
	UPROPERTY()
	int32 ReplicatedItemsKey = 0;	

////////////////////////////////////////////// Functions ///////////////////////////////////////////////////////////////	

/*
 * Replication
 */

public:
	virtual void GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const override;
	virtual bool ReplicateSubobjects(UActorChannel* Channel, FOutBunch* Bunch, FReplicationFlags* RepFlags) override;

private:
	UFUNCTION()
	void OnReplicated_Items();
	
/*
 * Behaviour
 */

private:
	
	UFNRInventoryItem* AddItem(UFNRInventoryItem* Item);

public:	
	
	UFUNCTION(BlueprintCallable, Category = "Inventory")
	FItemAddResult TryAddItem(class UFNRInventoryItem* Item);
	
	UFUNCTION(BlueprintCallable, Category = "Inventory")
	FItemAddResult TryAddItemFromClass(TSubclassOf<UFNRInventoryItem> ItemClass, const int32 Quantity = 1);

	FItemAddResult TryAddItem_Internal(UFNRInventoryItem* Item);

	UFUNCTION(BlueprintCallable, Category = "Inventory")
	bool RemoveItem(UFNRInventoryItem* Item);
	
	int32 ConsumeItem(UFNRInventoryItem* Item);
	int32 ConsumeItem(UFNRInventoryItem* Item, const int32 Quantity);

	UFUNCTION(BlueprintCallable, Category = "Items")
	void UseItem(UFNRInventoryItem* Item);

	UFUNCTION(Server, Reliable)
	void ServerUseItem(UFNRInventoryItem* Item);

	UFUNCTION(BlueprintCallable, Category = "Items")
	void DropItem(UFNRInventoryItem* Item, const int32 Quantity);

	UFUNCTION(Server, Reliable)
	void ServerDropItem(UFNRInventoryItem* Item, const int32 Quantity);

protected:

	UFUNCTION()
	void OnItemModified_Internal();
	
/*
 * Helpers
 */

public:	
	UFUNCTION(BlueprintPure, Category = "Inventory")
	FORCEINLINE float GetWeightCapacity() const { return WeightCapacity; };

	UFUNCTION(BlueprintPure, Category = "Inventory")
	FORCEINLINE int32 GetCapacity() const { return Capacity; }

	UFUNCTION(BlueprintPure, Category = "Inventory")
	FORCEINLINE TArray<UFNRInventoryItem*> GetItems() const { return Items; }

	UFUNCTION(BlueprintPure, Category = "Inventory")
	float GetCurrentWeight() const;

	UFUNCTION(BlueprintCallable, Category = "Inventory")
	void SetWeightCapacity(const float NewWeightCapacity);

	UFUNCTION(BlueprintCallable, Category = "Inventory")
	void SetCapacity(const int32 NewCapacity);

	/**Return true if we have a given amount of an item*/
	UFUNCTION(BlueprintPure, Category = "Inventory")
	bool HasItem(TSubclassOf <UFNRInventoryItem> ItemClass, const int32 Quantity = 1) const;

	/**Return the first item with the same class as a given Item*/
	UFUNCTION(BlueprintPure, Category = "Inventory")
	UFNRInventoryItem* FindItem(UFNRInventoryItem* Item) const;

	/**Return all items with the same class as a given Item*/
	UFUNCTION(BlueprintPure, Category = "Inventory")
	TArray<UFNRInventoryItem*> FindItems(UFNRInventoryItem* Item) const;

	/**Return the first item with the same class as ItemClass.*/
	UFUNCTION(BlueprintPure, Category = "Inventory")
	UFNRInventoryItem* FindItemByClass(TSubclassOf<UFNRInventoryItem> ItemClass) const;

	/**Get all inventory items that are a child of ItemClass. Useful for grabbing all weapons, all food, etc*/
	UFUNCTION(BlueprintPure, Category = "Inventory")
	TArray<UFNRInventoryItem*> FindItemsByClass(TSubclassOf<UFNRInventoryItem> ItemClass) const;
	
};
