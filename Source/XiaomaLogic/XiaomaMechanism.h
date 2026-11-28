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
 * 宝箱某一段的视觉部件。多段宝箱每段各有一套（各自的符文盘、石碑、压力板），
 * 按 StageIndex 取用，非当前段压暗表示"还没轮到"。
 */
USTRUCT()
struct FXiaomaChestStageVisuals
{
    GENERATED_BODY()

    UPROPERTY()
    TArray<TObjectPtr<UStaticMeshComponent>> OrderGems;

    UPROPERTY()
    TArray<TObjectPtr<UStaticMeshComponent>> RuneDiscs;

    UPROPERTY()
    TArray<TObjectPtr<UStaticMeshComponent>> RuneGlyphs;

    UPROPERTY()
    TArray<TObjectPtr<UMaterialInstanceDynamic>> RuneMaterials;

    UPROPERTY()
    TObjectPtr<UStaticMeshComponent> SteleBase = nullptr;

    UPROPERTY()
    TObjectPtr<UStaticMeshComponent> SteleSlab = nullptr;

    UPROPERTY()
    TObjectPtr<UStaticMeshComponent> SteleGlyph = nullptr;

    UPROPERTY()
    TObjectPtr<UStaticMeshComponent> DigMarker = nullptr;

    UPROPERTY()
    TObjectPtr<UMaterialInstanceDynamic> DigMaterial = nullptr;

    UPROPERTY()
    TObjectPtr<UStaticMeshComponent> PlatePad = nullptr;

    UPROPERTY()
    TObjectPtr<UStaticMeshComponent> PlateGlow = nullptr;

    UPROPERTY()
    TObjectPtr<UMaterialInstanceDynamic> PlateMaterial = nullptr;

    UPROPERTY()
    TObjectPtr<UMaterialInstanceDynamic> PlateGlowMaterial = nullptr;

    /** 压力板上此刻有没有箱子，只影响视觉反馈 */
    bool bPlatePressed = false;
};

/**
 * 宝箱：四种开箱谜题，可以按顺序串联成组合宝箱。
 * - Rule  ：指定行/列/色区放对小马后自动弹开（M2）
 * - Runes ：按宝箱上方的颜色顺序踩亮场上符文格，踩错全部熄灭（M3）
 * - Stele ：先按 E 读石碑拿线索，再走到发光处按 E 挖出宝箱（M3）
 * - Push  ：把木箱推到压力板上，宝箱一次性打开（M4）
 *
 * 组合宝箱（JSON 的 `puzzle.then`）严格串行：只有当前段的机关响应输入，
 * 前一段做完，下一段才亮起来；最后一段完成宝箱才真正打开。
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

    /** 所有段都完成（可以被领取） */
    bool IsUnlocked() const { return Def.Stages.Num() > 0 && StageIndex >= Def.Stages.Num(); }
    bool IsClaimed() const { return bClaimed; }
    /** 已完成的段数；等于 GetNumStages() 时表示全部解开 */
    int32 GetStageIndex() const { return StageIndex; }
    int32 GetNumStages() const { return Def.Stages.Num(); }
    /** 当前段；全部完成后返回 nullptr */
    const FXiaomaChestStage* GetActiveStage() const;
    /** 这一格是不是某一阶段的石碑格（已完成的阶段也算，念线索要用） */
    const FXiaomaChestStage* FindSteleStage(FIntPoint Cell) const;
    /** 当前段是不是推箱：压力板只在它的段上生效 */
    bool IsPushStageActive() const;
    /** 当前段是否已经完成（全部完成时恒为 true） */
    bool IsActiveStageComplete() const { return IsStageComplete(StageIndex); }
    /** 当前段压力板所在格，不是推箱段时返回 (-1,-1) */
    FIntPoint GetActivePlate() const;
    const FXiaomaChestDef& GetDef() const { return Def; }

    /** 玩家此刻该看到的提示文案（按当前段与进度） */
    FString BuildPrompt() const;

    /** 推箱宝箱：木箱到位时由 GameMode 直接解锁当前段，一次性锁存 */
    void UnlockByMechanism();

    /** 压力板上此刻有没有箱子，只影响视觉反馈 */
    void SetPlatePressed(bool bPressed);

    virtual void OnBoardChanged(const FXiaomaLevel& Level, const TArray<uint8>& Cells) override;
    virtual void OnHerdsmanEnterCell(FIntPoint Cell) override;
    virtual bool TryInteract(FIntPoint Cell) override;
    virtual void SaveState(TArray<uint8>& Out) const override;
    virtual void LoadState(const TArray<uint8>& In) override;

private:
    /** 一段谜题完成所需的最大进度值（符文=个数、石碑=2 即挖出、其余=1） */
    static int32 MaxProgressForStage(const FXiaomaChestStage& Stage);
    /** 当前段是否已完成 */
    bool IsStageComplete(int32 Index) const;
    /** 把当前段记为完成，推进到下一段并给出提示；Prefix 会接在提示前面 */
    void CompleteActiveStage(const FString& Prefix = FString());
    /** 这一格是不是"还没轮到"的后续段符文格 */
    bool IsFutureRuneCell(FIntPoint Cell) const;
    void RefreshVisuals();
    void RefreshRuneVisuals(int32 StageNum);
    void BuildStageVisuals(int32 StageNum);
    void HandleRuneStep(FIntPoint Cell);
    void TryClaim();
    bool IsBuried() const;
    FVector OffsetFromChest(FIntPoint Cell) const;
    FXiaomaChestStageVisuals& Visuals(int32 StageNum) { return StageVisuals[StageNum]; }

    FXiaomaChestDef Def;
    bool bClaimed = false;
    /** 正在发开箱通知：通知期间状态还是“没开”，防止重入重复发奖励 */
    bool bClaiming = false;
    /** 已完成的段数，等于 Def.Stages.Num() 表示全部解开 */
    int32 StageIndex = 0;
    /** 每段各自的进度：符文=点亮数、石碑=0未读/1读过/2挖出、规则与推箱=0/1 */
    TArray<uint8> StageProgress;
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

    /** 每段一套部件，下标与 Def.Stages 对齐 */
    UPROPERTY()
    TArray<FXiaomaChestStageVisuals> StageVisuals;
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
