// monke_island

#include "Player/IslanderHUD.h"
#include "UI/Tapao/TapaoUISubsystem.h"
#include "Engine/GameInstance.h"
#include "DayNight/DayNightCycleActor.h"
#include "Storm/WindStormActor.h"
#include "CanvasItem.h"
#include "Engine/Canvas.h"
#include "Engine/Engine.h"
#include "Engine/Font.h"
#include "GameFramework/Character.h"
#include "Components/PrimitiveComponent.h"
#include "Interaction/IslanderGrabComponent.h"
#include "Interaction/IslanderInteractable.h"
#include "Items/ValuableComponent.h"
#include "Player/IslanderCharacter.h"
#include "Player/IslanderCombatComponent.h"
#include "Player/IslanderHealthComponent.h"
#include "Player/IslanderMovementComponent.h"
#include "Weapons/InventoryItemActor.h"
#include "Weapons/InventoryItemDefinition.h"
#include "Weapons/IslanderInventoryComponent.h"
#include "Weapons/MusketWeapon.h"
#include "Weapons/CutlassWeapon.h"

float AIslanderHUD::DrawBar(float X, float Y, float Scale, float Fraction, const FLinearColor& Color, const FString& Label)
{
	const FVector2D Size = BarSize * Scale;

	FCanvasTileItem Back(FVector2D(X, Y), Size, FLinearColor(0.0f, 0.0f, 0.0f, 0.45f));
	Back.BlendMode = SE_BLEND_Translucent;
	Canvas->DrawItem(Back);

	FCanvasTileItem Fill(FVector2D(X, Y), FVector2D(Size.X * FMath::Clamp(Fraction, 0.0f, 1.0f), Size.Y), Color);
	Fill.BlendMode = SE_BLEND_Translucent;
	Canvas->DrawItem(Fill);

	FCanvasTextItem Text(FVector2D(X + Size.X + 10.0f * Scale, Y - 3.0f * Scale), FText::FromString(Label), GEngine->GetSmallFont(), FLinearColor::White);
	Text.Scale = FVector2D(Scale, Scale);
	Text.EnableShadow(FLinearColor::Black);
	Canvas->DrawItem(Text);

	return Y - Size.Y - 10.0f * Scale;
}

