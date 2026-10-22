#if WITH_DEV_AUTOMATION_TESTS
#include "Misc/AutomationTest.h"
#include "Player/Epic/CourierEpicAnimationConfig.h"
#include "Player/CourierCharacter.h"
#include "Tests/WorldTestSupport.h"
#include "Components/SkeletalMeshComponent.h"
#if WITH_EDITOR
#include "Misc/FileHelper.h"
#include "Misc/Paths.h"
#include "Serialization/JsonReader.h"
#include "Serialization/JsonSerializer.h"
#include "AssetRegistry/AssetRegistryModule.h"
#endif

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FEpicIncompleteConfigTest,"CityCourier.EpicAnimation.Assets.IncompleteSetupRejected",EAutomationTestFlags::EditorContext|EAutomationTestFlags::EngineFilter)
bool FEpicIncompleteConfigTest::RunTest(const FString&)
{
    FTestCourierWorld F;
    auto* Rider=F.World->SpawnActor<ACourierCharacter>();
    auto* Previous=Rider->GetMesh()->GetAnimClass();
    auto* Config=NewObject<UCourierEpicAnimationConfig>();
    FText Reason;
    TestFalse(TEXT("Empty setup rejected"),Config->ValidateFor(Rider->GetMesh()->GetSkeletalMeshAsset(),Reason));
    TestFalse(TEXT("Character cannot enable incomplete setup"),Rider->TryEnableEpicAnimation(Config).bSucceeded);
    TestEqual(TEXT("Existing animation class unchanged"),Rider->GetMesh()->GetAnimClass(),Previous);
    TestFalse(TEXT("Missing setup explains failure"),Reason.IsEmpty());
    return true;
}

#if WITH_EDITOR
IMPLEMENT_SIMPLE_AUTOMATION_TEST(FEpicSourceClosureTest,"CityCourier.EpicAnimation.Assets.SourceAndDependencyClosure",EAutomationTestFlags::EditorContext|EAutomationTestFlags::EngineFilter)
bool FEpicSourceClosureTest::RunTest(const FString&)
{
    const FString SavedRoot=FPaths::ProjectSavedDir()/TEXT("Validation/EpicSamples");
    const FString Root=FPaths::FileExists(SavedRoot/TEXT("gasp.json"))?SavedRoot:FPaths::ProjectDir()/TEXT("docs/art/epic-character-samples");
    auto Read=[&](const TCHAR* Name) -> TSharedPtr<FJsonObject>
    {
        FString Text; TSharedPtr<FJsonObject> Object;
        if(!FFileHelper::LoadFileToString(Text,*(Root/Name)) || !FJsonSerializer::Deserialize(TJsonReaderFactory<>::Create(Text),Object))
            AddError(FString::Printf(TEXT("Missing or invalid Epic audit: %s"),Name));
        return Object;
    };
    TMap<FString,FString> Sources;
    for(const TCHAR* File:{TEXT("gasp.json"),TEXT("stack_o_bot.json"),TEXT("content_examples.json")})
    {
        auto Report=Read(File); if(!Report) return false;
        TestEqual(TEXT("Source UE version"),Report->GetStringField(TEXT("engine")),FString(TEXT("5.6")));
        TestTrue(TEXT("Source closure complete"),Report->GetBoolField(TEXT("closure_complete")));
        for(const auto& Value:Report->GetArrayField(TEXT("assets")))
        {
            const auto Asset=Value->AsObject();
            const FString Hash=Asset->GetStringField(TEXT("sha256"));
            TestEqual(TEXT("SHA-256 recorded"),Hash.Len(),64);
            if(Report->GetStringField(TEXT("source"))==TEXT("gasp")) Sources.Add(Asset->GetStringField(TEXT("package")),Hash);
        }
    }
    auto Migration=Read(TEXT("migration.json")); if(!Migration) return false;
    auto& Registry=FModuleManager::LoadModuleChecked<FAssetRegistryModule>(TEXT("AssetRegistry")).Get();
    for(const auto& Value:Migration->GetArrayField(TEXT("packages")))
    {
        const auto Package=Value->AsObject();
        const FString Source=Package->GetStringField(TEXT("source"));
        const FString Destination=Package->GetStringField(TEXT("destination"));
        const auto* Hash=Sources.Find(Source);
        TestTrue(TEXT("Every package has audited provenance"),Hash && *Hash==Package->GetStringField(TEXT("sha256")));
        TArray<FAssetData> Assets;
        Registry.GetAssetsByPackageName(FName(*Destination),Assets);
        TestFalse(*Destination,Assets.IsEmpty());
        TArray<FName> Dependencies;
        Registry.GetDependencies(FName(*Destination),Dependencies,UE::AssetRegistry::EDependencyCategory::Package,UE::AssetRegistry::FDependencyQuery(UE::AssetRegistry::EDependencyQuery::Hard));
        for(const FName Dependency:Dependencies)
        {
            if(!Dependency.ToString().StartsWith(TEXT("/Game/"))) continue;
            TArray<FAssetData> Found; Registry.GetAssetsByPackageName(Dependency,Found);
            TestFalse(*FString::Printf(TEXT("Hard dependency resolves: %s"),*Dependency.ToString()),Found.IsEmpty());
        }
    }
    return true;
}
#endif
#endif
