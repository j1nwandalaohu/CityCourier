#pragma once
#include "UObject/Interface.h"
#include "Player/CourierActionTimeline.h"
#include "CourierStrikeReceiver.generated.h"
class AActor;
struct FCourierStrikeContext
{
    FGuid InstanceId;
    TWeakObjectPtr<AActor> Instigator;
    FVector Origin=FVector::ZeroVector;
    ECourierAction Action=ECourierAction::None;
};
UINTERFACE(MinimalAPI)
class UCourierStrikeReceiver : public UInterface { GENERATED_BODY() };
class CITYCOURIER_API ICourierStrikeReceiver
{
    GENERATED_BODY()
public:
    virtual bool ReceiveCourierStrike(FGuid InstanceId,const FVector& Origin,ECourierAction Action)=0;
    virtual bool ReceiveCourierStrikeContext(const FCourierStrikeContext& Context)
    { return ReceiveCourierStrike(Context.InstanceId,Context.Origin,Context.Action); }
};
