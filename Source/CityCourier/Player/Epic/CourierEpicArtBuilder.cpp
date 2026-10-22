#include "Player/Epic/CourierEpicArtBuilder.h"
#include "Player/Epic/CourierEpicAnimationConfig.h"
#if WITH_EDITOR
#include "RetargetEditor/IKRetargetBatchOperation.h"
#include "Retargeter/IKRetargeter.h"
#include "AssetRegistry/AssetRegistryModule.h"
#include "AssetToolsModule.h"
#include "Animation/AnimSequence.h"
#include "Animation/AnimMontage.h"
#include "Animation/AnimData/IAnimationDataModel.h"
#include "Animation/AnimData/IAnimationDataController.h"
#include "Animation/Skeleton.h"
#include "UObject/UObjectHash.h"
#include "Animation/AnimNotifies/AnimNotify.h"
#include "Animation/AnimNotifies/AnimNotifyState.h"
#include "Engine/SkeletalMesh.h"
#include "Misc/PackageName.h"
#include "PoseSearch/PoseSearchDatabase.h"
#include "PoseSearch/PoseSearchSchema.h"
#include "PoseSearch/PoseSearchDerivedData.h"
#include "StructUtils/InstancedStruct.h"
#include "Framework/Application/SlateApplication.h"
#include "Engine/AssetUserData.h"
namespace
{
bool RemoveSourceNotify(const UObject* Notify)
{
    return Notify && (Notify->GetClass()->HasAnyClassFlags(CLASS_CompiledFromBlueprint)
        || Notify->GetClass()->GetFName()==TEXT("AnimNotifyState_PoseSearchBranchIn"));
}
bool IsLocomotionSequence(const UAnimSequence* Sequence)
{
    const FString Name=Sequence->GetName();
    return Sequence->GetAdditiveAnimType()==AAT_None && !Name.Contains(TEXT("Traversal")) && !Name.Contains(TEXT("Crouch"))
        && !Name.Contains(TEXT("AO_")) && !Name.Contains(TEXT("Pose_Base")) && !Name.Contains(TEXT("AimOffset"));
}
bool ConfigureForwardGaits(UCourierEpicAnimationConfig* Config,FText& OutReason)
{
    auto Fail=[&](const TCHAR* Message) { OutReason=FText::FromString(Message); return false; };
    auto* Ground=Config->LocomotionDatabase.LoadSynchronous();
    if(!Ground || !Ground->Schema) return Fail(TEXT("Ground schema is required for forward gaits"));
    const FString Root=TEXT("/Game/CityCourier/Animation/Epic");
    auto* Jog=LoadObject<UAnimSequence>(nullptr,*(Root/TEXT("Clips/CC_M_Neutral_Run_Loop_F.CC_M_Neutral_Run_Loop_F")));
    auto* Sprint=LoadObject<UAnimSequence>(nullptr,*(Root/TEXT("Clips/CC_M_Neutral_Sprint_Loop_F.CC_M_Neutral_Sprint_Loop_F")));
    if(!Jog || !Sprint || Jog->GetSkeleton()!=Sprint->GetSkeleton()) return Fail(TEXT("Retargeted forward run/sprint loops are required"));
    auto ReferenceSpeed=[](UAnimSequence* Clip)
    {
        TArray<FTransform> Keys;
        Clip->GetDataModel()->GetBoneTrackTransforms(Clip->GetSkeleton()->GetReferenceSkeleton().GetBoneName(0),Keys);
        return Keys.Num()>1 && Clip->GetPlayLength()>0?float((Keys.Last().GetTranslation()-Keys[0].GetTranslation()).Size2D()/Clip->GetPlayLength()):0.f;
    };
    const float JogSpeed=ReferenceSpeed(Jog),SprintSpeed=ReferenceSpeed(Sprint);
    if(!FMath::IsFinite(JogSpeed) || !FMath::IsFinite(SprintSpeed) || JogSpeed<80 || SprintSpeed<=JogSpeed || SprintSpeed>1500)
        return Fail(TEXT("Forward gait root trajectories do not provide valid reference speeds"));
    auto* SprintDB=Config->SprintDatabase.LoadSynchronous();
    const FString SprintPackage=Root/TEXT("PSD_CourierSprint");
    if(SprintDB && SprintDB->GetOutermost()->GetName()!=SprintPackage) return Fail(TEXT("Sprint database is not owned by this generator"));
    if(!SprintDB)
    {
        if(FPackageName::DoesPackageExist(SprintPackage) || FindPackage(nullptr,*SprintPackage))
            return Fail(TEXT("An unowned sprint database already exists"));
        SprintDB=NewObject<UPoseSearchDatabase>(CreatePackage(*SprintPackage),TEXT("PSD_CourierSprint"),RF_Public|RF_Standalone|RF_Transactional);
        FAssetRegistryModule::AssetCreated(SprintDB);
    }
    for(const auto& Pair:{TPair<UPoseSearchDatabase*,UAnimSequence*>(Ground,Jog),TPair<UPoseSearchDatabase*,UAnimSequence*>(SprintDB,Sprint)})
    {
        auto* DB=Pair.Key;
        DB->Schema=Ground->Schema;
        while(DB->GetNumAnimationAssets()>0) DB->RemoveAnimationAssetAt(DB->GetNumAnimationAssets()-1);
        Pair.Value->bLoop=true; Pair.Value->MarkPackageDirty();
        auto Entry=FInstancedStruct::Make<FPoseSearchDatabaseSequence>();
        Entry.GetMutable<FPoseSearchDatabaseSequence>().Sequence=Pair.Value;
        DB->AddAnimationAsset(MoveTemp(Entry));
        using namespace UE::PoseSearch;
        if(FAsyncPoseSearchDatabasesManagement::RequestAsyncBuildIndex(DB,ERequestAsyncBuildFlag::NewRequest|ERequestAsyncBuildFlag::WaitForCompletion)!=EAsyncBuildIndexResult::Success)
            return Fail(TEXT("Cannot index the explicit forward gait database"));
        DB->MarkPackageDirty();
    }
    Config->SprintDatabase=SprintDB;
    Config->JogReferenceSpeed=JogSpeed; Config->SprintReferenceSpeed=SprintSpeed;
    Config->ValidationFingerprint.Empty(); Config->MarkPackageDirty();
    OutReason=FText::GetEmpty(); return true;
}
void NormalizeCourierAnimation(UAnimationAsset* Asset)
{
    if(auto* Sequence=Cast<UAnimSequence>(Asset))
    {
        Sequence->bEnableRootMotion=Sequence->GetName().Contains(TEXT("Traversal"));
        // IK retarget's pelvis translation is in cm; this authored skeleton's
        // parent carries a 100x unit conversion. Limb locals are already correct.
        const auto& Ref=Sequence->GetSkeleton()->GetReferenceSkeleton();
        const int32 Pelvis=Ref.FindBoneIndex(TEXT("pelvis"));
        if(Pelvis==INDEX_NONE) return;
        Sequence->bUseNormalizedRootMotionScale=false;
        Sequence->bForceRootLock=true;
        Sequence->RootMotionRootLock=ERootMotionRootLock::RefPose;
        if(Sequence->RefPoseSeq && Sequence->RefPoseSeq->GetSkeleton()!=Sequence->GetSkeleton())
        {
            const FString Name=TEXT("CC_")+Sequence->RefPoseSeq->GetName();
            const FString Package=TEXT("/Game/CityCourier/Animation/Epic/Clips/")+Name;
            if(auto* Base=LoadObject<UAnimSequence>(nullptr,*(Package+TEXT(".")+Name)))
                if(Base->GetSkeleton()==Sequence->GetSkeleton()) Sequence->RefPoseSeq=Base;
        }
        if(Sequence->GetAdditiveAnimType()==AAT_None)
        {
            TArray<FTransform> RootKeys;
            Sequence->GetDataModel()->GetBoneTrackTransforms(Ref.GetBoneName(0),RootKeys);
            const FVector RootScale=Ref.GetRefBonePose()[0].GetScale3D();
            if(RootKeys.ContainsByPredicate([&](const FTransform& Key){return !Key.GetScale3D().Equals(RootScale,.001);}))
            {
                TArray<FVector3f> Positions,Scales; TArray<FQuat4f> Rotations;
                for(const auto& Key:RootKeys)
                { Positions.Add(FVector3f(Key.GetTranslation())); Rotations.Add(FQuat4f(Key.GetRotation())); Scales.Add(FVector3f(RootScale)); }
                Sequence->GetController().SetBoneTrackKeys(Ref.GetBoneName(0),Positions,Rotations,Scales,false);
            }
        }
        TArray<FTransform> Keys;
        Sequence->GetDataModel()->GetBoneTrackTransforms(TEXT("pelvis"),Keys);
        double Maximum=0;
        for(const auto& Key:Keys) Maximum=FMath::Max(Maximum,Key.GetTranslation().Size());
        double ParentScale=1;
        for(int32 Parent=Ref.GetParentIndex(Pelvis);Parent!=INDEX_NONE;Parent=Ref.GetParentIndex(Parent)) ParentScale*=Ref.GetRefBonePose()[Parent].GetScale3D().X;
        if(Sequence->GetAdditiveAnimType()==AAT_None && ParentScale>10 && Maximum>Ref.GetRefBonePose()[Pelvis].GetTranslation().Size()*10)
        {
            TArray<FVector3f> Positions,Scales; TArray<FQuat4f> Rotations;
            for(const auto& Key:Keys)
            { Positions.Add(FVector3f(Key.GetTranslation()/ParentScale)); Rotations.Add(FQuat4f(Key.GetRotation())); Scales.Add(FVector3f(Key.GetScale3D())); }
            Sequence->GetController().SetBoneTrackKeys(TEXT("pelvis"),Positions,Rotations,Scales,false);
        }
    }
    if(auto* Base=Cast<UAnimSequenceBase>(Asset))
    {
        Base->Notifies.RemoveAll([](const FAnimNotifyEvent& Event)
        {
            const UObject* Notify=Event.Notify?static_cast<UObject*>(Event.Notify.Get()):static_cast<UObject*>(Event.NotifyStateClass.Get());
            return RemoveSourceNotify(Notify);
        });
        Base->RefreshCacheData();
    }
    if(const auto* UserData=Asset->GetAssetUserDataArray())
    {
        const auto Copy=*UserData;
        for(const auto* Data:Copy) if(Data && (Data->IsEditorOnly() || Data->GetClass()->GetName().Contains(TEXT("AnimationModifiers")))) Asset->RemoveUserDataOfClass(Data->GetClass());
    }
    // Detached Blueprint notify exports otherwise remain in the saved package,
    // loading the original sample character even though no event references them.
    TArray<UObject*> Children; GetObjectsWithOuter(Asset,Children,true);
    for(auto* Child:Children)
        if((Child->IsA<UAnimNotify>() || Child->IsA<UAnimNotifyState>()) && RemoveSourceNotify(Child))
            Child->Rename(nullptr,GetTransientPackage(),REN_DontCreateRedirectors|REN_NonTransactional);
    Asset->MarkPackageDirty();
}
}
#endif

