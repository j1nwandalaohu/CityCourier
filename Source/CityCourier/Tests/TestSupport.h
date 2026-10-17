#pragma once
#include "Core/DeliveryTypes.h"

namespace CourierTest
{
inline FDeliveryOffer MakeOffer(double DeadlineSeconds = 120.0, int64 FeeCents = 2000)
{
    FDeliveryOffer Offer;
    Offer.InstanceId = FGuid::NewGuid();
    Offer.MerchantId = TEXT("merchant_test");
    Offer.DestinationId = TEXT("destination_test");
    Offer.BaseFeeCents = FeeCents;
    Offer.DeadlineSeconds = DeadlineSeconds;
    return Offer;
}
}
