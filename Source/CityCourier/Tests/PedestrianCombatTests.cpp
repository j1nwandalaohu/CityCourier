#if WITH_DEV_AUTOMATION_TESTS
#include "Misc/AutomationTest.h"
#include "Tests/StationTestSupport.h"
#include "Ambient/AmbientPedestrian.h"
#include "Player/CourierStrikeReceiver.h"
#include "Player/CourierActionComponent.h"
#include "Player/CourierActionAnimation.h"
#include "Ambient/CitizenAnimInstance.h"
#include "Components/BoxComponent.h"
#include "Components/SkeletalMeshComponent.h"
#include "GameFramework/CharacterMovementComponent.h"
#include "Animation/AnimSequence.h"

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FPedestrianStrikeContractTest,"CityCourier.Ambient.Combat.StrikeReceiver",
    EAutomationTestFlags::EditorContext|EAutomationTestFlags::EngineFilter)
bool FPedestrianStrikeContractTest::RunTest(const FString& Parameters)
{
    FTestCourierWorld F;
    F.Box(FVector(0,0,-50),FVector(10000,10000,50));
    auto* Ped=F.World->SpawnActor<AAmbientPedestrian>();
    Ped->SetPatrol({FVector(0,0,90),FVector(4000,0,90)});
    Ped->SetPopulationActive(true);
    auto* Receiver=Cast<ICourierStrikeReceiver>(Ped);
    if(TestNotNull(TEXT("Street pedestrians receive the existing punch contact"),Receiver))
        TestTrue(TEXT("A valid light punch starts a reaction"),Receiver->ReceiveCourierStrike(FGuid::NewGuid(),FVector(90,0,90),ECourierAction::LightL));
    return true;
}
IMPLEMENT_SIMPLE_AUTOMATION_TEST(FCitizenFallSpaceTest,"CityCourier.Ambient.Combat.FallSpace",
    EAutomationTestFlags::EditorContext|EAutomationTestFlags::EngineFilter)
bool FCitizenFallSpaceTest::RunTest(const FString&)
{
    FTestCourierWorld F;F.Box(FVector(0,0,-50),FVector(10000,10000,50));
    auto* P=F.World->SpawnActor<AAmbientPedestrian>();
    P->SetPatrol({FVector(0,0,90),FVector(4000,0,90)});P->SetPopulationActive(true);
    F.Box(FVector(-70,0,90),FVector(10,100,90));
    TestTrue(TEXT("A constrained heavy punch still reacts"),P->ReceiveCourierStrike(FGuid::NewGuid(),FVector(90,0,90),ECourierAction::Heavy));
    TestTrue(TEXT("A wall behind the citizen prevents a fall through masonry"),P->GetReaction()==ECitizenReaction::Staggering);
    return true;
}

struct FCitizenCombatFixture : FTestCourierWorld
{
    AAmbientPedestrian* Ped;
    FCitizenCombatFixture()
    {
        Box(FVector(0,0,-50),FVector(10000,10000,50));
        Ped=World->SpawnActor<AAmbientPedestrian>();Ped->DispatchBeginPlay();
        Ped->SetPatrol({FVector(0,0,90),FVector(4000,0,90)});Ped->SetPopulationActive(true);
    }
    bool Hit(ECourierAction Action=ECourierAction::Heavy)
    {return Ped->ReceiveCourierStrike(FGuid::NewGuid(),Ped->GetActorLocation()+FVector(90,0,0),Action);}
};
IMPLEMENT_SIMPLE_AUTOMATION_TEST(FCitizenReactionTest,"CityCourier.Ambient.Combat.ReactionLifecycle",
    EAutomationTestFlags::EditorContext|EAutomationTestFlags::EngineFilter)
