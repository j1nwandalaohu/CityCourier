#include "Profile/ProfileRules.h"

int64 FProfileRules::GetUpgradeCost(EUpgradeKind Kind, int32 Level)
{
    if (Level < 0 || Level >= 2) return -1;
    switch (Kind)
    {
    case EUpgradeKind::Power: return Level == 0 ? 6000 : 12000;
    case EUpgradeKind::Handling: return Level == 0 ? 4000 : 8000;
    case EUpgradeKind::Battery: return Level == 0 ? 5000 : 10000;
    default: return -1;
    }
}

bool FProfileRules::Validate(const FPlayerProfile& P)
{
    return P.Version == 1 && P.BalanceCents >= 0 && P.TotalEarnedCents >= 0 &&
        P.CompletedCount >= 0 && P.FailedCount >= 0 &&
        P.PowerLevel >= 0 && P.PowerLevel <= 2 && P.HandlingLevel >= 0 && P.HandlingLevel <= 2 &&
        P.BatteryLevel >= 0 && P.BatteryLevel <= 2 && FMath::IsFinite(P.MasterVolume) &&
        P.MasterVolume >= 0 && P.MasterVolume <= 1 && P.QualityLevel >= 0 && P.QualityLevel <= 2;
}
