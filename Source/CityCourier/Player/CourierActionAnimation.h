#pragma once
#include "CoreMinimal.h"
#include "Player/CourierActionTimeline.h"
class UAnimSequence;
class USkeletalMeshComponent;
struct CITYCOURIER_API FCourierActionAnimation
{
    static FName SequenceName(ECourierAction Action);
    static UAnimSequence* Load(FName Name);
    static bool Compatible(const USkeletalMeshComponent* Mesh,const UAnimSequence* Sequence,double MinimumDuration=0);
    static bool SampleFist(const USkeletalMeshComponent* Mesh,const UAnimSequence* Sequence,bool bLeft,double Time,FVector& Out,bool bUseLocomotionPelvis=false);
};
