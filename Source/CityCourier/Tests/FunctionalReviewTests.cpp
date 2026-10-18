#if WITH_DEV_AUTOMATION_TESTS
#include "Misc/AutomationTest.h"
#include "Tests/StationTestSupport.h"
#include "Delivery/DeliveryNavigationSubsystem.h"
#include "Profile/ProfileSaveGame.h"

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FBlockedOfferReviewTest,"CityCourier.FunctionalUpgrade.Review.BlockedOffers",EAutomationTestFlags::EditorContext|EAutomationTestFlags::EngineFilter)
bool FBlockedOfferReviewTest::RunTest(const FString&)
{
    FStationFixture F;
    auto* D=F.World->GetSubsystem<UDeliverySubsystem>();
    if(!TestTrue(TEXT("Configure real controller delivery system"),D->Configure(F.City,F.Profile,F.Bike))) return false;
    if(!TestEqual(TEXT("Clear roads offer three cards"),D->GetOffers().Num(),3)) return false;
    const auto Offer=D->GetOffers()[0]; TArray<AActor*> Blockers;
    for(const auto& Shop:F.City->Merchants) Blockers.Add(F.Box(Shop.Position+FVector(0,0,100),FVector(50,50,100)));
    TestFalse(TEXT("Displayed card becomes blocked before click"),F.Controller->AcceptOffer(Offer.InstanceId));
    TestTrue(TEXT("Route-specific rejection reaches player"),F.Controller->GetActionFeedback().ToString().Contains(TEXT("路线")));
    TestFalse(TEXT("Rejected card never starts timer"),D->GetState().IsActive());
    if(D->GetState().IsActive()) D->TryCancel(EDeliveryFailure::Cancelled);
    if(!TestFalse(TEXT("A displayed candidate remains for defensive API check"),D->GetOffers().IsEmpty())) return false;
    TestFalse(TEXT("Direct delivery API also rejects blockage"),D->TryAccept(D->GetOffers()[0].InstanceId));
    if(D->GetState().IsActive()) D->TryCancel(EDeliveryFailure::Cancelled);
    TestFalse(TEXT("Blocked roads cannot refresh three offers"),D->RefreshOffers(F.City->Station.Position));
    TestEqual(TEXT("No physically blocked cards are offered"),D->GetOffers().Num(),0);
    for(auto* Blocker:Blockers) Blocker->Destroy();
    TestTrue(TEXT("Removing obstacles restores candidates"),D->RefreshOffers(F.City->Station.Position));
    if(!D->GetOffers().IsEmpty()) TestTrue(TEXT("Clear card still accepts"),D->TryAccept(D->GetOffers()[0].InstanceId));
    return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FFutureEnvelopeReviewTest,"CityCourier.FunctionalUpgrade.Review.FutureEnvelope",EAutomationTestFlags::EditorContext|EAutomationTestFlags::EngineFilter)
bool FFutureEnvelopeReviewTest::RunTest(const FString&)
{
    auto Storage=MakeShared<FTestProfileStorage>(); FPlayerProfile Newer; Newer.BalanceCents=9999;
    TArray<uint8> Future,Backup; UProfileSaveGame::Encode(Newer,Future); UProfileSaveGame::Encode(FPlayerProfile{},Backup);
    // The payload length and CRC remain valid; only the versioned CPR envelope changes.
    const uint32 FutureMagic=0x43505233; FMemory::Memcpy(Future.GetData(),&FutureMagic,sizeof(FutureMagic));
    Storage->Files.Add(FProfileRepository::PrimaryName,Future); Storage->Files.Add(FProfileRepository::BackupName,Backup);
    auto* P=NewObject<UProfileSubsystem>(NewObject<UGameInstance>()); P->SetStorageForTests(Storage); P->LoadProfile();
    TestTrue(TEXT("Newer envelope requires explicit choice"),P->NeedsReset());
    TestFalse(TEXT("No autosave or manual retry over unsupported envelope"),P->SaveProfile());
    TestTrue(TEXT("Primary remains byte-identical in place"),Storage->Files.Contains(FProfileRepository::PrimaryName) && Storage->Files[FProfileRepository::PrimaryName]==Future);
    TestTrue(TEXT("Backup remains byte-identical"),Storage->Files.Contains(FProfileRepository::BackupName) && Storage->Files[FProfileRepository::BackupName]==Backup);
    TestEqual(TEXT("Newer primary not archived as corruption"),Storage->Preserved.Num(),0);
    TestFalse(TEXT("No corrupt marker created"),Storage->Files.Contains(FProfileRepository::ResetMarkerName));
    return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FBikeArrivalReviewTest,"CityCourier.FunctionalUpgrade.Review.OffRoadBikeArrival",EAutomationTestFlags::EditorContext|EAutomationTestFlags::EngineFilter)
bool FBikeArrivalReviewTest::RunTest(const FString&)
{
    FStationFixture F;
    auto* City=DuplicateObject<UCityDefinition>(F.City,F.World);
    City->Roads.Nodes={{0,FVector(0,0,0)},{1,FVector(10000,0,0)}}; City->Roads.Edges={{0,1,100,true}};
    City->Station.Position=FVector::ZeroVector; City->Station.EntranceNode=0;
    for(auto& L:City->Merchants) { L.Position=FVector(0,100,0); L.EntranceNode=0; }
    for(auto& L:City->Destinations) { L.Position=FVector(10000,100,0); L.EntranceNode=1; }
    auto* D=NewObject<UDeliverySubsystem>(F.World); if(!TestTrue(TEXT("Off-road fixture configures"),D->Configure(City,F.Profile,F.Bike))) return false;
    auto* N=NewObject<UDeliveryNavigationSubsystem>(F.World); N->Configure(D);
    F.Bike->SetActorLocation(FVector(2000,500,52));
    N->SetNavigationMode(ECourierNavigationMode::ParkedBike);
    N->RefreshRoute(FVector(2000,800,90)); TestFalse(TEXT("Three metres away not arrived"),N->HasArrived());
    N->RefreshRoute(FVector(2000,650,90));
    TestTrue(TEXT("Beside off-road bike arrives without road detour"),N->HasArrived());
    TestTrue(TEXT("Nearby remaining distance follows clear final approach"),FMath::IsNearlyEqual(N->GetRemainingMeters(),FVector::Distance(FVector(2000,650,90),F.Bike->GetActorLocation())/100,.01));
    N->UpdateNavigation(FVector(2000,500,90),.5); TestTrue(TEXT("At bike stays arrived"),N->HasArrived());
    F.Box(FVector(2000,575,100),FVector(100,10,100));
    N->RefreshRoute(FVector(2000,650,90)); TestFalse(TEXT("Short arrival never bypasses wall"),N->HasArrived());
    return true;
}
#endif
