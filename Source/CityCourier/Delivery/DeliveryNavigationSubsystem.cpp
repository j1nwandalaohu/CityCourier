#include "Delivery/DeliveryNavigationSubsystem.h"
#include "Delivery/DeliverySubsystem.h"
#include "Delivery/NavigationGuidance.h"
#include "World/CityDefinition.h"
#include "GameFramework/PlayerController.h"
#include "EngineUtils.h"
#include "GameFramework/Pawn.h"
#include "GameFramework/Character.h"
#include "Components/CapsuleComponent.h"
#include "Vehicles/ScooterPawn.h"
#include "Ambient/AmbientVehicle.h"
#include "Buildings/CourierBuildingSubsystem.h"
void UDeliveryNavigationSubsystem::Configure(UDeliverySubsystem* System)
{
    if(Delivery) Delivery->OnDeliveryChanged.RemoveAll(this);
    Delivery=System; ObservedOrder=Delivery?Delivery->GetState().Offer.InstanceId:FGuid();
    if(Delivery) Delivery->OnDeliveryChanged.AddWeakLambda(this,[this]()
    {
        const auto& S=Delivery->GetState();
        if(S.IsActive() && S.Offer.InstanceId!=ObservedOrder)
        { ObservedOrder=S.Offer.InstanceId; Mode=ECourierNavigationMode::Order; RefreshRoute(RiderPosition()); }
        else if(Mode==ECourierNavigationMode::Order && ResolveTarget()!=Target) RefreshRoute(RiderPosition());
    });
}
FVector UDeliveryNavigationSubsystem::RiderPosition() const
{
    if(const auto* PC=GetWorld()->GetFirstPlayerController()) if(const APawn* P=PC->GetPawn()) return P->GetActorLocation();
    return FVector::ZeroVector;
}
FName UDeliveryNavigationSubsystem::ResolveTarget() const
{
    if(!Delivery || !Delivery->GetCity()) return NAME_None;
    if(Mode==ECourierNavigationMode::Station) return Delivery->GetCity()->Station.LocationId;
    if(Mode==ECourierNavigationMode::ParkedBike) return IsValid(Delivery->GetScooter())?FName(TEXT("ParkedScooter")):NAME_None;
    return Delivery->GetTargetId();
}
FCourierActionResult UDeliveryNavigationSubsystem::SetNavigationMode(ECourierNavigationMode NewMode)
{
    if(!Delivery || !Delivery->GetCity()) return FCourierActionResult::Failure(TEXT("导航暂不可用"));
    if(NewMode==ECourierNavigationMode::Order && !Delivery->GetState().IsActive()) return FCourierActionResult::Failure(TEXT("当前没有配送订单"));
    if(NewMode==ECourierNavigationMode::ParkedBike)
    {
        auto* Bike=Delivery->GetScooter(); if(!IsValid(Bike)) return FCourierActionResult::Failure(TEXT("找不到车辆"));
        if(Bike->GetRider()) return FCourierActionResult::Failure(TEXT("你已经在车上"));
    }
    if(NewMode!=ECourierNavigationMode::Order && NewMode!=ECourierNavigationMode::Station && NewMode!=ECourierNavigationMode::ParkedBike)
        return FCourierActionResult::Failure(TEXT("导航目标不可用"));
    Mode=NewMode; RefreshRoute(RiderPosition()); return FCourierActionResult::Success();
}
FText UDeliveryNavigationSubsystem::GetTargetLabel() const
{
    if(ResolveTarget().IsNone()) return FText::GetEmpty();
    if(Mode==ECourierNavigationMode::ParkedBike) return FText::FromString(TEXT("停放的电动车"));
    return Delivery->GetLocationLabel(ResolveTarget());
}
void UDeliveryNavigationSubsystem::Tick(float Delta)
{
    if (const auto* PC=GetWorld()->GetFirstPlayerController()) if (const APawn* Pawn=PC->GetPawn()) UpdateNavigation(Pawn->GetActorLocation(),Delta);
}
bool UDeliveryNavigationSubsystem::RefreshRoute(FVector Position)
{
    Route={}; RouteSegment=0; Target=ResolveTarget(); CheckSeconds=0; RemainingMeters=0; TargetPosition=FVector::ZeroVector; bArrived=false; OriginRoom=NAME_None;
    if (!Delivery || !Delivery->GetCity() || Target.IsNone() || Position.ContainsNaN()) return false;
    if(Mode==ECourierNavigationMode::ParkedBike)
    { TargetPosition=Delivery->GetScooter()->GetActorLocation(); Route=Delivery->GetCity()->FindRouteToPosition(Position,TargetPosition); }
    else
    { const auto* L=Delivery->GetCity()->FindLocation(Target); if(!L) return false; TargetPosition=L->Position; Route=Delivery->GetCity()->FindRouteFrom(Position,Target); }
    if(const auto* Interiors=GetWorld()->GetSubsystem<UCourierBuildingSubsystem>(); Interiors && Interiors->IsEnabled())
    {
        const auto* C=Interiors->GetCatalog(); const auto Address=Interiors->Locate(Position);
        if(Address.IsValid()) OriginRoom=C->Buildings[Address.Building].Rooms[Address.Room].Id;
        if(Mode==ECourierNavigationMode::ParkedBike) Route=Delivery->GetCity()->FindJourneyToPosition(Position,TargetPosition,*C);
        else Route=Delivery->GetCity()->FindJourneyFrom(Position,Target,*C);
        if(Route.bReachable && !Route.Points.IsEmpty()) TargetPosition=Route.Points.Last();
    }
    if (Route.bReachable && !IsRouteClear(Route)) Route={};
    UpdateRemaining(Position); return Route.bReachable;
}
void UDeliveryNavigationSubsystem::UpdateRemaining(FVector Position)
{
    RemainingMeters=0; bArrived=false; if(!Route.bReachable) return;
    const double FinalMeters=FVector::Distance(Position,TargetPosition)/100;
    // A nearby off-road bike can have overlapping out-and-back road connectors.
    // Arrival uses a clear final approach, never the outward connector's progress.
    if(Mode==ECourierNavigationMode::ParkedBike && FinalMeters<=2 && IsRouteClear({Position,TargetPosition}))
    { RemainingMeters=FinalMeters; bArrived=true; return; }
    const auto Projection=FNavigationRouteProjection::FromRoute(Route,GetRouteGroundPosition(Position),RouteSegment);
    if(Projection.Segment!=INDEX_NONE) RouteSegment=Projection.Segment;
    if(Projection.LengthCentimeters>UE_SMALL_NUMBER && Projection.Segment!=INDEX_NONE)
        RemainingMeters=FMath::Max(0.0,Route.LengthMeters*(1-Projection.ProgressCentimeters/Projection.LengthCentimeters))+Projection.DistanceCentimeters/100;
    else RemainingMeters=FVector::Distance(Position,TargetPosition)/100;
    if(Route.bHasInteriors) RemainingMeters=FMath::Max(RemainingMeters,FinalMeters);
    bArrived=RemainingMeters<=2 && FinalMeters<=2;
    if(bArrived && !Route.TargetRoom.IsNone())
    {
        const auto* Interiors=GetWorld()->GetSubsystem<UCourierBuildingSubsystem>(); const auto Address=Interiors->Locate(Position);
        bArrived=Address.IsValid() && Interiors->GetCatalog()->Buildings[Address.Building].Rooms[Address.Room].Id==Route.TargetRoom;
    }
}
FVector UDeliveryNavigationSubsystem::GetRouteGroundPosition(FVector Position) const
{
    // Use the same ground position for the distance, deviation and UI arrow.
    if(const auto* PC=GetWorld()->GetFirstPlayerController())
    {
        if(const auto* Character=Cast<ACharacter>(PC->GetPawn()))
        { Position.Z-=Character->GetCapsuleComponent()->GetScaledCapsuleHalfHeight(); return Position; }
        if(Cast<AScooterPawn>(PC->GetPawn())) { Position.Z=0; return Position; }
    }
    // Interior journey callers without a possessed pawn use standing capsule centers.
    if(Route.bHasInteriors) Position.Z-=88.;
    return Position;
}

