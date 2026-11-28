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
DECLARE_DELEGATE_TwoParams(FXiaomaChestNoticeDelegate, const FString& /*Message*/, bool /*bPositive*/);

/**
 * 宝箱：四种开箱谜题。
 * - Rule  ：指定行/列/色区放对小马后自动弹开（M2）
 * - Runes ：按宝箱上方的颜色顺序踩亮场上符文格，踩错全部熄灭（M3）
 * - Stele ：先按 E 读石碑拿线索，再走到发光处按 E 挖出宝箱（M3）
 * - Push  ：把木箱推到压力板上，宝箱一次性打开（M4）
 */
UCLASS()
class XIAOMALOGIC_API AXiaomaChest : public AXiaomaMechanism
{
    GENERATED_BODY()

public:
    AXiaomaChest();

    void SetupChest(const FXiaomaChestDef& InDef, const FVector& WorldLocation);
    void SetOnOpened(FXiaomaChestOpenedDelegate InDelegate) { OnChestOpened = InDelegate; }
    void SetOnNotice(FXiaomaChestNoticeDelegate InDelegate) { OnChestNotice = InDelegate; }

    bool IsUnlocked() const { return bUnlocked; }
    bool IsClaimed() const { return bClaimed; }
    bool IsSteleRead() const { return bSteleRead; }
    int32 GetRuneProgress() const { return RuneProgress; }
    const FXiaomaChestDef& GetDef() const { return Def; }

    /** 玩家此刻该看到的提示文案（按谜题类型与当前进度） */
    FString BuildPrompt() const;

    /** 推箱宝箱：木箱到位时由 GameMode 直接解锁，一次性锁存（推走箱子也不会重新上锁） */
    void UnlockByMechanism();

    /** 压力板上此刻有没有箱子，只影响视觉反馈 */
    void SetPlatePressed(bool bPressed);

    virtual void OnBoardChanged(const FXiaomaLevel& Level, const TArray<uint8>& Cells) override;
    virtual void OnHerdsmanEnterCell(FIntPoint Cell) override;
    virtual bool TryInteract(FIntPoint Cell) override;
    virtual void SaveState(TArray<uint8>& Out) const override;
    virtual void LoadState(const TArray<uint8>& In) override;

private:
    void RefreshVisuals();
    void RefreshRuneVisuals();
    void BuildRuneVisuals();
    void BuildSteleVisuals();
    void BuildPushVisuals();
    void HandleRuneStep(FIntPoint Cell);
    void TryClaim();
    bool IsBuried() const;
    FVector OffsetFromChest(FIntPoint Cell) const;

    FXiaomaChestDef Def;
    bool bUnlocked = false;
    bool bClaimed = false;
    /** 正在发开箱通知：通知期间状态还是“没开”，防止重入重复发奖励 */
    bool bClaiming = false;
    bool bSteleRead = false;
    bool bDug = false;
    int32 RuneProgress = 0;
    FXiaomaChestOpenedDelegate OnChestOpened;
    FXiaomaChestNoticeDelegate OnChestNotice;

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

    /** 符文宝箱：盖子上方按顺序排列的颜色宝石，以及场上对应颜色的符文格 */
    UPROPERTY()
    TArray<TObjectPtr<UStaticMeshComponent>> OrderGems;

    UPROPERTY()
    TArray<TObjectPtr<UStaticMeshComponent>> RuneDiscs;

    UPROPERTY()
    TArray<TObjectPtr<UStaticMeshComponent>> RuneGlyphs;

    UPROPERTY()
    TArray<TObjectPtr<UMaterialInstanceDynamic>> RuneMaterials;

    /** 石碑宝箱：石碑本体（底座/碑身/浮空符印）与埋藏点提示 */
    UPROPERTY()
    TObjectPtr<UStaticMeshComponent> SteleBase;

    UPROPERTY()
    TObjectPtr<UStaticMeshComponent> SteleSlab;

