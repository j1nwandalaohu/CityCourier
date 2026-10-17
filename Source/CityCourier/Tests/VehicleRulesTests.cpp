#if WITH_DEV_AUTOMATION_TESTS
#include "Misc/AutomationTest.h"
#include "Vehicles/VehicleRules.h"
#include <limits>

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FVehicleRulesTest, "CityCourier.VehicleRules.RangeAndUpgrades",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FVehicleRulesTest::RunTest(const FString& Parameters)
{
    for (int32 Level = 0; Level < 3; ++Level)
    {
        const double Range = 3000.0 + Level * 1000;
        TestEqual(TEXT("Published battery range"), FVehicleRules::GetRangeMeters(Level), Range);
        TestEqual(TEXT("Full range drains battery"), FVehicleRules::ConsumeDistance(1, Range, Level), 0.f);
        TestEqual(TEXT("Actual reverse travel also consumes distance"), FVehicleRules::ConsumeDistance(1, Range / 2, Level), .5f);
        TestEqual(TEXT("Published speed upgrade"), FVehicleRules::GetMaxSpeedKmh(Level), 36.f + 6 * Level);
    }
    TestEqual(TEXT("Cannot charge by negative distance"), FVehicleRules::ConsumeDistance(.5f, -100, 0), .5f);
    TestEqual(TEXT("Invalid distance ignored"), FVehicleRules::ConsumeDistance(.5f, std::numeric_limits<double>::infinity(), 0), .5f);
    AddExpectedError(TEXT("Invalid vehicle upgrade level"), EAutomationExpectedErrorFlags::Contains, 2);
    TestEqual(TEXT("Bad power defaults safely"), FVehicleRules::GetMaxSpeedKmh(99), 36.f);
    TestEqual(TEXT("Bad battery defaults safely"), FVehicleRules::GetRangeMeters(-1), 3000.0);
    return true;
}
#endif
