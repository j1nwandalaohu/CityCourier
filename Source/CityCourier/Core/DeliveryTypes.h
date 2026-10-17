#pragma once
#include "CoreMinimal.h"
#include <limits>

enum class EDeliveryStatus : uint8 { Idle, AwaitingPickup, Delivering, Completed, Failed };
enum class EDeliveryFailure : uint8 { None, Expired, Damaged, Cancelled, Rescue };

struct FDeliveryOffer
{
    FGuid InstanceId;
    FName MerchantId;
    FName DestinationId;
    int64 BaseFeeCents = 0;
    double DeadlineSeconds = 0;
};

struct FDeliveryState
{
    FDeliveryOffer Offer;
    EDeliveryStatus Status = EDeliveryStatus::Idle;
    EDeliveryFailure Failure = EDeliveryFailure::None;
    double ElapsedSeconds = 0;
    int32 IntegrityPercent = 100;
    double LastDamageElapsedSeconds = -std::numeric_limits<double>::infinity();

    bool IsActive() const
    {
        return Status == EDeliveryStatus::AwaitingPickup || Status == EDeliveryStatus::Delivering;
    }
};
