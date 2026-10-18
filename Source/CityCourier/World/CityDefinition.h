#pragma once
#include "Engine/DataAsset.h"
#include "World/RoadGraph.h"
#include "CityDefinition.generated.h"

UENUM(BlueprintType)
enum class EDeliveryLocationKind : uint8 { Merchant, Destination, Station };
USTRUCT(BlueprintType)
struct CITYCOURIER_API FCourierLocationDefinition
{
    GENERATED_BODY()
    UPROPERTY(EditAnywhere, BlueprintReadWrite) FName LocationId;
    UPROPERTY(EditAnywhere, BlueprintReadWrite) FText DisplayName;
    UPROPERTY(EditAnywhere, BlueprintReadWrite) FVector Position = FVector::ZeroVector;
    UPROPERTY(EditAnywhere, BlueprintReadWrite) int32 EntranceNode = -1;
    UPROPERTY(EditAnywhere, BlueprintReadWrite) EDeliveryLocationKind Kind = EDeliveryLocationKind::Destination;
};
USTRUCT(BlueprintType)
struct CITYCOURIER_API FOrderFee
{
    GENERATED_BODY()
    UPROPERTY(EditAnywhere, BlueprintReadWrite) FName MerchantId;
    UPROPERTY(EditAnywhere, BlueprintReadWrite) FName DestinationId;
    UPROPERTY(EditAnywhere, BlueprintReadWrite) int64 FeeCents = 2000;
};
UCLASS(BlueprintType)
class CITYCOURIER_API UCityDefinition : public UDataAsset
{
    GENERATED_BODY()
public:
    UPROPERTY(EditAnywhere, BlueprintReadWrite) FRoadGraph Roads;
    UPROPERTY(EditAnywhere, BlueprintReadWrite) TArray<FCourierLocationDefinition> Merchants;
    UPROPERTY(EditAnywhere, BlueprintReadWrite) TArray<FCourierLocationDefinition> Destinations;
    UPROPERTY(EditAnywhere, BlueprintReadWrite) FCourierLocationDefinition Station;
    UPROPERTY(EditAnywhere, BlueprintReadWrite) TArray<FOrderFee> Fees;
    bool Validate(TArray<FString>& Errors) const;
    const FCourierLocationDefinition* FindLocation(FName Id) const;
    int64 GetFee(FName Merchant, FName Destination) const;
    FRouteResult FindRouteBetween(const FCourierLocationDefinition& From, const FCourierLocationDefinition& To) const;
    FRouteResult FindRouteFrom(FVector Position, FName Destination) const;
    FRouteResult FindRouteToPosition(FVector From,FVector To) const;
};
