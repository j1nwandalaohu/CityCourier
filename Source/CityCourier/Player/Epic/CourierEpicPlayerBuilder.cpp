#include "Player/Epic/CourierEpicArtBuilder.h"
#include "Player/Epic/CourierEpicAnimationConfig.h"
#include "Player/CourierCharacter.h"
#if WITH_EDITOR
#include "Engine/SkeletalMesh.h"
#include "Kismet2/BlueprintEditorUtils.h"
#include "Kismet2/KismetEditorUtilities.h"
#include "PoseSearch/PoseSearchDatabase.h"
#include "PoseSearch/PoseSearchDerivedData.h"
#endif
bool UCourierEpicArtBuilder::EnableOnPlayer(UCourierEpicAnimationConfig* Config,UBlueprint* PlayerBlueprint,const FString& Fingerprint,FText& OutReason)
{
#if WITH_EDITOR
    if(!Config || !PlayerBlueprint || PlayerBlueprint->GetName()!=TEXT("BP_CourierCharacter") || Fingerprint.Len()!=64)
    { OutReason=FText::FromString(TEXT("Expected owned config, real courier blueprint and SHA-256 asset validation fingerprint")); return false; }
    auto* Defaults=PlayerBlueprint->GeneratedClass?Cast<ACourierCharacter>(PlayerBlueprint->GeneratedClass->GetDefaultObject()):nullptr;
    if(!Defaults || (Defaults->EpicAnimationConfig && Defaults->EpicAnimationConfig!=Config))
    { OutReason=FText::FromString(TEXT("Player has an unrelated configuration")); return false; }
    for(auto* DB:{Config->LocomotionDatabase.LoadSynchronous(),Config->SprintDatabase.LoadSynchronous(),Config->AirDatabase.LoadSynchronous()})
    {
        if(!DB || UE::PoseSearch::FAsyncPoseSearchDatabasesManagement::RequestAsyncBuildIndex(DB,UE::PoseSearch::ERequestAsyncBuildFlag::NewRequest|UE::PoseSearch::ERequestAsyncBuildFlag::WaitForCompletion)!=UE::PoseSearch::EAsyncBuildIndexResult::Success)
        { OutReason=FText::FromString(TEXT("Runtime Pose Search index is not ready")); return false; }
    }
    const FString Previous=Config->ValidationFingerprint;
    Config->ValidationFingerprint=Fingerprint;
    if(!Config->ValidateFor(Config->TargetMesh.LoadSynchronous(),OutReason)) { Config->ValidationFingerprint=Previous; return false; }
    Defaults->EpicAnimationConfig=Config;
    FBlueprintEditorUtils::MarkBlueprintAsModified(PlayerBlueprint);
    FKismetEditorUtilities::CompileBlueprint(PlayerBlueprint);
    if(PlayerBlueprint->Status==BS_Error) { Config->ValidationFingerprint=Previous; return false; }
    Config->MarkPackageDirty(); PlayerBlueprint->MarkPackageDirty(); return true;
#else
    return false;
#endif
}
