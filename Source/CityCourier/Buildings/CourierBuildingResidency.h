#pragma once
#include "CoreMinimal.h"

struct FCourierBuildingResidencyEntry
{
    int32 Index=INDEX_NONE;
    double DistanceCm=MAX_dbl;
    bool bReady=false;
    bool bPinned=false;
};
struct CITYCOURIER_API FCourierBuildingResidency
{
    static TArray<int32> Select(const TArray<FCourierBuildingResidencyEntry>& Entries);
};
