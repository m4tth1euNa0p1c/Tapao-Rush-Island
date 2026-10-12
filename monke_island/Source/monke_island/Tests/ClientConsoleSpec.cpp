// monke_island

#include "Misc/AutomationTest.h"
#include "monke_islandPlayerController.h"

#if WITH_AUTOMATION_TESTS

#include "HAL/IConsoleManager.h"

// Run with: ./scripts/ue.ps1 test -Filter monke_island.Net.ClientConsole
// The host's console, cheats and pause stay the host's in development builds (security review T-269 M3, T-296)
BEGIN_DEFINE_SPEC(FClientConsoleSpec, "monke_island.Net.ClientConsole",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)
END_DEFINE_SPEC(FClientConsoleSpec)

void FClientConsoleSpec::Define()
{
	It("keeps serverexec, EnableCheats and pause for the host's own controller", [this]()
	{
		TestTrue(TEXT("the host"), Amonke_islandPlayerController::IsHostConsoleAllowed(true, false));
		TestFalse(TEXT("a client"), Amonke_islandPlayerController::IsHostConsoleAllowed(false, false));
		TestTrue(TEXT("a client the host lets in for a test"), Amonke_islandPlayerController::IsHostConsoleAllowed(false, true));
	});

	It("lets no client in by default", [this]()
	{
		const IConsoleVariable* Allow = IConsoleManager::Get().FindConsoleVariable(TEXT("monke.Net.AllowClientCheats"));
		if (TestNotNull(TEXT("the host's switch"), Allow))
		{
			TestEqual(TEXT("off"), Allow->GetInt(), 0);
		}
	});
}

#endif
