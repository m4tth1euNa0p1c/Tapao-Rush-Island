// monke_island

#include "Misc/AutomationTest.h"
#include "Settings/SettingsMath.h"
#include "Settings/SettingsRules.h"

#if WITH_AUTOMATION_TESTS

// Run with: ./scripts/ue.ps1 test -Filter monke_island.Settings
BEGIN_DEFINE_SPEC(FSettingsSpec, "monke_island.Settings",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)
	USettingsRules* Rules = nullptr;
END_DEFINE_SPEC(FSettingsSpec)

void FSettingsSpec::Define()
{
	BeforeEach([this]()
	{
		Rules = NewObject<USettingsRules>();
		Rules->AddToRoot();
	});

	AfterEach([this]()
	{
		Rules->RemoveFromRoot();
		Rules = nullptr;
	});

	Describe("Defaults (cahier §18)", [this]()
	{
		It("starts with master 80, music 50, the other channels 80, FOV 90 and 60 fps", [this]()
		{
			const FMonkeSettingsValues Values = SettingsMath::Defaults(*Rules);
			TestEqual(TEXT("master"), Values.Volume(ESettingsVolume::Master), 0.8f);
			TestEqual(TEXT("music"), Values.Volume(ESettingsVolume::Music), 0.5f);
			TestEqual(TEXT("ambience"), Values.Volume(ESettingsVolume::Ambience), 0.8f);
			TestEqual(TEXT("interface"), Values.Volume(ESettingsVolume::Interface), 0.8f);
			TestEqual(TEXT("field of view"), Values.FieldOfView, 90.0f);
			TestEqual(TEXT("frame rate"), Values.FrameRateLimit, 60);
			TestFalse(TEXT("no inversion"), Values.bInvertY);
		});
	});

	Describe("Sanitize", [this]()
	{
		It("brings a broken ini back inside the bounds", [this]()
		{
			FMonkeSettingsValues Values;
			Values.Volume(ESettingsVolume::Master) = 7.0f;
			Values.Volume(ESettingsVolume::Effects) = -1.0f;
			Values.Volume(ESettingsVolume::Music) = NAN;
			Values.FieldOfView = 200.0f;
			Values.Sensitivity = 0.0f;
			Values.WindowMode = 9;
			Values.FrameRateLimit = -30;
			SettingsMath::Sanitize(*Rules, Values);
			TestEqual(TEXT("master"), Values.Volume(ESettingsVolume::Master), 1.0f);
			TestEqual(TEXT("effects"), Values.Volume(ESettingsVolume::Effects), 0.0f);
			TestTrue(TEXT("music is a number again"), FMath::IsFinite(Values.Volume(ESettingsVolume::Music)));
			TestEqual(TEXT("fov"), Values.FieldOfView, static_cast<float>(Rules->FieldOfViewRange.Y));
			TestEqual(TEXT("sensitivity"), Values.Sensitivity, static_cast<float>(Rules->SensitivityRange.X));
			TestEqual(TEXT("window mode"), Values.WindowMode, 2);
			TestEqual(TEXT("frame rate"), Values.FrameRateLimit, 0);
		});
	});

	Describe("Mouse", [this]()
	{
		It("scales the controller's own look scales and flips only the pitch when inverted", [this]()
		{
			const FVector2D Base(2.5f, -2.5f);
			const FVector2D Normal = SettingsMath::LookScales(Base, 2.0f, false);
			const FVector2D Inverted = SettingsMath::LookScales(Base, 2.0f, true);
			TestEqual(TEXT("yaw doubled"), static_cast<float>(Normal.X), 5.0f);
			TestEqual(TEXT("pitch doubled, same sign"), static_cast<float>(Normal.Y), -5.0f);
			TestEqual(TEXT("yaw not inverted"), static_cast<float>(Inverted.X), 5.0f);
			TestEqual(TEXT("pitch inverted"), static_cast<float>(Inverted.Y), 5.0f);
		});
	});

	Describe("Display", [this]()
	{
		It("only reapplies the display when a display value changed", [this]()
		{
			FMonkeSettingsValues A;
			FMonkeSettingsValues B = A;
			B.Volume(ESettingsVolume::Ambience) = 0.1f;
			TestFalse(TEXT("volume only"), A.DisplayDiffers(B));
			B.FrameRateLimit = 144;
			TestTrue(TEXT("frame rate"), A.DisplayDiffers(B));
		});

		It("labels the unlimited frame rate", [this]()
		{
			TestEqual(TEXT("unlimited"), SettingsMath::FrameRateLabel(0), FString(TEXT("Illimité")));
			TestEqual(TEXT("144"), SettingsMath::FrameRateLabel(144), FString(TEXT("144")));
		});
	});
}

#endif
