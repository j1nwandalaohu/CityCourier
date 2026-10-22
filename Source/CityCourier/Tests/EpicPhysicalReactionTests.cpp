#if WITH_DEV_AUTOMATION_TESTS && WITH_EDITOR
#include "Misc/AutomationTest.h"
#include "Tests/StationTestSupport.h"
#include "Player/Epic/CourierPhysicalReactionComponent.h"
#include "Components/SkeletalMeshComponent.h"
#include "PhysicsEngine/PhysicsAsset.h"
#include "PhysicsEngine/BodyInstance.h"
#include "GameFramework/WorldSettings.h"
#include "GameFramework/CharacterMovementComponent.h"

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FEpicLocalReactionTest,"CityCourier.PhysicalReaction.LocalImpulseAndRecovery",EAutomationTestFlags::EditorContext|EAutomationTestFlags::EngineFilter)
bool FEpicLocalReactionTest::RunTest(const FString&)
{
    FStationFixture F; F.Rider->DispatchBeginPlay();
    auto* Mesh=F.Rider->GetMesh();
    auto* Asset=LoadObject<UPhysicsAsset>(nullptr,TEXT("/Game/CityCourier/Animation/Epic/PHYS_Courier.PHYS_Courier"));
    if(!TestNotNull(TEXT("Courier body asset generated"),Asset)) return false;
    Mesh->SetPhysicsAsset(Asset,true);
    if(!TestNotNull(TEXT("Generated arm body exists"),Mesh->GetBodyInstance(TEXT("upperarm_r"))) || !TestNotNull(TEXT("Generated pelvis body exists"),Mesh->GetBodyInstance(TEXT("pelvis")))) return false;
    F.Rider->SetActorLocation(FVector(0,0,90));
    F.World->InitializeActorsForPlay(FURL()); F.World->BeginPlay(); F.World->GetWorldSettings()->NotifyBeginPlay();
    F.Rider->GetCharacterMovement()->SetMovementMode(MOVE_Walking);
    Mesh->TickAnimation(.05f,false); Mesh->RefreshBoneTransforms();
    auto* Reaction=NewObject<UCourierPhysicalReactionComponent>(F.Rider); Reaction->RegisterComponent();
    const auto Collision=Mesh->GetCollisionEnabled(); const FVector Position=F.Rider->GetActorLocation();
    TestFalse(TEXT("Root cannot ragdoll"),Reaction->ApplyLocalImpulse(TEXT("pelvis"),FVector(1000,0,0)));
    TestTrue(TEXT("Upper body accepts impulse"),Reaction->ApplyLocalImpulse(TEXT("upperarm_r"),FVector(2000,0,0)));
    TestTrue(TEXT("Selected arm simulates"),Mesh->GetBodyInstance(TEXT("upperarm_r"))->IsInstanceSimulatingPhysics());
    TestFalse(TEXT("Pelvis stays controlled"),Mesh->GetBodyInstance(TEXT("pelvis"))->IsInstanceSimulatingPhysics());
    Reaction->ApplyLocalImpulse(TEXT("upperarm_r"),FVector(2000,0,0));
    TestTrue(TEXT("Repeated hit total budget bounded"),Reaction->GetImpulseUsed()<=Reaction->MaximumImpulse);
    const FTransform Before=Mesh->GetBodyInstance(TEXT("upperarm_r"))->GetUnrealWorldTransform();
    for(int32 Frame=0;Frame<6;++Frame) F.World->Tick(LEVELTICK_All,1.f/60);
    const auto* Arm=Mesh->GetBodyInstance(TEXT("upperarm_r"));
    TestTrue(TEXT("Impulse produces actual physical body motion"),!Arm->GetUnrealWorldTransform().Equals(Before,.01) || !Arm->GetUnrealWorldVelocity().IsNearlyZero(.01));
    TestTrue(TEXT("Physical reaction keeps capsule near controlled position"),FVector::Dist(F.Rider->GetActorLocation(),Position)<5);
    Reaction->TickComponent(.5f,LEVELTICK_All,nullptr);
    TestFalse(TEXT("Reaction finishes"),Reaction->IsActive());
    TestFalse(TEXT("Arm simulation restored"),Mesh->GetBodyInstance(TEXT("upperarm_r"))->IsInstanceSimulatingPhysics());
    TestEqual(TEXT("Collision restored"),Mesh->GetCollisionEnabled(),Collision);
    TestTrue(TEXT("Capsule is never teleported by reaction"),FVector::Dist(F.Rider->GetActorLocation(),Position)<5);
    return true;
}
IMPLEMENT_SIMPLE_AUTOMATION_TEST(FEpicReactionRestoreTest,"CityCourier.PhysicalReaction.InterruptionsRestorePriorState",EAutomationTestFlags::EditorContext|EAutomationTestFlags::EngineFilter)
bool FEpicReactionRestoreTest::RunTest(const FString&)
{
    FStationFixture F; F.Rider->DispatchBeginPlay(); auto* Mesh=F.Rider->GetMesh();
    auto* Asset=LoadObject<UPhysicsAsset>(nullptr,TEXT("/Game/CityCourier/Animation/Epic/PHYS_Courier.PHYS_Courier"));
    if(!TestNotNull(TEXT("Courier body asset generated"),Asset)) return false;
    Mesh->SetPhysicsAsset(Asset,true);
    auto* Body=Mesh->GetBodyInstance(TEXT("upperarm_r")); if(!TestNotNull(TEXT("Arm body"),Body)) return false;
    Mesh->SetCollisionEnabled(ECollisionEnabled::QueryAndPhysics); Body->SetInstanceSimulatePhysics(true); Body->PhysicsBlendWeight=.3f;
    auto* Reaction=NewObject<UCourierPhysicalReactionComponent>(F.Rider); Reaction->RegisterComponent();
    TestTrue(TEXT("Existing partial simulation accepted"),Reaction->ApplyLocalImpulse(TEXT("upperarm_r"),FVector(100,0,0)));
    Reaction->Cancel(); Reaction->Cancel();
    TestTrue(TEXT("Prior simulation preserved"),Body->IsInstanceSimulatingPhysics());
    TestEqual(TEXT("Prior blend preserved"),Body->PhysicsBlendWeight,.3f);
    TestTrue(TEXT("Second reaction accepted"),Reaction->ApplyLocalImpulse(TEXT("upperarm_r"),FVector(100,0,0)));
    F.Rider->SetMounted(true); Reaction->TickComponent(.01f,LEVELTICK_All,nullptr);
    TestFalse(TEXT("Mount interrupts reaction"),Reaction->IsActive());
    TestTrue(TEXT("Mount preserves original body simulation"),Body->IsInstanceSimulatingPhysics());
    F.Rider->SetMounted(false);
    TestTrue(TEXT("Reaction before EndPlay"),Reaction->ApplyLocalImpulse(TEXT("upperarm_r"),FVector(100,0,0)));
    Reaction->EndPlay(EEndPlayReason::Destroyed);
    TestFalse(TEXT("EndPlay releases reaction"),Reaction->IsActive());
    TestEqual(TEXT("EndPlay restores blend"),Body->PhysicsBlendWeight,.3f);
    TestFalse(TEXT("Missing bone rejected"),Reaction->ApplyLocalImpulse(TEXT("missing"),FVector(100,0,0)));
    return true;
}
#endif
