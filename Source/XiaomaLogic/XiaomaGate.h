#pragma once

#include "CoreMinimal.h"
#include "XiaomaMechanism.h"
#include "XiaomaGate.generated.h"

class AXiaomaBoardActor;
class UMaterialInstanceDynamic;
class UStaticMesh;
class UStaticMeshComponent;

DECLARE_DELEGATE_OneParam(FXiaomaGateOpenedDelegate, AXiaomaGate* /*Gate*/);

/**
 * 色区围栏（M4）：把某个色区四周用矮栏杆封住，主角进不去区域内的格子。
 *
 * - 开关是一块同色方形踏板，必须摆在区域外（门关着时区域里根本进不去）；
 * - 踩上踏板后围栏**永久打开**，不能再手动关上——要"关回去"只能按 Z 撤销，
 *   这样既不会出现"人被关在栏里、开关又在栏外"的死局，也省掉一切被困判定；
 * - 它是唯一允许盖住正解格的阻碍：关卡靠它把"什么时候进这个色区"变成谜题，
 *   开关摆得远一点，绕路的代价就写进 par 里；
 * - 状态只有 1 字节（开/关），进撤销快照与存档。
 */
UCLASS()
class XIAOMALOGIC_API AXiaomaGate : public AXiaomaMechanism
{
    GENERATED_BODY()

public:
    AXiaomaGate();

    void SetupGate(const FXiaomaGateDef& InDef, const TArray<FIntPoint>& InRegionCells,
        const AXiaomaBoardActor* InBoard);

    void SetOnOpened(FXiaomaGateOpenedDelegate InDelegate) { OnOpened = InDelegate; }

    virtual bool BlocksCell(FIntPoint Cell) const override;
    virtual void OnHerdsmanEnterCell(FIntPoint Cell) override;
    virtual void SaveState(TArray<uint8>& Out) const override;
    virtual void LoadState(const TArray<uint8>& In) override;

    /**
     * 存档/撤销兜底用：直接开门，不广播委托、不压快照。
     * 只在"人已经在栏内、门却关着"这种不该出现的组合下调用，开门总比把人卡死好。
     */
    void ForceOpen();

    bool IsOpen() const { return bOpen; }
    int32 GetRegion() const { return Region; }
    FIntPoint GetSwitchCell() const { return SwitchCell; }
    bool IsRegionCell(FIntPoint Cell) const { return RegionCells.Contains(Cell); }

private:
    void Open();
    void RefreshVisuals();

    UStaticMeshComponent* AddPart(const TCHAR* BaseName, UStaticMesh* Mesh, const FVector& Location,
        const FVector& Scale, const FColor& Colour, TObjectPtr<UMaterialInstanceDynamic>* OutMaterial = nullptr);

    int32 Region = 0;
    /** 开关格：校验工具保证它在区域外，也保证它站得住 */
    FIntPoint SwitchCell = FIntPoint(-1, -1);
    TSet<FIntPoint> RegionCells;
    bool bOpen = false;
    /** 开关踏板的世界位置（相对本 actor），RefreshVisuals 要靠它重算按钮高度 */
    FVector SwitchPadCenter = FVector::ZeroVector;
    /** 按钮的原色：按下之后按这个颜色压暗，而不是一律变灰 */
    FColor SwitchColour = FColor(200, 200, 200);

    FXiaomaGateOpenedDelegate OnOpened;

    UPROPERTY()
    TObjectPtr<USceneComponent> SceneRoot;

    /** 栏杆：区域边界上每一段横栏 + 一根立柱。开门后整体隐藏，撤销关门时再显示 */
    UPROPERTY()
    TArray<TObjectPtr<UStaticMeshComponent>> RailMeshes;

    UPROPERTY()
    TObjectPtr<UStaticMeshComponent> SwitchPad;

    UPROPERTY()
    TObjectPtr<UStaticMeshComponent> SwitchButton;

    UPROPERTY()
    TObjectPtr<UMaterialInstanceDynamic> SwitchMaterial;
};
