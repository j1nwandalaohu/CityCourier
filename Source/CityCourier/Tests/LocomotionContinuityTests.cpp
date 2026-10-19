#if WITH_DEV_AUTOMATION_TESTS && WITH_EDITOR
#include "Misc/AutomationTest.h"
#include "Tests/StationTestSupport.h"
#include "Player/CourierAnimInstance.h"
#include "Player/CourierActionComponent.h"
#include "Animation/AnimSequence.h"
#include "Animation/AnimMontage.h"
#include "Animation/Skeleton.h"
#include "Components/SkeletalMeshComponent.h"
#include "GameFramework/CharacterMovementComponent.h"

namespace
{
FVector ClipBone(const UAnimSequence* Clip,FName Bone,double Seconds,bool bLoop=true)
{
    const auto& Ref=Clip->GetSkeleton()->GetReferenceSkeleton();
    FTransform Chain=FTransform::Identity;
    const FAnimExtractContext Context(bLoop?FMath::Fmod(Seconds,double(Clip->GetPlayLength())):FMath::Clamp(Seconds,0.,double(Clip->GetPlayLength())));
    for(int32 Index=Ref.FindBoneIndex(Bone);Index!=INDEX_NONE;Index=Ref.GetParentIndex(Index))
    {
        FTransform Local;Clip->GetBoneTransform(Local,FSkeletonPoseBoneIndex(Index),Context,false);
        Chain=Chain*Local;
    }
    return Chain.GetLocation();
}
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FMovingLandingMomentumTest,"CityCourier.CharacterActions.Continuity.MovingLandingMomentum",
    EAutomationTestFlags::EditorContext|EAutomationTestFlags::EngineFilter)
bool FMovingLandingMomentumTest::RunTest(const FString&)
{
    FStationFixture F;F.Rider->DispatchBeginPlay();F.Rider->SetSprinting(true);
    auto* Move=F.Rider->GetCharacterMovement();auto* Actions=F.Rider->GetActions();
    Move->SetMovementMode(MOVE_Falling);Move->SetMovementMode(MOVE_Walking);
    Move->Velocity=FVector(700,0,0);
    Actions->TickComponent(1.f/60,LEVELTICK_All,nullptr);
    AddInfo(FString::Printf(TEXT("Landing horizontal speed %.2f, max %.2f"),Move->Velocity.Size2D(),Move->MaxWalkSpeed));
    TestTrue(TEXT("Running landing retains physical horizontal momentum"),Move->Velocity.Size2D()>650);
    TestTrue(TEXT("Landing does not disable held sprint"),Move->MaxWalkSpeed>650);
    TestFalse(TEXT("Landing still blocks immediate combat input"),Actions->QueueAttack(false));
    return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FGaitCycleContinuityTest,"CityCourier.CharacterActions.Continuity.GaitCycle",
    EAutomationTestFlags::EditorContext|EAutomationTestFlags::EngineFilter)
bool FGaitCycleContinuityTest::RunTest(const FString&)
{
    FStationFixture F;F.Rider->DispatchBeginPlay();auto* Mesh=F.Rider->GetMesh();
    auto* Anim=Cast<UCourierAnimInstance>(Mesh->GetAnimInstance());
    if(!TestNotNull(TEXT("Production locomotion instance"),Anim) || Anim->Clips.Num()!=4)return false;
    const auto* Jog=Anim->Clips[1].Get();const auto* Sprint=Anim->Clips[2].Get();
    TGuardValue<uint64> FrameGuard(GFrameCounter,GFrameCounter);
    auto Step=[&](){++GFrameCounter;Mesh->TickAnimation(1.f/60,false);Mesh->RefreshBoneTransforms();};
    F.Rider->GetCharacterMovement()->Velocity=FVector(420,0,0);
    for(int I=0;I<60;++I)Step();
    const double Phase=FMath::Fmod((420./473.2)/Jog->GetPlayLength(),1.);
    F.Rider->GetCharacterMovement()->Velocity=FVector(700,0,0);
    for(int I=0;I<12;++I)Step();
    const double Expected=Phase*Sprint->GetPlayLength()+.2*(700./715.9);
    for(FName Bone:{FName(TEXT("foot_l")),FName(TEXT("foot_r"))})
    {
        const double Error=FVector::Distance(Mesh->GetBoneLocation(Bone,EBoneSpaces::ComponentSpace),ClipBone(Sprint,Bone,Expected));
        AddInfo(FString::Printf(TEXT("%s phase-preserving gait error %.3f cm"),*Bone.ToString(),Error));
        TestTrue(TEXT("Jog to sprint continues the same foot cycle after crossfade"),Error<1.);
    }
    return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FRunningLandingPoseTest,"CityCourier.CharacterActions.Continuity.RunningLandingPose",
    EAutomationTestFlags::EditorContext|EAutomationTestFlags::EngineFilter)
