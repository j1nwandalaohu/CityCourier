#include "Player/Epic/CourierPhysicalReactionComponent.h"
#include "Player/CourierCharacter.h"
#include "Components/SkeletalMeshComponent.h"
#include "PhysicsEngine/PhysicalAnimationComponent.h"
#include "PhysicsEngine/PhysicsAsset.h"
#include "PhysicsEngine/SkeletalBodySetup.h"
#include "PhysicsEngine/BodyInstance.h"
#include "Engine/SkeletalMesh.h"
UCourierPhysicalReactionComponent::UCourierPhysicalReactionComponent() { PrimaryComponentTick.bCanEverTick=true; }
bool UCourierPhysicalReactionComponent::ApplyLocalImpulse(FName Bone,FVector Impulse)
{
    auto* Rider=Cast<ACourierCharacter>(GetOwner());
    if(!Rider || Rider->IsMounted() || Impulse.ContainsNaN() || Impulse.IsNearlyZero() || !FMath::IsFinite(MaximumImpulse) || MaximumImpulse<=0) return false;
    auto* Mesh=Rider->GetMesh(); auto* Asset=Mesh->GetPhysicsAsset();
    const auto* SkeletalMesh=Mesh->GetSkeletalMeshAsset();
    if(!Asset || !SkeletalMesh || !Mesh->GetBodyInstance(Bone) || Bone==NAME_None || Bone==TEXT("pelvis") || Bone==TEXT("root")) return false;
    const auto& Skeleton=SkeletalMesh->GetRefSkeleton();
    const int32 Index=Skeleton.FindBoneIndex(Bone),Spine=Skeleton.FindBoneIndex(TEXT("spine"));
    if(Index==INDEX_NONE || Spine==INDEX_NONE || (Index!=Spine && !Skeleton.BoneIsChildOf(Index,Spine))) return false;
    if(ActiveMesh.IsValid() && ActiveMesh!=Mesh) Cancel();
    if(!IsActive())
    {
        ActiveMesh=Mesh; SavedCollision=Mesh->GetCollisionEnabled(); SavedResponses=Mesh->GetCollisionResponseToChannels(); bSavedBlendPhysics=Mesh->bBlendPhysics;
        if(!PhysicalAnimation)
        { PhysicalAnimation=NewObject<UPhysicalAnimationComponent>(GetOwner()); PhysicalAnimation->RegisterComponent(); }
        PhysicalAnimation->SetSkeletalMeshComponent(Mesh);
        Mesh->SetCollisionEnabled(ECollisionEnabled::QueryAndPhysics);
        Mesh->SetCollisionResponseToChannel(ECC_Pawn,ECR_Ignore);
        Mesh->bBlendPhysics=false;
    }
    FPhysicalAnimationData Drive; Drive.bIsLocalSimulation=true; Drive.OrientationStrength=600; Drive.AngularVelocityStrength=60; Drive.MaxAngularForce=2500;
    for(const auto& Setup:Asset->SkeletalBodySetups)
    {
        const int32 Child=Skeleton.FindBoneIndex(Setup->BoneName);
        if(Child==INDEX_NONE) continue;
        if(Child!=Index && !Skeleton.BoneIsChildOf(Child,Index)) continue;
        auto* Body=Mesh->GetBodyInstance(Setup->BoneName); if(!Body) continue;
        if(!SavedBodies.ContainsByPredicate([&](const FSavedBody& Item){return Item.Name==Setup->BoneName;}))
            SavedBodies.Add({Setup->BoneName,Body->IsInstanceSimulatingPhysics(),Body->PhysicsBlendWeight});
        PhysicalAnimation->ApplyPhysicalAnimationSettings(Setup->BoneName,Drive);
        Body->SetInstanceSimulatePhysics(true,true); Body->PhysicsBlendWeight=.65f;
    }
    if(SavedBodies.IsEmpty()) { Cancel(); return false; }
    const FVector Bounded=Impulse.GetClampedToMaxSize(FMath::Max(0.f,MaximumImpulse-ImpulseUsed));
    ImpulseUsed+=Bounded.Size(); Elapsed=0;
    Mesh->AddImpulse(Bounded,Bone,false);
    return true;
}
void UCourierPhysicalReactionComponent::Cancel()
{
    if(auto* Mesh=ActiveMesh.Get())
    {
        for(const auto& Saved:SavedBodies) if(auto* Body=Mesh->GetBodyInstance(Saved.Name))
        { Body->SetInstanceSimulatePhysics(Saved.bSimulating,true); Body->PhysicsBlendWeight=Saved.Blend; }
        Mesh->bBlendPhysics=bSavedBlendPhysics; Mesh->SetCollisionResponseToChannels(SavedResponses); Mesh->SetCollisionEnabled(SavedCollision);
    }
    if(PhysicalAnimation) PhysicalAnimation->SetSkeletalMeshComponent(nullptr);
    SavedBodies.Reset(); ActiveMesh.Reset(); Elapsed=0; ImpulseUsed=0;
}
void UCourierPhysicalReactionComponent::TickComponent(float Delta,ELevelTick TickType,FActorComponentTickFunction* Function)
{
    Super::TickComponent(Delta,TickType,Function);
    if(!IsActive()) return;
    auto* Rider=Cast<ACourierCharacter>(GetOwner()); auto* Mesh=ActiveMesh.Get();
    if(!Rider || Rider->IsMounted() || !Mesh || Rider->GetMesh()!=Mesh || RecoverySeconds<=0) { Cancel(); return; }
    Elapsed+=Delta;
    const float Alpha=FMath::Clamp(Elapsed/RecoverySeconds,0.f,1.f);
    for(const auto& Saved:SavedBodies) if(auto* Body=Mesh->GetBodyInstance(Saved.Name)) Body->PhysicsBlendWeight=FMath::Lerp(.65f,Saved.Blend,Alpha);
    if(Alpha>=1) Cancel();
}
void UCourierPhysicalReactionComponent::EndPlay(EEndPlayReason::Type Reason) { Cancel(); Super::EndPlay(Reason); }
