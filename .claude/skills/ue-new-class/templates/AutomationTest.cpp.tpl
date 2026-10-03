// monke_island

#include "Misc/AutomationTest.h"

#if WITH_AUTOMATION_TESTS

// Run with: ./scripts/ue.ps1 test -Filter monke_island.{{TestPath}}
BEGIN_DEFINE_SPEC({{ClassName}}Spec, "monke_island.{{TestPath}}",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)
END_DEFINE_SPEC({{ClassName}}Spec)

void {{ClassName}}Spec::Define()
{
	Describe("{{Description}}", [this]()
	{
		It("should behave as specified", [this]()
		{
			TestTrue(TEXT("placeholder"), true);
		});
	});
}

#endif // WITH_AUTOMATION_TESTS
