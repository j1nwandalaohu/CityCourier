#pragma once
#include "Engine/DataAsset.h"
#include "CourierBuildingDefinition.generated.h"

USTRUCT(BlueprintType)
struct CITYCOURIER_API FCourierBuildingFloor
{
    GENERATED_BODY()
    UPROPERTY(EditAnywhere,BlueprintReadWrite) FName Id;
    UPROPERTY(EditAnywhere,BlueprintReadWrite) int32 Number=1;
    UPROPERTY(EditAnywhere,BlueprintReadWrite) float Elevation=0;
    UPROPERTY(EditAnywhere,BlueprintReadWrite) bool bRoof=false;
};
USTRUCT(BlueprintType)
struct CITYCOURIER_API FCourierBuildingNode
{
    GENERATED_BODY()
    UPROPERTY(EditAnywhere,BlueprintReadWrite) FName Id;
    UPROPERTY(EditAnywhere,BlueprintReadWrite) FVector Position=FVector::ZeroVector;
    UPROPERTY(EditAnywhere,BlueprintReadWrite) int32 Floor=0;
    UPROPERTY(EditAnywhere,BlueprintReadWrite) FName RoomId;
};
USTRUCT(BlueprintType)
struct CITYCOURIER_API FCourierBuildingEdge
{
    GENERATED_BODY()
    UPROPERTY(EditAnywhere,BlueprintReadWrite) int32 From=INDEX_NONE;
    UPROPERTY(EditAnywhere,BlueprintReadWrite) int32 To=INDEX_NONE;
    UPROPERTY(EditAnywhere,BlueprintReadWrite) FName Kind=TEXT("flat");
    UPROPERTY(EditAnywhere,BlueprintReadWrite) FName DoorId;
    UPROPERTY(EditAnywhere,BlueprintReadWrite) double LengthMeters=0;
};
USTRUCT(BlueprintType)
struct CITYCOURIER_API FCourierRoomDefinition
{
    GENERATED_BODY()
    UPROPERTY(EditAnywhere,BlueprintReadWrite) FName Id;
    UPROPERTY(EditAnywhere,BlueprintReadWrite) int32 Floor=1;
    UPROPERTY(EditAnywhere,BlueprintReadWrite) FName Kind;
    UPROPERTY(EditAnywhere,BlueprintReadWrite) FString Unit;
    UPROPERTY(EditAnywhere,BlueprintReadWrite) FText Label;
    UPROPERTY(EditAnywhere,BlueprintReadWrite) FVector Minimum=FVector::ZeroVector;
    UPROPERTY(EditAnywhere,BlueprintReadWrite) FVector Maximum=FVector::ZeroVector;
    UPROPERTY(EditAnywhere,BlueprintReadWrite) FVector StandingPoint=FVector::ZeroVector;
    UPROPERTY(EditAnywhere,BlueprintReadWrite) int32 RouteNode=INDEX_NONE;
    bool Contains(FVector Local) const { return FBox(Minimum,Maximum).IsInsideOrOn(Local); }
    FString Doorplate() const;
};
USTRUCT(BlueprintType)
struct CITYCOURIER_API FCourierDoorDefinition
{
    GENERATED_BODY()
    UPROPERTY(EditAnywhere,BlueprintReadWrite) FName Id;
    UPROPERTY(EditAnywhere,BlueprintReadWrite) FName FromRoom;
    UPROPERTY(EditAnywhere,BlueprintReadWrite) FName ToRoom;
    UPROPERTY(EditAnywhere,BlueprintReadWrite) FVector Hinge=FVector::ZeroVector;
    UPROPERTY(EditAnywhere,BlueprintReadWrite) float ClosedYaw=0;
    UPROPERTY(EditAnywhere,BlueprintReadWrite) float OpenAngle=90;
    UPROPERTY(EditAnywhere,BlueprintReadWrite) float Width=100;
    UPROPERTY(EditAnywhere,BlueprintReadWrite) float Height=220;
    UPROPERTY(EditAnywhere,BlueprintReadWrite) bool bExternal=false;
    UPROPERTY(EditAnywhere,BlueprintReadWrite) int32 NodeA=INDEX_NONE;
    UPROPERTY(EditAnywhere,BlueprintReadWrite) int32 NodeB=INDEX_NONE;
};
USTRUCT(BlueprintType)
struct CITYCOURIER_API FCourierBuildingBox
{
    GENERATED_BODY()
    UPROPERTY(EditAnywhere,BlueprintReadWrite) FName Id;
    UPROPERTY(EditAnywhere,BlueprintReadWrite) FVector Center=FVector::ZeroVector;
    UPROPERTY(EditAnywhere,BlueprintReadWrite) FVector Size=FVector::OneVector;
    UPROPERTY(EditAnywhere,BlueprintReadWrite) float Yaw=0;
    UPROPERTY(EditAnywhere,BlueprintReadWrite) FName Kind;
    UPROPERTY(EditAnywhere,BlueprintReadWrite) FName Material;
};
USTRUCT(BlueprintType)
struct CITYCOURIER_API FCourierRoomPlacement
{
    GENERATED_BODY()
    UPROPERTY(EditAnywhere,BlueprintReadWrite) FName RoomId;
    UPROPERTY(EditAnywhere,BlueprintReadWrite) FName Kind;
    UPROPERTY(EditAnywhere,BlueprintReadWrite) FVector Position=FVector::ZeroVector;
    UPROPERTY(EditAnywhere,BlueprintReadWrite) float Yaw=0;
    UPROPERTY(EditAnywhere,BlueprintReadWrite) int32 Floor=1;
};
USTRUCT(BlueprintType)
struct CITYCOURIER_API FCourierBuildingDefinition
{
    GENERATED_BODY()
    UPROPERTY(EditAnywhere,BlueprintReadWrite) FName Id;
    UPROPERTY(EditAnywhere,BlueprintReadWrite) FVector Origin=FVector::ZeroVector;
    UPROPERTY(EditAnywhere,BlueprintReadWrite) float Yaw=0;
    UPROPERTY(EditAnywhere,BlueprintReadWrite) FVector Size=FVector::ZeroVector;
    UPROPERTY(EditAnywhere,BlueprintReadWrite) int32 Variant=0;
    UPROPERTY(EditAnywhere,BlueprintReadWrite) FName Kind;
    UPROPERTY(EditAnywhere,BlueprintReadWrite) int32 EntranceNode=INDEX_NONE;
    UPROPERTY(EditAnywhere,BlueprintReadWrite) FName SourceTag;
    UPROPERTY(EditAnywhere,BlueprintReadWrite) FName SourceCollisionTag;
    UPROPERTY(EditAnywhere,BlueprintReadWrite) TArray<FCourierBuildingFloor> Floors;
    UPROPERTY(EditAnywhere,BlueprintReadWrite) TArray<FCourierRoomDefinition> Rooms;
    UPROPERTY(EditAnywhere,BlueprintReadWrite) TArray<FCourierDoorDefinition> Doors;
    UPROPERTY(EditAnywhere,BlueprintReadWrite) TArray<FCourierBuildingNode> Nodes;
    UPROPERTY(EditAnywhere,BlueprintReadWrite) TArray<FCourierBuildingEdge> Edges;
    UPROPERTY(EditAnywhere,BlueprintReadWrite) TArray<FCourierBuildingBox> Boxes;
    UPROPERTY(EditAnywhere,BlueprintReadWrite) TArray<FCourierRoomPlacement> Placements;
    UPROPERTY(EditAnywhere,BlueprintReadWrite) TArray<FName> ExpectedRoomIds;
    FTransform Transform() const { return FTransform(FRotator(0,Yaw,0),Origin); }
    FVector ToWorld(FVector Local) const { return Transform().TransformPosition(Local); }
    FVector ToLocal(FVector World) const { return Transform().InverseTransformPosition(World); }
    int32 FindRoom(FName RoomId) const;
    int32 LocateRoom(FVector World) const;
    bool Validate(TArray<FString>& Errors) const;
};
USTRUCT(BlueprintType)
struct CITYCOURIER_API FCourierIndoorBinding
{
    GENERATED_BODY()
    UPROPERTY(EditAnywhere,BlueprintReadWrite) FName LocationId;
    UPROPERTY(EditAnywhere,BlueprintReadWrite) FName BuildingId;
    UPROPERTY(EditAnywhere,BlueprintReadWrite) FName RoomId;
    UPROPERTY(EditAnywhere,BlueprintReadWrite) FVector StreetPosition=FVector::ZeroVector;
    UPROPERTY(EditAnywhere,BlueprintReadWrite) FName Role;
};
UCLASS(BlueprintType)
class CITYCOURIER_API UCityBuildingCatalog : public UDataAsset
{
    GENERATED_BODY()
public:
    UPROPERTY(EditAnywhere,BlueprintReadWrite) int32 SchemaVersion=1;
    UPROPERTY(EditAnywhere,BlueprintReadWrite) FString SourceDigest;
    UPROPERTY(EditAnywhere,BlueprintReadWrite) TArray<FCourierBuildingDefinition> Buildings;
    UPROPERTY(EditAnywhere,BlueprintReadWrite) TArray<FCourierIndoorBinding> Bindings;
    UPROPERTY(EditAnywhere,BlueprintReadWrite) TArray<FName> ExpectedBuildingIds;
    UFUNCTION(BlueprintCallable,Category="Courier|Buildings") bool ValidateCatalog() const;
    UFUNCTION(BlueprintCallable,Category="Courier|Buildings") bool ImportJsonFile(const FString& Filename);
    bool Validate(TArray<FString>& Errors) const;
    int32 FindBuilding(FName Id) const;
    const FCourierIndoorBinding* FindBinding(FName LocationId) const;
    FString RoomAddress(int32 BuildingIndex,int32 RoomIndex) const;
};
