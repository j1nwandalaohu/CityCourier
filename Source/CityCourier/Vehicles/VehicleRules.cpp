#include "Vehicles/VehicleRules.h"
namespace
{
int32 SafeLevel(int32 Level)
{
    if (Level >= 0 && Level <= 2) return Level;
    UE_LOG(LogTemp, Error, TEXT("Invalid vehicle upgrade level %d; using level zero"), Level); return 0;
}
}
float FVehicleRules::GetMaxSpeedKmh(int32 Level) { return 36.f + 6.f * SafeLevel(Level); }
double FVehicleRules::GetRangeMeters(int32 Level) { return 3000.0 + 1000.0 * SafeLevel(Level); }
float FVehicleRules::ConsumeDistance(float Fraction, double Distance, int32 Level)
{
    Fraction = FMath::IsFinite(Fraction) ? FMath::Clamp(Fraction, 0.f, 1.f) : 0.f;
    if (!FMath::IsFinite(Distance) || Distance < 0) return Fraction;
    return static_cast<float>(FMath::Clamp(Fraction - Distance / GetRangeMeters(Level), 0.0, 1.0));
}
