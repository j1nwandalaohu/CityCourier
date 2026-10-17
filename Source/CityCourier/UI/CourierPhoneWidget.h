#pragma once
#include "Blueprint/UserWidget.h"
#include "CourierPhoneWidget.generated.h"
UCLASS()
class CITYCOURIER_API UCourierPhoneWidget : public UUserWidget
{
    GENERATED_BODY()
public:
    virtual void NativeOnInitialized() override;
    void RefreshFromState();
private:
    UFUNCTION() void AcceptFirst();
    UFUNCTION() void AcceptSecond();
    UFUNCTION() void AcceptThird();
    UFUNCTION() void CancelOrder();
    UFUNCTION() void Rescue();
    void Accept(int32 Index);
    UPROPERTY() TObjectPtr<class UTextBlock> Status;
    UPROPERTY() TObjectPtr<class UTextBlock> CancelText;
    UPROPERTY() TObjectPtr<class UButton> CancelButton;
    UPROPERTY() TObjectPtr<class UButton> RescueButton;
    UPROPERTY() TObjectPtr<class UTextBlock> RescueText;
    UPROPERTY() TArray<TObjectPtr<class UBorder>> Cards;
    UPROPERTY() TArray<TObjectPtr<class UTextBlock>> OfferText;
    bool bConfirmCancel=false;
    bool bConfirmRescue=false;
};