    UPROPERTY()
    TObjectPtr<UStaticMeshComponent> SteleGlyph;

    UPROPERTY()
    TObjectPtr<UStaticMeshComponent> DigMarker;

    UPROPERTY()
    TObjectPtr<UMaterialInstanceDynamic> DigMaterial;

    /** 推箱宝箱：压力板的铁盘与光环，位置由宝箱格与压力板格算出 */
    UPROPERTY()
    TObjectPtr<UStaticMeshComponent> PlatePad;

    UPROPERTY()
    TObjectPtr<UStaticMeshComponent> PlateGlow;

    UPROPERTY()
    TObjectPtr<UMaterialInstanceDynamic> PlateMaterial;

    UPROPERTY()
    TObjectPtr<UMaterialInstanceDynamic> PlateGlowMaterial;

    bool bPlatePressed = false;
};

/**
 * 静态障碍（M3）：石头、木桩或灌木占据一格。主角进不去，也不能在上面召唤小马。
 */
UCLASS()
class XIAOMALOGIC_API AXiaomaObstacle : public AXiaomaMechanism
{
    GENERATED_BODY()

public:
    AXiaomaObstacle();

    /** Variant 0 = 石头，1 = 木桩，2 = 灌木 */
    void SetupObstacle(const FIntPoint& InCell, const FVector& WorldLocation, int32 Variant);

    FIntPoint GetCell() const { return ObstacleCell; }
    virtual bool BlocksCell(FIntPoint Cell) const override { return Cell == ObstacleCell; }

private:
    FIntPoint ObstacleCell = FIntPoint(-1, -1);

    UPROPERTY()
    TObjectPtr<USceneComponent> SceneRoot;

    UPROPERTY()
    TObjectPtr<UStaticMeshComponent> MainMesh;

    UPROPERTY()
    TObjectPtr<UStaticMeshComponent> TrimMesh;

    UPROPERTY()
    TObjectPtr<UMaterialInstanceDynamic> MainMaterial;
};

/**
 * 推箱木箱（M4）：占据整格，主角撞上去会按格推动。
 * 不能推出棋盘，也推不进障碍、别的木箱、宝箱或石碑所在的格。
 * 状态只有一个字节：当前所在格的序号。
 */
UCLASS()
class XIAOMALOGIC_API AXiaomaPushBox : public AXiaomaMechanism
{
    GENERATED_BODY()

public:
    AXiaomaPushBox();

    void SetupBox(const FIntPoint& InCell, const FVector& WorldLocation);

    FIntPoint GetCell() const { return BoxCell; }
    FIntPoint GetStartCell() const { return StartCell; }
    virtual bool BlocksCell(FIntPoint Cell) const override { return Cell == BoxCell; }

    /** 挪到相邻格；调用方负责先确认目标格可进。位移用 0.12 秒补间，避免瞬移感 */
    void MoveToCell(const FIntPoint& InCell, const FVector& WorldLocation);

    virtual void SaveState(TArray<uint8>& Out) const override;
    virtual void LoadState(const TArray<uint8>& In) override;
    virtual void Tick(float DeltaSeconds) override;

private:
    void SnapToCell(const FIntPoint& InCell, const FVector& WorldLocation);

    FIntPoint BoxCell = FIntPoint(-1, -1);
    FIntPoint StartCell = FIntPoint(-1, -1);
    /** 补间目标点，到达后 Tick 自动停 */
    FVector SlideTarget = FVector::ZeroVector;
    bool bSliding = false;

    UPROPERTY()
    TObjectPtr<USceneComponent> SceneRoot;

    UPROPERTY()
    TObjectPtr<UStaticMeshComponent> BodyMesh;

    UPROPERTY()
    TObjectPtr<UStaticMeshComponent> TopMesh;

    UPROPERTY()
    TObjectPtr<UMaterialInstanceDynamic> BodyMaterial;

    UPROPERTY()
    TObjectPtr<UMaterialInstanceDynamic> TopMaterial;
};
