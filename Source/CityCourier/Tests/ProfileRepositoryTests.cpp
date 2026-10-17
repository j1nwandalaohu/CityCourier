#if WITH_DEV_AUTOMATION_TESTS
#include "Misc/AutomationTest.h"
#include "Profile/ProfileSaveGame.h"
#include "Tests/ProfileTestStorage.h"
#include "Misc/Paths.h"

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FProfileRepositoryTest, "CityCourier.ProfileStorage.Recovery",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FProfileRepositoryTest::RunTest(const FString& Parameters)
{
    auto Storage = MakeShared<FTestProfileStorage>();
    FProfileRepository Repository(Storage); FPlayerProfile P, Loaded; FString Error;
    TestTrue(TEXT("New profile"), Repository.Load(Loaded, Error) == EProfileLoadResult::NewProfile);
    P.BalanceCents = 12345; P.TotalEarnedCents = 30000; P.CompletedCount = 15; P.FailedCount = 3;
    P.PowerLevel = 1; P.HandlingLevel = 2; P.BatteryLevel = 1; P.MasterVolume = 0.3f; P.QualityLevel = 2;
    TestTrue(TEXT("Initial save"), Repository.Save(P, Error));
    TestTrue(TEXT("Round trip"), Repository.Load(Loaded, Error) == EProfileLoadResult::Loaded);
    TestTrue(TEXT("All fields round trip"), P == Loaded);
    const auto Original = Storage->Files[FProfileRepository::PrimaryName];
    P.BalanceCents += 100;
    TestTrue(TEXT("Second save"), Repository.Save(P, Error));
    TestTrue(TEXT("Previous validated bytes backed up"), Storage->Files[FProfileRepository::BackupName] == Original);
    Storage->Files[FProfileRepository::PrimaryName] = {0, 1, 2};
    TestTrue(TEXT("Corrupt primary falls back"), Repository.Load(Loaded, Error) == EProfileLoadResult::RecoveredBackup);
    TestEqual(TEXT("Previous balance recovered"), Loaded.BalanceCents, int64(12345));
    TestEqual(TEXT("Corrupt file preserved"), Storage->Preserved.Num(), 1);
    TestTrue(TEXT("Recovered save repairs primary"), Repository.Save(Loaded, Error));
    TestTrue(TEXT("Recovery keeps valid backup"), Storage->Files[FProfileRepository::BackupName] == Original);
    for (int32 BadCase = 0; BadCase < 4; ++BadCase)
    {
        auto BadStorage = MakeShared<FTestProfileStorage>(); FProfileRepository BadRepo(BadStorage);
        FPlayerProfile Bad;
        if (BadCase == 0) Bad.Version = 2;
        if (BadCase == 1) Bad.BalanceCents = -1;
        if (BadCase == 2) Bad.BatteryLevel = 3;
        TArray<uint8> Bytes; UProfileSaveGame::Encode(Bad, Bytes);
        if (BadCase == 3) Bytes = {8, 9};
        BadStorage->Files.Add(FProfileRepository::PrimaryName, Bytes);
        BadStorage->Files.Add(FProfileRepository::BackupName, TArray<uint8>{7, 6});
        TestTrue(TEXT("No valid snapshot requires reset"), BadRepo.Load(Loaded, Error) == EProfileLoadResult::NeedsReset);
        TestFalse(TEXT("Error explains reset"), Error.IsEmpty());
        TestEqual(TEXT("Both bad originals preserved"), BadStorage->Preserved.Num(), 2);
        FProfileRepository Restart(BadStorage);
        TestTrue(TEXT("Restart must not silently bypass reset choice"), Restart.Load(Loaded, Error) == EProfileLoadResult::NeedsReset);
    }
    return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FProfileStorageFaultsTest, "CityCourier.ProfileStorage.Faults",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FProfileStorageFaultsTest::RunTest(const FString& Parameters)
{
    for (int32 Failure = 0; Failure < 4; ++Failure)
    {
        auto Storage = MakeShared<FTestProfileStorage>(); FProfileRepository Repository(Storage);
        FPlayerProfile P, Loaded; FString Error; P.BalanceCents = 100;
        Repository.Save(P, Error);
        const auto Before = Storage->Files[FProfileRepository::PrimaryName];
        if (Failure == 0) Storage->FailWrite = FProfileRepository::PrimaryName;
        if (Failure == 1) Storage->FailReplace = FProfileRepository::PrimaryName;
        if (Failure == 2) Storage->FailRead = FProfileRepository::PrimaryName;
        if (Failure == 3) Storage->FailWrite = FProfileRepository::BackupName;
        P.BalanceCents = 200;
        TestFalse(TEXT("Save reports storage failure"), Repository.Save(P, Error));
        TestFalse(TEXT("Storage failure message present"), Error.IsEmpty());
        TestTrue(TEXT("Old primary still intact"), Storage->Files[FProfileRepository::PrimaryName] == Before);
        Storage->FailRead.Empty(); Storage->FailWrite.Empty(); Storage->FailReplace.Empty();
        TestTrue(TEXT("Old progress recoverable"), Repository.Load(Loaded, Error) == EProfileLoadResult::Loaded);
        TestEqual(TEXT("Old balance unchanged"), Loaded.BalanceCents, int64(100));
    }
    auto Storage = MakeShared<FTestProfileStorage>(); FProfileRepository Repository(Storage);
    Storage->Files.Add(FProfileRepository::PrimaryName, TArray<uint8>{1}); Storage->bFailPreserve = true;
    FPlayerProfile P; FString Error;
    TestTrue(TEXT("Preserve failure requires reset"), Repository.Load(P, Error) == EProfileLoadResult::NeedsReset);
    TestFalse(TEXT("Cannot overwrite unpreserved corrupt file"), Repository.Save(P, Error));
    TestTrue(TEXT("Original remains"), Storage->Files.Contains(FProfileRepository::PrimaryName));
    return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FProfileDiskRoundtripTest, "CityCourier.ProfileStorage.RealDisk",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FProfileDiskRoundtripTest::RunTest(const FString& Parameters)
{
    // Unique test directory, never the player's real SaveGames folder.
    auto Storage = MakeShared<FFileProfileStorage>(FPaths::Combine(FPaths::ProjectSavedDir(), TEXT("Tests"), FGuid::NewGuid().ToString()));
    FProfileRepository Repository(Storage); FPlayerProfile P, Loaded; FString Error;
    P.BalanceCents = 2000;
    TestTrue(TEXT("Real atomic file write"), Repository.Save(P, Error));
    P.BalanceCents = 4000;
    TestTrue(TEXT("Real replacement"), Repository.Save(P, Error));
    TestTrue(TEXT("Real file loads"), Repository.Load(Loaded, Error) == EProfileLoadResult::Loaded);
    TestEqual(TEXT("Latest file balance"), Loaded.BalanceCents, int64(4000));
    TestFalse(TEXT("Storage disallows path traversal"), Storage->WriteAtomic(TEXT("../outside.sav"), TArray<uint8>{1}));
    TestTrue(TEXT("Simulate corrupted primary"), Storage->WriteAtomic(FProfileRepository::PrimaryName, TArray<uint8>{1, 2}));
    TestTrue(TEXT("Real backup recovery"), Repository.Load(Loaded, Error) == EProfileLoadResult::RecoveredBackup);
    TestEqual(TEXT("Real previous balance"), Loaded.BalanceCents, int64(2000));
    return true;
}
#endif