TArray<FString> UCourierEpicArtBuilder::GetNotifyClasses(UAnimSequenceBase* Animation)
{
    TArray<FString> Result;
#if WITH_EDITOR
    if(Animation) for(const auto& Event:Animation->Notifies)
    {
        const UObject* Notify=Event.Notify?static_cast<UObject*>(Event.Notify.Get()):static_cast<UObject*>(Event.NotifyStateClass.Get());
        if(Notify) Result.Add(Notify->GetClass()->GetPathName());
    }
#endif
    return Result;
}

bool UCourierEpicArtBuilder::ReuseRetargetedAssets(UCourierEpicAnimationConfig* Config,const TArray<UAnimationAsset*>& Assets)
{
#if WITH_EDITOR
    const auto* Mesh=Config?Config->TargetMesh.LoadSynchronous():nullptr;
    if(!Mesh || Assets.IsEmpty()) return false;
    for(auto* Asset:Assets) if(!Asset || Asset->GetSkeleton()!=Mesh->GetSkeleton() || (!Asset->IsA<UAnimSequence>() && !Asset->IsA<UAnimMontage>())) return false;
    Config->RetargetedSequences.Reset(); Config->RetargetedMontages.Reset();
    for(auto* Asset:Assets)
    {
        NormalizeCourierAnimation(Asset);
        if(auto* Sequence=Cast<UAnimSequence>(Asset))
        {
            Sequence->MarkPackageDirty(); Config->RetargetedSequences.Add(Sequence);
        }
        else Config->RetargetedMontages.Add(CastChecked<UAnimMontage>(Asset));
    }
    Config->ValidationFingerprint.Empty(); Config->MarkPackageDirty();
    return true;
#else
    return false;
#endif
}

