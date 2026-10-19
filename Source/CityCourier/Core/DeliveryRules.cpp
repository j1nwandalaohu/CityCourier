#include "Core/DeliveryRules.h"

namespace
{
bool ExpireIfDue(FDeliveryState& State)
{
    if (State.IsActive() && State.ElapsedSeconds >= State.Offer.DeadlineSeconds + 60.0)
    {
        State.Status = EDeliveryStatus::Failed;
        State.Failure = EDeliveryFailure::Expired;
        return true;
    }
    return false;
}
}

bool FDeliveryRules::Accept(FDeliveryState& State, const FDeliveryOffer& Offer)
{
    if (State.IsActive() || !Offer.InstanceId.IsValid() || Offer.InstanceId == State.Offer.InstanceId ||
        Offer.MerchantId.IsNone() || Offer.DestinationId.IsNone() ||
        Offer.BaseFeeCents < 1600 || Offer.BaseFeeCents > 3200 ||
        !FMath::IsFinite(Offer.DeadlineSeconds) || Offer.DeadlineSeconds < 120.0 || Offer.DeadlineSeconds > 300.0)
    {
        return false;
    }
    State = FDeliveryState{};
    State.Offer = Offer;
    State.Status = EDeliveryStatus::AwaitingPickup;
    return true;
}

bool FDeliveryRules::Pickup(FDeliveryState& State, FName MerchantId)
{
    ExpireIfDue(State);
    if (State.Status != EDeliveryStatus::AwaitingPickup || MerchantId != State.Offer.MerchantId) return false;
    State.Status = EDeliveryStatus::Delivering;
    State.IntegrityPercent = 100;
    return true;
}

void FDeliveryRules::Advance(FDeliveryState& State, double DeltaSeconds)
{
    if (!State.IsActive() || !FMath::IsFinite(DeltaSeconds) || DeltaSeconds < 0.0) return;
    // Cap before addition so a very large finite delta cannot overflow the clock.
    const double End = State.Offer.DeadlineSeconds + 60.0;
    State.ElapsedSeconds += FMath::Min(DeltaSeconds, FMath::Max(0.0, End - State.ElapsedSeconds));
    ExpireIfDue(State);
}

void FDeliveryRules::ApplyImpact(FDeliveryState& State, double SpeedKmh)
{
    ExpireIfDue(State);
    if (State.Status != EDeliveryStatus::Delivering || !FMath::IsFinite(SpeedKmh) || SpeedKmh < 8.0 ||
        State.ElapsedSeconds - State.LastDamageElapsedSeconds < 1.0) return;
    const int32 Damage = FMath::Clamp(FMath::RoundToInt((FMath::Min(SpeedKmh, 28.0) - 8.0) * 1.5), 1, 30);
    State.IntegrityPercent = FMath::Max(0, State.IntegrityPercent - Damage);
    State.LastDamageElapsedSeconds = State.ElapsedSeconds;
    if (State.IntegrityPercent == 0)
    {
        State.Status = EDeliveryStatus::Failed;
        State.Failure = EDeliveryFailure::Damaged;
    }
}

bool FDeliveryRules::Deliver(FDeliveryState& State, FName DestinationId, int64& OutPayoutCents)
{
    OutPayoutCents = 0;
    ExpireIfDue(State);
    if (State.Status != EDeliveryStatus::Delivering || DestinationId != State.Offer.DestinationId) return false;
    const int64 TimePercent = State.ElapsedSeconds >= State.Offer.DeadlineSeconds ? 60 : 100;
    OutPayoutCents = State.Offer.BaseFeeCents * State.IntegrityPercent * TimePercent / 10000;
    State.Status = EDeliveryStatus::Completed;
    return true;
}

bool FDeliveryRules::Cancel(FDeliveryState& State, EDeliveryFailure Reason)
{
    ExpireIfDue(State);
    if (!State.IsActive() || (Reason != EDeliveryFailure::Cancelled && Reason != EDeliveryFailure::Rescue)) return false;
    State.Status = EDeliveryStatus::Failed;
    State.Failure = Reason;
    return true;
}

double FDeliveryRules::CalculateDeadline(double RouteMeters)
{
    if (!FMath::IsFinite(RouteMeters) || RouteMeters < 0.0) return -1.0;
    return FMath::Clamp(FMath::CeilToDouble(90.0 + FMath::Min(RouteMeters, 1050.0) / 5.0), 120.0, 300.0);
}
double FDeliveryRules::CalculateDeadline(const FRouteResult& A,const FRouteResult& B)
{
    if(!A.bReachable || !B.bReachable) return -1;
    for(const auto* R:{&A,&B}) for(double M:{R->RoadMeters,R->IndoorFlatMeters,R->StairMeters})
        if(!FMath::IsFinite(M) || M<0) return -1;
    return FMath::Clamp(FMath::CeilToDouble(90+(A.RoadMeters+B.RoadMeters)/5+(A.IndoorFlatMeters+B.IndoorFlatMeters)/2.5+
        (A.StairMeters+B.StairMeters)/1.5+(A.RequiredDoors.Num()+B.RequiredDoors.Num())*2),120.,300.);
}
