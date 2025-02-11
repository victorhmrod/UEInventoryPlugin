// Copyright Vinipi Studios 2024. All Rights Reserved.


#include "Core/FNRInventoryComponent.h"

#include "Components/CapsuleComponent.h"
#include "Core/FNRInventoryItem.h"
#include "Engine/ActorChannel.h"
#include "GameFramework/Character.h"
#include "Net/UnrealNetwork.h"
#include "Utils/RbsPickupInterface.h"

#define LOCTEXT_NAMESPACE "Inventory"

UFNRInventoryComponent::UFNRInventoryComponent()
{
	PrimaryComponentTick.bCanEverTick = false;
	SetIsReplicatedByDefault(true);
}

/*
 * Replication
 */

void UFNRInventoryComponent::GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const
{
	Super::GetLifetimeReplicatedProps(OutLifetimeProps);

	DOREPLIFETIME(UFNRInventoryComponent, Items);
}

bool UFNRInventoryComponent::ReplicateSubobjects(UActorChannel* Channel, FOutBunch* Bunch, FReplicationFlags* RepFlags)
{
	bool bWroteSomething = Super::ReplicateSubobjects(Channel, Bunch, RepFlags);

	//Check if the array of items needs to replicate
	if (Channel->KeyNeedsToReplicate(0, ReplicatedItemsKey))
	{
		for (auto& Item : Items)
		{
			if (Channel->KeyNeedsToReplicate(Item->GetUniqueID(), Item->RepKey))
			{
				bWroteSomething |= Channel->ReplicateSubobject(Item, *Bunch, *RepFlags);
			}
		}
	}

	return bWroteSomething;
}

/*
 * Behaviour
 */

UFNRInventoryItem* UFNRInventoryComponent::AddItem(UFNRInventoryItem* Item)
{
	if (GetOwner()->GetLocalRole() < ROLE_Authority)
		return nullptr;

	UFNRInventoryItem* NewItem = NewObject<UFNRInventoryItem>(GetOwner(), Item->GetClass());
	NewItem->SetQuantity(Item->GetQuantity());
	NewItem->OwningInventory = this;
	NewItem->AddedToInventory(this);
	Items.Add(NewItem);
	OnReplicated_Items();
	NewItem->MarkDirtyForReplication();

	NewItem->OnItemModified.AddDynamic(this, &ThisClass::OnItemModified_Internal);

	return NewItem;
}

FItemAddResult UFNRInventoryComponent::TryAddItem(UFNRInventoryItem* Item)
{
	return TryAddItem_Internal(Item);
}

FItemAddResult UFNRInventoryComponent::TryAddItemFromClass(TSubclassOf<UFNRInventoryItem> ItemClass, const int32 Quantity)
{
	UFNRInventoryItem* Item = NewObject<UFNRInventoryItem>(GetOwner(), ItemClass);
	Item->SetQuantity(Quantity);
	
	return TryAddItem_Internal(Item);
}

