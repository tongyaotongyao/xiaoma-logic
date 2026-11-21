#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "XiaomaPuzzle.h"
#include "XiaomaMechanism.generated.h"

class UStaticMeshComponent;
class UMaterialInstanceDynamic;

/**
 * 棋盘机关基类：统一定义阻挡、进入、交互、棋盘变化与撤销快照接口。
 */
UCLASS()
class XIAOMALOGIC_API AXiaomaMechanism : public AActor
{
    GENERATED_BODY()

public:
    AXiaomaMechanism();

    /** 这一格当前是否阻挡主角进入 */
    virtual bool BlocksCell(FIntPoint Cell) const { return false; }

    /** 主角进入某一格时触发 */
    virtual void OnHerdsmanEnterCell(FIntPoint Cell) {}

    /** 玩家按 E 交互时触发，返回 true 表示已被当前机关处理 */
    virtual bool TryInteract(FIntPoint Cell) { return false; }

    /** 棋盘上的小马摆放变化时触发（例如规则开箱检查） */
    virtual void OnBoardChanged(const FXiaomaLevel& Level, const TArray<uint8>& Cells) {}

    /** 保存当前机关状态供 Z 撤销使用 */
    virtual void SaveState(TArray<uint8>& Out) const {}

    /** 从快照恢复机关状态 */
    virtual void LoadState(const TArray<uint8>& In) {}
};

DECLARE_DELEGATE_ThreeParams(FXiaomaChestOpenedDelegate, EXiaomaItemType /*Reward*/, FIntPoint /*Cell*/, FString& /*OutMessage*/);

/**
 * 规则宝箱（M2）：当指定行、列或色区放对小马时自动解锁并弹开，玩家走过去按 E 或进入即可领取奖励。
 */
UCLASS()
class XIAOMALOGIC_API AXiaomaChest : public AXiaomaMechanism
{
    GENERATED_BODY()

public:
    AXiaomaChest();

    void SetupChest(const FXiaomaChestDef& InDef, const FVector& WorldLocation);
    void SetOnOpened(FXiaomaChestOpenedDelegate InDelegate) { OnChestOpened = InDelegate; }

    bool IsUnlocked() const { return bUnlocked; }
    bool IsClaimed() const { return bClaimed; }
    const FXiaomaChestDef& GetDef() const { return Def; }

    virtual void OnBoardChanged(const FXiaomaLevel& Level, const TArray<uint8>& Cells) override;
    virtual void OnHerdsmanEnterCell(FIntPoint Cell) override;
    virtual bool TryInteract(FIntPoint Cell) override;
    virtual void SaveState(TArray<uint8>& Out) const override;
    virtual void LoadState(const TArray<uint8>& In) override;

private:
    void RefreshVisuals();
    void TryClaim();

    FXiaomaChestDef Def;
    bool bUnlocked = false;
    bool bClaimed = false;
    FXiaomaChestOpenedDelegate OnChestOpened;

    UPROPERTY()
    TObjectPtr<USceneComponent> SceneRoot;

    UPROPERTY()
    TObjectPtr<UStaticMeshComponent> BaseMesh;

    UPROPERTY()
    TObjectPtr<UStaticMeshComponent> LidMesh;

    UPROPERTY()
    TObjectPtr<UStaticMeshComponent> GemMesh;

    UPROPERTY()
    TObjectPtr<UMaterialInstanceDynamic> BaseMaterial;

    UPROPERTY()
    TObjectPtr<UMaterialInstanceDynamic> GemMaterial;
};
