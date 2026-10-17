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
    auto Step=[&](float Time){ Mesh->TickAnimation(Time,false); Mesh->RefreshBoneTransforms(); };
    Step(.2f); TestEqual(TEXT("Idle"),Anim->AnimationIndex,0);
    const FVector Standing=Mesh->GetBoneLocation(TEXT("pelvis"),EBoneSpaces::ComponentSpace);
    Rider->GetCharacterMovement()->Velocity=FVector(420,0,0); Step(.2f);
    TestEqual(TEXT("Walk selected"),Anim->AnimationIndex,1);
    const FVector Foot=Mesh->GetBoneLocation(TEXT("foot_l"),EBoneSpaces::ComponentSpace);
    Step(.25f); TestTrue(TEXT("Walk actually animates foot bones"),FVector::Distance(Foot,Mesh->GetBoneLocation(TEXT("foot_l"),EBoneSpaces::ComponentSpace))>2);
    Rider->GetCharacterMovement()->Velocity=FVector(700,0,0); Step(.2f); TestEqual(TEXT("Run selected"),Anim->AnimationIndex,2);
    Rider->SetMounted(true); Step(.3f); Step(.2f); TestEqual(TEXT("Ride selected"),Anim->AnimationIndex,3);
    TestTrue(TEXT("Riding pelvis lowered"),Mesh->GetBoneLocation(TEXT("pelvis"),EBoneSpaces::ComponentSpace).Z<Standing.Z-5);
    TestTrue(TEXT("Riding hands extend toward handlebars"),Mesh->GetBoneLocation(TEXT("hand_l"),EBoneSpaces::ComponentSpace).X>20);
    return true;
}
#endif
