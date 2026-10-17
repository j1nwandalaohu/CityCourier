#if WITH_DEV_AUTOMATION_TESTS
#include "Misc/AutomationTest.h"
#include "Core/DeliveryRules.h"
#include "Tests/TestSupport.h"
#include <limits>

using namespace CourierTest;
constexpr auto RuleFlags = EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter;

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FDeliveryBoundariesTest, "CityCourier.DeliveryRules.Boundaries", RuleFlags)
bool FDeliveryBoundariesTest::RunTest(const FString& Parameters)
{
    // Catches off-by-one deadline comparisons and premature monetary rounding.
    TestEqual(TEXT("Minimum deadline"), FDeliveryRules::CalculateDeadline(0), 120.0);
    TestEqual(TEXT("Road distance determines deadline"), FDeliveryRules::CalculateDeadline(600), 210.0);
    TestEqual(TEXT("Fractional route rounds up"), FDeliveryRules::CalculateDeadline(151), 121.0);
    TestEqual(TEXT("Maximum deadline"), FDeliveryRules::CalculateDeadline(2000), 300.0);
    struct FCase { double Time; bool CanDeliver; int64 Payout; };
    for (const FCase& Case : {FCase{119.999, true, 1600}, {120.0, true, 960},
                             {179.999, true, 960}, {180.0, false, 0}, {181.0, false, 0}})
    {
        FDeliveryState State;
        const auto Offer = MakeOffer();
        TestTrue(TEXT("Accept valid offer"), FDeliveryRules::Accept(State, Offer));
        TestTrue(TEXT("Pickup at merchant"), FDeliveryRules::Pickup(State, Offer.MerchantId));
        State.IntegrityPercent = 80;
        FDeliveryRules::Advance(State, Case.Time);
        int64 Payout = -1;
        TestEqual(FString::Printf(TEXT("Delivery at %.3f seconds"), Case.Time),
            FDeliveryRules::Deliver(State, Offer.DestinationId, Payout), Case.CanDeliver);
        TestEqual(TEXT("Expected payout in cents"), Payout, Case.Payout);
        if (!Case.CanDeliver)
        {
            TestTrue(TEXT("Final boundary expired"), State.Status == EDeliveryStatus::Failed && State.Failure == EDeliveryFailure::Expired);
        }
        Payout = 777;
        TestFalse(TEXT("Terminal delivery rejected"), FDeliveryRules::Deliver(State, Offer.DestinationId, Payout));
        TestEqual(TEXT("Rejected delivery clears output"), Payout, int64(0));
    }
    FDeliveryState State;
    const auto Offer = MakeOffer(120, 1601);
    FDeliveryRules::Accept(State, Offer);
    FDeliveryRules::Pickup(State, Offer.MerchantId);
    State.IntegrityPercent = 99;
    FDeliveryRules::Advance(State, 120);
    int64 Payout = 0;
    TestTrue(TEXT("Late delivery accepted"), FDeliveryRules::Deliver(State, Offer.DestinationId, Payout));
    TestEqual(TEXT("Round down exactly once"), Payout, int64(950));
    return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FDeliveryTransitionsTest, "CityCourier.DeliveryRules.Transitions", RuleFlags)
