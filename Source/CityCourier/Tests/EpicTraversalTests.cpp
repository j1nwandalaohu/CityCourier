#if WITH_DEV_AUTOMATION_TESTS
#include "Misc/AutomationTest.h"
#include "Tests/StationTestSupport.h"
#include "Player/Epic/CourierTraversalComponent.h"
#include "Player/Epic/CourierEpicAnimationConfig.h"
#include "Player/CourierActionComponent.h"
#include "GameFramework/CharacterMovementComponent.h"
#include "Animation/AnimMontage.h"
#include "Animation/AnimInstance.h"
#include "Components/SkeletalMeshComponent.h"
#include "GameFramework/WorldSettings.h"

struct FEpicTraversalFixture : FTestCourierWorld
{
    ACourierCharacter* Rider;
    FEpicTraversalFixture()
    {
        Box(FVector(0,0,-50),FVector(40000,40000,50));
        Rider=World->SpawnActor<ACourierCharacter>(FVector(0,0,90),FRotator::ZeroRotator);
        Rider->GetCharacterMovement()->bRunPhysicsWithNoController=true;
        World->InitializeActorsForPlay(FURL()); World->BeginPlay(); World->GetWorldSettings()->NotifyBeginPlay();
        Rider->GetCharacterMovement()->SetMovementMode(MOVE_Walking);
        Rider->GetActions()->TickComponent(.3f,LEVELTICK_All,nullptr);
        auto* Config=NewObject<UCourierEpicAnimationConfig>(Rider);
        Config->VaultMontage=LoadObject<UAnimMontage>(nullptr,TEXT("/Game/CityCourier/Animation/Epic/Clips/CC_AM_M_Neutral_Traversal_Vault_1_0_walk_F_Rfoot.CC_AM_M_Neutral_Traversal_Vault_1_0_walk_F_Rfoot"));
        Config->MantleMontage=LoadObject<UAnimMontage>(nullptr,TEXT("/Game/CityCourier/Animation/Epic/Clips/CC_AM_M_Neutral_Traversal_Mantle_1_0_stand_F_Lfoot.CC_AM_M_Neutral_Traversal_Mantle_1_0_stand_F_Lfoot"));
        Rider->EpicAnimationConfig=Config;
    }
    AActor* Obstacle()
    {
        auto* Result=Box(FVector(140,0,45),FVector(40,100,45));
        Result->GetRootComponent()->SetMobility(EComponentMobility::Static); return Result;
    }
};
IMPLEMENT_SIMPLE_AUTOMATION_TEST(FEpicTraversalCandidateTest,"CityCourier.Traversal.ValidObstacleAndLanding",EAutomationTestFlags::EditorContext|EAutomationTestFlags::EngineFilter)
bool FEpicTraversalCandidateTest::RunTest(const FString&)
{
    FEpicTraversalFixture F; F.Obstacle();
    FCourierTraversalCandidate Candidate;
    TestTrue(TEXT("Ninety cm authored obstacle has a clear landing"),F.Rider->GetTraversal()->FindCandidate(Candidate));
    TestTrue(TEXT("Candidate has actual root motion montage"),Candidate.Montage && Candidate.Montage->HasRootMotion());
    TestTrue(TEXT("Starts a real traversal"),F.Rider->GetTraversal()->TryStart().bSucceeded);
    TestTrue(TEXT("Owns traversal state"),F.Rider->GetTraversal()->IsActive());
    const auto* Playing=F.Rider->GetMesh()->GetAnimInstance()->GetCurrentActiveMontage();
    TestTrue(TEXT("Visible montage uses the existing gameplay slot"),Playing && Playing->SlotAnimTracks.Num()>0 && Playing->SlotAnimTracks[0].SlotName==TEXT("FullBodyAction"));
    F.Rider->GetTraversal()->Cancel();
    TestFalse(TEXT("Cancel releases action state"),F.Rider->GetTraversal()->IsActive());
    TestFalse(TEXT("Cancel does not leave flying movement"),F.Rider->GetCharacterMovement()->IsFlying());
    return true;
}
IMPLEMENT_SIMPLE_AUTOMATION_TEST(FEpicTraversalBlockedTest,"CityCourier.Traversal.MovingBaseAndBlockedExit",EAutomationTestFlags::EditorContext|EAutomationTestFlags::EngineFilter)
bool FEpicTraversalBlockedTest::RunTest(const FString&)
{
    FEpicTraversalFixture F; auto* Box=F.Obstacle(); FCourierTraversalCandidate Candidate;
    Box->GetRootComponent()->SetMobility(EComponentMobility::Movable);
    TestFalse(TEXT("Movable bases use ordinary movement"),F.Rider->GetTraversal()->FindCandidate(Candidate));
    Box->GetRootComponent()->SetMobility(EComponentMobility::Static);
    F.Box(FVector(245,0,150),FVector(25,100,150));
    TestFalse(TEXT("Blocked exit rejects fixed-world traversal"),F.Rider->GetTraversal()->FindCandidate(Candidate));
    return true;
}
IMPLEMENT_SIMPLE_AUTOMATION_TEST(FEpicTraversalRootMotionTest,"CityCourier.Traversal.RootMotionMovesCapsule",EAutomationTestFlags::EditorContext|EAutomationTestFlags::EngineFilter)
bool FEpicTraversalRootMotionTest::RunTest(const FString&)
{
    FEpicTraversalFixture F; F.Obstacle(); FCourierTraversalCandidate Candidate;
    if(!TestTrue(TEXT("Validated target"),F.Rider->GetTraversal()->FindCandidate(Candidate))) return false;
    const FVector Exit=Candidate.Component->GetComponentTransform().TransformPosition(Candidate.LocalExit);
    if(!TestTrue(TEXT("Start authored root motion"),F.Rider->GetTraversal()->TryStart().bSucceeded)) return false;
    for(int32 Frame=0;Frame<180;++Frame)
    {
        F.Rider->GetCharacterMovement()->TickComponent(1.f/60,LEVELTICK_All,nullptr);
        F.Rider->GetMesh()->RefreshBoneTransforms();
        F.Rider->GetActions()->TickComponent(1.f/60,LEVELTICK_All,nullptr);
        F.Rider->GetTraversal()->TickComponent(1.f/60,LEVELTICK_All,nullptr);
    }
    const FVector Feet=F.Rider->GetActorLocation()-FVector(0,0,88);
    AddInfo(FString::Printf(TEXT("Traversal feet=%s validated exit=%s"),*Feet.ToString(),*Exit.ToString()));
    TestFalse(TEXT("Traversal releases its lock"),F.Rider->GetTraversal()->IsActive());
    TestTrue(TEXT("Capsule actually clears the obstacle"),Feet.X>215);
    TestTrue(TEXT("Root motion finishes near the validated ground exit"),FVector::Dist(Feet,Exit)<30);
    return true;
}
#endif
