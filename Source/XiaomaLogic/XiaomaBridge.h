#pragma once

#include "CoreMinimal.h"
#include "XiaomaMechanism.h"
#include "XiaomaBridge.generated.h"

class AXiaomaBoardActor;

DECLARE_DELEGATE_OneParam(FXiaomaBridgeSteppedDelegate, AXiaomaBridge* /*Bridge*/);
DECLARE_DELEGATE_OneParam(FXiaomaBridgeCollapsingDelegate, AXiaomaBridge* /*Bridge*/);

/**
 * 塌陷木桥（M4）：一格大小的木板，走过一次之后塌成坑，不能再进入。
 * - 主角**踩上桥格时**完好无损（可以站着、可以原路回头），
 *   **离开那一格的那一刻**才塌，视觉上是"身后断了"，不会莫名其妙掉下去；
 * - 塌陷的那一刻压一条撤销快照（此时桥还没变），所以按 Z 能把桥修好（策划案 §4 要求）；
 * - 桥是机关：状态（完好/已塌）要进撤销快照和存档，1 字节。
 */
UCLASS()
class XIAOMALOGIC_API AXiaomaBridge : public AXiaomaMechanism
{
    GENERATED_BODY()

public:
    AXiaomaBridge();

    void SetupBridge(const FIntPoint& InCell, const FVector& WorldLocation,
        const AXiaomaBoardActor* InBoard, bool bAlongColumns);

    void SetOnStepped(FXiaomaBridgeSteppedDelegate InDelegate) { OnStepped = InDelegate; }
    /** 即将塌陷（桥此刻还完好）：调用方借此压一条"桥还在"的撤销快照 */
    void SetOnCollapsing(FXiaomaBridgeCollapsingDelegate InDelegate) { OnCollapsing = InDelegate; }

    virtual bool BlocksCell(FIntPoint Cell) const override;
    virtual void OnHerdsmanEnterCell(FIntPoint Cell) override;
    virtual void SaveState(TArray<uint8>& Out) const override;
    virtual void LoadState(const TArray<uint8>& In) override;

    /**
     * 撤销/读档/挨撞弹回都会直接搬运主角而不广播"进入新格"，
     * 所以桥的"主角还站在这儿吗"要由 GameMode 手动同步一次。
     * 这里只同步标志、不触发塌陷，也**不压撤销快照**：撤销恢复出来的位置
     * 本身就是历史里的状态，再压一条会让历史栈顶被同一状态反复占满。
     */
    void SyncHerdsmanCell(FIntPoint Cell);

    FIntPoint GetBridgeCell() const { return BridgeCell; }
    bool IsCollapsed() const { return bCollapsed; }

    /**
     * 主角是从哪一格踩上来的（GameMode 在 OnStepped 里写入）。
     * 撤销塌陷时主角要退回这一格，也就是"这次过桥没发生"。
     */
    void SetApproachCell(FIntPoint Cell) { ApproachCell = Cell; }
    FIntPoint GetApproachCell() const { return ApproachCell; }

private:
    void Collapse();
    void RefreshVisuals();

    void AddPart(const TCHAR* BaseName, UStaticMeshComponent*& OutComp, const FVector& Location,
        const FVector& Scale, const FColor& Colour, bool bCastShadow);

    FIntPoint BridgeCell = FIntPoint::ZeroValue;
    /** 上桥前站的格（撤销塌陷时主角退回这里）；没记录时退回桥格本身 */
    FIntPoint ApproachCell = FIntPoint::ZeroValue;
    bool bCollapsed = false;
    /** 主角此刻是否正踩在桥格上；为真时一旦离开就塌 */
    bool bHerdsmanOn = false;
    bool bRunsAlongColumns = false;

    FXiaomaBridgeSteppedDelegate OnStepped;
    FXiaomaBridgeCollapsingDelegate OnCollapsing;

    UPROPERTY()
    TObjectPtr<USceneComponent> SceneRoot;

    /** 完好的桥板（三块木条，中间留缝） */
    UPROPERTY()
    TArray<TObjectPtr<UStaticMeshComponent>> PlankMeshes;

    /** 塌陷后的坑洞（深色圆盘 + 内侧阴影） */
    UPROPERTY()
    TArray<TObjectPtr<UStaticMeshComponent>> PitMeshes;
};