double UDeliveryNavigationSubsystem::DistanceFromRoute(FVector Position) const
{
    const auto Projection=FNavigationRouteProjection::FromRoute(Route,GetRouteGroundPosition(Position),RouteSegment);
    return Projection.Segment==INDEX_NONE?MAX_dbl:Projection.DistanceCentimeters;
}
void UDeliveryNavigationSubsystem::UpdateNavigation(FVector Position,double Delta)
{
    if (!Delivery || Position.ContainsNaN() || !FMath::IsFinite(Delta)) return;
    if(Mode==ECourierNavigationMode::ParkedBike && (!IsValid(Delivery->GetScooter()) || Delivery->GetScooter()->GetRider())) Mode=ECourierNavigationMode::Order;
    if (ResolveTarget()!=Target) { RefreshRoute(Position); return; }
    // Only route planning is throttled; distance and arrival follow every frame.
    UpdateRemaining(Position);
    CheckSeconds+=FMath::Max(0.0,Delta);
    if (CheckSeconds<.5) return;
    CheckSeconds=0;
    const bool BikeMoved=Mode==ECourierNavigationMode::ParkedBike && IsValid(Delivery->GetScooter()) && FVector::Distance(TargetPosition,Delivery->GetScooter()->GetActorLocation())>200;
    bool RoomChanged=false;
    if(const auto* Interiors=GetWorld()->GetSubsystem<UCourierBuildingSubsystem>(); Interiors && Interiors->IsEnabled())
    { const auto A=Interiors->Locate(Position); const FName Room=A.IsValid()?Interiors->GetCatalog()->Buildings[A.Building].Rooms[A.Room].Id:NAME_None; RoomChanged=Room!=OriginRoom; }
    if (!Target.IsNone() && (!Route.bReachable || DistanceFromRoute(Position)>250 || BikeMoved || RoomChanged)) RefreshRoute(Position);
}
bool UDeliveryNavigationSubsystem::IsRouteClear(const FRouteResult& Candidate) const
{
    if(!Candidate.bReachable) return false;
    if(!Candidate.bHasInteriors) return IsRouteClear(Candidate.Points);
    if(Candidate.SegmentKinds.Num()!=Candidate.Points.Num()-1) return false;
    for(int32 I=0;I<Candidate.SegmentKinds.Num();++I)
        if(Candidate.SegmentKinds[I]==TEXT("road") && !IsRouteClear({Candidate.Points[I],Candidate.Points[I+1]})) return false;
    return true;
}
bool UDeliveryNavigationSubsystem::IsRouteClear(const TArray<FVector>& Points) const
{
    FCollisionQueryParams Params(SCENE_QUERY_STAT(CourierNavigation),false);
    for (TActorIterator<APawn> It(GetWorld()); It; ++It) Params.AddIgnoredActor(*It);
      // Road estimates exclude moving population; solid city obstacles still block connectors.
      for (TActorIterator<AAmbientVehicle> It(GetWorld()); It; ++It) Params.AddIgnoredActor(*It);
    for(int32 I=1;I<Points.Num();++I)
    {
        FVector A=Points[I-1],B=Points[I]; A.Z=FMath::Min(A.Z,B.Z)+100; B.Z=A.Z;
          FHitResult Hit;
          if(GetWorld()->LineTraceSingleByChannel(Hit,A,B,ECC_Visibility,Params))
          {
              UE_LOG(LogTemp,Verbose,TEXT("Navigation blocked by %s at %s, segment %s -> %s"),*GetNameSafe(Hit.GetActor()),*Hit.Location.ToString(),*A.ToString(),*B.ToString());
              return false;
          }
    }
    return Points.Num()>1;
}