bool FRunningLandingPoseTest::RunTest(const FString&)
{
    FStationFixture F;F.Rider->DispatchBeginPlay();F.Rider->SetSprinting(true);
    auto* Landing=F.World->SpawnActor<ACourierCharacter>(F.Rider->GetActorLocation()+FVector(300,0,0),FRotator::ZeroRotator);
    Landing->DispatchBeginPlay();Landing->SetSprinting(true);
    auto* Move=Landing->GetCharacterMovement();
    auto* Mesh=Landing->GetMesh();auto* Reference=F.Rider->GetMesh();
    TGuardValue<uint64> FrameGuard(GFrameCounter,GFrameCounter);
    auto Step=[&](float Delta)
    {
        ++GFrameCounter;
        Reference->TickAnimation(Delta,false);Reference->RefreshBoneTransforms();
        Mesh->TickAnimation(Delta,false);Mesh->RefreshBoneTransforms();
    };
    F.Rider->GetCharacterMovement()->Velocity=FVector(700,0,0);Move->Velocity=FVector(700,0,0);
    for(int I=0;I<31;++I)Step(1.f/60);
    Move->SetMovementMode(MOVE_Falling);Move->SetMovementMode(MOVE_Walking);Move->Velocity=FVector(700,0,0);
    for(int I=0;I<6;++I)
    {
        Landing->GetActions()->TickComponent(1.f/60,LEVELTICK_All,nullptr);
        Step(1.f/60);
    }
    double Largest=0;
    for(FName Bone:{FName(TEXT("foot_l")),FName(TEXT("foot_r"))})
        Largest=FMath::Max(Largest,FVector::Distance(Mesh->GetBoneLocation(Bone,EBoneSpaces::ComponentSpace),Reference->GetBoneLocation(Bone,EBoneSpaces::ComponentSpace)));
    AddInfo(FString::Printf(TEXT("Moving landing foot deviation from running %.3f cm"),Largest));
    TestTrue(TEXT("Moving landing keeps running legs instead of a full planted idle pose"),Largest<14.);
    return true;
}
IMPLEMENT_SIMPLE_AUTOMATION_TEST(FAirbornePoseCoverageTest,"CityCourier.CharacterActions.Continuity.AirbornePoseCoverage",
    EAutomationTestFlags::EditorContext|EAutomationTestFlags::EngineFilter)