bool FCitizenReactionTest::RunTest(const FString&)
{
    FCitizenCombatFixture F;auto* P=F.Ped;
    for(int I=0;I<60;++I)P->Tick(1.f/60);
    TestTrue(TEXT("Fixture patrol is moving"),P->GetSpeedCm()>10);
    const FVector Before=P->GetActorLocation();
    TestTrue(TEXT("First light punch accepted"),F.Hit(ECourierAction::LightL));
    TestTrue(TEXT("First light staggers"),P->GetReaction()==ECitizenReaction::Staggering);
    P->Tick(.3f);
    TestTrue(TEXT("Hit reaction stops patrol immediately"),P->GetActorLocation().Equals(Before,.01) && P->GetSpeedCm()==0);
    TestTrue(TEXT("Second light accepted"),F.Hit(ECourierAction::LightR));
    TestTrue(TEXT("Two light punches knock down"),P->GetReaction()==ECitizenReaction::Falling);
    P->Tick(1.06f);
    TestTrue(TEXT("Fall finishes into down hold"),P->GetReaction()==ECitizenReaction::Down);
    TestTrue(TEXT("Down collision fits a grounded body"),P->Body->GetScaledBoxExtent().Z<25);
    TestTrue(TEXT("Expanding the fallen body cannot lift a nearby player"),P->Body->GetCollisionResponseToChannel(ECC_Pawn)==ECR_Ignore);
    TestTrue(TEXT("Mesh origin stays on street"),FMath::Abs(P->Person->GetComponentLocation().Z)<.01);
    TestFalse(TEXT("Down targets do not restart their timers"),F.Hit());
    P->Tick(2.f);
    TestTrue(TEXT("Clear space starts authored recovery"),P->GetReaction()==ECitizenReaction::GettingUp);
    TestTrue(TEXT("Recovery restores standing player collision"),P->Body->GetCollisionResponseToChannel(ECC_Pawn)==ECR_Block);
    P->Tick(2.71f);
    TestTrue(TEXT("Recovery resumes walking"),P->GetReaction()==ECitizenReaction::Walking);
    for(int I=0;I<120;++I)P->Tick(1.f/60);
    TestTrue(TEXT("Old patrol resumes after recovery"),FVector::DistSquared2D(Before,P->GetActorLocation())>FMath::Square(50.));
    TestTrue(TEXT("Single heavy punch accepted"),F.Hit());
    TestTrue(TEXT("Heavy immediately starts knockdown"),P->GetReaction()==ECitizenReaction::Falling);
    return true;
}
IMPLEMENT_SIMPLE_AUTOMATION_TEST(FCitizenSafetyTest,"CityCourier.Ambient.Combat.InputPoolingAndClearance",
    EAutomationTestFlags::EditorContext|EAutomationTestFlags::EngineFilter)
bool FCitizenSafetyTest::RunTest(const FString&)
{
    FCitizenCombatFixture F;auto* P=F.Ped;const auto Id=FGuid::NewGuid();
    TestFalse(TEXT("Invalid identifier rejected"),P->ReceiveCourierStrike(FGuid(),FVector(90,0,90),ECourierAction::LightL));
    TestFalse(TEXT("Nonattack action rejected"),P->ReceiveCourierStrike(Id,FVector(90,0,90),ECourierAction::HitFront));
    TestFalse(TEXT("Remote strike rejected"),P->ReceiveCourierStrike(Id,FVector(1000,0,90),ECourierAction::Heavy));
    TestTrue(TEXT("Light accepted once"),P->ReceiveCourierStrike(Id,FVector(90,0,90),ECourierAction::LightL));
    TestFalse(TEXT("One fist cannot hit twice"),P->ReceiveCourierStrike(Id,FVector(90,0,90),ECourierAction::LightL));
    TestTrue(TEXT("Duplicate contact does not cause knockdown"),P->GetReaction()==ECitizenReaction::Staggering);
    P->Tick(1.3f);
    F.Hit(ECourierAction::LightR);
    TestTrue(TEXT("Separated light punches do not accumulate forever"),P->GetReaction()==ECitizenReaction::Staggering);
    F.Hit();P->Tick(1.06f);
    auto* Ceiling=F.Box(FVector(0,0,125),FVector(70,70,15));
    P->Tick(3.f);
    TestTrue(TEXT("Cannot stand into an obstacle"),P->GetReaction()==ECitizenReaction::Down);
    Ceiling->Destroy();P->Tick(.02f);
    TestTrue(TEXT("Cleared obstacle permits recovery"),P->GetReaction()==ECitizenReaction::GettingUp);
    P->SetPopulationActive(false);
    TestTrue(TEXT("Pool disable clears reaction"),P->GetReaction()==ECitizenReaction::Walking);
    TestEqual(TEXT("Recycled standing collision restored"),P->Body->GetScaledBoxExtent().Z,85.);
    TestFalse(TEXT("Disabled pedestrians reject hits"),F.Hit());
    P->SetPatrol({FVector(0,0,90),FVector(4000,0,90)});P->SetPopulationActive(true);
    TestTrue(TEXT("A reused actor accepts fresh input"),P->ReceiveCourierStrike(Id,FVector(90,0,90),ECourierAction::LightL));
    TestTrue(TEXT("Pool resets combo and strike history"),P->GetReaction()==ECitizenReaction::Staggering);
    return true;
}
IMPLEMENT_SIMPLE_AUTOMATION_TEST(FCitizenContactTest,"CityCourier.Ambient.Combat.ProductionFistAndWall",
    EAutomationTestFlags::EditorContext|EAutomationTestFlags::EngineFilter)
