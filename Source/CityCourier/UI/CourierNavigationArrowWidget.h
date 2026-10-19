#pragma once
#include "Blueprint/UserWidget.h"
#include "Delivery/NavigationGuidance.h"
#include "CourierNavigationArrowWidget.generated.h"

UCLASS()
class CITYCOURIER_API UCourierNavigationArrowWidget : public UUserWidget
{
    GENERATED_BODY()
public:
    virtual void NativeOnInitialized() override;
    virtual void NativeTick(const FGeometry& Geometry,float Delta) override;
    virtual int32 NativePaint(const FPaintArgs& Args,const FGeometry& Geometry,const FSlateRect& CullingRect,FSlateWindowElementList& Out,int32 Layer,const FWidgetStyle& Style,bool bParentEnabled) const override;
private:
    FNavigationGuidance Cue;
    FString Title,Distance,Instruction;
    double Heading=0;
    bool bHasTarget=false,bArrived=false,bReachable=false,bHeadingInitialized=false;
};
