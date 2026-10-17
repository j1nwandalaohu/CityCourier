#pragma once
#include "Blueprint/UserWidget.h"
#include "CourierMapWidget.generated.h"
UCLASS()
class CITYCOURIER_API UCourierMapWidget : public UUserWidget
{
    GENERATED_BODY()
public:
    bool bMini=false;
    virtual void NativeOnInitialized() override;
    virtual int32 NativePaint(const FPaintArgs& Args,const FGeometry& Geometry,const FSlateRect& CullingRect,FSlateWindowElementList& Out,int32 Layer,const FWidgetStyle& Style,bool bParentEnabled) const override;
};

