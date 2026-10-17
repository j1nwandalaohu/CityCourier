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
    void ResetDialog() { bConfirmQuit=false; bConfirmReset=false; bExitWarned=false; PendingUpgrade=-1; ServiceMessage.Empty(); }
private:
    UFUNCTION() void Resume();
    UFUNCTION() void RetrySave();
    UFUNCTION() void ResetSave();
    UFUNCTION() void Quit();
    UFUNCTION() void CommitVolume();
    UFUNCTION() void QualityLow();
    UFUNCTION() void QualityMedium();
    UFUNCTION() void QualityHigh();
    UFUNCTION() void Charge();
    UFUNCTION() void UpgradePower();
    UFUNCTION() void UpgradeHandling();
    UFUNCTION() void UpgradeBattery();
    void Purchase(int32 Kind);
    void SetQuality(int32 Level);
    UPROPERTY() TObjectPtr<class UTextBlock> Title;
    UPROPERTY() TObjectPtr<class UTextBlock> Body;
    UPROPERTY() TObjectPtr<class UTextBlock> SaveMessage;
    UPROPERTY() TObjectPtr<class UTextBlock> QuitText;
    UPROPERTY() TObjectPtr<class UTextBlock> ResetText;
    UPROPERTY() TObjectPtr<class UVerticalBox> Settings;
    UPROPERTY() TObjectPtr<class UVerticalBox> StationActions;
    UPROPERTY() TObjectPtr<class UTextBlock> ChargeText;
    UPROPERTY() TArray<TObjectPtr<class UTextBlock>> UpgradeTexts;
    UPROPERTY() TObjectPtr<class USlider> Volume;
    UPROPERTY() TObjectPtr<class UButton> RetryButton;
    UPROPERTY() TObjectPtr<class UButton> ResetButton;
    bool bConfirmQuit=false,bConfirmReset=false,bExitWarned=false;
    int32 PendingUpgrade=-1;
    FString ServiceMessage;
};

