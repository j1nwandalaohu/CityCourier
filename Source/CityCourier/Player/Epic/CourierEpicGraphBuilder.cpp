#include "Player/Epic/CourierEpicArtBuilder.h"
#include "Player/Epic/CourierEpicAnimationConfig.h"
#if WITH_EDITOR
#include "Animation/AnimBlueprint.h"
#include "Animation/AnimSequence.h"
#include "Animation/Skeleton.h"
#include "AnimGraphNode_Root.h"
#include "AnimGraphNode_SequencePlayer.h"
#include "AnimGraphNode_BlendListByInt.h"
#include "AnimGraphNode_BlendListByBool.h"
#include "AnimGraphNode_Inertialization.h"
#include "AnimGraphNode_Slot.h"
#include "AnimGraphNode_SaveCachedPose.h"
#include "AnimGraphNode_UseCachedPose.h"
#include "AnimGraphNode_LayeredBoneBlend.h"
#include "AnimGraphNode_ControlRig.h"
#include "AnimNode_ControlRig.h"
#include "ControlRig.h"
#include "AnimGraphNode_MotionMatching.h"
#include "AnimGraphNode_PoseSearchHistoryCollector.h"
#include "K2Node_VariableGet.h"
#include "Kismet2/BlueprintEditorUtils.h"
#include "Kismet2/KismetEditorUtilities.h"
#include "Player/CourierAnimInstance.h"
#include "Player/CourierActionAnimation.h"
#include "PoseSearch/PoseSearchDatabase.h"
#include "UObject/UnrealType.h"
#include "Chooser.h"
#include "BoolColumn.h"
#include "ObjectChooser_Asset.h"
#include "ChooserPropertyAccess.h"
#include "AssetRegistry/AssetRegistryModule.h"
#include "Misc/PackageName.h"

namespace
{
template<class T> T* AddNode(UEdGraph* Graph)
{
    FGraphNodeCreator<T> Creator(*Graph);
    auto* Node=Creator.CreateNode(); Creator.Finalize(); return Node;
}
template<class T> T& RuntimeNode(UObject* Node)
{
    auto* Property=FindFProperty<FStructProperty>(Node->GetClass(),TEXT("Node"));
    check(Property);
    return *Property->ContainerPtrToValuePtr<T>(Node);
}
UK2Node_VariableGet* Variable(UEdGraph* Graph,FName Name)
{
    FGraphNodeCreator<UK2Node_VariableGet> Creator(*Graph);
    auto* Node=Creator.CreateNode(); Node->VariableReference.SetSelfMember(Name); Creator.Finalize(); return Node;
}
}
#endif

