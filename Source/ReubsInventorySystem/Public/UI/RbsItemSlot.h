// Copyright Vinipi Studios 2024. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Blueprint/UserWidget.h"
#include "RbsItemSlot.generated.h"

class UFNRInventoryItem;

UCLASS(Blueprintable)
class REUBSINVENTORYSYSTEM_API URbsItemSlot : public UUserWidget
{
	GENERATED_BODY()

public:
	UPROPERTY(BlueprintReadOnly, Category = "Item", meta = (ExposeOnSpawn = true))
	TObjectPtr<UFNRInventoryItem> Item;
};