bool UCourierEpicArtBuilder::BuildRetargetAssets(UCourierEpicAnimationConfig* Config,FText& OutReason)
{
    auto Fail=[&](const TCHAR* Message) { OutReason=FText::FromString(Message); return false; };
#if WITH_EDITOR
    if(!FSlateApplication::IsInitialized()) return Fail(TEXT("UE 5.6 IK batch retarget requires editor Slate; run with -ExecutePythonScript instead of a Python commandlet"));
    if(!Config) return Fail(TEXT("Missing Epic animation configuration"));
    auto* Source=Config->SourceMesh.LoadSynchronous();
    auto* Target=Config->TargetMesh.LoadSynchronous();
    auto* Retargeter=Config->Retargeter.LoadSynchronous();
    if(!Source || !Target || !Retargeter || Config->SourceAnimations.IsEmpty())
        return Fail(TEXT("Source mesh, target mesh, IK retargeter and audited animations are required"));
    if(!Config->RetargetedSequences.IsEmpty() || !Config->RetargetedMontages.IsEmpty())
        return Fail(TEXT("Outputs already exist; verify the generation receipt before reusing them"));
    TArray<FAssetData> Inputs;
    for(const auto& Ref:Config->SourceAnimations)
    {
        auto* Animation=Ref.LoadSynchronous();
        if(!Animation || Animation->GetSkeleton()!=Source->GetSkeleton())
            return Fail(TEXT("Audited source animation has a different skeleton"));
        Inputs.Emplace(Animation);
    }
    auto Outputs=UIKRetargetBatchOperation::DuplicateAndRetarget(Inputs,Source,Target,Retargeter,TEXT(""),TEXT(""),TEXT("CC_"),TEXT(""),true);
    if(Outputs.IsEmpty()) return Fail(TEXT("IK batch operation produced no assets"));
    TArray<FAssetRenameData> Renames;
    for(const FAssetData& Data:Outputs)
    {
        auto* Animation=Cast<UAnimationAsset>(Data.GetAsset());
        if(!Animation || Animation->GetSkeleton()!=Target->GetSkeleton())
            return Fail(TEXT("Retarget output does not use the courier skeleton"));
        const FString Path=TEXT("/Game/CityCourier/Animation/Epic/Clips");
        const FString Name=Animation->GetName();
        const FString Destination=Path/Name;
        if(FindPackage(nullptr,*Destination) || FPackageName::DoesPackageExist(Destination))
            return Fail(TEXT("Retarget output conflicts with an existing package"));
        Renames.Emplace(Animation,Path,Name);
    }
    auto& Tools=FModuleManager::LoadModuleChecked<FAssetToolsModule>(TEXT("AssetTools")).Get();
    if(!Tools.RenameAssets(Renames)) return Fail(TEXT("Cannot move generated animation packages"));
    for(const FAssetRenameData& Rename:Renames)
    {
        auto* Animation=Cast<UAnimationAsset>(Rename.Asset.Get());
        NormalizeCourierAnimation(Animation);
        if(auto* Sequence=Cast<UAnimSequence>(Animation))
        {
            const bool bTraversal=Sequence->GetName().Contains(TEXT("Traversal"));
            Sequence->bEnableRootMotion=bTraversal;
            // Blueprint Foley/gameplay notifies reference the entire sample character.
            // Courier gameplay owns its own events. Retain only native notifies/states.
            Sequence->Notifies.RemoveAll([](const FAnimNotifyEvent& Event)
            {
                const UObject* Notify=Event.Notify?static_cast<UObject*>(Event.Notify.Get()):static_cast<UObject*>(Event.NotifyStateClass.Get());
                return Notify && Notify->GetClass()->HasAnyClassFlags(CLASS_CompiledFromBlueprint);
            });
            Sequence->MarkPackageDirty();
            Config->RetargetedSequences.Add(Sequence);
        }
        else if(auto* Montage=Cast<UAnimMontage>(Animation)) Config->RetargetedMontages.Add(Montage);
    }
    Config->ValidationFingerprint.Empty();
    return BuildPoseSearchDatabases(Config,OutReason);
#else
    return Fail(TEXT("Asset generation requires an editor build"));
#endif
}

