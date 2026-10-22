#include "Player/Epic/CourierObjectInteractionComponent.h"
#include "Player/CourierCharacter.h"
#include "Player/CourierActionComponent.h"
#include "World/CourierGrabbable.h"
#include "World/CourierInteractionButton.h"
#include "Components/PrimitiveComponent.h"
#include "Components/SkeletalMeshComponent.h"
#include "Components/StaticMeshComponent.h"
#include "GameFramework/CharacterMovementComponent.h"
#include "PhysicsEngine/PhysicsHandleComponent.h"
#include "Engine/World.h"

UCourierObjectInteractionComponent::UCourierObjectInteractionComponent() { PrimaryComponentTick.bCanEverTick=true; }
bool UCourierObjectInteractionComponent::CheckTarget(AActor* Actor,FVector Point,FText& Reason) const
{
    const auto* Rider=Cast<ACourierCharacter>(GetOwner());
    if(!Rider || !IsValid(Actor) || Actor->IsActorBeingDestroyed() || Point.ContainsNaN() || !GetWorld()) return false;
    if(!Rider->CanReachInteraction(Actor,Point,Reason)) return false;
    FHitResult Hit; FCollisionQueryParams Params(SCENE_QUERY_STAT(CourierObjectReach),false,Rider);
    if(GetWorld()->LineTraceSingleByChannel(Hit,Rider->GetPawnViewLocation(),Point,ECC_Visibility,Params) && Hit.GetActor()!=Actor)
    { Reason=FText::FromString(TEXT("目标被遮挡")); return false; }
    return true;
}
bool UCourierObjectInteractionComponent::CanReachHand(FVector Point,bool bLeft) const
{
    const auto* Rider=Cast<ACourierCharacter>(GetOwner()); if(!Rider || Point.ContainsNaN()) return false;
    const auto* Mesh=Rider->GetMesh();
    const FName Shoulder=bLeft?TEXT("upperarm_l"):TEXT("upperarm_r");
    const FName Elbow=bLeft?TEXT("forearm_l"):TEXT("forearm_r");
    const FName Hand=bLeft?TEXT("hand_l"):TEXT("hand_r");
    if(Mesh->GetBoneIndex(Shoulder)==INDEX_NONE || Mesh->GetBoneIndex(Elbow)==INDEX_NONE || Mesh->GetBoneIndex(Hand)==INDEX_NONE) return false;
    const FVector A=Mesh->GetSocketLocation(Shoulder),B=Mesh->GetSocketLocation(Elbow),C=Mesh->GetSocketLocation(Hand);
    const double Length=FMath::Min(90.,FVector::Dist(A,B)+FVector::Dist(B,C));
    return Length>5 && FVector::Dist(A,Point)<=Length+1;
}
FCourierActionResult UCourierObjectInteractionComponent::TryGrab(UPrimitiveComponent* Target)
{
    auto* Rider=Cast<ACourierCharacter>(GetOwner()); FText Reason;
    if(IsBusy() || !Rider || !Rider->CanPerformGroundAction(Reason)) return {false,Reason};
    auto* Object=IsValid(Target)?Cast<ACourierGrabbable>(Target->GetOwner()):nullptr;
    if(!Object || Object->Body!=Target || !Object->CanInteract(Rider,Reason)) return Reason.IsEmpty()?FCourierActionResult::Failure(TEXT("这个物体不能拿起")):FCourierActionResult{false,Reason};
    const float Mass=Target->GetMass();
    if(!FMath::IsFinite(Mass) || Mass<=0 || !FMath::IsFinite(MaximumMassKg) || Mass>MaximumMassKg) return FCourierActionResult::Failure(TEXT("物体太重，无法拿起"));
    if(!CheckTarget(Object,Target->GetComponentLocation(),Reason)) return {false,Reason};
    auto* Handle=Rider->FindComponentByClass<UPhysicsHandleComponent>();
    if(!Handle || !Target->IsSimulatingPhysics()) return FCourierActionResult::Failure(TEXT("物体暂时无法搬动"));
    SavedPawnResponse=Target->GetCollisionResponseToChannel(ECC_Pawn); SavedCollision=Target->GetCollisionEnabled(); bSavedSimulating=Target->IsSimulatingPhysics();
    Target->SetCollisionResponseToChannel(ECC_Pawn,ECR_Ignore);
    Handle->GrabComponentAtLocationWithRotation(Target,NAME_None,Target->GetCenterOfMass(),Target->GetComponentRotation());
    if(Handle->GetGrabbedComponent()!=Target)
    { Target->SetCollisionResponseToChannel(ECC_Pawn,SavedPawnResponse); return FCourierActionResult::Failure(TEXT("无法拿稳物体，请重试")); }
    ++Generation; State=EState::Holding; Held=Target; Object->Holder=Rider; Elapsed=0;
    return FCourierActionResult::Success();
}
FCourierActionResult UCourierObjectInteractionComponent::TryPress(AActor* Target)
{
    auto* Rider=Cast<ACourierCharacter>(GetOwner()); auto* Button=Cast<ACourierInteractionButton>(Target); FText Reason;
    if(IsBusy() || !Rider || !Rider->CanPerformGroundAction(Reason)) return {false,Reason};
    if(!Button || !Button->CanInteract(Rider,Reason) || !CheckTarget(Button,Button->GetInteractionLocation(),Reason)) return {false,Reason};
    if(!CanReachHand(Button->GetInteractionLocation(),Button->bUseLeftHand)) return FCourierActionResult::Failure(TEXT("请再靠近按钮"));
    ++Generation; State=EState::Pressing; PressTarget=Button; Elapsed=0; bSubmitted=false;
    return FCourierActionResult::Success();
}
void UCourierObjectInteractionComponent::Release()
{
    ++Generation;
    if(auto* Rider=Cast<ACourierCharacter>(GetOwner()))
    {
        if(auto* Handle=Rider->FindComponentByClass<UPhysicsHandleComponent>()) Handle->ReleaseComponent();
        if(auto* Body=Held.Get())
        {
            Body->SetCollisionResponseToChannel(ECC_Pawn,SavedPawnResponse);
            Body->SetCollisionEnabled(SavedCollision);
            if(Body->IsSimulatingPhysics()!=bSavedSimulating) Body->SetSimulatePhysics(bSavedSimulating);
            if(auto* Object=Cast<ACourierGrabbable>(Body->GetOwner())) if(Object->Holder.Get()==Rider) Object->Holder.Reset();
        }
    }
    State=EState::None; Held.Reset(); PressTarget.Reset(); Elapsed=0; bSubmitted=false;
}
void UCourierObjectInteractionComponent::Cancel() { Release(); }
FCourierInteractionPose UCourierObjectInteractionComponent::GetPose() const
{
    FCourierInteractionPose Pose;
    const auto* Rider=Cast<ACourierCharacter>(GetOwner()); if(!Rider) return Pose;
    if(State==EState::Holding && Held.IsValid())
    {
        const FVector Center=Held->GetComponentLocation();
        const FVector Side=Rider->GetActorRightVector()*FMath::Min(22.,Held->Bounds.BoxExtent.Y);
        Pose.LeftHandWorld=FTransform(Rider->GetMesh()->GetSocketQuaternion(TEXT("hand_l")),Center-Side);
        Pose.RightHandWorld=FTransform(Rider->GetMesh()->GetSocketQuaternion(TEXT("hand_r")),Center+Side);
        Pose.LeftWeight=Pose.RightWeight=1;
    }
    else if((State==EState::Pressing || State==EState::Recovering) && PressTarget.IsValid())
    {
        if(const auto* Target=Cast<ICourierInteractable>(PressTarget.Get())) Target->GetInteractionPose(Rider,Pose);
        const float Weight=State==EState::Recovering?FMath::Clamp(1.f-(Elapsed-RecoveryStarted-.05f)/.2f,0.f,1.f):FMath::Clamp(Elapsed/FMath::Max(.05f,ContactDelay),0.f,1.f);
        Pose.LeftWeight*=Weight; Pose.RightWeight*=Weight;
    }
    return Pose;
}
void UCourierObjectInteractionComponent::TickComponent(float Delta,ELevelTick TickType,FActorComponentTickFunction* Function)
{
    Super::TickComponent(Delta,TickType,Function);
    if(State==EState::None || !FMath::IsFinite(Delta) || Delta<=0) return;
    auto* Rider=Cast<ACourierCharacter>(GetOwner());
    if(!Rider || Rider->IsMounted() || Rider->GetActions()->IsSuppressed() || Rider->GetActions()->IsGroundActionBlocked() || !Rider->GetCharacterMovement()->IsMovingOnGround()) { Cancel(); return; }
    Elapsed+=Delta;
    if(State==EState::Holding)
    {
        auto* Body=Held.Get(); auto* Handle=Rider->FindComponentByClass<UPhysicsHandleComponent>();
        if(!Body || !Handle || Handle->GetGrabbedComponent()!=Body || !Body->IsSimulatingPhysics() || FVector::Dist(Body->GetComponentLocation(),Rider->GetActorLocation())>220) { Cancel(); return; }
        const FVector Goal=Rider->GetActorLocation()+Rider->GetActorForwardVector()*60+FVector(0,0,20);
        FHitResult Hit; FCollisionQueryParams Params(SCENE_QUERY_STAT(CourierHeldObjectPath),false,Rider); Params.AddIgnoredActor(Body->GetOwner());
        if(GetWorld()->SweepSingleByChannel(Hit,Body->GetComponentLocation(),Goal,FQuat::Identity,ECC_Visibility,FCollisionShape::MakeSphere(FMath::Min(25.,Body->Bounds.BoxExtent.GetMax())),Params)) { Cancel(); return; }
        Handle->SetTargetLocationAndRotation(Goal,Rider->GetActorRotation());
        return;
    }
    auto* Button=Cast<ACourierInteractionButton>(PressTarget.Get()); FText Reason;
    if(!Button || !Button->CanInteract(Rider,Reason) || !CheckTarget(Button,Button->GetInteractionLocation(),Reason)) { Cancel(); return; }
    if(State==EState::Recovering) { if(Elapsed>=RecoveryStarted+.25f) Release(); return; }
    if(Elapsed>=ContactDelay && !bSubmitted)
    {
        const FName Bone=Button->bUseLeftHand?TEXT("hand_l"):TEXT("hand_r");
        if(FVector::Dist(Rider->GetMesh()->GetSocketLocation(Bone),Button->GetInteractionLocation())<=5)
        {
            const uint64 Before=Generation; bSubmitted=true; State=EState::Recovering; RecoveryStarted=Elapsed;
            Button->SubmitPress(Rider);
            if(Generation!=Before) return;
        }
        else if(Elapsed>ContactDelay+1) Cancel();
    }
}
void UCourierObjectInteractionComponent::EndPlay(EEndPlayReason::Type Reason) { Cancel(); Super::EndPlay(Reason); }
