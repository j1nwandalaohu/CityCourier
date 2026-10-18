#if WITH_DEV_AUTOMATION_TESTS && WITH_EDITOR
#include "Misc/AutomationTest.h"
#include "Engine/SkeletalMesh.h"
#include "Engine/StaticMesh.h"
#include "Animation/AnimSequence.h"
#include "Animation/AnimBlueprint.h"
#include "Engine/Font.h"
#include "Engine/World.h"
#include "Engine/Level.h"
#include "Engine/StaticMeshActor.h"
#include "Components/StaticMeshComponent.h"
#include "Misc/FileHelper.h"
#include "Misc/Paths.h"
#include "Tests/WorldTestSupport.h"
#include "Player/CourierCharacter.h"
#include "Player/CourierAnimInstance.h"
#include "Components/SkeletalMeshComponent.h"
#include "GameFramework/CharacterMovementComponent.h"
IMPLEMENT_SIMPLE_AUTOMATION_TEST(FArtAssetLoadTest,"CityCourier.ArtAssets.OriginalMeshesAndAnimation",EAutomationTestFlags::EditorContext|EAutomationTestFlags::EngineFilter)
bool FArtAssetLoadTest::RunTest(const FString&)
{
    auto* Rider=LoadObject<USkeletalMesh>(nullptr,TEXT("/Game/CityCourier/Art/Characters/SK_Courier.SK_Courier"));
    if(TestNotNull(TEXT("Original rigged rider"),Rider))
    {
        TestNotNull(TEXT("Persisted skeleton dependency"),Rider->GetSkeleton());
        for(const auto& Slot:Rider->GetMaterials()) TestNotNull(TEXT("Rider material assigned and saved"),Slot.MaterialInterface.Get());
        TestTrue(TEXT("Human stature in centimeters"),Rider->GetBounds().BoxExtent.Z>75 && Rider->GetBounds().BoxExtent.Z<105);
        for(FName Bone:{TEXT("pelvis"),TEXT("head"),TEXT("upperarm_l"),TEXT("thigh_r")}) TestTrue(TEXT("Rig bone exists"),Rider->GetRefSkeleton().FindBoneIndex(Bone)!=INDEX_NONE);
        for(const TCHAR* Name:{TEXT("Idle"),TEXT("Walk"),TEXT("Run"),TEXT("Ride")})
        {
            auto* Animation=LoadObject<UAnimSequence>(nullptr,*FString::Printf(TEXT("/Game/CityCourier/Art/Characters/A_%s.A_%s"),Name,Name));
            if(TestNotNull(Name,Animation)) { TestTrue(TEXT("Animated sequence duration"),Animation->GetPlayLength()>.4); TestEqual(TEXT("Shared skeleton"),Animation->GetSkeleton(),Rider->GetSkeleton()); }
        }
    }
    auto* Scooter=LoadObject<UStaticMesh>(nullptr,TEXT("/Game/CityCourier/Art/Scooter/SM_Scooter.SM_Scooter"));
    if(TestNotNull(TEXT("Original scooter"),Scooter)) TestTrue(TEXT("Scooter length is plausible"),Scooter->GetBounds().BoxExtent.X>70 && Scooter->GetBounds().BoxExtent.X<120);
    TestNotNull(TEXT("Animation Blueprint"),LoadObject<UAnimBlueprint>(nullptr,TEXT("/Game/CityCourier/Blueprints/ABP_Courier.ABP_Courier")));
    TestNotNull(TEXT("Chinese font"),LoadObject<UFont>(nullptr,TEXT("/Game/CityCourier/UI/Fonts/F_CourierChinese.F_CourierChinese")));
    FString Manifest; TestTrue(TEXT("Asset manifest readable"),FFileHelper::LoadFileToString(Manifest,*FPaths::Combine(FPaths::ProjectDir(),TEXT("docs/art/asset-manifest.csv"))));
    for(const TCHAR* Id:{TEXT("courier_rider"),TEXT("courier_scooter"),TEXT("courier_shop"),TEXT("courier_residential")})
    {
        TArray<FString> Lines; Manifest.ParseIntoArrayLines(Lines);
        TestTrue(TEXT("Original non-placeholder asset provenance"),Lines.ContainsByPredicate([Id](const FString& L){return L.StartsWith(FString(Id)+TEXT(",")) && L.EndsWith(TEXT(",false"));}));
    }
    return true;
}
IMPLEMENT_SIMPLE_AUTOMATION_TEST(FArtDistrictTest,"CityCourier.ArtAssets.AuthoredStreet",EAutomationTestFlags::EditorContext|EAutomationTestFlags::EngineFilter)
bool FArtDistrictTest::RunTest(const FString&)
{
    auto* World=LoadObject<UWorld>(nullptr,TEXT("/Game/CityCourier/Maps/L_CourierCity.L_CourierCity"));
    if(!TestNotNull(TEXT("City map"),World)) return false;
    int32 OriginalBuildings=0,StreetProps=0,Signs=0;
    for(AActor* A:World->PersistentLevel->Actors)
    {
        if(!A) continue;
        if(A->ActorHasTag(TEXT("CC_Art_Prop"))) ++StreetProps;
        if(A->ActorHasTag(TEXT("CC_Art_Sign"))) ++Signs;
        if(!A->Tags.ContainsByPredicate([](FName T){return T.ToString().StartsWith(TEXT("CC_City_Block_"));})) continue;
        auto* MeshActor=Cast<AStaticMeshActor>(A); UStaticMesh* Mesh=MeshActor?MeshActor->GetStaticMeshComponent()->GetStaticMesh():nullptr;
        if(Mesh && Mesh->GetPathName().StartsWith(TEXT("/Game/CityCourier/Art/Architecture/"))) ++OriginalBuildings;
        else AddError(TEXT("A district building remains a graybox"));
    }
    TestTrue(TEXT("At least 100 authored facade buildings"),OriginalBuildings>=100);
    TestTrue(TEXT("Street furnishing"),StreetProps>=30); TestTrue(TEXT("Original Chinese signage"),Signs>=12);
    return true;
}
IMPLEMENT_SIMPLE_AUTOMATION_TEST(FArtAnimationTest,"CityCourier.ArtAssets.RuntimeAnimationPoses",EAutomationTestFlags::EditorContext|EAutomationTestFlags::EngineFilter)
bool FArtAnimationTest::RunTest(const FString&)
{
    FTestCourierWorld F;
    auto* Rider=F.World->SpawnActor<ACourierCharacter>(); Rider->DispatchBeginPlay();
    auto* Mesh=Rider->GetMesh(); auto* Anim=Cast<UCourierAnimInstance>(Mesh->GetAnimInstance());
    if(!TestNotNull(TEXT("Native-backed animation Blueprint running"),Anim)) return false;
    TestEqual(TEXT("All four authored clips are assigned"),Anim->Clips.Num(),4);
    TGuardValue<uint64> FrameGuard(GFrameCounter,GFrameCounter);
    auto Step=[&](float Time){ ++GFrameCounter; Mesh->TickAnimation(Time,false); Mesh->RefreshBoneTransforms(); };
    Step(.2f); TestEqual(TEXT("Idle"),Anim->AnimationIndex,0);
    const FVector Standing=Mesh->GetBoneLocation(TEXT("pelvis"),EBoneSpaces::ComponentSpace);
    Rider->GetCharacterMovement()->Velocity=FVector(420,0,0); Step(.2f);
    TestEqual(TEXT("Walk selected"),Anim->AnimationIndex,1);
    const FVector Foot=Mesh->GetBoneLocation(TEXT("foot_l"),EBoneSpaces::ComponentSpace);
    Step(.25f); AddInfo(FString::Printf(TEXT("Walk foot displacement: %f"),FVector::Distance(Foot,Mesh->GetBoneLocation(TEXT("foot_l"),EBoneSpaces::ComponentSpace)))); TestTrue(TEXT("Walk actually animates foot bones"),FVector::Distance(Foot,Mesh->GetBoneLocation(TEXT("foot_l"),EBoneSpaces::ComponentSpace))>2);
    Rider->GetCharacterMovement()->Velocity=FVector(700,0,0); Step(.2f); TestEqual(TEXT("Run selected"),Anim->AnimationIndex,2);
    Rider->SetMounted(true); Step(.3f); Step(.2f); TestEqual(TEXT("Ride selected"),Anim->AnimationIndex,3);
    TestTrue(TEXT("Riding pelvis lowered"),Mesh->GetBoneLocation(TEXT("pelvis"),EBoneSpaces::ComponentSpace).Z<Standing.Z-5);
    TestTrue(TEXT("Riding hands extend toward handlebars"),Mesh->GetBoneLocation(TEXT("hand_l"),EBoneSpaces::ComponentSpace).X>20);
    return true;
}

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
