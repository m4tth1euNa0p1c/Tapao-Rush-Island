// monke_island

#include "Boats/ExpeditionArrivalPoint.h"
#include "Components/ArrowComponent.h"
#include "Components/BillboardComponent.h"
#include "Components/SceneComponent.h"

AExpeditionArrivalPoint::AExpeditionArrivalPoint()
{
	PrimaryActorTick.bCanEverTick = false;
	bReplicates = false;
	SetCanBeDamaged(false);

	Root = CreateDefaultSubobject<USceneComponent>(TEXT("Root"));
	Root->SetMobility(EComponentMobility::Static);
	SetRootComponent(Root);

#if WITH_EDITORONLY_DATA
	Arrow = CreateEditorOnlyDefaultSubobject<UArrowComponent>(TEXT("Bow"));
	if (Arrow)
	{
		Arrow->SetupAttachment(Root);
		Arrow->ArrowColor = FColor(255, 170, 40);
		Arrow->ArrowSize = 4.0f;
		Arrow->ArrowLength = 120.0f;
		Arrow->bIsScreenSizeScaled = false;
	}
	Sprite = CreateEditorOnlyDefaultSubobject<UBillboardComponent>(TEXT("Sprite"));
	if (Sprite)
	{
		Sprite->SetupAttachment(Root);
	}
#endif
}

FTransform AExpeditionArrivalPoint::GetBoatTransform() const
{
	return FTransform(FRotator(0.0f, GetActorRotation().Yaw, 0.0f), GetActorLocation());
}
