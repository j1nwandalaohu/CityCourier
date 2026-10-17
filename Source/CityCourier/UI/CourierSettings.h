#pragma once
#include "UObject/Object.h"
#include "Profile/PlayerProfile.h"
#include "CourierSettings.generated.h"
UCLASS()
class CITYCOURIER_API UCourierSettings : public UObject
{
    GENERATED_BODY()
public:
    static void Apply(const FPlayerProfile& Profile);
};

