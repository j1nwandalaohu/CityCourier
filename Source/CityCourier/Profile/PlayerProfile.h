#pragma once
#include "CoreMinimal.h"
#include "Profile/OrderHistory.h"
#include "PlayerProfile.generated.h"

enum class EUpgradeKind : uint8 { Power, Handling, Battery };
enum class EProfileLoadResult : uint8 { NewProfile, Loaded, RecoveredBackup, NeedsReset };

USTRUCT()
struct CITYCOURIER_API FPlayerProfile
{
    GENERATED_BODY()
    UPROPERTY(SaveGame) int32 Version = 2;
    UPROPERTY(SaveGame) int64 BalanceCents = 0;
    UPROPERTY(SaveGame) int64 TotalEarnedCents = 0;
    UPROPERTY(SaveGame) int32 CompletedCount = 0;
    UPROPERTY(SaveGame) int32 FailedCount = 0;
    UPROPERTY(SaveGame) int32 PowerLevel = 0;
    UPROPERTY(SaveGame) int32 HandlingLevel = 0;
    UPROPERTY(SaveGame) int32 BatteryLevel = 0;
    UPROPERTY(SaveGame) float MasterVolume = 0.8f;
    UPROPERTY(SaveGame) int32 QualityLevel = 1;
    UPROPERTY(SaveGame) TArray<FOrderHistoryEntry> OrderHistory;
    bool operator==(const FPlayerProfile& Other) const = default;
};

struct FSettlement
{
    FGuid InstanceId;
    bool bCompleted = false;
    int64 PayoutCents = 0;
    FOrderHistoryEntry History;
};
