#if WITH_DEV_AUTOMATION_TESTS && WITH_EDITOR
#include "Misc/AutomationTest.h"
#include "Tests/StationTestSupport.h"
#include "Player/CourierCharacter.h"
#include "Player/CourierAnimInstance.h"
#include "Player/CourierActionAnimation.h"
#include "Animation/AnimSequence.h"
#include "Components/SkeletalMeshComponent.h"
#include "Player/CourierActionComponent.h"
#include "Player/CourierController.h"
#include "World/CourierTrainingTarget.h"
#include "Components/BoxComponent.h"
#include "GameFramework/CharacterMovementComponent.h"

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FProductionLocomotionTest,"CityCourier.CharacterActions.Production.NewLocomotion",
    EAutomationTestFlags::EditorContext|EAutomationTestFlags::EngineFilter)
bool FProductionLocomotionTest::RunTest(const FString&)
{
    FStationFixture F;F.Rider->DispatchBeginPlay();
    auto* Anim=Cast<UCourierAnimInstance>(F.Rider->GetMesh()->GetAnimInstance());
    if(!TestNotNull(TEXT("Production animation instance"),Anim) || !TestEqual(TEXT("Four locomotion clips"),Anim->Clips.Num(),4))return false;
    TestEqual(TEXT("Ordinary movement uses promoted new jog"),Anim->Clips[1]->GetName(),FString(TEXT("A_Jog")));
    TestEqual(TEXT("Shift uses promoted new sprint"),Anim->Clips[2]->GetName(),FString(TEXT("A_Sprint")));
    TestEqual(TEXT("Seated animation retained"),Anim->Clips[3]->GetName(),FString(TEXT("A_Ride")));
    return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FProductionPunchTest,"CityCourier.CharacterActions.Production.PunchContactPoses",
    EAutomationTestFlags::EditorContext|EAutomationTestFlags::EngineFilter)
bool FProductionPunchTest::RunTest(const FString&)
{
    FStationFixture F;
    auto* RiderClass=LoadClass<ACourierCharacter>(nullptr,TEXT("/Game/CityCourier/Blueprints/BP_CourierCharacter.BP_CourierCharacter_C"));
    if(!TestNotNull(TEXT("Actual production rider blueprint"),RiderClass))return false;
    auto* Rider=F.World->SpawnActor<ACourierCharacter>(RiderClass,F.Rider->GetActorLocation(),FRotator::ZeroRotator);
    Rider->DispatchBeginPlay();auto* Mesh=Rider->GetMesh();
    for(auto Action:{ECourierAction::LightL,ECourierAction::LightR,ECourierAction::Heavy})
    {
        const bool Heavy=Action==ECourierAction::Heavy,Left=Action==ECourierAction::LightL;
        const double Duration=Heavy?.95:.55,From=Heavy?.34:.14,To=Heavy?.46:.22;
        auto* Clip=FCourierActionAnimation::Load(FCourierActionAnimation::SequenceName(Action));
        if(!TestNotNull(TEXT("Real production punching sequence exists"),Clip))continue;
        TestTrue(TEXT("Same production skeleton, sufficient duration, no root motion"),FCourierActionAnimation::Compatible(Mesh,Clip,Duration));
        FVector Initial;TestTrue(TEXT("Initial fist sampled"),FCourierActionAnimation::SampleFist(Mesh,Clip,Left,0,Initial));
        double Peak=-MAX_dbl,ContactPeak=-MAX_dbl,Travel=0;
        for(int I=0;I<=120;++I)
        {
            const double Time=Duration*I/120;FVector Fist;
            if(!FCourierActionAnimation::SampleFist(Mesh,Clip,Left,Time,Fist))return false;
            const double Reach=FVector::DotProduct(Fist-Rider->GetActorLocation(),Rider->GetActorForwardVector());
            Peak=FMath::Max(Peak,Reach);Travel=FMath::Max(Travel,FVector::Distance(Fist,Initial));
            if(Time>=From && Time<=To)ContactPeak=FMath::Max(ContactPeak,Reach);
        }
        AddInfo(FString::Printf(TEXT("%s initial=%s travel=%.2f peak=%.2f contact=%.2f meshRotation=%s"),*Clip->GetName(),*Initial.ToString(),Travel,Peak,ContactPeak,*Mesh->GetRelativeRotation().ToString()));
        TestTrue(TEXT("Punch has an actual forward strike, beyond a borrowed walk"),Travel>20 && Peak>30);
        TestTrue(TEXT("Visible extension aligns with gameplay contact window"),ContactPeak>=Peak-5);
    }
    return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FProductionPunchPlaybackTest,"CityCourier.CharacterActions.Production.RealPlaybackAndContact",
    EAutomationTestFlags::EditorContext|EAutomationTestFlags::EngineFilter)
bool FProductionPunchPlaybackTest::RunTest(const FString&)
{
    for(auto Action:{ECourierAction::LightL,ECourierAction::LightR,ECourierAction::Heavy})
    {
        FStationFixture F;
        auto* Class=LoadClass<ACourierCharacter>(nullptr,TEXT("/Game/CityCourier/Blueprints/BP_CourierCharacter.BP_CourierCharacter_C"));
        auto* Rider=F.World->SpawnActor<ACourierCharacter>(Class,F.Rider->GetActorLocation(),FRotator::ZeroRotator);
        F.Controller->Possess(Rider);F.Rider->Destroy();Rider->DispatchBeginPlay();
        Rider->GetCharacterMovement()->SetMovementMode(MOVE_Walking);
        auto* Mesh=Rider->GetMesh();auto* Actions=Rider->GetActions();
        TGuardValue<uint64> FrameGuard(GFrameCounter,GFrameCounter);
        auto Step=[&](float Delta){++GFrameCounter;Mesh->TickAnimation(Delta,false);Mesh->RefreshBoneTransforms();};
        Step(.1f);
        const bool Heavy=Action==ECourierAction::Heavy,Left=Action==ECourierAction::LightL;
        if(!TestTrue(TEXT("Real delivered asset admits input"),Actions->QueueAttack(Heavy)))return false;
        if(Action==ECourierAction::LightR)
        {
            Actions->TickComponent(.2f,LEVELTICK_All,nullptr);Step(.04f);
            TestTrue(TEXT("Second light click queues actual right punch"),Actions->QueueAttack(false));
            Actions->TickComponent(.30f,LEVELTICK_All,nullptr);Step(.12f);
        }
        else {Actions->TickComponent(Heavy?.28f:.06f,LEVELTICK_All,nullptr);Step(.12f);}
        TestTrue(TEXT("Correct real attack state"),Actions->GetTimeline().GetAction()==Action);
        auto* Clip=FCourierActionAnimation::Load(FCourierActionAnimation::SequenceName(Action));FVector Expected;
        TestTrue(TEXT("Production contact sample"),FCourierActionAnimation::SampleFist(Mesh,Clip,Left,Heavy?.40:.18,Expected));
        TestTrue(TEXT("Final rendered fist follows real punch rather than locomotion"),FVector::Distance(Mesh->GetSocketLocation(Left?TEXT("PunchTrace_L"):TEXT("PunchTrace_R")),Expected)<1.);
        auto* Target=F.World->SpawnActor<ACourierTrainingTarget>(Expected,FRotator::ZeroRotator);Target->Body->SetBoxExtent(FVector(6,6,10));
        Actions->TickComponent(.2f,LEVELTICK_All,nullptr);
        TestEqual(TEXT("Visible real punch produces one contact"),Target->GetHitCount(),1);
    }
    return true;
}
#endif
