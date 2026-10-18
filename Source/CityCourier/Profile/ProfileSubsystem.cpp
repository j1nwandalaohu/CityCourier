#include "Profile/ProfileSubsystem.h"
#include "Profile/ProfileRules.h"
#include "Misc/Paths.h"

void UProfileSubsystem::Initialize(FSubsystemCollectionBase& Collection)
{
    Super::Initialize(Collection); LoadProfile();
}
void UProfileSubsystem::Deinitialize()
{
    if (bLoaded && !bNeedsReset) SaveProfile();
    Super::Deinitialize();
}
#if WITH_DEV_AUTOMATION_TESTS
void UProfileSubsystem::SetStorageForTests(TSharedRef<IProfileStorage> Storage)
{
    check(!bLoaded); Repository = MakeUnique<FProfileRepository>(Storage);
}
#endif
EProfileLoadResult UProfileSubsystem::LoadProfile()
{
    if (bLoaded) return LoadResult;
    if (!Repository) Repository = MakeUnique<FProfileRepository>(MakeShared<FFileProfileStorage>(
        FPaths::Combine(FPaths::ProjectSavedDir(), TEXT("SaveGames"))));
    bool bMigrated=false; LoadResult = Repository->Load(Profile, LastError,&bMigrated); bLoaded = true;
    bNeedsReset = LoadResult == EProfileLoadResult::NeedsReset;
    if (bMigrated || LoadResult == EProfileLoadResult::NewProfile || LoadResult == EProfileLoadResult::RecoveredBackup)
    {
        bDirty = true; SaveProfile();
    }
    OnProfileChanged.Broadcast(); return LoadResult;
}
bool UProfileSubsystem::ResetProfileAfterConfirmation()
{
    if (!bLoaded || !bNeedsReset) return false;
    FPlayerProfile Fresh;
    if (!Repository->Save(Fresh, LastError)) { OnSaveFailed.Broadcast(LastError); return false; }
    Profile = Fresh; SettledInstances.Empty(); bNeedsReset = false; bDirty = false;
    LoadResult = EProfileLoadResult::Loaded; OnProfileChanged.Broadcast(); return true;
}
void UProfileSubsystem::Changed()
{
    bDirty = true; SaveProfile(); OnProfileChanged.Broadcast();
}
bool UProfileSubsystem::ApplySettlement(const FSettlement& S)
{
    if (!bLoaded || bNeedsReset || !S.InstanceId.IsValid() || SettledInstances.Contains(S.InstanceId) ||
        S.PayoutCents < 0 || S.PayoutCents > 3200 || (!S.bCompleted && S.PayoutCents != 0)) return false;
    if(!FOrderHistoryRules::Validate(S.History) || S.History.InstanceId!=S.InstanceId || S.History.bCompleted!=S.bCompleted ||
       S.History.PayoutCents!=S.PayoutCents || Profile.OrderHistory.ContainsByPredicate([&](const auto& H){return H.InstanceId==S.InstanceId;})) return false;
    if (S.bCompleted)
    {
        if (Profile.CompletedCount == MAX_int32 || Profile.BalanceCents > MAX_int64 - S.PayoutCents ||
            Profile.TotalEarnedCents > MAX_int64 - S.PayoutCents) return false;
        Profile.BalanceCents += S.PayoutCents; Profile.TotalEarnedCents += S.PayoutCents; ++Profile.CompletedCount;
    }
    else { if (Profile.FailedCount == MAX_int32) return false; ++Profile.FailedCount; }
    Profile.OrderHistory.Insert(S.History,0); if(Profile.OrderHistory.Num()>20) Profile.OrderHistory.SetNum(20);
    SettledInstances.Add(S.InstanceId); Changed(); return true;
}
bool UProfileSubsystem::TryPurchase(EUpgradeKind Kind)
{
    if (!bLoaded || bNeedsReset) return false;
    int32* Level = nullptr;
    switch (Kind)
    {
    case EUpgradeKind::Power: Level = &Profile.PowerLevel; break;
    case EUpgradeKind::Handling: Level = &Profile.HandlingLevel; break;
    case EUpgradeKind::Battery: Level = &Profile.BatteryLevel; break;
    default: return false;
    }
    const int64 Cost = FProfileRules::GetUpgradeCost(Kind, *Level);
    if (Cost < 0 || Profile.BalanceCents < Cost) return false;
    Profile.BalanceCents -= Cost; ++*Level; Changed(); return true;
}
bool UProfileSubsystem::SaveProfile()
{
    if (!bLoaded || bNeedsReset) return false;
    if (!Repository->Save(Profile, LastError)) { bDirty = true; OnSaveFailed.Broadcast(LastError); return false; }
    bDirty = false; return true;
}
bool UProfileSubsystem::UpdateSettings(float Volume, int32 Quality)
{
    if (!bLoaded || bNeedsReset || !FMath::IsFinite(Volume) || Volume < 0 || Volume > 1 || Quality < 0 || Quality > 2) return false;
    Profile.MasterVolume = Volume; Profile.QualityLevel = Quality; Changed(); return true;
}
