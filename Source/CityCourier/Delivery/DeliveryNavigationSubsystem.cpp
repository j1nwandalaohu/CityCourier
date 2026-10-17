#include "Delivery/DeliveryNavigationSubsystem.h"
#include "Delivery/DeliverySubsystem.h"
#include "World/CityDefinition.h"
#include "GameFramework/PlayerController.h"
#include "EngineUtils.h"
#include "GameFramework/Pawn.h"
void UDeliveryNavigationSubsystem::Tick(float Delta)
{
    if (const auto* PC=GetWorld()->GetFirstPlayerController()) if (const APawn* Pawn=PC->GetPawn()) UpdateNavigation(Pawn->GetActorLocation(),Delta);
}
bool UDeliveryNavigationSubsystem::RefreshRoute(FVector Position)
{
    Route={}; Target=Delivery ? Delivery->GetTargetId() : NAME_None; CheckSeconds=0;
    if (!Delivery || !Delivery->GetCity() || Target.IsNone() || Position.ContainsNaN()) return false;
    Route=Delivery->GetCity()->FindRouteFrom(Position,Target);
    if (Route.bReachable && !IsRouteClear(Route.Points)) Route={};
    return Route.bReachable;
}
double UDeliveryNavigationSubsystem::DistanceFromRoute(FVector Position) const
{
    double Distance=MAX_dbl; Position.Z=0;
    for(int32 I=1;I<Route.Points.Num();++I)
    {
        FVector A=Route.Points[I-1],B=Route.Points[I]; A.Z=B.Z=0;
        Distance=FMath::Min(Distance,FVector::Distance(Position,FMath::ClosestPointOnSegment(Position,A,B)));
    }
    return Distance;
}
void UDeliveryNavigationSubsystem::UpdateNavigation(FVector Position,double Delta)
{
    if (!Delivery) return;
    if (Delivery->GetTargetId()!=Target) { RefreshRoute(Position); return; }
    CheckSeconds+=FMath::Max(0.0,Delta);
    if (CheckSeconds<.5) return;
    CheckSeconds=0;
    if (!Target.IsNone() && (!Route.bReachable || DistanceFromRoute(Position)>1500)) RefreshRoute(Position);
}
bool UDeliveryNavigationSubsystem::IsRouteClear(const TArray<FVector>& Points) const
{
    FCollisionQueryParams Params(SCENE_QUERY_STAT(CourierNavigation),false);
    for (TActorIterator<APawn> It(GetWorld()); It; ++It) Params.AddIgnoredActor(*It);
    for(int32 I=1;I<Points.Num();++I)
    {
        FVector A=Points[I-1],B=Points[I]; A.Z=FMath::Min(A.Z,B.Z)+100; B.Z=A.Z;
        if(GetWorld()->LineTraceTestByChannel(A,B,ECC_Visibility,Params)) return false;
    }
    return Points.Num()>1;
}
