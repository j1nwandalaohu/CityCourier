#if WITH_DEV_AUTOMATION_TESTS && WITH_EDITOR
#include "Misc/AutomationTest.h"
#include "Engine/StaticMesh.h"
#include "Engine/SkeletalMesh.h"
#include "Animation/AnimSequence.h"
#include "Animation/Skeleton.h"
#include "Engine/Texture2D.h"
#include "Misc/FileHelper.h"
#include "Misc/Paths.h"
#include "Materials/Material.h"
#include "Components/SkeletalMeshComponent.h"
#include "Components/StaticMeshComponent.h"
#include "Art/CourierArtCharacter.h"
#include "Art/ScooterArtPawn.h"
#include "EngineUtils.h"
#include "FileHelpers.h"

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FCourierArtAssetsTest,"CityCourier.ArtAssets.LoadableProductionAssets",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FCourierArtAssetsTest::RunTest(const FString& Parameters)
{
    // A missing cooked mesh, broken animation skeleton or accidentally restored
    // prototype must fail independently of the generating script's exit code.
    const TCHAR* StaticAssets[] = {
        TEXT("/Game/CityCourier/Art/Scooter/SM_ScooterBody"),
        TEXT("/Game/CityCourier/Art/Scooter/SM_ScooterWheel"),
        TEXT("/Game/CityCourier/Art/Architecture/SM_ShopHouseA"),
        TEXT("/Game/CityCourier/Art/Architecture/SM_ShopHouseB"),
        TEXT("/Game/CityCourier/Art/Architecture/SM_ShopHouseC"),
        TEXT("/Game/CityCourier/Art/StreetProps/SM_PlanterTreeB"),
        TEXT("/Game/CityCourier/Art/StreetProps/SM_MarketStallProduce"),
        TEXT("/Game/CityCourier/Art/StreetProps/SM_MarketStallNoodles"),
        TEXT("/Game/CityCourier/Art/StreetProps/SM_AirConditioner")};
    for (const TCHAR* Path : StaticAssets)
    {
        const UStaticMesh* Mesh=LoadObject<UStaticMesh>(nullptr,Path);
        if (TestNotNull(Path,Mesh))
        {
            TestTrue(TEXT("Authored geometry has renderable triangles"),Mesh->GetNumTriangles(0)>100);
            TestTrue(TEXT("Authored mesh has materials"),Mesh->GetStaticMaterials().Num()>0);
            for(const auto& Slot:Mesh->GetStaticMaterials())
                TestTrue(TEXT("Static art uses project materials"),Slot.MaterialInterface && Slot.MaterialInterface->GetPathName().StartsWith(TEXT("/Game/CityCourier/Art/Materials/")));
        }
    }
    auto* Rider=LoadObject<USkeletalMesh>(nullptr,TEXT("/Game/CityCourier/Art/Characters/SK_Courier"));
    if(TestNotNull(TEXT("Rigged courier"),Rider))
    {
        // Loading the mesh can repair an unsaved skeleton in memory. A dirty
        // skeleton on a fresh test run must not mask missing persisted joints.
        if(TestNotNull(TEXT("Shared courier skeleton"),Rider->GetSkeleton()))
            TestFalse(TEXT("Shared skeleton did not require repair after load"),Rider->GetSkeleton()->GetOutermost()->IsDirty());
        TestTrue(TEXT("Human skeleton has articulated limbs"),Rider->GetRefSkeleton().GetNum()>=16);
        for(const TCHAR* Hand:{TEXT("l"),TEXT("r")})
            for(int32 Digit=1;Digit<=5;++Digit)
                for(int32 Segment=1;Segment<=3;++Segment)
                {
                    const FName Joint(*FString::Printf(TEXT("finger_%d_%d_%s"),Digit,Segment,Hand));
                    TestTrue(TEXT("Imported rider retains every finger joint"),Rider->GetRefSkeleton().FindBoneIndex(Joint)!=INDEX_NONE);
                    TestTrue(TEXT("Saved shared skeleton retains every finger joint"),Rider->GetSkeleton() && Rider->GetSkeleton()->GetReferenceSkeleton().FindBoneIndex(Joint)!=INDEX_NONE);
                }
        const float Height=Rider->GetBounds().BoxExtent.Z*2;
        TestTrue(TEXT("Rider uses real centimeter scale"),Height>=160 && Height<=205);
        for(const auto& Slot:Rider->GetMaterials())
        {
            if(TestNotNull(TEXT("Rider material assigned"),Slot.MaterialInterface.Get()))
            {
                TestTrue(TEXT("Rider material comes from project art"),Slot.MaterialInterface->GetPathName().StartsWith(TEXT("/Game/CityCourier/Art/Materials/")));
                TestTrue(TEXT("Rider material supports skeletal rendering"),Slot.MaterialInterface->GetMaterial()->GetUsageByFlag(MATUSAGE_SkeletalMesh));
            }
        }
        for(const TCHAR* Name:{TEXT("A_Idle"),TEXT("A_Walk"),TEXT("A_Run"),TEXT("A_Ride")})
        {
            auto* Anim=LoadObject<UAnimSequence>(nullptr,*FString::Printf(TEXT("/Game/CityCourier/Art/Characters/%s"),Name));
            if(TestNotNull(Name,Anim))
            {
                TestEqual(TEXT("Animation uses courier skeleton"),Anim->GetSkeleton(),Rider->GetSkeleton());
                TestTrue(TEXT("Animation has duration"),Anim->GetPlayLength()>0);
            }
        }
    }
    FString Manifest;
    TestTrue(TEXT("Source and license manifest exists"),FFileHelper::LoadFileToString(Manifest,*(FPaths::ProjectDir()/TEXT("docs/art/asset-manifest.csv"))));
    TestTrue(TEXT("Production courier provenance is recorded"),Manifest.Contains(TEXT("courier_rider")));
    return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FCourierArtMapTest,"CityCourier.ArtAssets.PreviewPreservesGameplayCollision",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FCourierArtMapTest::RunTest(const FString& Parameters)
{
    UWorld* World=UEditorLoadingAndSavingUtils::LoadMap(TEXT("/Game/CityCourier/Maps/L_CourierArtPreview"));
    if(!TestNotNull(TEXT("Separate art preview map"),World)) return false;
    int32 Facades=0,CollisionBlocks=0;
    for(TActorIterator<AActor> It(World);It;++It)
    {
        for(const FName& Tag:It->Tags)
        {
            if(Tag.ToString().StartsWith(TEXT("CC_Art_Building_")))
            {
                ++Facades;
                auto* Mesh=It->FindComponentByClass<UStaticMeshComponent>();
                if(TestNotNull(TEXT("Facade component"),Mesh))
                    TestEqual(TEXT("Visual facade does not add route collision"),Mesh->GetCollisionEnabled(),ECollisionEnabled::NoCollision);
            }
            if(Tag.ToString().StartsWith(TEXT("CC_City_Block_")))
            {
                ++CollisionBlocks;
                auto* Mesh=It->FindComponentByClass<UStaticMeshComponent>();
                if(TestNotNull(TEXT("Original collision component"),Mesh))
                    TestTrue(TEXT("Original block collision preserved"),Mesh->GetCollisionEnabled()!=ECollisionEnabled::NoCollision);
            }
        }
    }
    TestTrue(TEXT("Art map has dressed streets"),Facades>50);
    TestTrue(TEXT("Original city collision remains"),CollisionBlocks>50);
    auto* RiderClass=LoadClass<ACourierArtCharacter>(nullptr,TEXT("/Game/CityCourier/Blueprints/BP_CourierCharacter.BP_CourierCharacter_C"));
    auto* BikeClass=LoadClass<AScooterArtPawn>(nullptr,TEXT("/Game/CityCourier/Blueprints/BP_Scooter.BP_Scooter_C"));
    TestNotNull(TEXT("Rider blueprint uses art subclass"),RiderClass);
    TestNotNull(TEXT("Scooter blueprint uses art subclass"),BikeClass);
    return true;
}
#endif
