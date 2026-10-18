#pragma once
#include "Blueprint/WidgetTree.h"
#include "Components/TextBlock.h"
#include "Components/Button.h"
#include "Components/Border.h"
#include "Components/VerticalBox.h"
#include "Components/VerticalBoxSlot.h"
#include "Engine/Font.h"

namespace CourierUI
{
inline FLinearColor Ink(.025f,.04f,.05f,.96f), Accent(.98f,.66f,.16f,1), Muted(.58f,.68f,.70f,1);
inline FSlateFontInfo Font(int32 Size)
{ return FSlateFontInfo(LoadObject<UFont>(nullptr,TEXT("/Game/CityCourier/UI/Fonts/F_CourierChinese.F_CourierChinese")),Size); }
inline UTextBlock* Text(UWidgetTree* Tree,const FString& Value,int32 Size=24,FLinearColor Color=FLinearColor::White)
{
    auto* T=Tree->ConstructWidget<UTextBlock>(); T->SetFont(Font(Size)); T->SetColorAndOpacity(Color);
    T->SetText(FText::FromString(Value)); T->SetAutoWrapText(true); return T;
}
inline void Add(UVerticalBox* Column,UWidget* Child,float Padding=8)
{ Column->AddChildToVerticalBox(Child)->SetPadding(FMargin(0,Padding)); }
inline UBorder* Panel(UWidgetTree* Tree,UWidget* Child,FLinearColor Color=Ink,float Padding=28)
{
    auto* B=Tree->ConstructWidget<UBorder>(); B->SetBrushColor(Color); B->SetPadding(FMargin(Padding)); B->SetContent(Child); return B;
}
inline UButton* Button(UWidgetTree* Tree,const FString& Label,UTextBlock** OutLabel=nullptr)
{
    auto* B=Tree->ConstructWidget<UButton>(); B->SetBackgroundColor(FLinearColor(.13f,.21f,.23f));
    auto* T=Text(Tree,Label,24); T->SetAutoWrapText(false); T->SetMinDesiredWidth(64);
    auto Style=B->GetStyle(); Style.SetNormalPadding(FMargin(12,6)); Style.SetPressedPadding(FMargin(12,6)); B->SetStyle(Style);
    B->SetContent(T); if(OutLabel) *OutLabel=T; return B;
}
inline FString Money(int64 Cents) { return FString::Printf(TEXT("¥ %lld.%02lld"),Cents/100,FMath::Abs(Cents%100)); }
inline FString Distance(double Meters) { return Meters<1000?FString::Printf(TEXT("%.0f米"),Meters):FString::Printf(TEXT("%.1f公里"),Meters/1000); }
}

