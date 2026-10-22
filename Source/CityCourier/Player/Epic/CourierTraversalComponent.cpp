#include "Player/Epic/CourierTraversalComponent.h"
#include "Player/CourierCharacter.h"
#include "Player/CourierActionComponent.h"
#include "Player/Epic/CourierEpicAnimationConfig.h"
#include "GameFramework/CharacterMovementComponent.h"
#include "Components/CapsuleComponent.h"
#include "Components/SkeletalMeshComponent.h"
#include "Components/PrimitiveComponent.h"
#include "Animation/AnimMontage.h"
#include "Animation/AnimInstance.h"
#include "Animation/AnimNotifies/AnimNotify.h"
#include "Animation/AnimNotifies/AnimNotifyState.h"
#include "MotionWarpingComponent.h"
#include "RootMotionModifier_SkewWarp.h"
#include "Ambient/AmbientVehicle.h"
UCourierTraversalComponent::UCourierTraversalComponent() { PrimaryComponentTick.bCanEverTick=true; }
bool UCourierTraversalComponent::LandingClear(const FVector& Feet) const
{
    const auto* Rider=Cast<ACourierCharacter>(GetOwner());
    if(!Rider || !GetWorld()) return false;
    const auto* Capsule=Rider->GetCapsuleComponent();
    const float HalfHeight=Capsule->GetScaledCapsuleHalfHeight();
    FCollisionQueryParams Params(SCENE_QUERY_STAT(CourierTraversalLanding),false,Rider);
    return !GetWorld()->OverlapBlockingTestByChannel(Feet+FVector(0,0,HalfHeight+3),FQuat::Identity,ECC_Pawn,
        FCollisionShape::MakeCapsule(Capsule->GetScaledCapsuleRadius()-1,HalfHeight-1),Params);
}
bool UCourierTraversalComponent::FindCandidate(FCourierTraversalCandidate& OutCandidate) const
{
    OutCandidate=FCourierTraversalCandidate();
    const auto* Rider=Cast<ACourierCharacter>(GetOwner());
    const auto* Config=Rider?Rider->EpicAnimationConfig.Get():nullptr;
    if(!Config || !GetWorld()) return false;
    const auto* Movement=Rider->GetCharacterMovement();
    if(!Movement->IsMovingOnGround()) return false;
    const auto* Capsule=Rider->GetCapsuleComponent();
    const FVector Feet=Rider->GetActorLocation()-FVector(0,0,Capsule->GetScaledCapsuleHalfHeight());
    const FVector Forward=Rider->GetActorForwardVector().GetSafeNormal2D();
    const float Radius=Capsule->GetScaledCapsuleRadius();
    FCollisionQueryParams Params(SCENE_QUERY_STAT(CourierTraversalProbe),false,Rider);
    FHitResult Front;
    if(!GetWorld()->LineTraceSingleByChannel(Front,Feet+FVector(0,0,45),Feet+Forward*200+FVector(0,0,45),ECC_Pawn,Params)) return false;
    auto* Surface=Front.GetComponent();
    if(!IsValid(Surface) || !IsValid(Front.GetActor()) || Cast<AAmbientVehicle>(Front.GetActor()) ||
       Surface->Mobility!=EComponentMobility::Static || !Surface->GetComponentVelocity().IsNearlyZero()) return false;
    // This first source set contains 1.0 m clips. Keep the allowed warp close to their authored height.
    const float Minimum=FMath::Max(80.f,Config->MinimumTraversalHeight);
    const float Maximum=FMath::Min(110.f,Config->MaximumMantleHeight);
    const FVector Probe=Front.ImpactPoint+Forward*8;
    FHitResult Top;
    if(!GetWorld()->LineTraceSingleByChannel(Top,FVector(Probe.X,Probe.Y,Feet.Z+Maximum+10),FVector(Probe.X,Probe.Y,Feet.Z+Minimum-5),ECC_Pawn,Params) ||
       Top.GetComponent()!=Surface || Top.ImpactNormal.Z<Movement->GetWalkableFloorZ()) return false;
    const float Height=Top.ImpactPoint.Z-Feet.Z;
    if(Height<Minimum || Height>Maximum) return false;
    const bool bMantle=Height>Config->MaximumVaultHeight;
    FVector Exit;
    if(bMantle)
    {
        const FVector Center=Top.ImpactPoint+Forward*(Radius+8);
        FHitResult Floor;
        if(!GetWorld()->LineTraceSingleByChannel(Floor,Center+FVector(0,0,20),Center-FVector(0,0,20),ECC_Pawn,Params) ||
           Floor.GetComponent()!=Surface || Floor.ImpactNormal.Z<Movement->GetWalkableFloorZ()) return false;
        Exit=Floor.ImpactPoint;
    }
    else
    {
        bool bFound=false;
        for(float Depth=40;Depth<=180;Depth+=20)
        {
            const FVector Point=Top.ImpactPoint+Forward*Depth;
            FHitResult Floor;
            if(!GetWorld()->LineTraceSingleByChannel(Floor,Point+FVector(0,0,20),FVector(Point.X,Point.Y,Feet.Z-40),ECC_Pawn,Params)) continue;
            if(Floor.GetComponent()==Surface) continue;
            if(Floor.ImpactNormal.Z<Movement->GetWalkableFloorZ() || FMath::Abs(Floor.ImpactPoint.Z-Feet.Z)>35) return false;
            const FVector Beyond=Floor.ImpactPoint+Forward*(Radius+8);
            FHitResult Landing;
            if(!GetWorld()->LineTraceSingleByChannel(Landing,Beyond+FVector(0,0,35),Beyond-FVector(0,0,45),ECC_Pawn,Params) ||
               Landing.ImpactNormal.Z<Movement->GetWalkableFloorZ() || FMath::Abs(Landing.ImpactPoint.Z-Feet.Z)>35) return false;
            Exit=Landing.ImpactPoint; bFound=true; break;
        }
        if(!bFound) return false;
    }
    if(!LandingClear(Exit)) return false;
    // Check the capsule can occupy the high part of the arc without a ceiling.
    FHitResult Ceiling;
    const FVector HighStart=Rider->GetActorLocation()+FVector(0,0,Height+15);
    const FVector HighEnd=Exit+FVector(0,0,Capsule->GetScaledCapsuleHalfHeight()+Height+15);
    if(GetWorld()->SweepSingleByChannel(Ceiling,HighStart,HighEnd,FQuat::Identity,ECC_Pawn,
       FCollisionShape::MakeCapsule(Radius-1,Capsule->GetScaledCapsuleHalfHeight()-1),Params)) return false;
    auto* Montage=(bMantle?Config->MantleMontage:Config->VaultMontage).LoadSynchronous();
    if(!Montage || Montage->GetSkeleton()!=Rider->GetMesh()->GetSkeletalMeshAsset()->GetSkeleton() || !Montage->HasRootMotion()) return false;
    OutCandidate.Actor=Front.GetActor(); OutCandidate.Component=Surface;
    OutCandidate.InitialTransform=Surface->GetComponentTransform();
    OutCandidate.LocalEntry=Surface->GetComponentTransform().InverseTransformPosition(Top.ImpactPoint);
    OutCandidate.LocalExit=Surface->GetComponentTransform().InverseTransformPosition(Exit);
    OutCandidate.bMantle=bMantle; OutCandidate.Height=Height; OutCandidate.Montage=Montage;
    return true;
}
FCourierActionResult UCourierTraversalComponent::TryStart()
{
    auto* Rider=Cast<ACourierCharacter>(GetOwner()); FText Reason;
    if(bActive || !Rider || !Rider->CanPerformGroundAction(Reason)) return {false,Reason};
    FCourierTraversalCandidate Found;
    if(!FindCandidate(Found)) return FCourierActionResult::Failure(TEXT("No safe authored traversal and landing"));
    auto* Anim=Rider->GetMesh()->GetAnimInstance();
    auto* Warping=Rider->FindComponentByClass<UMotionWarpingComponent>();
    if(!Anim || !Warping) return FCourierActionResult::Failure(TEXT("Traversal animation or motion warping is missing"));
    Rider->GetActions()->Cancel();
    Candidate=Found; Elapsed=0;
    // Adapt the sample's slot without editing the retained source montage.
    Candidate.Montage=DuplicateObject<UAnimMontage>(Found.Montage,this,MakeUniqueObjectName(this,UAnimMontage::StaticClass(),Found.Montage->GetFName()));
    for(auto& Track:Candidate.Montage->SlotAnimTracks) Track.SlotName=TEXT("FullBodyAction");
    Candidate.Montage->Notifies.RemoveAll([](const FAnimNotifyEvent& Event)
    {
        const UObject* Notify=Event.Notify?static_cast<UObject*>(Event.Notify.Get()):static_cast<UObject*>(Event.NotifyStateClass.Get());
        return Notify && Notify->GetClass()->HasAnyClassFlags(CLASS_CompiledFromBlueprint);
    });
    auto* Movement=Rider->GetCharacterMovement();
    SavedMode=Movement->MovementMode; SavedCustomMode=Movement->CustomMovementMode; bSavedOrientRotation=Movement->bOrientRotationToMovement;
    bActive=true; Movement->bOrientRotationToMovement=false; Movement->StopMovementImmediately(); Movement->SetMovementMode(MOVE_Flying);
    const FTransform Transform=Candidate.Component->GetComponentTransform();
    const FRotator Facing=Rider->GetActorRotation();
    Warping->AddOrUpdateWarpTargetFromLocationAndRotation(TEXT("FrontLedge"),Transform.TransformPosition(Candidate.LocalEntry),Facing);
    Warping->AddOrUpdateWarpTargetFromLocationAndRotation(TEXT("CourierExit"),Transform.TransformPosition(Candidate.LocalExit),Facing);
    // Source warp handles the ledge; a second native modifier aligns the remaining root motion to the validated exit.
    const float Length=Candidate.Montage->GetPlayLength();
    URootMotionModifier_SkewWarp::AddRootMotionModifierSkewWarp(Warping,Candidate.Montage,Length*.55f,Length-.02f,TEXT("CourierExit"),
        EWarpPointAnimProvider::None,FTransform::Identity,NAME_None,true,false,true);
    if(Anim->Montage_Play(Candidate.Montage)<=0) { Cancel(); return FCourierActionResult::Failure(TEXT("Traversal montage playback failed")); }
    return FCourierActionResult::Success();
}
void UCourierTraversalComponent::Cancel()
{
    if(!bActive) return;
    bActive=false;
    if(auto* Rider=Cast<ACourierCharacter>(GetOwner()))
    {
        if(auto* Anim=Rider->GetMesh()->GetAnimInstance()) if(Candidate.Montage) Anim->Montage_Stop(.08f,Candidate.Montage);
        if(auto* Warping=Rider->FindComponentByClass<UMotionWarpingComponent>())
        { Warping->RemoveWarpTarget(TEXT("FrontLedge")); Warping->RemoveWarpTarget(TEXT("CourierExit")); }
        auto* Movement=Rider->GetCharacterMovement(); Movement->bOrientRotationToMovement=bSavedOrientRotation;
        if(Movement->MovementMode==MOVE_Flying)
        {
            FFindFloorResult Floor; Movement->FindFloor(Rider->GetActorLocation(),Floor,false);
            if(SavedMode==MOVE_Walking) Movement->SetMovementMode(Floor.IsWalkableFloor() && Floor.FloorDist<=Movement->MaxStepHeight+3?MOVE_Walking:MOVE_Falling);
            else Movement->SetMovementMode(static_cast<EMovementMode>(SavedMode),SavedCustomMode);
        }
    }
    Candidate=FCourierTraversalCandidate();
}
void UCourierTraversalComponent::TickComponent(float Delta,ELevelTick TickType,FActorComponentTickFunction* Function)
{
    Super::TickComponent(Delta,TickType,Function);
    if(!bActive) return;
    auto* Rider=Cast<ACourierCharacter>(GetOwner());
    if(!Rider || !Candidate.Component.IsValid() || !Candidate.Actor.IsValid() || Rider->IsMounted() || Rider->GetActions()->IsSuppressed() || Rider->GetActions()->IsMovementLocked()) { Cancel(); return; }
    const auto* Surface=Candidate.Component.Get();
    if(!Surface->GetComponentTransform().Equals(Candidate.InitialTransform,.1) ||
       !LandingClear(Surface->GetComponentTransform().TransformPosition(Candidate.LocalExit))) { Cancel(); return; }
    Elapsed+=Delta;
    const auto* Anim=Rider->GetMesh()->GetAnimInstance();
    if(!Anim || !Anim->Montage_IsPlaying(Candidate.Montage) || Elapsed>Candidate.Montage->GetPlayLength()+.1f) Cancel();
}
void UCourierTraversalComponent::EndPlay(EEndPlayReason::Type Reason) { Cancel(); Super::EndPlay(Reason); }
