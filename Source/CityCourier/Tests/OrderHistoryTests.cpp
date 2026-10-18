#if WITH_DEV_AUTOMATION_TESTS
#include "Misc/AutomationTest.h"
#include "Tests/StationTestSupport.h"
#include "Tests/HistoryTestSupport.h"
#include "Profile/ProfileSaveGame.h"
#include "Profile/ProfileRules.h"
#include <limits>

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FHistoryTransactionsTest,"CityCourier.FunctionalUpgrade.History.BoundedAtomicSnapshot",EAutomationTestFlags::EditorContext|EAutomationTestFlags::EngineFilter)
bool FHistoryTransactionsTest::RunTest(const FString&)
{
    FStationFixture F; auto First=MakeTestSettlement(FGuid::NewGuid());
    F.Storage->FailWrite=FProfileRepository::PrimaryName;
    TestTrue(TEXT("Settlement accepted despite disk failure"),F.Profile->ApplySettlement(First));
    TestEqual(TEXT("History present in same memory update"),F.Profile->GetProfile().OrderHistory.Num(),1);
    TestTrue(TEXT("Save failure exposed"),F.Profile->HasUnsavedChanges());
    TestFalse(TEXT("Duplicate callback refused"),F.Profile->ApplySettlement(First));
    F.Storage->FailWrite.Empty(); TestTrue(TEXT("Retry saves snapshot only"),F.Profile->SaveProfile());
    FSettlement Last;
    for(int32 I=0;I<20;++I)
    {
        Last=MakeTestSettlement(FGuid::NewGuid()); Last.History.MerchantName=FString::ChrN(64,TEXT('店')); Last.History.DestinationName=FString::ChrN(64,TEXT('客'));
        // Deliberately backwards clocks: list must follow settlement insertion, not timestamp sorting.
        Last.History.SettledAtUtc=FDateTime(2026,10,18)-FTimespan::FromMinutes(I);
        TestTrue(TEXT("Next settlement accepted"),F.Profile->ApplySettlement(Last));
    }
    const auto& P=F.Profile->GetProfile(); TestEqual(TEXT("Bounded to20"),P.OrderHistory.Num(),20);
    TestEqual(TEXT("All21 incomes retained"),P.TotalEarnedCents,int64(42000)); TestEqual(TEXT("Count not capped"),P.CompletedCount,21);
    TestFalse(TEXT("Old callback still rejected after history eviction"),F.Profile->ApplySettlement(First));
    if(!P.OrderHistory.IsEmpty()) { TestEqual(TEXT("Newest first"),P.OrderHistory[0].InstanceId,Last.InstanceId); TestFalse(TEXT("Oldest evicted"),P.OrderHistory.ContainsByPredicate([&](const auto& H){return H.InstanceId==First.InstanceId;})); }
    TArray<uint8> Bytes; TestTrue(TEXT("20 longest names encode under cap"),UProfileSaveGame::Encode(P,Bytes)); TestTrue(TEXT("64KiB payload limit"),Bytes.Num()<=65548);
    auto* Reload=NewObject<UProfileSubsystem>(NewObject<UGameInstance>()); Reload->SetStorageForTests(F.Storage); Reload->LoadProfile();
    TestTrue(TEXT("Full snapshot reload equal"),Reload->GetProfile()==P);
    TestFalse(TEXT("Recent persisted instance cannot settle again"),Reload->ApplySettlement(Last));
    return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FHistoryRulesTest,"CityCourier.FunctionalUpgrade.History.OutcomesAndValidation",EAutomationTestFlags::EditorContext|EAutomationTestFlags::EngineFilter)
