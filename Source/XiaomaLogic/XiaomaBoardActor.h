#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "XiaomaPuzzle.h"
#include "XiaomaBoardActor.generated.h"

class UStaticMeshComponent;
class UMaterialInstanceDynamic;
class UPrimitiveComponent;
class UStaticMesh;

UCLASS()
class XIAOMALOGIC_API AXiaomaBoardActor : public AActor
{
    GENERATED_BODY()

public:
    AXiaomaBoardActor();
    void BuildBoard(const FXiaomaLevel& Level);
    void RefreshBoard(const FXiaomaLevel& Level, const TArray<uint8>& Cells);
    int32 FindCell(const UPrimitiveComponent* Component) const;
    void Celebrate();
    void FlashCell(int32 Index, bool bError);
    virtual void Tick(float DeltaSeconds) override;

private:
    UPROPERTY()
    TObjectPtr<USceneComponent> SceneRoot;

    UPROPERTY()
    TArray<TObjectPtr<UStaticMeshComponent>> Tiles;

    UPROPERTY()
    TArray<TObjectPtr<UStaticMeshComponent>> Ponies;

    UPROPERTY()
    TArray<TObjectPtr<UStaticMeshComponent>> MarkStrokesA;

    UPROPERTY()
    TArray<TObjectPtr<UStaticMeshComponent>> MarkStrokesB;

    UPROPERTY()
    TArray<TObjectPtr<UMaterialInstanceDynamic>> TileMaterials;

    UPROPERTY()
    TObjectPtr<UMaterialInstanceDynamic> MarkMaterial;

    UPROPERTY()
    TObjectPtr<UStaticMesh> FallbackPonyMesh;

    TArray<float> HopTime;
    TArray<FVector> PonyRestPositions;
    TArray<FLinearColor> TileColours;
    int32 FlashIndex = INDEX_NONE;
    float FlashRemaining = 0.f;
    FLinearColor FlashColour;
};