FItemAddResult UFNRInventoryComponent::TryAddItem_Internal(UFNRInventoryItem* Item)
{
	if (GetOwner()->GetLocalRole() < ROLE_Authority)
		return FItemAddResult::AddedNone(Item->GetQuantity(), LOCTEXT("InventoryCallingFunctionsFromClient", "ERROR | You're trying to add items from a client"));;

	const int32 AddAmount = Item->GetQuantity();
	if (Items.Num() + 1 > GetCapacity())
		return FItemAddResult::AddedNone(AddAmount, LOCTEXT("InventoryCapacityFullText", "Inventory Is Full"));

	if (GetCurrentWeight() + Item->Weight > GetWeightCapacity())
		return FItemAddResult::AddedNone(AddAmount, LOCTEXT("InventoryTooMuchWeightText", "Too Much Weight"));
	
	const int32 WeightMaxAddAmount = FMath::FloorToInt((WeightCapacity - GetCurrentWeight()) / Item->Weight);
	int32 ActualAddAmount = FMath::Min(AddAmount, WeightMaxAddAmount);
	if (ActualAddAmount <= 0)
		return FItemAddResult::AddedNone(AddAmount, LOCTEXT("InventoryErrorText", "Couldn't add any item"));
	
	if (Item->bStackable)
	{
		auto ExistingItems = FindItems(Item);
		for (auto Temp : ExistingItems)
		{
			if (Temp->GetQuantity() >= Temp->MaxStackSize)
				continue;
    			
			const int32 StackAddAmount = FMath::Min(ActualAddAmount, Temp->MaxStackSize - Temp->GetQuantity());
			if (StackAddAmount <= 0)
				continue;
    			
			Temp->SetQuantity(Temp->GetQuantity() + StackAddAmount);
			Item->SetQuantity(Item->GetQuantity() - StackAddAmount);
			ActualAddAmount -= StackAddAmount;
		}
	}
    
	if (ActualAddAmount > 0)
	{
		const int32 StacksToAdd = FMath::CeilToInt(float(ActualAddAmount) / float(Item->MaxStackSize));
		for (size_t i = 0; i < StacksToAdd; i++)
		{
			if (ActualAddAmount <= 0)
				break;
    				
			const int32 StackAddAmount = FMath::Min(ActualAddAmount, Item->MaxStackSize);
			if (StackAddAmount <= 0)
				continue;
    				
			ActualAddAmount -= StackAddAmount;
			Item->SetQuantity(StackAddAmount);
			AddItem(Item);
		}
	}

	if (ActualAddAmount > 0)
		return FItemAddResult::AddedSome(Item, AddAmount, ActualAddAmount, LOCTEXT("InventoryAddedSomeText", "Couldn't add all items"));
    		
	return FItemAddResult::AddedAll(Item, AddAmount);
}

bool UFNRInventoryComponent::RemoveItem(UFNRInventoryItem* Item)
{
	if (GetOwnerRole() < ROLE_Authority)
		return false;

	if (!IsValid(Item))
		return false;

	Item->OwningInventory = nullptr;
	Items.RemoveSingle(Item);
	Item->MarkDirtyForReplication();
	
	OnReplicated_Items();
	
	ReplicatedItemsKey++;
	
	return true;
}

int32 UFNRInventoryComponent::ConsumeItem(UFNRInventoryItem* Item)
{
	if (IsValid(Item))
	{
		return ConsumeItem(Item, Item->Quantity);
	}
	
	return 0;
}

int32 UFNRInventoryComponent::ConsumeItem(UFNRInventoryItem* Item, const int32 Quantity)
{
	if (GetOwner()->GetLocalRole() < ROLE_Authority)
		return 0;

	if (!IsValid(Item))
		return 0;

	const int32 RemoveQuantity = FMath::Min(Quantity, Item->GetQuantity());

	ensure(!(Item->GetQuantity() - RemoveQuantity < 0));

	Item->SetQuantity(Item->GetQuantity() - RemoveQuantity);

	if (Item->GetQuantity() <= 0)
	{
		RemoveItem(Item);
	}
	else
	{
		ClientRefreshInventory();
	}

	return RemoveQuantity;
}

void UFNRInventoryComponent::UseItem(UFNRInventoryItem* Item)
{
	if (GetOwnerRole() < ROLE_Authority)
		ServerUseItem(Item);

	if (GetOwner()->GetLocalRole() >= ROLE_Authority)
	{
		if (!IsValid(FindItem(Item)))
			return;
	}

	if (IsValid(Item))
	{
		Item->Use(this);
	}
}

void UFNRInventoryComponent::ServerUseItem_Implementation(UFNRInventoryItem* Item)
{
	UseItem(Item);
}

