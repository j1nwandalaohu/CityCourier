#pragma once
#include "Profile/PlayerProfile.h"

struct CITYCOURIER_API FProfileRules
{
    static int64 GetUpgradeCost(EUpgradeKind Kind, int32 CurrentLevel);
    static bool Validate(const FPlayerProfile& Profile);
    static bool ValidateSupported(const FPlayerProfile& Profile);
    static bool UpgradeToCurrent(FPlayerProfile& Profile,bool& bMigrated);
};
