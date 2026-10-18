#pragma once
#include "Profile/PlayerProfile.h"
inline FSettlement MakeTestSettlement(FGuid Id,bool bCompleted=true,int64 Payout=2000)
{
    FSettlement S{Id,bCompleted,Payout}; auto& H=S.History;
    H.InstanceId=Id; H.MerchantId=TEXT("TestShop"); H.DestinationId=TEXT("TestCustomer");
    H.MerchantName=TEXT("Shop"); H.DestinationName=TEXT("Customer"); H.SettledAtUtc=FDateTime(2026,10,18,1,2,3);
    H.bCompleted=bCompleted; H.FailureCode=bCompleted?0:static_cast<uint8>(EDeliveryFailure::Cancelled);
    H.BaseFeeCents=bCompleted?Payout:2000; H.PayoutCents=Payout; H.ElapsedSeconds=30; H.DeadlineSeconds=120; H.IntegrityPercent=100;
    return S;
}
