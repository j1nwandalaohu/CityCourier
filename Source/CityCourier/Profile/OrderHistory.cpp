#include "Profile/OrderHistory.h"

namespace
{
bool ValidName(const FString& Name)
{
    if(Name.IsEmpty()) return false;
    for(int32 I=0;I<Name.Len();++I)
    {
        const uint32 C=Name[I];
        if(C<32) return false;
        if(C>=0xD800 && C<=0xDBFF)
        { if(++I>=Name.Len() || Name[I]<0xDC00 || Name[I]>0xDFFF) return false; }
        else if(C>=0xDC00 && C<=0xDFFF) return false;
    }
    return true;
}
FString BoundedName(const FString& Name)
{
    if(!ValidName(Name)) return {};
    FString Result=Name.Left(64);
    if(!Result.IsEmpty() && Result[Result.Len()-1]>=0xD800 && Result[Result.Len()-1]<=0xDBFF) Result.LeftChopInline(1);
    return Result;
}
FString Money(int64 Cents) { return FString::Printf(TEXT("¥ %lld.%02lld"),Cents/100,Cents%100); }
}

FOrderHistoryEntry FOrderHistoryRules::MakeEntry(const FDeliveryState& S,int64 Payout,const FString& Merchant,const FString& Destination,FDateTime Time)
{
    FOrderHistoryEntry H; H.InstanceId=S.Offer.InstanceId; H.MerchantId=S.Offer.MerchantId; H.DestinationId=S.Offer.DestinationId;
    H.MerchantName=BoundedName(Merchant); H.DestinationName=BoundedName(Destination); H.SettledAtUtc=Time;
    H.bCompleted=S.Status==EDeliveryStatus::Completed; H.bLate=S.ElapsedSeconds>=S.Offer.DeadlineSeconds;
    H.FailureCode=static_cast<uint8>(S.Failure); H.BaseFeeCents=S.Offer.BaseFeeCents; H.PayoutCents=Payout;
    H.ElapsedSeconds=S.ElapsedSeconds; H.DeadlineSeconds=S.Offer.DeadlineSeconds; H.IntegrityPercent=S.IntegrityPercent;
    return H;
}
bool FOrderHistoryRules::Validate(const FOrderHistoryEntry& H)
{
    if(!H.InstanceId.IsValid() || H.MerchantId.IsNone() || H.DestinationId.IsNone() || H.MerchantId.ToString().Len()>64 || H.DestinationId.ToString().Len()>64 ||
       H.MerchantName.Len()>64 || H.DestinationName.Len()>64 || !ValidName(H.MerchantName) || !ValidName(H.DestinationName) ||
       H.SettledAtUtc.GetTicks()<=0 || H.SettledAtUtc>FDateTime::MaxValue() || H.BaseFeeCents<1600 || H.BaseFeeCents>3200 ||
       H.IntegrityPercent<0 || H.IntegrityPercent>100 || !FMath::IsFinite(H.ElapsedSeconds) || H.ElapsedSeconds<0 ||
       !FMath::IsFinite(H.DeadlineSeconds) || H.DeadlineSeconds<120 || H.DeadlineSeconds>300 || H.ElapsedSeconds>H.DeadlineSeconds+60 ||
       H.bLate!=(H.ElapsedSeconds>=H.DeadlineSeconds) || H.FailureCode>static_cast<uint8>(EDeliveryFailure::Rescue)) return false;
    if(H.bCompleted) return H.FailureCode==0 && H.IntegrityPercent>0 && H.ElapsedSeconds<H.DeadlineSeconds+60 &&
        H.PayoutCents==H.BaseFeeCents*H.IntegrityPercent*(H.bLate?60:100)/10000;
    if(H.FailureCode==0 || H.PayoutCents!=0) return false;
    if(H.FailureCode==static_cast<uint8>(EDeliveryFailure::Expired)) return H.ElapsedSeconds==H.DeadlineSeconds+60;
    if(H.FailureCode==static_cast<uint8>(EDeliveryFailure::Damaged)) return H.IntegrityPercent==0 && H.ElapsedSeconds<H.DeadlineSeconds+60;
    return H.ElapsedSeconds<H.DeadlineSeconds+60;
}
FText FOrderHistoryRules::DescribeOutcome(const FOrderHistoryEntry& H)
{
    if(H.bCompleted) return FText::FromString(H.bLate?TEXT("迟到送达"):TEXT("准时送达"));
    switch(static_cast<EDeliveryFailure>(H.FailureCode))
    {
        case EDeliveryFailure::Expired:return FText::FromString(TEXT("配送超时"));
        case EDeliveryFailure::Damaged:return FText::FromString(TEXT("餐品损毁"));
        case EDeliveryFailure::Cancelled:return FText::FromString(TEXT("主动取消"));
        case EDeliveryFailure::Rescue:return FText::FromString(TEXT("救援中止"));
        default:return FText::FromString(TEXT("订单未完成"));
    }
}
FText FOrderHistoryRules::DescribeIncome(const FOrderHistoryEntry& H)
{
    FString Text=DescribeOutcome(H).ToString()+TEXT("\n基础配送费  ")+Money(H.BaseFeeCents);
    if(H.bCompleted)
    {
        const int64 AfterIntegrity=H.BaseFeeCents*H.IntegrityPercent/100;
        Text+=FString::Printf(TEXT("\n餐品完整度  %d%% · 折减 %s\n时间调整  %s · 折减 %s"),H.IntegrityPercent,*Money(H.BaseFeeCents-AfterIntegrity),H.bLate?TEXT("迟到六折"):TEXT("准时"),*Money(AfterIntegrity-H.PayoutCents));
    }
    else Text+=TEXT("\n失败订单无收入，不扣余额。");
    Text+=TEXT("\n实际收入  ")+Money(H.PayoutCents); return FText::FromString(Text);
}
