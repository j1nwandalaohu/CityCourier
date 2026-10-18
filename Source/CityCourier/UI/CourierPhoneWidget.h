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
    UFUNCTION() void PreviewFirst();
    UFUNCTION() void PreviewSecond();
    UFUNCTION() void PreviewThird();
    UFUNCTION() void RefreshOffers();
    UFUNCTION() void OpenHistory();
    UFUNCTION() void NavigateStation();
    UFUNCTION() void NavigateBike();
    UFUNCTION() void NavigateOrder();
    void Preview(int32 Index);
    void Navigate(uint8 Mode);
    void Accept(int32 Index);
    UPROPERTY() TObjectPtr<class UTextBlock> Status;
    UPROPERTY() TObjectPtr<class UTextBlock> CancelText;
    UPROPERTY() TObjectPtr<class UButton> CancelButton;
    UPROPERTY() TObjectPtr<class UButton> RescueButton;
    UPROPERTY() TObjectPtr<class UTextBlock> RescueText;
    UPROPERTY() TArray<TObjectPtr<class UBorder>> Cards;
    UPROPERTY() TArray<TObjectPtr<class UTextBlock>> OfferText;
    UPROPERTY() TObjectPtr<class UVerticalBox> WorkPage;
    UPROPERTY() TObjectPtr<class UCourierHistoryWidget> History;
    UPROPERTY() TObjectPtr<class UButton> RefreshButton;
    UPROPERTY() TObjectPtr<class UButton> BikeButton;
    UPROPERTY() TObjectPtr<class UButton> OrderButton;
    UPROPERTY() TObjectPtr<class UTextBlock> BikeLabel;
    UPROPERTY() TObjectPtr<class UTextBlock> OrderLabel;
    TArray<FGuid> DisplayedIds;
    bool bWasHistory=false;
    bool bConfirmCancel=false;
    bool bConfirmRescue=false;
};

