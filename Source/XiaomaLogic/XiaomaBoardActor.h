#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "XiaomaPuzzle.h"
#include "XiaomaBoardActor.generated.h"

class UStaticMeshComponent;
class UTextRenderComponent;
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

private:
    UPROPERTY()
    TObjectPtr<USceneComponent> SceneRoot;

    UPROPERTY()
    TArray<TObjectPtr<UStaticMeshComponent>> Tiles;

    UPROPERTY()
    TArray<TObjectPtr<UStaticMeshComponent>> Ponies;

    UPROPERTY()
    TArray<TObjectPtr<UTextRenderComponent>> Marks;

    UPROPERTY()
    TArray<TObjectPtr<UMaterialInstanceDynamic>> TileMaterials;

    UPROPERTY()
    TObjectPtr<UStaticMesh> FallbackPonyMesh;
};