bool FDeliveryTransitionsTest::RunTest(const FString& Parameters)
{
    // Catches interactions bypassing pickup, target identity or terminal guards.
    FDeliveryState State;
    auto Offer = MakeOffer();
    int64 Payout = 99;
    TestFalse(TEXT("Idle cannot pickup"), FDeliveryRules::Pickup(State, Offer.MerchantId));
    TestFalse(TEXT("Idle cannot cancel"), FDeliveryRules::Cancel(State, EDeliveryFailure::Cancelled));
    TestTrue(TEXT("Accept"), FDeliveryRules::Accept(State, Offer));
    TestFalse(TEXT("Only one active order"), FDeliveryRules::Accept(State, MakeOffer()));
    TestFalse(TEXT("Cannot deliver before pickup"), FDeliveryRules::Deliver(State, Offer.DestinationId, Payout));
    TestFalse(TEXT("Wrong merchant rejected"), FDeliveryRules::Pickup(State, TEXT("wrong")));
    FDeliveryRules::ApplyImpact(State, 28);
    TestEqual(TEXT("No food damage before pickup"), State.IntegrityPercent, 100);
    FDeliveryRules::Advance(State, 120);
    TestTrue(TEXT("Can pickup in grace period"), FDeliveryRules::Pickup(State, Offer.MerchantId));
    TestFalse(TEXT("Cannot pickup twice"), FDeliveryRules::Pickup(State, Offer.MerchantId));
    TestFalse(TEXT("Wrong destination rejected"), FDeliveryRules::Deliver(State, TEXT("wrong"), Payout));
    TestTrue(TEXT("Valid delivery"), FDeliveryRules::Deliver(State, Offer.DestinationId, Payout));
    TestEqual(TEXT("Late full integrity payout"), Payout, int64(1200));
    FDeliveryRules::ApplyImpact(State, 28);
    FDeliveryRules::Advance(State, 999);
    TestTrue(TEXT("Completed remains terminal"), State.Status == EDeliveryStatus::Completed);
    TestEqual(TEXT("Terminal integrity frozen"), State.IntegrityPercent, 100);
    TestFalse(TEXT("Cannot cancel completed"), FDeliveryRules::Cancel(State, EDeliveryFailure::Cancelled));
    TestFalse(TEXT("Cannot replay same instance"), FDeliveryRules::Accept(State, Offer));
    Offer = MakeOffer();
    TestTrue(TEXT("Fresh order after completion"), FDeliveryRules::Accept(State, Offer));
    TestEqual(TEXT("New order clock reset"), State.ElapsedSeconds, 0.0);
    TestFalse(TEXT("No failure reason rejected"), FDeliveryRules::Cancel(State, EDeliveryFailure::None));
    TestTrue(TEXT("Rescue cancels active order"), FDeliveryRules::Cancel(State, EDeliveryFailure::Rescue));
    TestTrue(TEXT("Rescue cause retained"), State.Failure == EDeliveryFailure::Rescue);
    TestFalse(TEXT("Cancel once only"), FDeliveryRules::Cancel(State, EDeliveryFailure::Cancelled));
    TestTrue(TEXT("New offer after failure"), FDeliveryRules::Accept(State, MakeOffer()));
    TestTrue(TEXT("Manual cancellation"), FDeliveryRules::Cancel(State, EDeliveryFailure::Cancelled));
    FDeliveryState Uncollected;
    const auto UncollectedOffer = MakeOffer();
    FDeliveryRules::Accept(Uncollected, UncollectedOffer);
    FDeliveryRules::Advance(Uncollected, 180);
    TestFalse(TEXT("Expired before pickup"), FDeliveryRules::Pickup(Uncollected, UncollectedOffer.MerchantId));
    return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FDeliveryImpactTest, "CityCourier.DeliveryRules.Impact", RuleFlags)
