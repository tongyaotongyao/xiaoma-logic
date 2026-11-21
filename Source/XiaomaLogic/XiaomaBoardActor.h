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

    /** 根据点击组件查找是哪一匹小马（0~5），未命中返回 INDEX_NONE */
    int32 FindPonyColour(const UPrimitiveComponent* Component) const;

    /** 设置当前主角所在格高亮 */
    void SetHighlightedCell(int32 Index);

    /** 触发某色待命小马摇头拒绝动画 */
    void RejectStandbyPony(int32 ColourIndex);

    /** 触发某色小马被轻抚后的欢呼跳跃 */
    void PetPony(int32 ColourIndex);

    /** 获取指定格子的世界坐标（供机关和道具定位） */
    FVector GetCellWorldLocation(FIntPoint Cell) const;

    void Celebrate();
    void FlashCell(int32 Index, bool bError);
    virtual void Tick(float DeltaSeconds) override;

private:
    FVector GetStandbyLocation(int32 ColourIndex) const;

    UPROPERTY()
    TObjectPtr<USceneComponent> SceneRoot;

    UPROPERTY()
    TArray<TObjectPtr<UStaticMeshComponent>> Tiles;

    UPROPERTY()
    TArray<TObjectPtr<UStaticMeshComponent>> Ponies;

    /** 上侧待命区的 6 匹小马（按色区 0~5 排列） */
    UPROPERTY()
    TArray<TObjectPtr<UStaticMeshComponent>> StandbyPonies;

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
    TArray<int32> RegionAtCell;

    /** 小马从待命区飞入/飞回动画状态 */
    TArray<float> StandbyTravelTime;
    TArray<FVector> StandbyStartPos;
    TArray<FVector> StandbyEndPos;
    TArray<bool> StandbyTargetVisible;
    TArray<float> StandbyRejectTime;
    TArray<float> StandbyHopTime;

    int32 HighlightedCell = INDEX_NONE;
    int32 FlashIndex = INDEX_NONE;
    float FlashRemaining = 0.f;
    FLinearColor FlashColour;
};
