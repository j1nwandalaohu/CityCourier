#if WITH_DEV_AUTOMATION_TESTS
#include "Misc/AutomationTest.h"
#include "Tests/ProfileTestStorage.h"
#include "Profile/ProfileSaveGame.h"
#include "Profile/ProfileSubsystem.h"
#include "Misc/FileHelper.h"
#include "Misc/Paths.h"
#include "Engine/GameInstance.h"

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FProfileMigrationTest,"CityCourier.FunctionalUpgrade.Migration.LegacyPrimaryBackupAndRetry",EAutomationTestFlags::EditorContext|EAutomationTestFlags::EngineFilter)
bool FProfileMigrationTest::RunTest(const FString&)
{
    TArray<uint8> Old; if(!TestTrue(TEXT("Read true v1 fixture"),FFileHelper::LoadFileToArray(Old,*(FPaths::ProjectDir()/TEXT("Source/CityCourier/Tests/Fixtures/ProfileV1.sav"))))) return false;
    for(int32 Case=0;Case<3;++Case)
    {
        auto Storage=MakeShared<FTestProfileStorage>(); Storage->Files.Add(Case==1?FProfileRepository::BackupName:FProfileRepository::PrimaryName,Old);
        if(Case==2) Storage->FailWrite=FProfileRepository::PrimaryName;
        auto* P=NewObject<UProfileSubsystem>(NewObject<UGameInstance>()); P->SetStorageForTests(Storage); P->LoadProfile();
        const auto& V=P->GetProfile(); TestFalse(TEXT("Migration never requests reset"),P->NeedsReset()); TestEqual(TEXT("Current version"),V.Version,2);
        TestEqual(TEXT("Balance"),V.BalanceCents,int64(12345)); TestEqual(TEXT("Lifetime income"),V.TotalEarnedCents,int64(30000));
        TestEqual(TEXT("Completions"),V.CompletedCount,15); TestEqual(TEXT("Failures"),V.FailedCount,3);
        TestEqual(TEXT("Power"),V.PowerLevel,1); TestEqual(TEXT("Handling"),V.HandlingLevel,2); TestEqual(TEXT("Battery"),V.BatteryLevel,1);
        TestEqual(TEXT("Volume"),V.MasterVolume,.3f); TestEqual(TEXT("Quality"),V.QualityLevel,2); TestEqual(TEXT("No invented old history"),V.OrderHistory.Num(),0);
        if(Case==2) { TestTrue(TEXT("Failed migration save remains dirty"),P->HasUnsavedChanges()); TestTrue(TEXT("Original preserved"),Storage->Files[FProfileRepository::PrimaryName]==Old); Storage->FailWrite.Empty(); TestTrue(TEXT("Migration retry"),P->SaveProfile()); }
        if(Case!=1) TestTrue(TEXT("Original bytes backed up"),Storage->Files.Contains(FProfileRepository::BackupName) && Storage->Files[FProfileRepository::BackupName]==Old);
        FPlayerProfile Saved; TestTrue(TEXT("New bytes decode"),UProfileSaveGame::Decode(Storage->Files[FProfileRepository::PrimaryName],Saved)); TestEqual(TEXT("New bytes v2"),Saved.Version,2);
    }
    return true;
}
IMPLEMENT_SIMPLE_AUTOMATION_TEST(FFutureProfileTest,"CityCourier.FunctionalUpgrade.Migration.FutureVersionProtected",EAutomationTestFlags::EditorContext|EAutomationTestFlags::EngineFilter)
bool FFutureProfileTest::RunTest(const FString&)
{
    auto S=MakeShared<FTestProfileStorage>(); FPlayerProfile Future; Future.Version=999; Future.BalanceCents=9999;
    TArray<uint8> Bytes,Backup; UProfileSaveGame::Encode(Future,Bytes); UProfileSaveGame::Encode(FPlayerProfile{},Backup);
    S->Files.Add(FProfileRepository::PrimaryName,Bytes); S->Files.Add(FProfileRepository::BackupName,Backup);
    auto* P=NewObject<UProfileSubsystem>(NewObject<UGameInstance>()); P->SetStorageForTests(S); P->LoadProfile();
    TestTrue(TEXT("Future version needs explicit choice"),P->NeedsReset()); TestFalse(TEXT("Cannot auto-save over future version"),P->SaveProfile());
    TestTrue(TEXT("Future primary untouched"),S->Files.Contains(FProfileRepository::PrimaryName) && S->Files[FProfileRepository::PrimaryName]==Bytes);
    return true;
}
#endif
