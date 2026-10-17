#pragma once
#include "CoreMinimal.h"
struct CITYCOURIER_API FVehicleRules
{
    static float ConsumeDistance(float Fraction, double DistanceMeters, int32 BatteryLevel);
    static float GetMaxSpeedKmh(int32 PowerLevel);
    static double GetRangeMeters(int32 BatteryLevel);
};