bool FAirbornePoseCoverageTest::RunTest(const FString&)
{
    FStationFixture F;F.Rider->DispatchBeginPlay();
    auto* Mesh=F.Rider->GetMesh();auto* Move=F.Rider->GetCharacterMovement();auto* Actions=F.Rider->GetActions();
    auto* Anim=Mesh->GetAnimInstance();
    TGuardValue<uint64> FrameGuard(GFrameCounter,GFrameCounter);
    auto Step=[&](){++GFrameCounter;Actions->TickComponent(1.f/60,LEVELTICK_All,nullptr);Mesh->TickAnimation(1.f/60,false);Mesh->RefreshBoneTransforms();};
    F.Rider->SetSprinting(true);Move->Velocity=FVector(700,0,0);
    for(int I=0;I<30;++I)Step();
    Move->SetMovementMode(MOVE_Falling);Actions->JumpStarted();
    double Largest=0,MinimumHandoffWeight=1;
    for(int I=0;I<48;++I)
    {
        Move->Velocity=FVector(700,0,420-980.*I/60);Step();
        if(I>=7 && I<=16)MinimumHandoffWeight=FMath::Min(MinimumHandoffWeight,double(Anim->GetSlotMontageLocalWeight(TEXT("FullBodyAction"))));
        if(I<20)continue;
        auto* Montage=Anim->GetCurrentActiveMontage();
        if(!TestNotNull(TEXT("Airborne retains a movement montage"),Montage))return false;
        auto* Clip=Cast<UAnimSequence>(Montage->GetFirstAnimReference());
        if(!TestNotNull(TEXT("Airborne sequence"),Clip))return false;
        const bool bDescending=Move->Velocity.Z<-80;
        TestEqual(TEXT("Airborne uses tuck or descent according to vertical motion"),Clip->GetFName(),FName(bDescending?TEXT("A_JumpFall"):TEXT("A_JumpLoop")));
        if(bDescending && I<39)continue; // Allow the authored tuck-to-descent crossfade.
        for(FName Bone:{FName(TEXT("pelvis")),FName(TEXT("head")),FName(TEXT("foot_l")),FName(TEXT("foot_r"))})
            Largest=FMath::Max(Largest,FVector::Distance(Mesh->GetBoneLocation(Bone,EBoneSpaces::ComponentSpace),ClipBone(Clip,Bone,Anim->Montage_GetPosition(Montage),!bDescending)));
    }
    AddInfo(FString::Printf(TEXT("Airborne native/source pose deviation %.3f cm"),Largest));
    AddInfo(FString::Printf(TEXT("Takeoff/flight minimum slot coverage %.3f"),MinimumHandoffWeight));
    TestTrue(TEXT("Takeoff/flight handoff cannot expose the ground running pose"),MinimumHandoffWeight>.9);
    TestTrue(TEXT("Ground locomotion cannot deform the sustained flight pose"),Largest<.2);
    auto* Flight=Actions->MovementAnimations.FindRef(TEXT("A_JumpLoop")).Get();
    if(!Flight)return false;
    for(int I=0;I<=8;++I)
    {
        const double T=Flight->GetPlayLength()*I/8.;
        double MaxBend=0;
        for(const TCHAR* Side:{TEXT("l"),TEXT("r")})
        {
            const FVector Hip=ClipBone(Flight,FName(*FString::Printf(TEXT("thigh_%s"),Side)),T);
            const FVector Knee=ClipBone(Flight,FName(*FString::Printf(TEXT("shin_%s"),Side)),T);
            const FVector Foot=ClipBone(Flight,FName(*FString::Printf(TEXT("foot_%s"),Side)),T);
            MaxBend=FMath::Max(MaxBend,FMath::RadiansToDegrees(FMath::Acos(FMath::Clamp(FVector::DotProduct((Knee-Hip).GetSafeNormal(),(Foot-Knee).GetSafeNormal()),-1.,1.))));
        }
        AddInfo(FString::Printf(TEXT("Flight sample %.3f maximum knee bend %.2f degrees"),T,MaxBend));
    }
    return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FDescendingPoseTest,"CityCourier.CharacterActions.Continuity.DescendingPose",
    EAutomationTestFlags::EditorContext|EAutomationTestFlags::EngineFilter)
bool FDescendingPoseTest::RunTest(const FString&)
{
    FStationFixture F;F.Rider->DispatchBeginPlay();
    auto* Move=F.Rider->GetCharacterMovement();auto* Mesh=F.Rider->GetMesh();auto* Actions=F.Rider->GetActions();
    auto* Anim=Mesh->GetAnimInstance();
    TGuardValue<uint64> FrameGuard(GFrameCounter,GFrameCounter);
    auto Step=[&](){++GFrameCounter;Actions->TickComponent(1.f/60,LEVELTICK_All,nullptr);Mesh->TickAnimation(1.f/60,false);Mesh->RefreshBoneTransforms();};
    Move->SetMovementMode(MOVE_Falling);Actions->JumpStarted();
    Move->Velocity=FVector(420,0,300);
    for(int I=0;I<24;++I)Step();
    Move->Velocity=FVector(420,0,-200);
    for(int I=0;I<36;++I)Step();
    auto* Montage=Anim->GetCurrentActiveMontage();
    if(!TestNotNull(TEXT("Descending pose remains active on a long fall"),Montage))return false;
    auto* Clip=Cast<UAnimSequence>(Montage->GetFirstAnimReference());
    if(!Clip)return false;
    TestEqual(TEXT("Descent extends the legs for landing instead of looping the tuck"),Clip->GetFName(),FName(TEXT("A_JumpFall")));
    TestTrue(TEXT("Descending pose covers ground locomotion"),Anim->GetSlotMontageLocalWeight(TEXT("FullBodyAction"))>.99);
    return true;
}
#endif
