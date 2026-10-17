#pragma once
#include "Profile/PlayerProfile.h"

struct CITYCOURIER_API FProfileRules
{
    static int64 GetUpgradeCost(EUpgradeKind Kind, int32 CurrentLevel);
    static bool Validate(const FPlayerProfile& Profile);
};
