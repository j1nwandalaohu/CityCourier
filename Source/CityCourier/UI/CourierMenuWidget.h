#pragma once
#include "Blueprint/UserWidget.h"
#include "CourierMenuWidget.generated.h"
UCLASS()
class CITYCOURIER_API UCourierMenuWidget : public UUserWidget
{
    GENERATED_BODY()
public:
    virtual void NativeOnInitialized() override;
    void RefreshFromState();
    void ResetDialog() { bConfirmQuit=false; bConfirmReset=false; bExitWarned=false; }
private:
    UFUNCTION() void Resume();
    UFUNCTION() void RetrySave();
    UFUNCTION() void ResetSave();
    UFUNCTION() void Quit();
    UFUNCTION() void CommitVolume();
    UFUNCTION() void QualityLow();
    UFUNCTION() void QualityMedium();
    UFUNCTION() void QualityHigh();
    void SetQuality(int32 Level);
    UPROPERTY() TObjectPtr<class UTextBlock> Title;
    UPROPERTY() TObjectPtr<class UTextBlock> Body;
    UPROPERTY() TObjectPtr<class UTextBlock> SaveMessage;
    UPROPERTY() TObjectPtr<class UTextBlock> QuitText;
    UPROPERTY() TObjectPtr<class UTextBlock> ResetText;
    UPROPERTY() TObjectPtr<class UVerticalBox> Settings;
    UPROPERTY() TObjectPtr<class USlider> Volume;
    UPROPERTY() TObjectPtr<class UButton> RetryButton;
    UPROPERTY() TObjectPtr<class UButton> ResetButton;
    bool bConfirmQuit=false,bConfirmReset=false,bExitWarned=false;
};

