#if WITH_DEV_AUTOMATION_TESTS
#include "Misc/AutomationTest.h"
#include "Tests/WorldTestSupport.h"
#include "Tests/ProfileTestStorage.h"
#include "Delivery/DeliverySubsystem.h"
#include "Profile/ProfileSubsystem.h"
#include "Player/CourierCharacter.h"
#include "Player/CourierController.h"
#include "World/DeliveryLocation.h"
#include "Vehicles/ScooterPawn.h"
#include "Engine/GameInstance.h"

namespace
{
struct FDeliveryFixture : FTestCourierWorld
{
    UCityDefinition* City;
    UProfileSubsystem* Profile;
    UDeliverySubsystem* Delivery;
    ACourierCharacter* Rider;
    ACourierController* Controller;
    AScooterPawn* Scooter;
    TSharedRef<FTestProfileStorage> Storage = MakeShared<FTestProfileStorage>();
    double Now = 100;
    FDeliveryFixture()
    {
        Box(FVector(0,0,-50),FVector(40000,40000,50));
        City = LoadObject<UCityDefinition>(nullptr,TEXT("/Game/CityCourier/Data/DA_CityDefinition.DA_CityDefinition"));
        check(City);
        Profile = NewObject<UProfileSubsystem>(NewObject<UGameInstance>());
        Profile->SetStorageForTests(Storage); Profile->LoadProfile();
        Rider = World->SpawnActor<ACourierCharacter>(FVector(0,0,90),FRotator::ZeroRotator);
        Controller = World->SpawnActor<ACourierController>(); Controller->Possess(Rider);
        Scooter = World->SpawnActor<AScooterPawn>(FVector(120,0,52),FRotator::ZeroRotator);
        for (const auto& L : City->Merchants) SpawnLocation(L);
        for (const auto& L : City->Destinations) SpawnLocation(L);
        Delivery = NewObject<UDeliverySubsystem>(World);
        Delivery->SetClockForTests([this]() { return Now; });
        Delivery->Configure(City,Profile,Scooter);
    }
    void SpawnLocation(const FCourierLocationDefinition& L)
    {
        auto* A = World->SpawnActor<ADeliveryLocation>(L.Position,FRotator::ZeroRotator);
        A->LocationId = L.LocationId; A->LocationKind = L.Kind; A->CityDefinition = City;
    }
    void At(FName Id, double Offset = 90)
    { Rider->SetActorLocation(City->FindLocation(Id)->Position + FVector(0,0,Offset)); }
    FDeliveryOffer Accept()
    {
        Delivery->RefreshOffers(Rider->GetActorLocation());
        const auto O = Delivery->GetOffers()[0]; Delivery->TryAccept(O.InstanceId); return O;
    }
};
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FDeliveryWorldFlowTest,"CityCourier.DeliveryWorld.CompleteLoop",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FDeliveryWorldFlowTest::RunTest(const FString& Parameters)
{
    FDeliveryFixture F;
    TestTrue(TEXT("Offers generated"),F.Delivery->RefreshOffers(F.Rider->GetActorLocation()));
    TestEqual(TEXT("Three offers"),F.Delivery->GetOffers().Num(),3);
    TSet<FString> Pairs; TSet<FGuid> IDs;
    for (const auto& O : F.Delivery->GetOffers())
    {
        Pairs.Add(O.MerchantId.ToString()+O.DestinationId.ToString()); IDs.Add(O.InstanceId);
        TestEqual(TEXT("Configured fee"),O.BaseFeeCents,F.City->GetFee(O.MerchantId,O.DestinationId));
        TestTrue(TEXT("Legal time"),O.DeadlineSeconds>=120 && O.DeadlineSeconds<=300);
    }
    TestEqual(TEXT("Distinct pairs"),Pairs.Num(),3); TestEqual(TEXT("Fresh IDs"),IDs.Num(),3);
    const auto O = F.Delivery->GetOffers()[0]; const auto Other = F.Delivery->GetOffers()[1];
    int32 Finished = 0; F.Delivery->OnDeliveryFinished.AddLambda([&](const FSettlement&) { ++Finished; });
    TestTrue(TEXT("Accept first"),F.Delivery->TryAccept(O.InstanceId));
    TestFalse(TEXT("One active only"),F.Delivery->TryAccept(Other.InstanceId));
    TestFalse(TEXT("No refresh while active"),F.Delivery->RefreshOffers(FVector::ZeroVector));
    TestEqual(TEXT("Pickup target"),F.Delivery->GetTargetId(),O.MerchantId);
    F.At(O.MerchantId,201); TestFalse(TEXT("Real distance enforced"),F.Delivery->TryPickup(O.MerchantId,F.Rider));
    F.At(O.MerchantId); TestFalse(TEXT("Wrong shop rejected"),F.Delivery->TryPickup(TEXT("Wrong"),F.Rider));
    F.Rider->SetMounted(true); TestFalse(TEXT("Must walk"),F.Delivery->TryPickup(O.MerchantId,F.Rider)); F.Rider->SetMounted(false);
    TestTrue(TEXT("Pickup"),F.Delivery->TryPickup(O.MerchantId,F.Rider));
    TestEqual(TEXT("Recipient target"),F.Delivery->GetTargetId(),O.DestinationId);
    TestFalse(TEXT("Cannot deliver remotely"),F.Delivery->TryDeliver(O.DestinationId,F.Rider));
    F.At(O.DestinationId);
    F.Storage->FailWrite = FProfileRepository::PrimaryName;
    TestTrue(TEXT("Deliver with failed disk save"),F.Delivery->TryDeliver(O.DestinationId,F.Rider));
    TestEqual(TEXT("Income once"),F.Profile->GetProfile().BalanceCents,O.BaseFeeCents);
    TestTrue(TEXT("Unsaved state visible"),F.Profile->HasUnsavedChanges());
    TestFalse(TEXT("Cannot deliver twice"),F.Delivery->TryDeliver(O.DestinationId,F.Rider));
    F.Delivery->Tick(1); TestEqual(TEXT("Single finish broadcast"),Finished,1);
    TestEqual(TEXT("New candidates"),F.Delivery->GetOffers().Num(),3);
    for (const auto& Fresh : F.Delivery->GetOffers()) TestFalse(TEXT("No reused IDs"),IDs.Contains(Fresh.InstanceId));
    F.Storage->FailWrite.Empty(); F.Profile->SaveProfile();
    TestEqual(TEXT("Retry save does not pay again"),F.Profile->GetProfile().CompletedCount,1);
    return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FDeliveryWorldClockTest,"CityCourier.DeliveryWorld.ExactClockBoundaries",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FDeliveryWorldClockTest::RunTest(const FString& Parameters)
{
    for (bool TickFirst : {false,true}) for (bool FinalDeadline : {false,true})
    {
        FDeliveryFixture F; const auto O = F.Accept(); F.At(O.MerchantId);
        F.Delivery->TryPickup(O.MerchantId,F.Rider); F.At(O.DestinationId);
        F.Now += O.DeadlineSeconds + (FinalDeadline ? 60 : 0);
        if (TickFirst) F.Delivery->Tick(999);
        TestEqual(TEXT("Exact boundary independent of event order"),F.Delivery->TryDeliver(O.DestinationId,F.Rider),!FinalDeadline);
        F.Delivery->Tick(999);
        TestEqual(TEXT("Tick does not add delta twice"),F.Delivery->GetState().ElapsedSeconds,O.DeadlineSeconds+(FinalDeadline?60:0));
        TestEqual(TEXT("Grace payout vs expired"),F.Profile->GetProfile().BalanceCents,FinalDeadline ? int64(0) : O.BaseFeeCents*60/100);
        TestEqual(TEXT("Expired counted once"),F.Profile->GetProfile().FailedCount,FinalDeadline ? 1 : 0);
    }
    return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FDeliveryWorldDamageTest,"CityCourier.DeliveryWorld.MountedDamageAndCancellation",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FDeliveryWorldDamageTest::RunTest(const FString& Parameters)
{
    FDeliveryFixture F; const auto O = F.Accept();
    TestTrue(TEXT("Mount before pickup"),F.Scooter->TryMount(F.Rider));
    F.Scooter->OnEffectiveImpact.Broadcast(28);
    TestEqual(TEXT("Empty box not damaged"),F.Delivery->GetState().IntegrityPercent,100);
    F.Scooter->TryDismount(); F.At(O.MerchantId); F.Delivery->TryPickup(O.MerchantId,F.Rider);
    F.Scooter->OnEffectiveImpact.Broadcast(28);
    TestEqual(TEXT("Parked scooter not food damage"),F.Delivery->GetState().IntegrityPercent,100);
    F.Rider->SetActorLocation(F.Scooter->GetActorLocation()+FVector(0,-120,40)); F.Scooter->TryMount(F.Rider);
    F.Scooter->OnEffectiveImpact.Broadcast(28);
    TestEqual(TEXT("Mounted food impact"),F.Delivery->GetState().IntegrityPercent,70);
    F.Scooter->OnEffectiveImpact.Broadcast(28); TestEqual(TEXT("Cooldown"),F.Delivery->GetState().IntegrityPercent,70);
    for (int32 I=0; I<3; ++I) { F.Now += 1; F.Scooter->OnEffectiveImpact.Broadcast(28); }
    TestTrue(TEXT("Food destroyed"),F.Delivery->GetState().Status==EDeliveryStatus::Failed);
    TestEqual(TEXT("Failed once"),F.Profile->GetProfile().FailedCount,1);
    TestFalse(TEXT("Cancel cannot repeat failure"),F.Delivery->TryCancel(EDeliveryFailure::Cancelled));
    F.Accept(); TestTrue(TEXT("User cancel"),F.Delivery->TryCancel(EDeliveryFailure::Cancelled));
    F.Accept(); TestTrue(TEXT("Rescue cancel"),F.Delivery->TryCancel(EDeliveryFailure::Rescue));
    TestEqual(TEXT("Only three failures"),F.Profile->GetProfile().FailedCount,3);
    TestEqual(TEXT("Failures charge no cash"),F.Profile->GetProfile().BalanceCents,int64(0));
    return true;
}
#endif
