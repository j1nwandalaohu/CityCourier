#pragma once
#include "Components/ActorComponent.h"
#include "Core/DeliveryTypes.h"
#include "CourierAudioComponent.generated.h"
UENUM(BlueprintType)
enum class ECourierAudioCue:uint8 { Accepted,PickedUp,Delivered,Failed };
UCLASS(ClassGroup=Audio,meta=(BlueprintSpawnableComponent))
class CITYCOURIER_API UCourierAudioComponent:public UActorComponent
{
    GENERATED_BODY()
public:
    UCourierAudioComponent();
    virtual void BeginPlay() override;
    virtual void EndPlay(const EEndPlayReason::Type Reason) override;
    virtual void TickComponent(float DeltaTime,ELevelTick TickType,FActorComponentTickFunction* TickFunction)override;
    void BindDelivery(class UDeliverySubsystem* System);
    UFUNCTION(BlueprintCallable) void PlayDeliveryCue(ECourierAudioCue Cue);
    UFUNCTION(BlueprintCallable) void SetVehicleSpeed(float Kmh);
    int32 GetCueCount(ECourierAudioCue Cue)const{return CueCounts[(uint8)Cue];}
    float GetMotorGain()const{return MotorGain;}
private:
    void ObserveOrder();
    UPROPERTY() TObjectPtr<class UDeliverySubsystem> Delivery;
    UPROPERTY() TObjectPtr<class UAudioComponent> Motor;
    UPROPERTY() TObjectPtr<class UAudioComponent> CityBed;
    UPROPERTY() TArray<TObjectPtr<class UAudioComponent>> Cues;
    FGuid LastId;
    EDeliveryStatus LastStatus=EDeliveryStatus::Idle;
    int32 CueCounts[4]={0,0,0,0};
    float MotorGain=0,PreviousSpeed=0,BrakeCooldown=0;
};