bool FDeliveryImpactTest::RunTest(const FString& Parameters)
{
    // Catches low-speed damage, missing cooldown, and repeated terminal changes.
    FDeliveryState State;
    const auto Offer = MakeOffer();
    FDeliveryRules::Accept(State, Offer);
    FDeliveryRules::Pickup(State, Offer.MerchantId);
    FDeliveryRules::ApplyImpact(State, 7.99);
    TestEqual(TEXT("Below threshold undamaged"), State.IntegrityPercent, 100);
    FDeliveryRules::ApplyImpact(State, 8);
    TestEqual(TEXT("First threshold impact immediately applies"), State.IntegrityPercent, 99);
    FDeliveryRules::ApplyImpact(State, 28);
    TestEqual(TEXT("Same-frame impact ignored"), State.IntegrityPercent, 99);
    FDeliveryRules::Advance(State, 0.999);
    FDeliveryRules::ApplyImpact(State, 28);
    TestEqual(TEXT("Cooldown not yet elapsed"), State.IntegrityPercent, 99);
    FDeliveryRules::Advance(State, 0.001);
    FDeliveryRules::ApplyImpact(State, 28);
    TestEqual(TEXT("Exact one-second cooldown"), State.IntegrityPercent, 69);
    FDeliveryRules::Advance(State, 1);
    FDeliveryRules::ApplyImpact(State, 9);
    TestEqual(TEXT("Round damage half-up"), State.IntegrityPercent, 67);
    for (int32 Index = 0; Index < 3; ++Index)
    {
        FDeliveryRules::Advance(State, 1);
        FDeliveryRules::ApplyImpact(State, 1000000);
    }
    TestEqual(TEXT("Integrity bounded at zero"), State.IntegrityPercent, 0);
    TestTrue(TEXT("Food destroyed"), State.Status == EDeliveryStatus::Failed && State.Failure == EDeliveryFailure::Damaged);
    int64 Payout = 999;
    TestFalse(TEXT("No delivery after destruction"), FDeliveryRules::Deliver(State, Offer.DestinationId, Payout));
    TestEqual(TEXT("No destruction payout"), Payout, int64(0));
    FDeliveryRules::Advance(State, 9999);
    TestTrue(TEXT("Failure cause unchanged"), State.Failure == EDeliveryFailure::Damaged);
    return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FDeliveryInvalidInputTest, "CityCourier.DeliveryRules.InvalidInput", RuleFlags)
bool FDeliveryInvalidInputTest::RunTest(const FString& Parameters)
{
    // Catches invalid offers corrupting state and nonfinite time bypassing expiry.
    const double NaN = std::numeric_limits<double>::quiet_NaN();
    const double Infinity = std::numeric_limits<double>::infinity();
    for (double Bad : {-1.0, NaN, Infinity})
        TestEqual(TEXT("Reject invalid route"), FDeliveryRules::CalculateDeadline(Bad), -1.0);
    for (int64 Fee : {int64(-1), int64(0), int64(1599), int64(3201), std::numeric_limits<int64>::max()})
    {
        FDeliveryState State;
        TestFalse(TEXT("Invalid fee rejected"), FDeliveryRules::Accept(State, MakeOffer(120, Fee)));
        TestTrue(TEXT("Rejected offer preserves idle"), State.Status == EDeliveryStatus::Idle);
    }
    for (double Deadline : {119.9, 300.1, NaN, Infinity})
    {
        FDeliveryState State;
        TestFalse(TEXT("Invalid deadline rejected"), FDeliveryRules::Accept(State, MakeOffer(Deadline)));
    }
    for (int32 Missing = 0; Missing < 3; ++Missing)
    {
        auto Offer = MakeOffer();
        if (Missing == 0) Offer.InstanceId.Invalidate();
        if (Missing == 1) Offer.MerchantId = NAME_None;
        if (Missing == 2) Offer.DestinationId = NAME_None;
        FDeliveryState State;
        TestFalse(TEXT("Missing identity rejected"), FDeliveryRules::Accept(State, Offer));
    }
    FDeliveryState State;
    const auto Offer = MakeOffer();
    FDeliveryRules::Accept(State, Offer);
    FDeliveryRules::Pickup(State, Offer.MerchantId);
    FDeliveryRules::Advance(State, 2.0);
    for (double Bad : {-1.0, NaN, Infinity})
    {
        FDeliveryRules::Advance(State, Bad);
        FDeliveryRules::ApplyImpact(State, Bad);
        TestEqual(TEXT("Invalid delta does not change clock"), State.ElapsedSeconds, 2.0);
        TestEqual(TEXT("Invalid speed does not damage"), State.IntegrityPercent, 100);
    }
    FDeliveryRules::Advance(State, std::numeric_limits<double>::max());
    TestTrue(TEXT("Huge finite duration expires safely"), State.Status == EDeliveryStatus::Failed && FMath::IsFinite(State.ElapsedSeconds));
    return true;
}
#endif
