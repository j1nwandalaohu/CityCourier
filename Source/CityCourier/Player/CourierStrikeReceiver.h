#pragma once
#include "UObject/Interface.h"
#include "Player/CourierActionTimeline.h"
#include "CourierStrikeReceiver.generated.h"
UINTERFACE(MinimalAPI)
class UCourierStrikeReceiver : public UInterface { GENERATED_BODY() };
class CITYCOURIER_API ICourierStrikeReceiver
{
    GENERATED_BODY()
public:
    virtual bool ReceiveCourierStrike(FGuid InstanceId,const FVector& Origin,ECourierAction Action)=0;
};
