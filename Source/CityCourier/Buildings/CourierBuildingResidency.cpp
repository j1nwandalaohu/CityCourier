#include "Buildings/CourierBuildingResidency.h"
TArray<int32> FCourierBuildingResidency::Select(const TArray<FCourierBuildingResidencyEntry>& Entries)
{
    TArray<int32> Result; TArray<FCourierBuildingResidencyEntry> Candidates;
    for(const auto& E:Entries)
        if(E.bPinned) Result.AddUnique(E.Index);
        else if(E.DistanceCm<=3000 || (E.bReady && E.DistanceCm<=4500)) Candidates.Add(E);
    Candidates.Sort([](const auto& A,const auto& B){return A.DistanceCm==B.DistanceCm?A.Index<B.Index:A.DistanceCm<B.DistanceCm;});
    for(int32 I=0;I<FMath::Min(8,Candidates.Num());++I) Result.Add(Candidates[I].Index);
    return Result;
}
