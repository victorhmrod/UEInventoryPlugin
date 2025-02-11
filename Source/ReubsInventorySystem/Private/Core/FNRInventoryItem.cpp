// Copyright Vinipi Studios 2024. All Rights Reserved.

#include "Core/FNRInventoryItem.h"

#include "Core/FNRInventoryComponent.h"
#include "Net/UnrealNetwork.h"

#define LOCTEXT_NAMESPACE "Item"

UFNRInventoryItem::UFNRInventoryItem()
{
	DisplayName = LOCTEXT("Placeholder Name", "Item");
	Description = LOCTEXT("Placeholder Description", "Item");
}

#if WITH_EDITOR

void UFNRInventoryItem::PostEditChangeProperty(FPropertyChangedEvent& PropertyChangedEvent)
{
	Super::PostEditChangeProperty(PropertyChangedEvent);
	
	FName ChangedPropertyName = PropertyChangedEvent.Property ? PropertyChangedEvent.Property->GetFName() : NAME_None;

	// UPROPERTY clamping doesn't support using a variable to clamp so we do in here instead
	if (ChangedPropertyName == GET_MEMBER_NAME_CHECKED(UFNRInventoryItem, Quantity))
	{
		Quantity = FMath::Clamp(Quantity, 1, bStackable ? MaxStackSize : 1);
	}
	else if (ChangedPropertyName == GET_MEMBER_NAME_CHECKED(UFNRInventoryItem, bStackable))
	{
		if (!bStackable)
		{
			Quantity = 1;
			MaxStackSize = 1;
		}
	}
}

#endif

void UFNRInventoryItem::GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const
{
	UObject::GetLifetimeReplicatedProps(OutLifetimeProps);

	DOREPLIFETIME(UFNRInventoryItem, Quantity);
}

void UFNRInventoryItem::MarkDirtyForReplication()
{
	++RepKey;

	if (IsValid(OwningInventory))
	{
		OwningInventory->ReplicatedItemsKey++;
	}
}

void UFNRInventoryItem::OnRep_Quantity()
{
	OnItemModified.Broadcast();
}

void UFNRInventoryItem::Use_Implementation(UFNRInventoryComponent* Inventory)
{
}

void UFNRInventoryItem::AddedToInventory_Implementation(UFNRInventoryComponent* Inventory)
{
}

void UFNRInventoryItem::SetQuantity(const int32 NewQuantity)
{
	if (NewQuantity != Quantity)
	{
		Quantity = NewQuantity;
			//FMath::Clamp(NewQuantity, 0, bStackable ? MaxStackSize : 1);
		OnRep_Quantity();
		MarkDirtyForReplication();
	}
}

#undef LOCTEXT_NAMESPACE