void UFNRInventoryComponent::DropItem(UFNRInventoryItem* Item, const int32 Quantity)
{
	if (!IsValid(FindItem(Item)))
		return;

	if (GetOwnerRole() < ROLE_Authority)
	{
		ServerDropItem(Item, Quantity);
		return;
	}
	
	const int32 DroppedQuantity = ConsumeItem(Item, Quantity);

	FActorSpawnParameters SpawnParams;
	SpawnParams.Owner = GetOwner();
	SpawnParams.bNoFail = true;
	SpawnParams.SpawnCollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::AdjustIfPossibleButAlwaysSpawn;

	FVector SpawnLocation = GetOwner()->GetActorLocation();
	SpawnLocation.Z -= Cast<ACharacter>(GetOwner())->GetCapsuleComponent()->GetScaledCapsuleHalfHeight();
	
	FTransform SpawnTransform(GetOwner()->GetActorRotation(), SpawnLocation);

	ensure(Item->PickupClass);

	AActor* Pickup = GetWorld()->SpawnActor<AActor>(Item->PickupClass.LoadSynchronous(), SpawnTransform, SpawnParams);
	IRbsPickupInterface::Execute_SetPickupQuantity(Pickup, DroppedQuantity);
	IRbsPickupInterface::Execute_OnDropItem(Pickup);
}

void UFNRInventoryComponent::ServerDropItem_Implementation(UFNRInventoryItem* Item, const int32 Quantity)
{
	DropItem(Item, Quantity);
}

void UFNRInventoryComponent::OnItemModified_Internal()
{
	OnInventoryUpdated.Broadcast();
}

/*
 * Helpers
 */

bool UFNRInventoryComponent::HasItem(TSubclassOf<UFNRInventoryItem> ItemClass, const int32 Quantity) const
{
	UFNRInventoryItem* ItemToFind = FindItemByClass(ItemClass);
	if (IsValid(ItemToFind))
	{
		return ItemToFind->GetQuantity() >= Quantity;
	}
	return false;
}

UFNRInventoryItem* UFNRInventoryComponent::FindItem(UFNRInventoryItem* Item) const
{
	return FindItemByClass(Item->GetClass());
}

TArray<UFNRInventoryItem*> UFNRInventoryComponent::FindItems(UFNRInventoryItem* Item) const
{
	return FindItemsByClass(Item->GetClass());
}

UFNRInventoryItem* UFNRInventoryComponent::FindItemByClass(TSubclassOf<UFNRInventoryItem> ItemClass) const
{
	for (auto& Item : Items)
	{
		if (Item->GetClass() == ItemClass)
		{
			return Item;
		}
	}

	return nullptr;
}

TArray<UFNRInventoryItem*> UFNRInventoryComponent::FindItemsByClass(TSubclassOf<UFNRInventoryItem> ItemClass) const
{
	TArray<UFNRInventoryItem*> ItemsOfClas{};
	for (auto& Item : Items)
	{
		if (Item->GetClass() == ItemClass)
		{
			ItemsOfClas.Add(Item);
		}
	}

	return ItemsOfClas;
}

float UFNRInventoryComponent::GetCurrentWeight() const
{
	float Weight = 0.f;

	for (auto& Item : Items)
	{
		Weight += Item->GetStackWeight();
	}

	return Weight;
}

void UFNRInventoryComponent::SetWeightCapacity(const float NewWeightCapacity)
{
	WeightCapacity = NewWeightCapacity;
	OnInventoryUpdated.Broadcast();
}

void UFNRInventoryComponent::SetCapacity(const int32 NewCapacity)
{
	Capacity = NewCapacity;
	OnInventoryUpdated.Broadcast();
}

void UFNRInventoryComponent::OnReplicated_Items()
{
	OnInventoryUpdated.Broadcast();
}

void UFNRInventoryComponent::ClientRefreshInventory_Implementation()
{
	OnInventoryUpdated.Broadcast();
}

#undef LOCTEXT_NAMESPACE
