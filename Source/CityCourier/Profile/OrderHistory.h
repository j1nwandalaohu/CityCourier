#pragma once
#include "CoreMinimal.h"
#include "Core/DeliveryTypes.h"
#include "OrderHistory.generated.h"

USTRUCT()
struct CITYCOURIER_API FOrderHistoryEntry
{
    GENERATED_BODY()
    UPROPERTY(SaveGame) FGuid InstanceId;
    UPROPERTY(SaveGame) FString MerchantName;
    UPROPERTY(SaveGame) FString DestinationName;
    UPROPERTY(SaveGame) FName MerchantId;
    UPROPERTY(SaveGame) FName DestinationId;
    UPROPERTY(SaveGame) FDateTime SettledAtUtc;
    UPROPERTY(SaveGame) bool bCompleted=false;
    UPROPERTY(SaveGame) bool bLate=false;
    UPROPERTY(SaveGame) uint8 FailureCode=0;
    UPROPERTY(SaveGame) int64 BaseFeeCents=0;
    UPROPERTY(SaveGame) int64 PayoutCents=0;
    UPROPERTY(SaveGame) double ElapsedSeconds=0;
    UPROPERTY(SaveGame) double DeadlineSeconds=0;
    UPROPERTY(SaveGame) int32 IntegrityPercent=100;
    bool operator==(const FOrderHistoryEntry&) const = default;
};

struct CITYCOURIER_API FOrderHistoryRules
{
    static FOrderHistoryEntry MakeEntry(const FDeliveryState&,int64,const FString&,const FString&,FDateTime);
    static bool Validate(const FOrderHistoryEntry&);
    static FText DescribeIncome(const FOrderHistoryEntry&);
    static FText DescribeOutcome(const FOrderHistoryEntry&);
};
