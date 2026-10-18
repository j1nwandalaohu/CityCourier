#include "Player/CourierArtBuilder.h"
#if WITH_EDITOR
#include "Animation/AnimBlueprint.h"
#include "Animation/AnimSequence.h"
#include "AnimGraphNode_Root.h"
#include "AnimGraphNode_SequencePlayer.h"
#include "AnimGraphNode_BlendListByInt.h"
#include "AnimGraphNode_Slot.h"
#include "Animation/Skeleton.h"
#include "Engine/SkeletalMeshSocket.h"
#include "K2Node_VariableGet.h"
#include "Kismet2/BlueprintEditorUtils.h"
#include "Kismet2/KismetEditorUtilities.h"
#include "EdGraph/EdGraph.h"
#include "EdGraph/EdGraphSchema.h"
#include "Player/CourierAnimInstance.h"
#endif
bool UCourierArtBuilder::BuildAnimationBlueprint(UAnimBlueprint* Blueprint)
{
#if WITH_EDITOR
    if(!Blueprint || !Blueprint->TargetSkeleton) return false;
    // Preserve any artist-adjusted offsets when rebuilding the functional graph.
    for(const auto& Pair:{TPair<FName,FName>(TEXT("PunchTrace_L"),TEXT("hand_l")),TPair<FName,FName>(TEXT("PunchTrace_R"),TEXT("hand_r"))})
    {
        if(Blueprint->TargetSkeleton->GetReferenceSkeleton().FindBoneIndex(Pair.Value)==INDEX_NONE) return false;
        if(!Blueprint->TargetSkeleton->FindSocket(Pair.Key))
        {
            auto* Socket=NewObject<USkeletalMeshSocket>(Blueprint->TargetSkeleton,NAME_None,RF_Transactional);
            Socket->SocketName=Pair.Key; Socket->BoneName=Pair.Value; Socket->RelativeLocation=FVector(5,0,0);
            Blueprint->TargetSkeleton->Sockets.Add(Socket);
            Blueprint->TargetSkeleton->MarkPackageDirty();
        }
    }
    UEdGraph* Graph=nullptr;
    for(UEdGraph* G:Blueprint->FunctionGraphs) if(G->GetFName()==TEXT("AnimGraph")) { Graph=G; break; }
    if(!Graph) return false;
    UAnimGraphNode_Root* Root=nullptr;
    const auto OldNodes=Graph->Nodes;
    for(UEdGraphNode* Node:OldNodes)
        if(auto* R=Cast<UAnimGraphNode_Root>(Node)) { Root=R; Root->BreakAllNodeLinks(); }
        else FBlueprintEditorUtils::RemoveNode(Blueprint,Node,true);
    if(!Root) return false;
    FGraphNodeCreator<UAnimGraphNode_BlendListByInt> BlendCreator(*Graph);
    auto* Blend=BlendCreator.CreateNode(); BlendCreator.Finalize();
    // Constructor+PostPlacedNewNode create two poses.
    Blend->Node.AddPose(); Blend->Node.AddPose(); Blend->ReconstructNode(); Blend->NodePosX=-250;
    auto* Schema=Graph->GetSchema();
    Blueprint->TargetSkeleton->SetSlotGroupName(TEXT("FullBodyAction"),TEXT("CourierActions"));
    FGraphNodeCreator<UAnimGraphNode_Slot> SlotCreator(*Graph);
    auto* Slot=SlotCreator.CreateNode(); Slot->Node.SlotName=TEXT("FullBodyAction"); SlotCreator.Finalize();
    Slot->NodePosX=0; Root->NodePosX=250;
    if(!Schema->TryCreateConnection(Blend->FindPin(TEXT("Pose")),Slot->FindPin(TEXT("Source"))) ||
       !Schema->TryCreateConnection(Slot->FindPin(TEXT("Pose")),Root->FindPin(TEXT("Result")))) return false;
    FGraphNodeCreator<UK2Node_VariableGet> VariableCreator(*Graph);
    auto* Variable=VariableCreator.CreateNode(); Variable->VariableReference.SetSelfMember(TEXT("AnimationIndex")); VariableCreator.Finalize(); Variable->NodePosX=-550; Variable->NodePosY=-200;
    if(!Schema->TryCreateConnection(Variable->FindPin(TEXT("AnimationIndex")),Blend->FindPin(TEXT("ActiveChildIndex")))) return false;
    const TCHAR* Names[]={TEXT("Idle"),TEXT("Walk"),TEXT("Run"),TEXT("Ride")};
    for(int32 I=0;I<4;++I)
    {
        auto* Sequence=LoadObject<UAnimSequence>(nullptr,*FString::Printf(TEXT("/Game/CityCourier/Art/Characters/A_%s.A_%s"),Names[I],Names[I]));
        if(!Sequence) return false;
        FGraphNodeCreator<UAnimGraphNode_SequencePlayer> PlayerCreator(*Graph);
        auto* Player=PlayerCreator.CreateNode(); Player->Node.SetSequence(Sequence); PlayerCreator.Finalize();
        Player->NodePosX=-600; Player->NodePosY=I*180;
        if(!Schema->TryCreateConnection(Player->FindPin(TEXT("Pose")),Blend->FindPin(*FString::Printf(TEXT("BlendPose_%d"),I)))) return false;
    }
    FBlueprintEditorUtils::MarkBlueprintAsStructurallyModified(Blueprint); FKismetEditorUtilities::CompileBlueprint(Blueprint);
    return Blueprint->Status!=BS_Error;
#else
    return false;
#endif
}
