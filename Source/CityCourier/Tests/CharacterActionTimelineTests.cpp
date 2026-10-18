#if WITH_DEV_AUTOMATION_TESTS
#include "Misc/AutomationTest.h"
#include "Player/CourierActionTimeline.h"

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FActionWindowTest,"CityCourier.CharacterActions.Timeline.WindowsAndFrameSkips",EAutomationTestFlags::EditorContext|EAutomationTestFlags::EngineFilter)
bool FActionWindowTest::RunTest(const FString&)
{
    FCourierActionTimeline S;
    TestTrue(TEXT("Light starts"),S.RequestAttack(false));
    const FGuid First=S.GetInstanceId();
    TestEqual(TEXT("No contact in windup"),S.Advance(.13).Num(),0);
    const auto Hit=S.Advance(.20);
    TestEqual(TEXT("Skipped active window still emitted"),Hit.Num(),1);
    if(Hit.Num()) { TestEqual(TEXT("Active begins at .14"),Hit[0].Start,.14); TestEqual(TEXT("Active ends at .22"),Hit[0].End,.22); TestTrue(TEXT("Same instance"),Hit[0].InstanceId==First); }
    TestEqual(TEXT("No lingering contact in recovery"),S.Advance(.23).Num(),0);
    TestTrue(TEXT("Light ends"),S.GetAction()==ECourierAction::None);
    S.RequestAttack(true); const auto Heavy=S.Advance(1.5);
    TestEqual(TEXT("Entire heavy over one frame produces one interval"),Heavy.Num(),1);
    if(Heavy.Num()) { TestEqual(TEXT("Heavy .34"),Heavy[0].Start,.34); TestEqual(TEXT("Heavy .46"),Heavy[0].End,.46); }
    TestTrue(TEXT("Large frame releases action"),S.GetAction()==ECourierAction::None);
    TestTrue(TEXT("Instance changes"),First!=S.GetInstanceId());
    return true;
}
IMPLEMENT_SIMPLE_AUTOMATION_TEST(FActionComboTest,"CityCourier.CharacterActions.Timeline.ComboBoundaries",EAutomationTestFlags::EditorContext|EAutomationTestFlags::EngineFilter)
bool FActionComboTest::RunTest(const FString&)
{
    FCourierActionTimeline S; S.RequestAttack(false); S.Advance(.179);
    TestFalse(TEXT("Before window rejects buffer"),S.RequestAttack(false)); S.Advance(.001);
    TestTrue(TEXT("At .18 buffers right light"),S.RequestAttack(false)); S.Advance(.259);
    TestTrue(TEXT("No early chain"),S.GetAction()==ECourierAction::LightL);
    S.Advance(.0011); TestTrue(TEXT("Chain at .44"),S.GetAction()==ECourierAction::LightR);
    S.Advance(.20); TestFalse(TEXT("Second light rejects another light"),S.RequestAttack(false));
    TestTrue(TEXT("Second light permits heavy"),S.RequestAttack(true));
    S.Advance(.25); TestTrue(TEXT("Third strike heavy"),S.GetAction()==ECourierAction::Heavy);
    TestFalse(TEXT("Heavy cannot buffer fourth hit"),S.RequestAttack(false)); S.Advance(1);
    TestTrue(TEXT("Combo releases"),S.GetAction()==ECourierAction::None);
    S.RequestAttack(false); S.Advance(.2); S.RequestAttack(false); S.RequestAttack(true); S.Advance(.25);
    TestTrue(TEXT("Last valid buffered input replaces previous"),S.GetAction()==ECourierAction::Heavy);
    S.Cancel(); S.RequestAttack(false); S.Advance(.4);
    TestFalse(TEXT(".40 boundary excludes input"),S.RequestAttack(true)); S.Advance(.20);
    TestTrue(TEXT("Outside window cannot create ghost hit"),S.GetAction()==ECourierAction::None);
    return true;
}
IMPLEMENT_SIMPLE_AUTOMATION_TEST(FActionInterruptTest,"CityCourier.CharacterActions.Timeline.Interruptions",EAutomationTestFlags::EditorContext|EAutomationTestFlags::EngineFilter)
bool FActionInterruptTest::RunTest(const FString&)
{
    FCourierActionTimeline S; S.RequestAttack(false); S.Advance(.2); S.RequestAttack(true);
    S.ReceiveHit(true); TestTrue(TEXT("Reaction overrides strike"),S.GetAction()==ECourierAction::HitFront);
    S.Advance(.2); S.ReceiveHit(false); S.Advance(.201);
    TestTrue(TEXT("Repeated hit cannot extend stun"),S.GetAction()==ECourierAction::None);
    S.RequestAttack(false); S.Advance(.2); S.RequestAttack(true); S.ClearQueuedInput(); S.Advance(.4);
    TestTrue(TEXT("Clearing queue preserves action but prevents next"),S.GetAction()==ECourierAction::None);
    S.RequestAttack(false); S.Advance(.2); S.RequestAttack(true); S.Cancel();
    TestEqual(TEXT("Cancel closes contact permanently"),S.Advance(1).Num(),0);
    S.RequestAttack(true); S.Advance(-1); S.Advance(std::numeric_limits<double>::quiet_NaN());
    TestEqual(TEXT("Invalid time does not advance"),S.GetElapsed(),0.);
    return true;
}
#endif
