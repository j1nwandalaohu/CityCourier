#pragma once
#include "CoreMinimal.h"
#include "CourierInteractionPose.generated.h"

USTRUCT(BlueprintType)
struct FCourierInteractionPose
{
    GENERATED_BODY()
    UPROPERTY(BlueprintReadOnly) FTransform LeftHandWorld;
    UPROPERTY(BlueprintReadOnly) FTransform RightHandWorld;
    UPROPERTY(BlueprintReadOnly) float LeftWeight=0;
    UPROPERTY(BlueprintReadOnly) float RightWeight=0;
};
