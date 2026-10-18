#include "Vehicles/RiderRecoveryComponent.h"
#include "Player/CourierActionComponent.h"
#include "Vehicles/ScooterPawn.h"
#include "Vehicles/ScooterMovementComponent.h"
#include "World/RiderStation.h"
#include "World/CityDefinition.h"
#include "Player/CourierCharacter.h"
#include "Player/CourierController.h"
#include "Delivery/DeliverySubsystem.h"
#include "Components/BoxComponent.h"
#include "Components/CapsuleComponent.h"
#include "GameFramework/CharacterMovementComponent.h"
#include "Buildings/CourierBuildingSubsystem.h"
URiderRecoveryComponent::URiderRecoveryComponent() { PrimaryComponentTick.bCanEverTick=true; }
void URiderRecoveryComponent::Configure(UCityDefinition* Definition,AScooterPawn* Vehicle,ACourierCharacter* Courier,UDeliverySubsystem* Orders,ARiderStation* RestStation)
{ City=Definition; Bike=Vehicle; Rider=Courier; Delivery=Orders; Station=RestStation; bHasSafe=false; SetRecoverHeld(false); }
bool URiderRecoveryComponent::ValidateLocation(const FTransform& Candidate,FTransform& Out,FVector& Foot) const
{
    if(!City || !Bike.IsValid() || !Rider.IsValid() || Candidate.ContainsNaN()) return false;
    const FVector Position=Candidate.GetLocation();
    if(const auto* S=GetWorld()->GetSubsystem<UCourierBuildingSubsystem>(); S && S->IsEnabled() && S->Locate(Position).IsValid()) return false;
    double RoadDistance=MAX_dbl;
    for(const auto& E:City->Roads.Edges)
    {
        const auto* A=City->Roads.FindNode(E.From); const auto* B=City->Roads.FindNode(E.To); if(!A || !B) continue;
        RoadDistance=FMath::Min(RoadDistance,FMath::PointDistToSegment(FVector(Position.X,Position.Y,0),FVector(A->Position.X,A->Position.Y,0),FVector(B->Position.X,B->Position.Y,0)));
    }
    if(RoadDistance>1000) return false;
    FCollisionQueryParams Params(SCENE_QUERY_STAT(RiderRecovery),false,Bike.Get()); Params.AddIgnoredActor(Rider.Get());
    FHitResult Ground;
    if(!GetWorld()->LineTraceSingleByChannel(Ground,Position+FVector(0,0,60),Position-FVector(0,0,120),ECC_Visibility,Params) || Ground.Normal.Z<.7) return false;
    // A roof/wall top cannot replace the original road surface.
    if(FMath::Abs(Ground.ImpactPoint.Z+52-Position.Z)>35) return false;
    const FRotator Rotation(0,Candidate.Rotator().Yaw,0);
    const FVector BikePosition=Ground.ImpactPoint+FVector(0,0,52);
    if(GetWorld()->OverlapBlockingTestByChannel(BikePosition,Rotation.Quaternion(),ECC_Pawn,FCollisionShape::MakeBox(Bike->Collision->GetScaledBoxExtent()),Params)) return false;
    // Both wheels must have support at a comparable height.
    for(float Sign:{-1.f,1.f})
    {
        FHitResult Support; const FVector Probe=BikePosition+Rotation.Vector()*75*Sign;
        if(!GetWorld()->LineTraceSingleByChannel(Support,Probe,Probe-FVector(0,0,90),ECC_Visibility,Params) || Support.Normal.Z<.7 || FMath::Abs(Support.ImpactPoint.Z-Ground.ImpactPoint.Z)>25) return false;
    }
    const auto* Capsule=Rider->GetCapsuleComponent(); const float Half=Capsule->GetScaledCapsuleHalfHeight();
    const auto Shape=FCollisionShape::MakeCapsule(Capsule->GetScaledCapsuleRadius(),Half);
    const FVector Right=Rotation.RotateVector(FVector::RightVector);
    for(FVector Offset:{Right*150,Right*-150,Rotation.Vector()*200,Rotation.Vector()*-200})
    {
        FHitResult Floor; const FVector Probe=BikePosition+Offset;
        if(!GetWorld()->LineTraceSingleByChannel(Floor,Probe+FVector(0,0,60),Probe-FVector(0,0,120),ECC_Visibility,Params) || Floor.Normal.Z<.7 || FMath::Abs(Floor.ImpactPoint.Z-Ground.ImpactPoint.Z)>35) continue;
        const FVector FootCandidate=Floor.ImpactPoint+FVector(0,0,Half+2);
        if(GetWorld()->OverlapBlockingTestByChannel(FootCandidate,FQuat::Identity,ECC_Pawn,Shape,Params)) continue;
        // Mounted rider also needs head room at the seat.
        if(Rider->IsMounted() && GetWorld()->OverlapBlockingTestByChannel(BikePosition+FVector(-10,0,95),FQuat::Identity,ECC_Pawn,Shape,Params)) continue;
        if(const auto* S=GetWorld()->GetSubsystem<UCourierBuildingSubsystem>(); S && S->IsEnabled() && S->Locate(FootCandidate).IsValid()) continue;
        Foot=FootCandidate; Out=FTransform(Rotation,BikePosition); return true;
    }
    return false;
}
void URiderRecoveryComponent::RecordSafeLocation(const FTransform& Transform)
{
    FTransform Valid; FVector Foot;
    if(ValidateLocation(Transform,Valid,Foot)) { SafeLocation=Valid; bHasSafe=true; }
}
bool URiderRecoveryComponent::FindDestination(bool StationOnly,FTransform& Out,FVector& Foot) const
{
    if(!StationOnly && bHasSafe && ValidateLocation(SafeLocation,Out,Foot)) return true;
    if(!City || !Station.IsValid()) return false;
    const auto* Node=City->Roads.FindNode(City->Station.EntranceNode); if(!Node) return false;
    // Deterministic fallback candidates around the station's public road.
    for(FVector Offset:{FVector(400,0,52),FVector(-400,0,52),FVector(0,400,52),FVector(0,-400,52),FVector(0,0,52)})
        if(ValidateLocation(FTransform(FRotator::ZeroRotator,Node->Position+Offset),Out,Foot)) return true;
    return false;
}
void URiderRecoveryComponent::Relocate(const FTransform& Transform,FVector Foot)
{
    Rider->GetActions()->Cancel();
    Bike->ClearDriveInput(); Bike->GetScooterMovement()->ResetAfterTeleport();
    Bike->VisualRoot->SetRelativeRotation(FRotator::ZeroRotator);
    Bike->SetActorTransform(Transform,false,nullptr,ETeleportType::TeleportPhysics);
    Rider->ClearMovementInput(); Rider->GetCharacterMovement()->StopMovementImmediately();
    if(!Rider->IsMounted()) { Rider->SetActorLocationAndRotation(Foot,Transform.Rotator(),false,nullptr,ETeleportType::TeleportPhysics); Rider->GetCharacterMovement()->SetMovementMode(MOVE_Walking); }
    if(auto* PC=Cast<ACourierController>(GetOwner())) PC->SetControlRotation(FRotator(-12,Transform.Rotator().Yaw,0));
    if(auto* S=GetWorld()->GetSubsystem<UCourierBuildingSubsystem>(); S && S->IsEnabled()) S->UpdateResidency(Rider->GetActorLocation());
    SafeLocation=Transform; bHasSafe=true;
}
void URiderRecoveryComponent::SetRecoverHeld(bool Held)
{
    if(Held && bHeld) return;
    bHeld=Held; HeldSeconds=0; bUsedHold=false;
}
void URiderRecoveryComponent::TickComponent(float Delta,ELevelTick TickType,FActorComponentTickFunction* Function)
{
    Super::TickComponent(Delta,TickType,Function); if(!City || !Bike.IsValid() || !Rider.IsValid() || !FMath::IsFinite(Delta) || Delta<=0) return;
    auto* PC=Cast<ACourierController>(GetOwner()); if(PC && PC->GetInterfaceMode()!=ECourierInterfaceMode::Gameplay) { SetRecoverHeld(false); return; }
    if(bHeld)
    {
        HeldSeconds+=Delta;
        if(HeldSeconds>=2 && !bUsedHold) { TryRecover(); bUsedHold=true; }
        return; // Don't overwrite the pre-stuck sample during the recovery hold.
    }
    SampleSeconds+=Delta;
    if(SampleSeconds>=1)
    {
        SampleSeconds=0; FTransform Candidate=Bike->GetActorTransform();
        if(!Rider->IsMounted()) Candidate.SetLocation(Rider->GetActorLocation()-FVector(0,0,Rider->GetCapsuleComponent()->GetScaledCapsuleHalfHeight()+2-52));
        RecordSafeLocation(Candidate);
    }
}
bool URiderRecoveryComponent::TryRecover()
{
    if(!bHeld || HeldSeconds<2 || bUsedHold) return false;
    FTransform Transform; FVector Foot; if(!FindDestination(false,Transform,Foot)) return false;
    Relocate(Transform,Foot); bUsedHold=true;
    if(Delivery) Delivery->ApplyRecoveryPenalty(); return true;
}
bool URiderRecoveryComponent::CanRescue() const
{ return Bike.IsValid() && Station.IsValid() && Bike->GetBatteryFraction()<=0 && FVector::Dist2D(Bike->GetActorLocation(),Station->GetActorLocation())>2000; }
bool URiderRecoveryComponent::TryRescue()
{
    if(!CanRescue()) return false; FTransform Transform; FVector Foot; if(!FindDestination(true,Transform,Foot)) return false;
    Relocate(Transform,Foot); Bike->SetBatteryFraction(1);
    if(Delivery) Delivery->TryCancel(EDeliveryFailure::Rescue); return true;
}
