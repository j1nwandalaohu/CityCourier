#include "Ambient/CourierAudioComponent.h"
#include "Delivery/DeliverySubsystem.h"
#include "Components/AudioComponent.h"
#include "Sound/SoundWave.h"
#include "Kismet/GameplayStatics.h"
#include "GameFramework/PlayerController.h"
#include "Vehicles/ScooterPawn.h"
UCourierAudioComponent::UCourierAudioComponent(){PrimaryComponentTick.bCanEverTick=true;}
void UCourierAudioComponent::BeginPlay()
{
    Super::BeginPlay();
    auto Spawn=[&](const TCHAR* Name,float Volume)->UAudioComponent*
    {
        const FString Path=FString::Printf(TEXT("/Game/CityCourier/Audio/%s.%s"),Name,Name);
        auto* Component=UGameplayStatics::SpawnSound2D(this,LoadObject<USoundWave>(nullptr,*Path),Volume,1,0,nullptr,false,false);
        if(Component)Component->SetUISound(false);
        return Component;
    };
    CityBed=Spawn(TEXT("S_City"),.20f);Motor=Spawn(TEXT("S_Motor"),0.f);
    BindDelivery(GetWorld()->GetSubsystem<UDeliverySubsystem>());
}
void UCourierAudioComponent::EndPlay(const EEndPlayReason::Type Reason)
{
    if(Motor)Motor->Stop();if(CityBed)CityBed->Stop();for(UAudioComponent* Cue:Cues)if(Cue)Cue->Stop();
    Super::EndPlay(Reason);
}
void UCourierAudioComponent::BindDelivery(UDeliverySubsystem* System)
{
    if(Delivery || !System)return;Delivery=System;
    Delivery->OnDeliveryChanged.AddUObject(this,&UCourierAudioComponent::ObserveOrder);
    Delivery->OnDeliveryFinished.AddWeakLambda(this,[this](const FSettlement&){ObserveOrder();});
}
void UCourierAudioComponent::ObserveOrder()
{
    const auto& State=Delivery->GetState();
    if(LastId==State.Offer.InstanceId && LastStatus==State.Status)return;
    LastId=State.Offer.InstanceId;LastStatus=State.Status;
    switch(State.Status)
    {
        case EDeliveryStatus::AwaitingPickup:PlayDeliveryCue(ECourierAudioCue::Accepted);break;
        case EDeliveryStatus::Delivering:PlayDeliveryCue(ECourierAudioCue::PickedUp);break;
        case EDeliveryStatus::Completed:PlayDeliveryCue(ECourierAudioCue::Delivered);break;
        case EDeliveryStatus::Failed:PlayDeliveryCue(ECourierAudioCue::Failed);break;
        default:break;
    }
}
void UCourierAudioComponent::PlayDeliveryCue(ECourierAudioCue Cue)
{
    const uint8 Index=(uint8)Cue;if(Index>=4)return;++CueCounts[Index];
    const TCHAR* Names[]={TEXT("S_Accepted"),TEXT("S_PickedUp"),TEXT("S_Delivered"),TEXT("S_Failed")};
    const FString Path=FString::Printf(TEXT("/Game/CityCourier/Audio/%s.%s"),Names[Index],Names[Index]);
    Cues.RemoveAll([](UAudioComponent* Component){return !IsValid(Component);});
    if(auto* Sound=LoadObject<USoundWave>(nullptr,*Path))
        if(auto* Component=UGameplayStatics::SpawnSound2D(this,Sound,.55f,1,0,nullptr,false,true))
        {Component->SetUISound(false);Cues.Add(Component);}
}
void UCourierAudioComponent::SetVehicleSpeed(float Kmh)
{
    Kmh=FMath::IsFinite(Kmh)?FMath::Clamp(Kmh,0.f,60.f):0;
    MotorGain=Kmh<.3f?0:FMath::Lerp(.06f,.35f,Kmh/60);
    // Keep the virtualized loop alive at zero gain. AdjustVolume(...,0) ends its
    // active duration, so subsequent speed changes cannot restart that sound.
    if(Motor){Motor->SetVolumeMultiplier(MotorGain);Motor->SetPitchMultiplier(.65f+Kmh/45);}
}
void UCourierAudioComponent::TickComponent(float DeltaTime,ELevelTick TickType,FActorComponentTickFunction* TickFunction)
{
    Super::TickComponent(DeltaTime,TickType,TickFunction);BrakeCooldown=FMath::Max(0.f,BrakeCooldown-DeltaTime);
    float Speed=0;
    if(auto* PC=GetWorld()->GetFirstPlayerController())if(auto* Bike=Cast<AScooterPawn>(PC->GetPawn()))Speed=FMath::Abs(Bike->GetSpeedKmh());
    if(PreviousSpeed>5 && PreviousSpeed-Speed>DeltaTime*12 && BrakeCooldown<=0)
    {
        if(auto* Sound=LoadObject<USoundWave>(nullptr,TEXT("/Game/CityCourier/Audio/S_Brake.S_Brake")))
            if(auto* Cue=UGameplayStatics::SpawnSound2D(this,Sound,.16f,1,0,nullptr,false,true)){Cue->SetUISound(false);Cues.Add(Cue);}
        BrakeCooldown=1;
    }
    PreviousSpeed=Speed;SetVehicleSpeed(Speed);
    Cues.RemoveAll([](UAudioComponent* Component){return !IsValid(Component);});
}
