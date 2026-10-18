#pragma once
#include "Blueprint/UserWidget.h"
#include "Components/Button.h"
#include "Profile/OrderHistory.h"
#include "CourierHistoryWidget.generated.h"
class UCourierHistoryWidget;
UCLASS()
class CITYCOURIER_API UCourierHistoryButton : public UButton
{
    GENERATED_BODY()
public:
    void SetEntry(UCourierHistoryWidget* Parent,FGuid Id);
private:
    UFUNCTION() void Open();
    TWeakObjectPtr<UCourierHistoryWidget> Owner;
    FGuid EntryId;
};
UCLASS()
class CITYCOURIER_API UCourierHistoryWidget : public UUserWidget
{
    GENERATED_BODY()
public:
    virtual void NativeOnInitialized() override;
    void RefreshFromState();
    void OpenEntry(FGuid Id);
    void ResetPage();
private:
    UFUNCTION() void Back();
    void Rebuild();
    UPROPERTY() TObjectPtr<class UVerticalBox> Rows;
    UPROPERTY() TObjectPtr<class UTextBlock> BackLabel;
    TOptional<FOrderHistoryEntry> Selected;
    FGuid LastNewest;
    int32 LastCount=-1;
};
