// monke_island

#include "Misc/AutomationTest.h"
#include "Weapons/TorchItem.h"

#if WITH_AUTOMATION_TESTS

#include "UObject/UnrealType.h"

// Run with: ./scripts/ue.ps1 test -Filter monke_island.Weapons.Torch
// The fuel of the torch is computed on every machine from the last lighting, never sent every frame (network review
// T-268 R-40)
BEGIN_DEFINE_SPEC(FTorchSpec, "monke_island.Weapons.Torch",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)
END_DEFINE_SPEC(FTorchSpec)

void FTorchSpec::Define()
{
	It("burns down from the fuel left at the lighting, by the server's clock", [this]()
	{
		// lit at 100 s with 300 s of fuel
		TestEqual(TEXT("at the lighting"), ATorchItem::ComputeFuel(300.0f, 100.0, 100.0, true), 300.0f, 1.0e-3f);
		TestEqual(TEXT("a minute later"), ATorchItem::ComputeFuel(300.0f, 100.0, 160.0, true), 240.0f, 1.0e-3f);
		TestEqual(TEXT("burnt out, never below zero"), ATorchItem::ComputeFuel(300.0f, 100.0, 1000.0, true), 0.0f, 1.0e-3f);
	});

	It("keeps its fuel while out, and burns nothing on a clock that went back", [this]()
	{
		TestEqual(TEXT("out: what was left"), ATorchItem::ComputeFuel(120.0f, 100.0, 5000.0, false), 120.0f, 1.0e-3f);
		TestEqual(TEXT("a new world's clock"), ATorchItem::ComputeFuel(120.0f, 100.0, 3.0, true), 120.0f, 1.0e-3f);
		TestEqual(TEXT("no fuel from nothing"), ATorchItem::ComputeFuel(-5.0f, 0.0, 0.0, false), 0.0f, 1.0e-3f);
		TestEqual(TEXT("a broken value"), ATorchItem::ComputeFuel(NAN, 0.0, 0.0, true), 0.0f, 1.0e-3f);
	});

	It("replicates the lighting, not the fuel of every frame, at a low rate", [this]()
	{
		TestNull(TEXT("no fuel property sent every frame"), FindFProperty<FProperty>(ATorchItem::StaticClass(), TEXT("Fuel")));
		const FProperty* AtChange = FindFProperty<FProperty>(ATorchItem::StaticClass(), TEXT("FuelAtChange"));
		const FProperty* LitTime = FindFProperty<FProperty>(ATorchItem::StaticClass(), TEXT("LitServerTime"));
		TestTrue(TEXT("fuel at the change replicated"), AtChange && AtChange->HasAnyPropertyFlags(CPF_Net));
		TestTrue(TEXT("lighting time replicated"), LitTime && LitTime->HasAnyPropertyFlags(CPF_Net));
		TestTrue(TEXT("low net frequency, changes forced"), GetDefault<ATorchItem>()->GetNetUpdateFrequency() <= 10.0f);
	});
}

#endif
