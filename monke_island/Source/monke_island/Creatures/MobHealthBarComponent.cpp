// monke_island

#include "Creatures/MobHealthBarComponent.h"
#include "Blueprint/WidgetTree.h"
#include "Creatures/TikiDirector.h"
#include "Components/ProgressBar.h"
#include "Engine/World.h"
#include "GameFramework/PlayerController.h"
#include "Player/IslanderHealthComponent.h"

// ---------------------------------------------------------------- widget

TSharedRef<SWidget> UMobHealthBarWidget::RebuildWidget()
{
	if (WidgetTree && !WidgetTree->RootWidget)
	{
		Bar = WidgetTree->ConstructWidget<UProgressBar>(UProgressBar::StaticClass(), TEXT("Bar"));
		Bar->SetPercent(1.0f);
		WidgetTree->RootWidget = Bar;
	}
	return Super::RebuildWidget();
}

void UMobHealthBarWidget::SetFraction(float Fraction, const FLinearColor& Low, const FLinearColor& Full)
{
	if (Bar)
	{
		const float Clamped = FMath::Clamp(Fraction, 0.0f, 1.0f);
		Bar->SetPercent(Clamped);
		Bar->SetFillColorAndOpacity(FMath::Lerp(Low, Full, Clamped));
	}
}

// ---------------------------------------------------------------- component

UMobHealthBarComponent::UMobHealthBarComponent()
{
	PrimaryComponentTick.bCanEverTick = true;
	PrimaryComponentTick.bStartWithTickEnabled = false;
	SetWidgetSpace(EWidgetSpace::Screen);
	SetWidgetClass(UMobHealthBarWidget::StaticClass());
	SetDrawAtDesiredSize(false);
	SetCollisionEnabled(ECollisionEnabled::NoCollision);
	SetGenerateOverlapEvents(false);
	SetCastShadow(false);
	SetHiddenInGame(true);
}

void UMobHealthBarComponent::BeginPlay()
{
	// nobody looks at a dedicated server
	if (GetNetMode() == NM_DedicatedServer)
	{
		Super::BeginPlay();
		SetComponentTickEnabled(false);
		return;
	}
	SetDrawSize(FVector2D(BarSize.X, BarSize.Y));

	// just above the mob's head (or fin)
	if (const AActor* Owner = GetOwner())
	{
		FVector Origin;
		FVector Extent;
		Owner->GetActorBounds(true, Origin, Extent);
		SetWorldLocation(FVector(Owner->GetActorLocation().X, Owner->GetActorLocation().Y, Origin.Z + Extent.Z + Margin));
	}

	Super::BeginPlay();

	Health = GetOwner() ? GetOwner()->FindComponentByClass<UIslanderHealthComponent>() : nullptr;
	if (!Health)
	{
		SetComponentTickEnabled(false);
		return;
	}
	// the bush grid is built on its first use (tens of ms): now rather than at the first hit
	if (BushHideRadius > 0.0f)
	{
		if (ATikiDirector* Director = ATikiDirector::Get(this))
		{
			FVector Bush;
			Director->FindNearestBush(GetOwner()->GetActorLocation(), 1.0f, Bush);
		}
	}
	Health->OnHealthChanged.AddDynamic(this, &UMobHealthBarComponent::HandleHealthChanged);
	Health->OnDeath.AddDynamic(this, &UMobHealthBarComponent::HandleDeath);
	Refresh();
}

void UMobHealthBarComponent::HandleHealthChanged(float NewHealth, float Delta)
{
	bHurt |= Delta < 0.0f;
	Refresh();
}

void UMobHealthBarComponent::HandleDeath(AActor* Killer)
{
	Refresh();
}

void UMobHealthBarComponent::Refresh()
{
	if (!Health)
	{
		return;
	}
	const bool bWanted = !Health->IsDead() && (bHurt || !bOnlyWhenHurt);
	// the tick checks the distance to the view while the bar is wanted
	SetComponentTickEnabled(bWanted);
	if (!bWanted)
	{
		SetHiddenInGame(true);
		if (bAddedToScreen)
		{
			RemoveWidgetFromScreen();
		}
		return;
	}
	if (UMobHealthBarWidget* Bar = Cast<UMobHealthBarWidget>(GetUserWidgetObject()))
	{
		Bar->SetFraction(Health->GetHealthFraction(), LowColor, FullColor);
	}
}

void UMobHealthBarComponent::TickComponent(float DeltaTime, ELevelTick TickType, FActorComponentTickFunction* ThisTickFunction)
{
	const APlayerController* Viewer = GetWorld() ? GetWorld()->GetFirstPlayerController() : nullptr;
	bool bNear = false;
	if (Viewer && Viewer->IsLocalController())
	{
		FVector ViewLocation;
		FRotator ViewRotation;
		Viewer->GetPlayerViewPoint(ViewLocation, ViewRotation);
		bNear = FVector::DistSquared(ViewLocation, GetComponentLocation()) <= FMath::Square(MaxDistance);
	}
	// hidden in a bush: the bar must not give the mob away (checked a few times a second)
	BushCheckCountdown -= DeltaTime;
	if (BushHideRadius > 0.0f && bNear && BushCheckCountdown <= 0.0f)
	{
		BushCheckCountdown = BushCheckInterval;
		ATikiDirector* Director = ATikiDirector::Get(this);
		FVector Bush;
		bInBush = Director && Director->FindNearestBush(GetOwner()->GetActorLocation(), BushHideRadius, Bush);
	}
	bNear = bNear && !(BushHideRadius > 0.0f && bInBush);
	if (bHiddenInGame == bNear)
	{
		SetHiddenInGame(!bNear);
		if (bNear)
		{
			Refresh();
		}
	}
	Super::TickComponent(DeltaTime, TickType, ThisTickFunction);
}
