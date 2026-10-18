#if WITH_DEV_AUTOMATION_TESTS
#include "Misc/AutomationTest.h"
#include "Engine/GameInstance.h"
#include "Profile/ProfileSubsystem.h"
#include "Profile/ProfileRules.h"
#include "Tests/ProfileTestStorage.h"
#include "Tests/HistoryTestSupport.h"
#include <limits>

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FProfileTransactionsTest, "CityCourier.Profile.Transactions",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FProfileTransactionsTest::RunTest(const FString& Parameters)
{
    auto Storage = MakeShared<FTestProfileStorage>();
    auto* Profile = NewObject<UProfileSubsystem>(NewObject<UGameInstance>());
    Profile->SetStorageForTests(Storage);
    TestTrue(TEXT("First launch"), Profile->LoadProfile() == EProfileLoadResult::NewProfile);
    TestEqual(TEXT("Zero balance"), Profile->GetProfile().BalanceCents, int64(0));
    TestEqual(TEXT("Default volume"), Profile->GetProfile().MasterVolume, 0.8f);
    TestEqual(TEXT("Default medium quality"), Profile->GetProfile().QualityLevel, 1);
    FSettlement Completed=MakeTestSettlement(FGuid::NewGuid(), true, 2000);
    TestTrue(TEXT("Settlement applied"), Profile->ApplySettlement(Completed));
    TestFalse(TEXT("Duplicate settlement rejected"), Profile->ApplySettlement(Completed));
    TestEqual(TEXT("Money once"), Profile->GetProfile().BalanceCents, int64(2000));
    TestEqual(TEXT("Completion once"), Profile->GetProfile().CompletedCount, 1);
    FSettlement Failed=MakeTestSettlement(FGuid::NewGuid(), false, 0);
    TestTrue(TEXT("Failure counted"), Profile->ApplySettlement(Failed));
    TestFalse(TEXT("Failure once only"), Profile->ApplySettlement(Failed));
    TestEqual(TEXT("Failure count"), Profile->GetProfile().FailedCount, 1);
    TestEqual(TEXT("Failure does not charge"), Profile->GetProfile().BalanceCents, int64(2000));
    for (const FSettlement& Invalid : {FSettlement{FGuid(), true, 2000}, {FGuid::NewGuid(), false, 1},
                                       {FGuid::NewGuid(), true, -1}, {FGuid::NewGuid(), true, 3201}})
        TestFalse(TEXT("Malformed settlement rejected"), Profile->ApplySettlement(Invalid));
    TestFalse(TEXT("Cannot afford power"), Profile->TryPurchase(EUpgradeKind::Power));
    Profile->ApplySettlement(MakeTestSettlement(FGuid::NewGuid(), true, 2000));
    Profile->ApplySettlement(MakeTestSettlement(FGuid::NewGuid(), true, 2000));
    TestTrue(TEXT("Exact balance buys power 1"), Profile->TryPurchase(EUpgradeKind::Power));
    TestEqual(TEXT("Exact balance now zero"), Profile->GetProfile().BalanceCents, int64(0));
    TestEqual(TEXT("Power level one"), Profile->GetProfile().PowerLevel, 1);
    for (int32 I = 0; I < 6; ++I) Profile->ApplySettlement(MakeTestSettlement(FGuid::NewGuid(), true, 2000));
    TestTrue(TEXT("Power two"), Profile->TryPurchase(EUpgradeKind::Power));
    TestEqual(TEXT("Power two costs 12000"), Profile->GetProfile().BalanceCents, int64(0));
    TestFalse(TEXT("Max level no purchase"), Profile->TryPurchase(EUpgradeKind::Power));
    for (EUpgradeKind Kind : {EUpgradeKind::Handling, EUpgradeKind::Battery})
    {
        const int64 Expected[] = {Kind == EUpgradeKind::Handling ? 4000 : 5000, Kind == EUpgradeKind::Handling ? 8000 : 10000};
        for (int32 Level = 0; Level < 2; ++Level)
        {
            TestEqual(TEXT("Published upgrade price"), FProfileRules::GetUpgradeCost(Kind, Level), Expected[Level]);
            while (Profile->GetProfile().BalanceCents < Expected[Level])
                Profile->ApplySettlement(MakeTestSettlement(FGuid::NewGuid(), true, 2000));
            const int64 Before = Profile->GetProfile().BalanceCents;
            TestTrue(TEXT("Purchase upgrade"), Profile->TryPurchase(Kind));
            TestEqual(TEXT("Exact upgrade charge"), Profile->GetProfile().BalanceCents, Before - Expected[Level]);
        }
        TestFalse(TEXT("Full upgrade rejected"), Profile->TryPurchase(Kind));
    }
    TestEqual(TEXT("Invalid kind price"), FProfileRules::GetUpgradeCost(static_cast<EUpgradeKind>(99), 0), int64(-1));
    TestEqual(TEXT("Invalid level price"), FProfileRules::GetUpgradeCost(EUpgradeKind::Power, -1), int64(-1));
    TestTrue(TEXT("Valid settings"), Profile->UpdateSettings(0.4f, 2));
    TestFalse(TEXT("Bad volume rejected"), Profile->UpdateSettings(std::numeric_limits<float>::quiet_NaN(), 1));
    TestFalse(TEXT("Bad quality rejected"), Profile->UpdateSettings(0.5f, 3));
    TestEqual(TEXT("Settings unchanged after rejection"), Profile->GetProfile().MasterVolume, 0.4f);
    auto* Restarted = NewObject<UProfileSubsystem>(NewObject<UGameInstance>());
    Restarted->SetStorageForTests(Storage);
    TestTrue(TEXT("Restart loads"), Restarted->LoadProfile() == EProfileLoadResult::Loaded);
    TestEqual(TEXT("Restart keeps income"), Restarted->GetProfile().TotalEarnedCents, Profile->GetProfile().TotalEarnedCents);
    TestEqual(TEXT("Restart keeps upgrades"), Restarted->GetProfile().BatteryLevel, 2);
    return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FProfileSaveRetryTest, "CityCourier.Profile.SaveRetry",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FProfileSaveRetryTest::RunTest(const FString& Parameters)
{
    auto Storage = MakeShared<FTestProfileStorage>();
    auto* Profile = NewObject<UProfileSubsystem>(NewObject<UGameInstance>());
    Profile->SetStorageForTests(Storage); Profile->LoadProfile();
    int32 Failures = 0;
    Profile->OnSaveFailed.AddLambda([&Failures](const FString&) { ++Failures; });
    Storage->FailWrite = FProfileRepository::PrimaryName;
    const FSettlement Settlement=MakeTestSettlement(FGuid::NewGuid(), true, 2000);
    TestTrue(TEXT("Accepted transaction remains in memory despite disk failure"), Profile->ApplySettlement(Settlement));
    TestTrue(TEXT("Failed save visible"), Profile->HasUnsavedChanges() && Failures == 1);
    TestEqual(TEXT("Memory retains income"), Profile->GetProfile().BalanceCents, int64(2000));
    TestFalse(TEXT("Retry transaction rejected"), Profile->ApplySettlement(Settlement));
    Storage->FailWrite.Empty();
    TestTrue(TEXT("Retry only persistence"), Profile->SaveProfile());
    TestFalse(TEXT("Saved flag cleared"), Profile->HasUnsavedChanges());
    FProfileRepository Repository(Storage); FPlayerProfile Loaded; FString Error;
    TestTrue(TEXT("Saved retry loads"), Repository.Load(Loaded, Error) == EProfileLoadResult::Loaded);
    TestEqual(TEXT("No duplicate after save retry"), Loaded.BalanceCents, int64(2000));
    Storage->Files[FProfileRepository::PrimaryName] = {1, 2, 3};
    Storage->Files.Remove(FProfileRepository::BackupName);
    auto* Corrupt = NewObject<UProfileSubsystem>(NewObject<UGameInstance>());
    Corrupt->SetStorageForTests(Storage);
    TestTrue(TEXT("Corruption requires choice"), Corrupt->LoadProfile() == EProfileLoadResult::NeedsReset);
    TestFalse(TEXT("No automatic overwrite"), Corrupt->SaveProfile());
    TestFalse(TEXT("No transactions before reset"), Corrupt->ApplySettlement({FGuid::NewGuid(), true, 2000}));
    TestTrue(TEXT("Explicit reset creates default"), Corrupt->ResetProfileAfterConfirmation());
    TestEqual(TEXT("Reset zero"), Corrupt->GetProfile().BalanceCents, int64(0));
    TestTrue(TEXT("Bad bytes preserved"), Storage->Preserved.Num() > 0);
    return true;
}
#endif
