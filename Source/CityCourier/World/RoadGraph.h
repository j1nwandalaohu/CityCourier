#pragma once
#include "CoreMinimal.h"
#include "RoadGraph.generated.h"

USTRUCT(BlueprintType)
struct CITYCOURIER_API FRoadNode
{
    GENERATED_BODY()
    UPROPERTY(EditAnywhere, BlueprintReadWrite) int32 Id = -1;
    UPROPERTY(EditAnywhere, BlueprintReadWrite) FVector Position = FVector::ZeroVector;
    FRoadNode() = default;
    FRoadNode(int32 InId, FVector InPosition) : Id(InId), Position(InPosition) {}
};
USTRUCT(BlueprintType)
struct CITYCOURIER_API FRoadEdge
{
    GENERATED_BODY()
    UPROPERTY(EditAnywhere, BlueprintReadWrite) int32 From = -1;
    UPROPERTY(EditAnywhere, BlueprintReadWrite) int32 To = -1;
    UPROPERTY(EditAnywhere, BlueprintReadWrite) double LengthMeters = 0;
    UPROPERTY(EditAnywhere, BlueprintReadWrite) bool bBidirectional = true;
    FRoadEdge() = default;
    FRoadEdge(int32 A, int32 B, double Length, bool bBoth) : From(A), To(B), LengthMeters(Length), bBidirectional(bBoth) {}
};
struct FRouteResult
{
    bool bReachable = false;
    double LengthMeters = 0;
    TArray<FVector> Points;
    double RoadMeters=0;
    double IndoorFlatMeters=0;
    double StairMeters=0;
    TArray<FName> RequiredDoors;
    // One kind per segment; absent on legacy road-only results.
    TArray<FName> SegmentKinds;
    FName TargetBuilding;
    FName TargetRoom;
    int32 TargetFloor=0;
    bool bHasInteriors=false;
};
USTRUCT(BlueprintType)
struct CITYCOURIER_API FRoadGraph
{
    GENERATED_BODY()
    UPROPERTY(EditAnywhere, BlueprintReadWrite) TArray<FRoadNode> Nodes;
    UPROPERTY(EditAnywhere, BlueprintReadWrite) TArray<FRoadEdge> Edges;
    FRouteResult FindRoute(int32 From, int32 To) const;
    int32 FindNearestNode(FVector Position) const;
    bool Validate(TArray<FString>& Errors) const;
    const FRoadNode* FindNode(int32 Id) const;
    static double DistanceMeters(FVector A, FVector B) { return FVector::Distance(A, B) / 100.0; }
};
