#if WITH_DEV_AUTOMATION_TESTS
#include "Misc/AutomationTest.h"
#include "Tests/StationTestSupport.h"
#include "Ambient/AmbientTrafficManager.h"
#include "Ambient/AmbientVehicle.h"
#include "Ambient/AmbientPedestrian.h"
#include "Ambient/CitizenAnimInstance.h"
#include "Ambient/CourierAudioComponent.h"
#include "EngineUtils.h"
#include "Sound/SoundWave.h"
#include "Components/SkeletalMeshComponent.h"
#include "Animation/Skeleton.h"
#include "Animation/AnimSequence.h"
#include "Animation/AnimSingleNodeInstance.h"

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FAmbientPoolTest,"CityCourier.Ambient.BoundedPopulation",
    EAutomationTestFlags::EditorContext|EAutomationTestFlags::EngineFilter)
bool FAmbientPoolTest::RunTest(const FString& Parameters)
{
    FStationFixture F;
    auto* Manager=F.World->SpawnActor<AAmbientTrafficManager>(); Manager->Configure(F.City);
    const auto Before=F.Profile->GetProfile();
    for(int32 Second=0;Second<300;++Second)
    {
        const FVector Position(12000*FMath::Sin(Second*.03),12000*FMath::Cos(Second*.03),100);
        Manager->UpdatePopulation(Position);
        TestTrue(TEXT("At most six visible cars"),Manager->GetActiveVehicleCount()<=6);
        TestTrue(TEXT("At most twelve visible walkers"),Manager->GetActivePedestrianCount()<=12);
    }
    int32 Cars=0,Peds=0;
    for(TActorIterator<AAmbientVehicle> It(F.World);It;++It)
        if(It->IsA<AAmbientPedestrian>()) ++Peds; else ++Cars;
    TestEqual(TEXT("Five minutes reuses exactly six car actors"),Cars,6);
    TestEqual(TEXT("Five minutes reuses exactly twelve walker actors"),Peds,12);
    Manager->UpdatePopulation(FVector(0,-12000,100));
    TestTrue(TEXT("Nearby cars actually active"),Manager->GetActiveVehicleCount()>0);
    TestTrue(TEXT("Nearby walkers actually active"),Manager->GetActivePedestrianCount()>0);
    Manager->UpdatePopulation(FVector(1000000,1000000,0));
    TestEqual(TEXT("Distant cars disabled"),Manager->GetActiveVehicleCount(),0);
    TestEqual(TEXT("Distant pedestrians disabled"),Manager->GetActivePedestrianCount(),0);
    TestEqual(TEXT("Economy unchanged"),F.Profile->GetProfile().BalanceCents,Before.BalanceCents);
    TestEqual(TEXT("Order unchanged"),F.Delivery->GetState().Status,EDeliveryStatus::Idle);
    return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FAmbientObstacleTest,"CityCourier.Ambient.StopAndResume",
    EAutomationTestFlags::EditorContext|EAutomationTestFlags::EngineFilter)
bool FAmbientObstacleTest::RunTest(const FString& Parameters)
{
    FTestCourierWorld F; F.Box(FVector(0,0,-50),FVector(10000,10000,50));
    auto* Car=F.World->SpawnActor<AAmbientVehicle>();
    Car->SetPatrol({FVector(0,0,70),FVector(6000,0,70)}); Car->SetPopulationActive(true);
    auto* Blocker=F.Box(FVector(2000,0,90),FVector(40,80,90));
    for(int32 I=0;I<900;++I) Car->Tick(1.f/60);
    TestTrue(TEXT("Stops before player-sized blocker"),Car->GetActorLocation().X<1750);
    TestTrue(TEXT("Brakes to rest"),Car->GetSpeedCm()<1);
    Blocker->Destroy();
    for(int32 I=0;I<180;++I) Car->Tick(1.f/60);
    TestTrue(TEXT("Resumes when blocker leaves"),Car->GetActorLocation().X>2100);
    auto* Ped=F.World->SpawnActor<AAmbientPedestrian>();
    Ped->SetPatrol({FVector(0,1000,90),FVector(4000,1000,90)}); Ped->SetPopulationActive(true);
    F.Box(FVector(700,1000,150),FVector(40,300,150));
    for(int32 I=0;I<600;++I) Ped->Tick(1.f/60);
    TestTrue(TEXT("Walker cannot pass through wall"),Ped->GetActorLocation().X<630);
    auto* Lead=F.World->SpawnActor<AAmbientVehicle>();
    Lead->SetPatrol({FVector(1500,-1000,70),FVector(6000,-1000,70)});Lead->SetPopulationActive(true);
    Car->SetPatrol({FVector(0,-1000,70),FVector(6000,-1000,70)});
    for(int32 I=0;I<600;++I)Car->Tick(1.f/60);
    AddInfo(FString::Printf(TEXT("Following stop x=%.3f speed=%.3f lead=%s"),Car->GetActorLocation().X,Car->GetSpeedCm(),*Lead->GetActorLocation().ToString()));
    TestTrue(TEXT("Stops behind another vehicle with at least 50cm gap"),Lead->GetActorLocation().X-Car->GetActorLocation().X>460 && Car->GetSpeedCm()<1);
    return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FAmbientAudioTest,"CityCourier.Ambient.DeliveryCueTransitions",
    EAutomationTestFlags::EditorContext|EAutomationTestFlags::EngineFilter)