bool FCitizenContactTest::RunTest(const FString&)
{
    for(bool bWall:{false,true})
    {
        FStationFixture F;F.Rider->SetActorLocation(FVector(0,0,90));F.Rider->DispatchBeginPlay();
        F.Rider->GetCharacterMovement()->SetMovementMode(MOVE_Walking);
        auto* A=F.Rider->GetActions();auto* Clip=A->ActionAnimations.FindRef(ECourierAction::Heavy).Get();
        if(!TestNotNull(TEXT("Real shipped heavy punch animation"),Clip))return false;
        FVector Fist;
        TestTrue(TEXT("Real heavy trajectory sampled"),FCourierActionAnimation::SampleFist(F.Rider->GetMesh(),Clip,false,.4,Fist));
        AddInfo(FString::Printf(TEXT("Production fist: %s"),*Fist.ToString()));
        auto* P=F.World->SpawnActor<AAmbientPedestrian>();P->DispatchBeginPlay();
        P->SetPatrol({FVector(Fist.X,Fist.Y,90),FVector(Fist.X+4000,Fist.Y,90)});P->SetPopulationActive(true);
        if(bWall)
        {
            const FVector Mid=FVector(Fist.X,Fist.Y,0)*.5+FVector(0,0,120);
            // The punch's actual sampled direction, not an assumed world axis.
            auto* Wall=F.Box(Mid,FVector(200,2,120));
            Wall->SetActorRotation(FVector(Fist.X,Fist.Y,0).Rotation()+FRotator(0,90,0));
        }
        TestTrue(TEXT("Real heavy attack admitted"),A->QueueAttack(true));A->TickComponent(.6f,LEVELTICK_All,nullptr);
        TestTrue(bWall?TEXT("Wall prevents civilian strike"):TEXT("Shipped heavy fist knocks civilian down"),
            P->GetReaction()==(bWall?ECitizenReaction::Walking:ECitizenReaction::Falling));
    }
    return true;
}
IMPLEMENT_SIMPLE_AUTOMATION_TEST(FCitizenPoseTest,"CityCourier.Ambient.Combat.AuthoredVisiblePoses",
    EAutomationTestFlags::EditorContext|EAutomationTestFlags::EngineFilter)
bool FCitizenPoseTest::RunTest(const FString&)
{
    FCitizenCombatFixture F;auto* P=F.Ped;
    auto* Anim=Cast<UCitizenAnimInstance>(P->Person->GetAnimInstance());
    if(!TestNotNull(TEXT("Citizen native animation bridge"),Anim))return false;
    TestEqual(TEXT("All five citizen clips retained"),Anim->Clips.Num(),5);
    for(const auto& Clip:Anim->Clips)
        if(TestNotNull(TEXT("Authored citizen clip"),Clip.Get()))TestEqual(TEXT("Clip matches citizen skeleton"),Clip->GetSkeleton(),P->Person->GetSkeletalMeshAsset()->GetSkeleton());
    TGuardValue<uint64> FrameGuard(GFrameCounter,GFrameCounter);
    auto Pose=[&](float Dt){++GFrameCounter;P->Person->TickAnimation(Dt,false);P->Person->RefreshBoneTransforms();};
    Pose(.2f);const double StandingHead=P->Person->GetBoneLocation(TEXT("head")).Z;
    TestTrue(TEXT("Standing head has human height"),StandingHead>130 && StandingHead<180);
    F.Hit();P->Tick(1.06f);Pose(.2f);
    const double DownHead=P->Person->GetBoneLocation(TEXT("head")).Z;
    TestTrue(TEXT("Down holds an evaluated low skeletal pose"),DownHead>0 && DownHead<50);
    const FVector FallenHead=P->Person->GetBoneLocation(TEXT("head"))-P->Person->GetComponentLocation();
    TestTrue(TEXT("The body falls away from the strike along the actor facing axis"),
        FVector::DotProduct(FallenHead,P->GetActorForwardVector())<-40);
    Pose(.5f);
    TestTrue(TEXT("Down pose clamps instead of looping upright"),FMath::IsNearlyEqual(P->Person->GetBoneLocation(TEXT("head")).Z,DownHead,.1));
    P->Tick(2.f);P->Tick(1.5f);Pose(.2f);
    TestTrue(TEXT("Recovery evaluates a rising skeletal pose"),P->Person->GetBoneLocation(TEXT("head")).Z>DownHead+20);
    P->Tick(1.21f);Pose(.2f);
    TestTrue(TEXT("Recovered pose returns to human height"),P->Person->GetBoneLocation(TEXT("head")).Z>130);
    return true;
}
#endif
