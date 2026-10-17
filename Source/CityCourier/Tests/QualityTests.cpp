#if WITH_DEV_AUTOMATION_TESTS
#include "Misc/AutomationTest.h"
#include "HAL/IConsoleManager.h"
#include "UI/CourierSettings.h"
#include "Profile/PlayerProfile.h"
IMPLEMENT_SIMPLE_AUTOMATION_TEST(FCourierQualityTest,"CityCourier.Release.QualityPresets",
    EAutomationTestFlags::EditorContext|EAutomationTestFlags::EngineFilter)
bool FCourierQualityTest::RunTest(const FString& Parameters)
{
    auto Value=[](const TCHAR* Name){auto* C=IConsoleManager::Get().FindConsoleVariable(Name);return C?C->GetInt():-1;};
    FPlayerProfile Profile;
    Profile.QualityLevel=0;UCourierSettings::Apply(Profile);
    TestEqual(TEXT("Low uses lightweight lighting"),Value(TEXT("r.Lumen.DiffuseIndirect.Allow")),0);
    Profile.QualityLevel=1;UCourierSettings::Apply(Profile);
    TestEqual(TEXT("Medium includes Lumen GI"),Value(TEXT("r.Lumen.DiffuseIndirect.Allow")),1);
    TestEqual(TEXT("Medium includes Lumen reflections"),Value(TEXT("r.Lumen.Reflections.Allow")),1);
    TestEqual(TEXT("Hardware ray tracing disabled"),Value(TEXT("r.RayTracing")),0);
    Profile.QualityLevel=2;UCourierSettings::Apply(Profile);
    TestEqual(TEXT("High retains Lumen"),Value(TEXT("r.Lumen.DiffuseIndirect.Allow")),1);
    Profile.QualityLevel=1;UCourierSettings::Apply(Profile);
    return true;
}
#endif