bool FAmbientAudioTest::RunTest(const FString& Parameters)
{
    FStationFixture F;
    auto* Audio=NewObject<UCourierAudioComponent>(F.Controller); Audio->RegisterComponent(); Audio->BindDelivery(F.Delivery);
    const auto Offer=F.Delivery->GetOffers()[0];
    TestTrue(TEXT("Accept fixture order"),F.Delivery->TryAccept(Offer.InstanceId));
    F.Delivery->TryAccept(Offer.InstanceId);
    TestEqual(TEXT("One acceptance cue"),Audio->GetCueCount(ECourierAudioCue::Accepted),1);
    F.Delivery->TryCancel(EDeliveryFailure::Cancelled); F.Delivery->TryCancel(EDeliveryFailure::Cancelled);
    TestEqual(TEXT("One failure cue"),Audio->GetCueCount(ECourierAudioCue::Failed),1);
    const auto Next=F.Delivery->GetOffers()[0];F.Delivery->TryAccept(Next.InstanceId);
    for(FName Id:{Next.MerchantId,Next.DestinationId})
    {
        const auto* Definition=F.City->FindLocation(Id);
        auto* Location=F.World->SpawnActor<ADeliveryLocation>(Definition->Position,FRotator::ZeroRotator);
        Location->LocationId=Id;Location->LocationKind=Definition->Kind;
        F.Rider->SetActorLocation(Definition->Position+FVector(0,-100,90));
        if(Id==Next.MerchantId)
        {TestTrue(TEXT("Real pickup"),F.Delivery->TryPickup(Id,F.Rider));F.Delivery->TryPickup(Id,F.Rider);}
        else {TestTrue(TEXT("Real delivery"),F.Delivery->TryDeliver(Id,F.Rider));F.Delivery->TryDeliver(Id,F.Rider);}
    }
    TestEqual(TEXT("One pickup cue"),Audio->GetCueCount(ECourierAudioCue::PickedUp),1);
    TestEqual(TEXT("One delivery cue"),Audio->GetCueCount(ECourierAudioCue::Delivered),1);
    Audio->SetVehicleSpeed(36); TestTrue(TEXT("Motor responds to speed"),Audio->GetMotorGain()>0);
    Audio->SetVehicleSpeed(0); TestEqual(TEXT("Stopped motor silent"),Audio->GetMotorGain(),0.f);
    return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FAmbientSourceTest,"CityCourier.Ambient.RetainedPlayableAssets",
    EAutomationTestFlags::EditorContext|EAutomationTestFlags::EngineFilter)
bool FAmbientSourceTest::RunTest(const FString& Parameters)
{
    for(const TCHAR* Name:{TEXT("S_City"),TEXT("S_Motor"),TEXT("S_Brake"),TEXT("S_Accepted"),TEXT("S_PickedUp"),TEXT("S_Delivered"),TEXT("S_Failed")})
    {
        const FString Path=FString::Printf(TEXT("/Game/CityCourier/Audio/%s.%s"),Name,Name);
        auto* Sound=LoadObject<USoundWave>(nullptr,*Path);
        if(TestNotNull(Name,Sound))TestTrue(TEXT("Real nonempty waveform"),Sound->Duration>.2f);
    }
    TestNotNull(TEXT("Original car"),LoadObject<UStaticMesh>(nullptr,TEXT("/Game/CityCourier/Art/Ambient/SM_CityCar.SM_CityCar")));
    auto* Person=LoadObject<USkeletalMesh>(nullptr,TEXT("/Game/CityCourier/Art/Ambient/CharMorph/SK_CitizenCharMorph.SK_CitizenCharMorph"));
    if(TestNotNull(TEXT("Original ordinary clothes"),Person) && TestNotNull(TEXT("Citizen skeleton"),Person->GetSkeleton()))
    {
        const auto& MeshBones=Person->GetRefSkeleton();
        TestTrue(TEXT("NPC skeleton is independent of the player"),Person->GetSkeleton()!=LoadObject<USkeleton>(nullptr,TEXT("/Game/CityCourier/Art/Characters/SK_Courier_Skeleton.SK_Courier_Skeleton")));
        const auto& SkeletonBones=Person->GetSkeleton()->GetReferenceSkeleton();
        for(int32 Bone=0;Bone<MeshBones.GetNum();++Bone)
        {
            const FName Name=MeshBones.GetBoneName(Bone);
            const int32 Match=SkeletonBones.FindBoneIndex(Name);
            if(!TestTrue(*FString::Printf(TEXT("Citizen skeleton retains %s"),*Name.ToString()),Match!=INDEX_NONE)) continue;
            const int32 Parent=MeshBones.GetParentIndex(Bone),SkeletonParent=SkeletonBones.GetParentIndex(Match);
            TestEqual(*FString::Printf(TEXT("Citizen parent matches for %s"),*Name.ToString()),Parent==INDEX_NONE?NAME_None:MeshBones.GetBoneName(Parent),SkeletonParent==INDEX_NONE?NAME_None:SkeletonBones.GetBoneName(SkeletonParent));
        }
        FStationFixture F;
        auto* Walker=F.World->SpawnActor<AAmbientPedestrian>();Walker->DispatchBeginPlay();
        auto* Animation=Cast<UCitizenAnimInstance>(Walker->Person->GetAnimInstance());
        if(TestNotNull(TEXT("Citizen runtime walk player"),Animation))
        {
            auto* Walk=Animation->Clips.IsValidIndex(1)?Animation->Clips[1].Get():nullptr;
            if(TestNotNull(TEXT("Citizen runtime walk clip"),Walk))
                TestEqual(TEXT("Citizen runtime animation matches its own skeleton"),Walk->GetSkeleton(),Person->GetSkeleton());
        }
    }
    return true;
}
#endif
