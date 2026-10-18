#pragma once
#include "GameFramework/Actor.h"
#include "Player/CourierStrikeReceiver.h"
#include "CourierTrainingTarget.generated.h"

UCLASS()
class CITYCOURIER_API ACourierTrainingTarget : public AActor,public ICourierStrikeReceiver
{
    GENERATED_BODY()
public:
    ACourierTrainingTarget();
    virtual void Tick(float Delta) override;
    virtual bool ReceiveCourierStrike(FGuid InstanceId,const FVector& Origin,ECourierAction Action) override;
    int32 GetHitCount() const { return HitCount; }
    UPROPERTY(VisibleAnywhere) TObjectPtr<class UBoxComponent> Body;
    UPROPERTY(VisibleAnywhere) TObjectPtr<class UStaticMeshComponent> Visual;
    UPROPERTY(VisibleAnywhere) TObjectPtr<class UTextRenderComponent> Label;
private:
    TSet<FGuid> Received;
    int32 HitCount=0;
    float ReactionRemaining=0;
};
