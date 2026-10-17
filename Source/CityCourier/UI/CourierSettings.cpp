#include "UI/CourierSettings.h"
#include "Engine/Engine.h"
#include "GameFramework/GameUserSettings.h"
#include "AudioDevice.h"
#include "AudioDeviceHandle.h"
void UCourierSettings::Apply(const FPlayerProfile& P)
{
    if(!GEngine) return;
    if(auto* Settings=GEngine->GetGameUserSettings()) { Settings->SetOverallScalabilityLevel(FMath::Clamp(P.QualityLevel,0,2)); Settings->ApplyNonResolutionSettings(); }
    if(auto Device=GEngine->GetMainAudioDevice()) Device->SetTransientPrimaryVolume(FMath::Clamp(P.MasterVolume,0.f,1.f));
}

