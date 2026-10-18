#pragma once
#include "Buildings/CourierBuildingDefinition.h"

struct CITYCOURIER_API FCourierIndoorRoute
{
    bool bReachable=false;
    double FlatMeters=0;
    double StairMeters=0;
    TArray<FVector> Points;
    TArray<int32> Nodes;
    TArray<FName> Doors;
    double LengthMeters() const { return FlatMeters+StairMeters; }
};
struct CITYCOURIER_API FCourierInteriorRouting
{
    static FCourierIndoorRoute FindRoute(const FCourierBuildingDefinition& Building,int32 From,int32 To);
};
