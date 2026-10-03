// monke_island

#pragma once

#include "CoreMinimal.h"
#include "Blueprint/UserWidget.h"
#include "{{ClassName}}.generated.h"

class UTextBlock;

/**
 *  {{Description}}
 *  Logic and data live here; layout and styling live in the WBP_ subclass.
 */
UCLASS(abstract)
class MONKE_ISLAND_API U{{ClassName}} : public UUserWidget
{
	GENERATED_BODY()

protected:

	/** Optional title text: bound when the WBP_ contains a TextBlock with this exact name */
	UPROPERTY(meta=(BindWidgetOptional))
	TObjectPtr<UTextBlock> TitleText;

	virtual void NativeConstruct() override;

	virtual void NativeDestruct() override;

	/** Lets the Blueprint react when the displayed data changes */
	UFUNCTION(BlueprintImplementableEvent, Category="UI", meta=(DisplayName="Data Updated"))
	void BP_DataUpdated();
};
