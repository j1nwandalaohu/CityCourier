#pragma once
#include "Core/DeliveryTypes.h"

/** Pure rules. World integration must synchronize elapsed time before interactions. */
struct CITYCOURIER_API FDeliveryRules
{
    static bool Accept(FDeliveryState& State, const FDeliveryOffer& Offer);
    static bool Pickup(FDeliveryState& State, FName MerchantId);
    static void Advance(FDeliveryState& State, double DeltaSeconds);
    static void ApplyImpact(FDeliveryState& State, double SpeedKmh);
    static bool Deliver(FDeliveryState& State, FName DestinationId, int64& OutPayoutCents);
    static bool Cancel(FDeliveryState& State, EDeliveryFailure Reason);
    static double CalculateDeadline(double RouteMeters);
};
