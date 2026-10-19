#pragma once
#include "Blueprint/UserWidget.h"
#include "Player/CourierController.h"
#include "CourierHUDWidget.generated.h"
UCLASS()
class CITYCOURIER_API UCourierHUDWidget : public UUserWidget
{
    GENERATED_BODY()
public:
    virtual void NativeOnInitialized() override;
    virtual void NativeTick(const FGeometry& Geometry,float Delta) override;
    virtual FReply NativeOnPreviewKeyDown(const FGeometry& Geometry,const FKeyEvent& Event) override;
    void RefreshFromState();
    void ShowMode(ECourierInterfaceMode Mode);
private:
    UFUNCTION() void ReturnFromMap();
    UPROPERTY() TObjectPtr<class UButton> MapBackButton;
    UPROPERTY() TObjectPtr<class UTextBlock> MapBackLabel;
    UPROPERTY() TObjectPtr<class UBorder> ObjectivePanel;
    UPROPERTY() TObjectPtr<class UTextBlock> Controls;
    UPROPERTY() TObjectPtr<class UTextBlock> Objective;
    UPROPERTY() TObjectPtr<class UTextBlock> Time;
    UPROPERTY() TObjectPtr<class UTextBlock> Balance;
    UPROPERTY() TObjectPtr<class UTextBlock> Vehicle;
    UPROPERTY() TObjectPtr<class UTextBlock> Prompt;
    UPROPERTY() TObjectPtr<class UTextBlock> SaveWarning;
    UPROPERTY() TObjectPtr<class UCourierPhoneWidget> Phone;
    UPROPERTY() TObjectPtr<class UCourierMapWidget> Map;
    UPROPERTY() TObjectPtr<class UCourierMapWidget> MiniMap;
    UPROPERTY() TObjectPtr<class UCourierNavigationArrowWidget> NavigationArrow;
    UPROPERTY() TObjectPtr<class UCourierMenuWidget> Menu;
    double RefreshTime=0;
};