bool UCourierEpicArtBuilder::BuildPoseSearchDatabases(UCourierEpicAnimationConfig* Config,FText& OutReason)
{
    auto Fail=[&](const TCHAR* Message) { OutReason=FText::FromString(Message); return false; };
#if WITH_EDITOR
    auto* Target=Config?Config->TargetMesh.LoadSynchronous():nullptr;
    if(!Target || Config->RetargetedSequences.IsEmpty()) return Fail(TEXT("Retargeted courier sequences are required"));
    if(Config->LocomotionDatabase.LoadSynchronous() && Config->AirDatabase.LoadSynchronous())
    {
        using namespace UE::PoseSearch;
        for(auto* DB:{Config->LocomotionDatabase.LoadSynchronous(),Config->AirDatabase.LoadSynchronous()})
        {
            if(!DB->Schema || DB->Schema->GetSkeleton(DB->Schema->GetDefaultRole())!=Target->GetSkeleton())
                return Fail(TEXT("Existing database has an incompatible skeleton"));
            for(int32 Index=DB->GetNumAnimationAssets()-1;Index>=0;--Index)
            {
                const auto* Entry=DB->GetDatabaseAnimationAsset<FPoseSearchDatabaseSequence>(Index);
                if(!Entry || !Entry->Sequence || !IsLocomotionSequence(Entry->Sequence)) DB->RemoveAnimationAssetAt(Index);
            }
            if(FAsyncPoseSearchDatabasesManagement::RequestAsyncBuildIndex(DB,ERequestAsyncBuildFlag::NewRequest|ERequestAsyncBuildFlag::WaitForCompletion)!=EAsyncBuildIndexResult::Success)
                return Fail(TEXT("Cannot refresh Pose Search index"));
            DB->MarkPackageDirty();
        }
        return ConfigureForwardGaits(Config,OutReason);
    }
    const FString Root=TEXT("/Game/CityCourier/Animation/Epic");
    auto CreateAsset=[&](UClass* Class,const FString& Name) -> UObject*
    {
        const FString PackageName=Root/Name;
        if(FindPackage(nullptr,*PackageName) || FPackageName::DoesPackageExist(PackageName)) return nullptr;
        auto* Object=NewObject<UObject>(CreatePackage(*PackageName),Class,*Name,RF_Public|RF_Standalone|RF_Transactional);
        FAssetRegistryModule::AssetCreated(Object);
        Object->MarkPackageDirty();
        return Object;
    };
    auto* Schema=Cast<UPoseSearchSchema>(CreateAsset(UPoseSearchSchema::StaticClass(),TEXT("PSS_Courier")));
    if(!Schema) return Fail(TEXT("Schema already exists; refusing to overwrite"));
    Schema->AddSkeleton(Target->GetSkeleton());
    Schema->AddDefaultChannels();
    for(bool bAir:{false,true})
    {
        auto* DB=Cast<UPoseSearchDatabase>(CreateAsset(UPoseSearchDatabase::StaticClass(),bAir?TEXT("PSD_CourierAir"):TEXT("PSD_CourierGround")));
        if(!DB) return Fail(TEXT("Database already exists; refusing to overwrite"));
        DB->Schema=Schema;
        for(const auto& Ref:Config->RetargetedSequences)
        {
            auto* Sequence=Ref.LoadSynchronous();
            const FString Name=Sequence->GetName();
            if(!IsLocomotionSequence(Sequence)) continue;
            const bool bJump=Name.Contains(TEXT("Jump")) || Name.Contains(TEXT("Land"));
            if(bJump!=bAir) continue;
            auto Entry=FInstancedStruct::Make<FPoseSearchDatabaseSequence>();
            Entry.GetMutable<FPoseSearchDatabaseSequence>().Sequence=Sequence;
            DB->AddAnimationAsset(MoveTemp(Entry));
        }
        if(DB->GetNumAnimationAssets()==0) return Fail(TEXT("Retarget selection does not cover ground and air"));
        using namespace UE::PoseSearch;
        if(FAsyncPoseSearchDatabasesManagement::RequestAsyncBuildIndex(DB,ERequestAsyncBuildFlag::NewRequest|ERequestAsyncBuildFlag::WaitForCompletion)!=EAsyncBuildIndexResult::Success)
            return Fail(TEXT("Pose Search index generation failed"));
        if(bAir) Config->AirDatabase=DB; else Config->LocomotionDatabase=DB;
    }
    Config->MarkPackageDirty();
    return ConfigureForwardGaits(Config,OutReason);
#else
    return Fail(TEXT("Asset generation requires an editor build"));
#endif
}