void AIslanderHUD::DrawHUD()
{
	Super::DrawHUD();

	const ACharacter* Character = Cast<ACharacter>(GetOwningPawn());
	if (!Character || !Canvas)
	{
		return;
	}

	const float Scale = Canvas->ClipY / 1080.0f;
	const float X = Margin.X * Scale;
	float Y = Canvas->ClipY - Margin.Y * Scale;

	// the TAPAO HUD draws the gauges when it is up (UTapaoUISubsystem); these provisional bars stand in otherwise
	const UGameInstance* GameInstance = GetGameInstance();
	const UTapaoUISubsystem* Tapao = GameInstance ? GameInstance->GetSubsystem<UTapaoUISubsystem>() : nullptr;
	const bool bBars = !Tapao || !Tapao->IsHudShown();
	if (const UIslanderMovementComponent* Move = bBars ? Cast<UIslanderMovementComponent>(Character->GetCharacterMovement()) : nullptr)
	{
		const bool bExhausted = Move->IsExhausted();
		Y = DrawBar(X, Y, Scale, Move->GetStaminaFraction(), bExhausted ? ExhaustedColor : StaminaColor,
			bExhausted ? TEXT("Endurance - épuisé") : TEXT("Endurance"));
	}
	if (const UIslanderHealthComponent* Health = bBars ? Character->FindComponentByClass<UIslanderHealthComponent>() : nullptr)
	{
		Y = DrawBar(X, Y, Scale, Health->GetHealthFraction(), HealthColor,
			Health->IsDead() ? TEXT("Santé - mort") : FString::Printf(TEXT("Santé %.0f"), Health->GetHealth()));
	}

	// stance line above the bars; the day and the wind first (T-067)
	TArray<FString> States;
	if (const ADayNightCycleActor* Clock = ADayNightCycleActor::Get(this))
	{
		States.Add(FString::Printf(TEXT("Jour %d"), Clock->GetDayNumber()));
	}
	if (const AWindStormActor* Storm = AWindStormActor::Get(this))
	{
		if (Storm->IsDeadlineWind())
		{
			States.Add(TEXT("Tempête"));
		}
		else if (Storm->IsBlowing())
		{
			States.Add(FString::Printf(TEXT("Coup de vent (niveau %d)"), Storm->GetGustLevel() + 1));
		}
		if (Storm->IsBlowing() && Storm->GetShelterFactor(Character) < 1.0f)
		{
			States.Add(TEXT("À l'abri"));
		}
	}
	if (Character->bIsCrouched)
	{
		States.Add(TEXT("Accroupi"));
	}
	if (const UIslanderCombatComponent* Combat = Character->FindComponentByClass<UIslanderCombatComponent>())
	{
		if (Combat->IsGuarding())
		{
			States.Add(TEXT("Garde"));
		}
	}
	if (const UIslanderMovementComponent* Move = Cast<UIslanderMovementComponent>(Character->GetCharacterMovement()))
	{
		if (Move->IsSprinting())
		{
			States.Add(TEXT("Course"));
		}
		if (Move->IsSwimming())
		{
			States.Add(TEXT("Nage"));
		}
		if (Move->GetCarriedWeight() > 0.0f)
		{
			States.Add(FString::Printf(TEXT("Charge %.0f kg"), Move->GetCarriedWeight()));
		}
	}
	// crosshair dot + "F  <action>" prompt (cahier §4: target and action only within reach and in sight)
	if (const AIslanderCharacter* Islander = Cast<AIslanderCharacter>(Character))
	{
		const UIslanderGrabComponent* Grab = Islander->GetGrabComponent();
		const FVector2D Center(Canvas->ClipX * 0.5f, Canvas->ClipY * 0.5f);

		FString Action;
		FString Details;
		FString Secondary;
		if (Grab && !Islander->IsRagdoll())
		{
			if (const UPrimitiveComponent* Held = Grab->GetHeldComponent())
			{
				const UValuableComponent* Valuable = Held->GetOwner() ? Held->GetOwner()->FindComponentByClass<UValuableComponent>() : nullptr;
				Action = TEXT("Lâcher");
				Details = Valuable ? FString::Printf(TEXT("%.0f kg · %d $"), Held->GetMass(), Valuable->GetValue()) : FString::Printf(TEXT("%.0f kg"), Held->GetMass());
				Secondary = TEXT("Clic gauche  lancer      Molette  distance");
			}
			else
			{
				AActor* Interactable = nullptr;
				UPrimitiveComponent* Physics = nullptr;
				FVector Point;
				if (Grab->FindInteractionTarget(Interactable, Physics, Point))
				{
					if (Interactable)
					{
						Action = IIslanderInteractable::Execute_GetInteractionPrompt(Interactable, const_cast<AIslanderCharacter*>(Islander)).ToString();
					}
					else if (Physics)
					{
						const UValuableComponent* Valuable = Physics->GetOwner() ? Physics->GetOwner()->FindComponentByClass<UValuableComponent>() : nullptr;
						Action = TEXT("Soulever");
						Details = Valuable ? FString::Printf(TEXT("%.0f kg · %d $"), Physics->GetMass(), Valuable->GetValue()) : FString::Printf(TEXT("%.0f kg"), Physics->GetMass());
						if (Physics->GetMass() > Grab->LiftMassKg)
						{
							Details += TEXT("  (lourd : à plusieurs)");
						}
					}
				}
			}
		}

		// the dot grows and turns gold on a target
		if (!Islander->IsRagdoll())
		{
			const bool bTarget = !Action.IsEmpty();
			const float Size = (bTarget ? 9.0f : 5.0f) * Scale;
			FCanvasTileItem Border(Center - FVector2D(Size * 0.5f + Scale, Size * 0.5f + Scale), FVector2D(Size + 2.0f * Scale, Size + 2.0f * Scale), FLinearColor(0.0f, 0.0f, 0.0f, 0.55f));
			Border.BlendMode = SE_BLEND_Translucent;
			Canvas->DrawItem(Border);
			FCanvasTileItem Dot(Center - FVector2D(Size * 0.5f, Size * 0.5f), FVector2D(Size, Size),
				bTarget ? FLinearColor(1.0f, 0.82f, 0.25f, 0.95f) : FLinearColor(1.0f, 1.0f, 1.0f, 0.85f));
			Dot.BlendMode = SE_BLEND_Translucent;
			Canvas->DrawItem(Dot);
		}

		if (!Action.IsEmpty())
		{
			// key badge "F" then the action, under the dot
			const FVector2D Badge(Center.X - 12.0f * Scale, Center.Y + 22.0f * Scale);
			const FVector2D BadgeSize(24.0f * Scale, 24.0f * Scale);
			FCanvasTileItem BadgeBack(Badge, BadgeSize, FLinearColor(0.05f, 0.05f, 0.05f, 0.7f));
			BadgeBack.BlendMode = SE_BLEND_Translucent;
			Canvas->DrawItem(BadgeBack);
			FCanvasBoxItem BadgeBox(Badge, BadgeSize);
			BadgeBox.SetColor(FLinearColor(1.0f, 0.82f, 0.25f));
			Canvas->DrawItem(BadgeBox);
			FCanvasTextItem Key(Badge + FVector2D(7.0f, 3.0f) * Scale, FText::FromString(TEXT("F")), GEngine->GetMediumFont(), FLinearColor(1.0f, 0.82f, 0.25f));
			Key.Scale = FVector2D(Scale, Scale);
			Canvas->DrawItem(Key);

			FCanvasTextItem Text(Badge + FVector2D(BadgeSize.X + 8.0f * Scale, 3.0f * Scale), FText::FromString(Action), GEngine->GetMediumFont(), FLinearColor::White);
			Text.Scale = FVector2D(Scale, Scale);
			Text.EnableShadow(FLinearColor::Black);
			Canvas->DrawItem(Text);

			float LineY = Badge.Y + BadgeSize.Y + 6.0f * Scale;
			for (const FString& Line : { Details, Secondary })
			{
				if (Line.IsEmpty())
				{
					continue;
				}
				FCanvasTextItem Small(FVector2D(Badge.X, LineY), FText::FromString(Line), GEngine->GetSmallFont(), FLinearColor(1.0f, 0.92f, 0.7f, 0.9f));
				Small.Scale = FVector2D(Scale, Scale);
				Small.EnableShadow(FLinearColor::Black);
				Canvas->DrawItem(Small);
				LineY += 16.0f * Scale;
			}
		}
	}

	// inventory slots at the bottom centre, ammunition of the musket above them (T-069)
	if (const UIslanderInventoryComponent* Inventory = Character->FindComponentByClass<UIslanderInventoryComponent>())
	{
		const int32 SlotCount = Inventory->SlotCount;
		// with the TAPAO HUD the lines below sit centred on its item bar (4 slots of 64 px, 18 px from the bottom)
		const FVector2D SlotSize = bBars ? FVector2D(118.0f * Scale, 34.0f * Scale) : FVector2D(64.0f * Scale, 64.0f * Scale);
		const float Gap = 6.0f * Scale;
		const float Left = Canvas->ClipX * 0.5f - (SlotCount * SlotSize.X + (SlotCount - 1) * Gap) * 0.5f;
		const float Top = bBars ? Canvas->ClipY - Margin.Y * Scale - SlotSize.Y : Canvas->ClipY - (18.0f + 64.0f + 4.0f) * Scale;
		const float Centre = Canvas->ClipX * 0.5f;
		// the TAPAO HUD has its own item bar: this provisional strip only stands in without it
		for (int32 Slot = 0; bBars && Slot < SlotCount; ++Slot)
		{
			const FVector2D Pos(Left + Slot * (SlotSize.X + Gap), Top);
			const bool bActive = Slot == Inventory->GetActiveSlot();
			FCanvasTileItem Back(Pos, SlotSize, FLinearColor(0.0f, 0.0f, 0.0f, bActive ? 0.6f : 0.35f));
			Back.BlendMode = SE_BLEND_Translucent;
			Canvas->DrawItem(Back);
			if (bActive)
			{
				FCanvasBoxItem Box(Pos, SlotSize);
				Box.SetColor(FLinearColor(1.0f, 0.82f, 0.25f));
				Canvas->DrawItem(Box);
			}
			const AInventoryItemActor* Item = Inventory->GetItem(Slot);
			const UInventoryItemDefinition* Definition = Item ? Item->GetDefinition() : nullptr;
			const FString Label = FString::Printf(TEXT("%d  %s"), Slot + 1, Definition ? *Definition->DisplayName.ToString() : TEXT(""));
			FCanvasTextItem Text(Pos + FVector2D(8.0f, 9.0f) * Scale, FText::FromString(Label), GEngine->GetSmallFont(),
				bActive ? FLinearColor(1.0f, 0.82f, 0.25f) : FLinearColor(1.0f, 1.0f, 1.0f, 0.8f));
			Text.Scale = FVector2D(Scale, Scale);
			Text.EnableShadow(FLinearColor::Black);
			Canvas->DrawItem(Text);
		}

		if (const AMusketWeapon* Musket = Cast<AMusketWeapon>(Inventory->GetEquippedItem()))
		{
			const int32 Total = Musket->GetPouchBalls() + (Musket->IsLoaded() ? 1 : 0);
			FString Line = FString::Printf(TEXT("%s · %d balles"), Musket->IsLoaded() ? TEXT("Chargé") : TEXT("Vide"), Total);
			if (Musket->IsReloading())
			{
				Line = FString::Printf(TEXT("Rechargement %.0f %% · %d balles"), Musket->GetReloadProgress() * 100.0f, Total);
			}
			else if (!Musket->IsLoaded())
			{
				Line += Total > 0 ? TEXT("  (R recharger)") : TEXT("  (plus de balles)");
			}
			const float LineY = Top - 22.0f * Scale;
			if (Musket->IsReloading())
			{
				const float BarWidth = SlotCount * SlotSize.X + (SlotCount - 1) * Gap;
				FCanvasTileItem Bar(FVector2D(Left, Top - 6.0f * Scale), FVector2D(BarWidth * Musket->GetReloadProgress(), 3.0f * Scale), FLinearColor(1.0f, 0.82f, 0.25f, 0.9f));
				Bar.BlendMode = SE_BLEND_Translucent;
				Canvas->DrawItem(Bar);
			}
			FCanvasTextItem Ammo(FVector2D(Centre, LineY), FText::FromString(Line), GEngine->GetSmallFont(), FLinearColor(1.0f, 0.92f, 0.7f, 0.95f));
			Ammo.bCentreX = true;
			Ammo.Scale = FVector2D(Scale, Scale);
			Ammo.EnableShadow(FLinearColor::Black);
			Canvas->DrawItem(Ammo);
		}

		// cutlass: charge of the finisher, edge, damage numbers of the last blows (T-070)
		if (const ACutlassWeapon* Cutlass = Cast<ACutlassWeapon>(Inventory->GetEquippedItem()))
		{
			FString Line = Cutlass->IsParrying() ? TEXT("Parade") : TEXT("Sabre");
			Line += FString::Printf(TEXT(" · fil %.0f %%"), Cutlass->GetSharpness() * 100.0f);
			if (const UIslanderMovementComponent* CutlassMove = Cast<UIslanderMovementComponent>(Character->GetCharacterMovement());
				CutlassMove && Cutlass->Rules && CutlassMove->GetStaminaFraction() < Cutlass->Rules->StaminaCostFraction)
			{
				Line += TEXT("  (trop essoufflé)");
			}
			if (const float Charge = Cutlass->GetChargeProgress(); Charge > 0.0f)
			{
				const float BarWidth = SlotCount * SlotSize.X + (SlotCount - 1) * Gap;
				FCanvasTileItem Bar(FVector2D(Left, Top - 6.0f * Scale), FVector2D(BarWidth * Charge, 3.0f * Scale),
					Charge >= 1.0f ? FLinearColor(1.0f, 0.3f, 0.2f, 0.95f) : FLinearColor(1.0f, 0.82f, 0.25f, 0.9f));
				Bar.BlendMode = SE_BLEND_Translucent;
				Canvas->DrawItem(Bar);
			}
			FCanvasTextItem Edge(FVector2D(Centre, Top - 22.0f * Scale), FText::FromString(Line), GEngine->GetSmallFont(), FLinearColor(1.0f, 0.92f, 0.7f, 0.95f));
			Edge.bCentreX = true;
			Edge.Scale = FVector2D(Scale, Scale);
			Edge.EnableShadow(FLinearColor::Black);
			Canvas->DrawItem(Edge);

			const double Now = GetWorld()->GetTimeSeconds();
			for (const FCutlassHitMarker& Marker : Cutlass->GetHitMarkers())
			{
				const float Age = static_cast<float>(Now - Marker.Time);
				if (Age > 1.2f)
				{
					continue;
				}
				const FVector Screen = Project(Marker.Location + FVector(0.0f, 0.0f, 40.0f * Age), false);
				if (Screen.Z <= 0.0f)
				{
					continue;
				}
				const FString Number = Marker.bKill ? FString::Printf(TEXT("-%.0f  TRANCHÉ !"), Marker.Damage) : FString::Printf(TEXT("-%.0f"), Marker.Damage);
				FCanvasTextItem Damage(FVector2D(Screen.X, Screen.Y), FText::FromString(Number), GEngine->GetMediumFont(),
					FLinearColor(1.0f, 0.18f, 0.12f, FMath::Clamp(1.2f - Age, 0.0f, 1.0f)));
				Damage.Scale = FVector2D(Scale, Scale);
				Damage.bCentreX = true;
				Damage.EnableShadow(FLinearColor::Black);
				Canvas->DrawItem(Damage);
			}
		}
	}

	if (States.Num() > 0)
	{
		FCanvasTextItem Text(FVector2D(X, Y - 6.0f * Scale), FText::FromString(FString::Join(States, TEXT(" · "))), GEngine->GetSmallFont(), FLinearColor(1.0f, 1.0f, 1.0f, 0.85f));
		Text.Scale = FVector2D(Scale, Scale);
		Text.EnableShadow(FLinearColor::Black);
		Canvas->DrawItem(Text);
	}
}
