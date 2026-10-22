#include "Player/Epic/CourierEpicArtBuilder.h"
#include "Player/Epic/CourierEpicAnimationConfig.h"
#if WITH_EDITOR
#include "Factories/PhysicsAssetFactory.h"
#include "PhysicsAssetUtils.h"
#include "PhysicsEngine/PhysicsAsset.h"
#include "Engine/SkeletalMesh.h"
#include "AssetRegistry/AssetRegistryModule.h"
#endif
bool UCourierEpicArtBuilder::BuildPhysicsAsset(UCourierEpicAnimationConfig* Config,FText& OutReason)
{
#if WITH_EDITOR
    if(Config && Config->TargetMesh.LoadSynchronous())
    {
        const FString Package=TEXT("/Game/CityCourier/Animation/Epic/PHYS_Courier");
        auto* Asset=LoadObject<UPhysicsAsset>(nullptr,*(Package+TEXT(".PHYS_Courier")));
        if(Asset && Config->PhysicsAsset.LoadSynchronous()!=Asset) { OutReason=FText::FromString(TEXT("Physics asset ownership conflict")); return false; }
        if(!Asset)
            Asset=NewObject<UPhysicsAsset>(CreatePackage(*Package),TEXT("PHYS_Courier"),RF_Public|RF_Standalone|RF_Transactional);
        FPhysAssetCreateParams Parameters;
        const float UnitScale=Config->TargetMesh.LoadSynchronous()->GetRefSkeleton().GetRefBonePose()[0].GetScale3D().GetAbsMax();
        Parameters.MinBoneSize=5/FMath::Max(UnitScale,1.f);
        if(!FPhysicsAssetUtils::CreateFromSkeletalMesh(Asset,Config->TargetMesh.LoadSynchronous(),Parameters,OutReason,false,false)) return false;
        if(Asset && !Asset->SkeletalBodySetups.IsEmpty())
        { Config->PhysicsAsset=Asset; Config->MarkPackageDirty(); Asset->MarkPackageDirty(); FAssetRegistryModule::AssetCreated(Asset); return true; }
    }
#endif
    OutReason=FText::FromString(TEXT("Cannot generate courier physics bodies")); return false;
}