bool FHistoryRulesTest::RunTest(const FString&)
{
    FDeliveryState S; S.Offer={FGuid::NewGuid(),TEXT("Shop"),TEXT("Customer"),1999,120};
    S.Status=EDeliveryStatus::Completed; S.ElapsedSeconds=120; S.IntegrityPercent=73;
    const auto H=FOrderHistoryRules::MakeEntry(S,875,TEXT("店"),TEXT("客"),FDateTime(2026,10,18));
    TestTrue(TEXT("Late complete valid"),FOrderHistoryRules::Validate(H));
    TestTrue(TEXT("Late flag and exact875 cents"),H.bLate && H.bCompleted && H.PayoutCents==875);
    const FString Explanation=FOrderHistoryRules::DescribeIncome(H).ToString();
    TestTrue(TEXT("Exact income with no double rounding"),Explanation.Contains(TEXT("8.75")) && Explanation.Contains(TEXT("5.40")) && Explanation.Contains(TEXT("5.84")));
    for(auto Cause:{EDeliveryFailure::Expired,EDeliveryFailure::Damaged,EDeliveryFailure::Cancelled,EDeliveryFailure::Rescue})
    {
        S.Status=EDeliveryStatus::Failed; S.Failure=Cause; S.IntegrityPercent=Cause==EDeliveryFailure::Damaged?0:73; S.ElapsedSeconds=Cause==EDeliveryFailure::Expired?180:30;
        const auto Failed=FOrderHistoryRules::MakeEntry(S,0,TEXT("店"),TEXT("客"),FDateTime(2026,10,18));
        TestTrue(TEXT("Failure cause retained"),FOrderHistoryRules::Validate(Failed) && Failed.FailureCode==static_cast<uint8>(Cause) && !Failed.bCompleted && Failed.PayoutCents==0);
        TestFalse(TEXT("Specific failure outcome"),FOrderHistoryRules::DescribeOutcome(Failed).IsEmpty());
    }
    auto Bad=H; Bad.FailureCode=255; TestFalse(TEXT("Invalid enum"),FOrderHistoryRules::Validate(Bad));
    Bad=H; Bad.ElapsedSeconds=std::numeric_limits<double>::quiet_NaN(); TestFalse(TEXT("NaN"),FOrderHistoryRules::Validate(Bad));
    Bad=H; Bad.FailureCode=static_cast<uint8>(EDeliveryFailure::Cancelled); TestFalse(TEXT("Contradictory completion"),FOrderHistoryRules::Validate(Bad));
    S.Status=EDeliveryStatus::Completed; S.Failure=EDeliveryFailure::None; S.ElapsedSeconds=30; S.IntegrityPercent=100;
    FString Name=FString::ChrN(63,TEXT('A')); Name.AppendChar(0xD83D); Name.AppendChar(0xDE80); Name+=TEXT("tail");
    const auto Truncated=FOrderHistoryRules::MakeEntry(S,1999,Name,Name,FDateTime(2026,10,18));
    TestEqual(TEXT("Never split surrogate pair at limit"),Truncated.MerchantName.Len(),63);
    FPlayerProfile P; P.OrderHistory={H,H}; TestFalse(TEXT("Duplicate IDs invalid"),FProfileRules::Validate(P));
    return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FHistoryTerminalTest,"CityCourier.FunctionalUpgrade.History.TerminalTransitions",EAutomationTestFlags::EditorContext|EAutomationTestFlags::EngineFilter)
bool FHistoryTerminalTest::RunTest(const FString&)
{
    for(int32 Case=0;Case<6;++Case)
    {
        FStationFixture F; const auto O=F.Delivery->GetOffers()[0]; F.Delivery->TryAccept(O.InstanceId);
        for(FName Id:{O.MerchantId,O.DestinationId}) { const auto* Def=F.City->FindLocation(Id); auto* L=F.World->SpawnActor<ADeliveryLocation>(Def->Position,FRotator::ZeroRotator); L->LocationId=Id; }
        if(Case<3)
        {
            F.Rider->SetActorLocation(F.City->FindLocation(O.MerchantId)->Position+FVector(0,0,90)); TestTrue(TEXT("Pick up"),F.Delivery->TryPickup(O.MerchantId,F.Rider));
            if(Case==2)
            {
                F.Bike->SetActorLocation(F.Rider->GetActorLocation()+FVector(0,150,-38)); TestTrue(TEXT("Mount for damage"),F.Bike->TryMount(F.Rider));
                for(int32 I=0;I<4;++I) { F.Now+=1; F.Bike->OnEffectiveImpact.Broadcast(28); }
            }
            else { if(Case==1) F.Now+=O.DeadlineSeconds; F.Rider->SetActorLocation(F.City->FindLocation(O.DestinationId)->Position+FVector(0,0,90)); TestTrue(TEXT("Deliver"),F.Delivery->TryDeliver(O.DestinationId,F.Rider)); }
        }
        else if(Case==3) { F.Now+=O.DeadlineSeconds+60; F.Delivery->Tick(1); }
        else F.Delivery->TryCancel(Case==4?EDeliveryFailure::Cancelled:EDeliveryFailure::Rescue);
        F.Delivery->Tick(1); F.Delivery->TryCancel(EDeliveryFailure::Cancelled);
        const auto& Rows=F.Profile->GetProfile().OrderHistory; TestEqual(TEXT("Terminal exactly once"),Rows.Num(),1);
        if(!Rows.IsEmpty()) { TestEqual(TEXT("Same instance"),Rows[0].InstanceId,O.InstanceId); TestEqual(TEXT("Matches authoritative payout"),Rows[0].PayoutCents,F.Delivery->GetLastSettlement().PayoutCents); TestTrue(TEXT("Metadata validates"),FOrderHistoryRules::Validate(Rows[0])); }
    }
    return true;
}
#endif