bool UCourierEpicArtBuilder::BuildAnimationBlueprint(UCourierEpicAnimationConfig* Config,UAnimBlueprint* Blueprint,FText& OutReason)
{
    auto Fail=[&](const TCHAR* Message) { OutReason=FText::FromString(Message); return false; };
#if WITH_EDITOR
    if(!Config || !Blueprint || !Blueprint->GetName().StartsWith(TEXT("ABP_CourierEpic")) || !Blueprint->TargetSkeleton)
        return Fail(TEXT("Use the reserved courier Epic animation blueprint and target skeleton"));
    auto* Ground=Config->LocomotionDatabase.LoadSynchronous();
    auto* Sprint=Config->SprintDatabase.LoadSynchronous();
    auto* Air=Config->AirDatabase.LoadSynchronous();
    if(!Ground || !Sprint || !Air) return Fail(TEXT("Retargeted jog, sprint and air databases are required"));
    const FString ChooserPackage=TEXT("/Game/CityCourier/Animation/Epic/CHT_CourierDatabase");
    auto* Chooser=LoadObject<UChooserTable>(nullptr,*(ChooserPackage+TEXT(".CHT_CourierDatabase")));
    if(Chooser && Config->DatabaseChooser.LoadSynchronous()!=Chooser) return Fail(TEXT("Chooser is not owned by this config"));
    if(!Chooser)
    {
        Chooser=NewObject<UChooserTable>(CreatePackage(*ChooserPackage),TEXT("CHT_CourierDatabase"),RF_Public|RF_Standalone|RF_Transactional);
        FAssetRegistryModule::AssetCreated(Chooser);
    }
    Chooser->ContextData.Reset(); Chooser->ColumnsStructs.Reset(); Chooser->ResultsStructs.Reset(); Chooser->CookedResults.Reset();
    Chooser->OutputObjectType=UPoseSearchDatabase::StaticClass();
    auto Context=FInstancedStruct::Make<FContextObjectTypeClass>();
    Context.GetMutable<FContextObjectTypeClass>().Class=UCourierAnimInstance::StaticClass();
    Chooser->ContextData.Add(MoveTemp(Context));
    auto Column=FInstancedStruct::Make<FBoolColumn>();
    auto& BoolColumn=Column.GetMutable<FBoolColumn>();
    BoolColumn.InputValue.InitializeAs<FBoolContextProperty>();
    BoolColumn.InputValue.GetMutable<FBoolContextProperty>().Binding.PropertyBindingChain.Add(TEXT("bAirborne"));
    BoolColumn.RowValuesWithAny={EBoolColumnCellValue::MatchFalse,EBoolColumnCellValue::MatchTrue};
    Chooser->ColumnsStructs.Add(MoveTemp(Column));
    for(auto* Database:{Ground,Air})
    {
        auto Result=FInstancedStruct::Make<FAssetChooser>(); Result.GetMutable<FAssetChooser>().Asset=Database;
        Chooser->ResultsStructs.Add(MoveTemp(Result));
    }
    Chooser->Compile(true); Chooser->MarkPackageDirty(); FAssetRegistryModule::AssetCreated(Chooser);
    Config->DatabaseChooser=Chooser;
    UEdGraph* Graph=nullptr;
    for(UEdGraph* Candidate:Blueprint->FunctionGraphs) if(Candidate->GetFName()==TEXT("AnimGraph")) Graph=Candidate;
    if(!Graph) return Fail(TEXT("Missing AnimGraph"));
    UAnimGraphNode_Root* Root=nullptr;
    const auto Existing=Graph->Nodes;
    for(UEdGraphNode* Node:Existing)
        if(auto* Result=Cast<UAnimGraphNode_Root>(Node)) { Root=Result; Root->BreakAllNodeLinks(); }
        else FBlueprintEditorUtils::RemoveNode(Blueprint,Node,true);
    if(!Root) return Fail(TEXT("Missing graph output"));
    const auto* Schema=Graph->GetSchema();
    bool bLinked=true;
    auto Link=[&](UEdGraphNode* From,const TCHAR* Output,UEdGraphNode* To,const TCHAR* Input)
    { bLinked=Schema->TryCreateConnection(From->FindPin(Output),To->FindPin(Input)) && bLinked; };
    auto* AirBlend=AddNode<UAnimGraphNode_BlendListByBool>(Graph);
    auto* GaitBlend=AddNode<UAnimGraphNode_BlendListByBool>(Graph);
    int32 I=0;
    for(auto* Database:{Ground,Sprint,Air})
    {
        auto* Match=AddNode<UAnimGraphNode_MotionMatching>(Graph);
        if(I<2)
        {
            for(auto& Optional:Match->ShowPinForProperties)
                if(Optional.PropertyName==TEXT("PlayRateMultiplier")) Optional.bShowPin=true;
            Match->ReconstructNode();
            if(!Match->FindPin(TEXT("PlayRateMultiplier"))) return Fail(TEXT("Ground gait play-rate pin is unavailable"));
            const TCHAR* RateName=I==0?TEXT("JogPlayRate"):TEXT("SprintPlayRate");
            auto* Rate=Variable(Graph,RateName);
            Link(Rate,RateName,Match,TEXT("PlayRateMultiplier"));
        }
        auto& MatchNode=RuntimeNode<FAnimNode_MotionMatching>(Match);
        auto* DatabaseProperty=FindFProperty<FObjectPropertyBase>(FAnimNode_MotionMatching::StaticStruct(),TEXT("Database"));
        check(DatabaseProperty); DatabaseProperty->SetObjectPropertyValue_InContainer(&MatchNode,Database);
        if(auto* Pin=Match->FindPin(TEXT("Database"))) Schema->TrySetDefaultObject(*Pin,Database);
        Match->NodePosX=-1000; Match->NodePosY=I*250;
        if(I<2) Link(Match,TEXT("Pose"),GaitBlend,I==0?TEXT("BlendPose_1"):TEXT("BlendPose_0"));
        else Link(Match,TEXT("Pose"),AirBlend,TEXT("BlendPose_0"));
        ++I;
    }
    auto* GaitVariable=Variable(Graph,TEXT("bUseSprintDatabase"));
    Link(GaitVariable,TEXT("bUseSprintDatabase"),GaitBlend,TEXT("bActiveValue"));
    Link(GaitBlend,TEXT("Pose"),AirBlend,TEXT("BlendPose_1"));
    auto* AirVariable=Variable(Graph,TEXT("bUseAirDatabase"));
    Link(AirVariable,TEXT("bUseAirDatabase"),AirBlend,TEXT("bActiveValue"));
    auto* Postures=AddNode<UAnimGraphNode_BlendListByInt>(Graph);
    for(int32 Pose=2;Pose<7;++Pose) Postures->Node.AddPose();
    Postures->ReconstructNode();
    Link(AirBlend,TEXT("Pose"),Postures,TEXT("BlendPose_0"));
    auto* PostureVariable=Variable(Graph,TEXT("EpicPostureIndex"));
    Link(PostureVariable,TEXT("EpicPostureIndex"),Postures,TEXT("ActiveChildIndex"));
    const TCHAR* Names[]={TEXT("A_Ride"),TEXT("A_CrouchIdle"),TEXT("A_CrouchWalk"),TEXT("A_ProneIdle"),TEXT("A_ProneCrawl"),TEXT("A_IdleRelaxed")};
    for(int32 Pose=0;Pose<6;++Pose)
    {
        auto* Sequence=FCourierActionAnimation::Load(FName(Names[Pose]));
        if(!Sequence || Sequence->GetSkeleton()!=Blueprint->TargetSkeleton) return Fail(TEXT("Existing courier posture clip is missing or incompatible"));
        auto* Player=AddNode<UAnimGraphNode_SequencePlayer>(Graph); Player->Node.SetSequence(Sequence);
        Link(Player,TEXT("Pose"),Postures,*FString::Printf(TEXT("BlendPose_%d"),Pose+1));
    }
    auto* History=AddNode<UAnimGraphNode_PoseSearchHistoryCollector>(Graph);
    auto& HistoryNode=RuntimeNode<FAnimNode_PoseSearchHistoryCollector>(History);
    HistoryNode.bGenerateTrajectory=true; HistoryNode.PoseCount=8;
    auto* Slot=AddNode<UAnimGraphNode_Slot>(Graph); Slot->Node.SlotName=TEXT("FullBodyAction");
    Blueprint->TargetSkeleton->SetSlotGroupName(TEXT("FullBodyAction"),TEXT("CourierActions"));
    auto* Inertia=AddNode<UAnimGraphNode_Inertialization>(Graph);
    Link(Postures,TEXT("Pose"),History,TEXT("Source"));
    auto* Cached=AddNode<UAnimGraphNode_SaveCachedPose>(Graph);
    Cached->CacheName=TEXT("CourierLocomotion");
    Link(History,TEXT("Pose"),Cached,TEXT("Pose"));
    auto UseLocomotion=[&]()
    {
        auto* Use=AddNode<UAnimGraphNode_UseCachedPose>(Graph);
        Use->SaveCachedPoseNode=Cached;
        return Use;
    };
    Link(UseLocomotion(),TEXT("Pose"),Slot,TEXT("Source"));
    auto* Legs=AddNode<UAnimGraphNode_LayeredBoneBlend>(Graph);
    if(Legs->Node.BlendPoses.IsEmpty()) Legs->Node.AddPose();
    Legs->Node.LayerSetup.SetNum(1);
    FBranchFilter RootFilter; RootFilter.BoneName=TEXT("CourierRig"); RootFilter.BlendDepth=1;
    FBranchFilter SpineFilter; SpineFilter.BoneName=TEXT("spine"); SpineFilter.BlendDepth=-1;
    Legs->Node.LayerSetup[0].BranchFilters={RootFilter,SpineFilter};
    Legs->ReconstructNode();
    Link(Slot,TEXT("Pose"),Legs,TEXT("BasePose"));
    Link(UseLocomotion(),TEXT("Pose"),Legs,TEXT("BlendPoses_0"));
    auto* LegWeight=Variable(Graph,TEXT("LocomotionLegWeight"));
    Link(LegWeight,TEXT("LocomotionLegWeight"),Legs,TEXT("BlendWeights_0"));
    Link(Legs,TEXT("Pose"),Inertia,TEXT("Source"));
    auto* RigClass=Config->ContactRig.LoadSynchronous();
    if(!RigClass) return Fail(TEXT("Courier contact rig is required"));
    FGraphNodeCreator<UAnimGraphNode_ControlRig> RigCreator(*Graph);
    auto* Rig=RigCreator.CreateNode();
    RuntimeNode<FAnimNode_ControlRig>(Rig).SetControlRigClass(RigClass);
    RigCreator.Finalize();
    for(const TCHAR* Name:{TEXT("LeftHandGoal"),TEXT("RightHandGoal"),TEXT("LeftFootGoal"),TEXT("RightFootGoal"),TEXT("LeftHandAlpha"),TEXT("RightHandAlpha"),TEXT("LeftFootAlpha"),TEXT("RightFootAlpha"),TEXT("ContactPelvisOffset"),TEXT("ContactEnabled")})
        Rig->AddSourceTargetProperties(FName(Name),FName(Name));
    Link(Inertia,TEXT("Pose"),Rig,TEXT("Source"));
    Link(Rig,TEXT("Pose"),Root,TEXT("Result"));
    if(!bLinked) return Fail(TEXT("Epic graph has unconnected required pins"));
    FBlueprintEditorUtils::MarkBlueprintAsStructurallyModified(Blueprint);
    FKismetEditorUtilities::CompileBlueprint(Blueprint);
    if(Blueprint->Status==BS_Error || !Blueprint->GeneratedClass) return Fail(TEXT("Epic animation blueprint did not compile"));
    auto* Defaults=Cast<UCourierAnimInstance>(Blueprint->GeneratedClass->GetDefaultObject());
    if(!Defaults) return Fail(TEXT("Epic graph must derive from CourierAnimInstance"));
    Defaults->bUseEpicGraph=true;
    Defaults->EpicGraphVersion=UCourierAnimInstance::RequiredEpicGraphVersion;
    Blueprint->MarkPackageDirty();
    Config->AnimationClass=Blueprint->GeneratedClass;
    Config->ValidationFingerprint.Empty(); Config->MarkPackageDirty();
    OutReason=FText::GetEmpty(); return true;
#else
    return Fail(TEXT("Epic graph generation requires an Editor build"));
#endif
}
