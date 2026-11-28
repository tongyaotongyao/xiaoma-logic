#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Character.h"
#include "XiaomaHerdsman.generated.h"

class USpringArmComponent;
class UCameraComponent;
class UStaticMeshComponent;

DECLARE_DELEGATE_OneParam(FXiaomaHerdsmanCellDelegate, FIntPoint /*NewCell*/);
DECLARE_DELEGATE_RetVal_TwoParams(bool, FXiaomaHerdsmanPushDelegate, FIntPoint /*挡住去路的格*/, FIntPoint /*推动方向，每轴 -1/0/1*/);
DECLARE_DELEGATE(FXiaomaHerdsmanSlideDelegate);

/**
 * 牧马小精灵主角：自由移动、跟随相机与 Tab 全局俯视、格子坐标换算与缓冲。
 */
UCLASS()
class XIAOMALOGIC_API AXiaomaHerdsman : public ACharacter
{
    GENERATED_BODY()

public:
    AXiaomaHerdsman();

    void MoveForwardInput(float Value);
    void MoveRightInput(float Value);

    void TeleportToCell(FIntPoint Cell);
    void SetOverviewCamera(bool bEnable);
    void SetMovementEnabled(bool bEnabled);
    void SetSpeedBoost(float DurationSeconds);
    bool IsSpeedBoostActive() const { return BoostRemaining > 0.f; }
    float GetBoostRemaining() const { return BoostRemaining; }

    FIntPoint GetCurrentCell() const { return CurrentCell; }
    int32 GetCurrentCellIndex() const { return CurrentCell.X * 6 + CurrentCell.Y; }
    /**
     * 进入 CurrentCell 之前所在的那一格。
     * 塌陷木桥用它记住"从哪一格上的桥"，撤销时好把主角放回上桥前的位置。
     * 传送（撤销/读档/挨撞弹回）之后两者一起对齐，避免把传送前那一格当成来路。
     */
    FIntPoint GetPreviousCell() const { return PreviousCell; }

    /** 由 GameMode 注入：某一格当前是否被机关封住（障碍、关上的围栏等） */
    void SetCellBlockedPredicate(TFunction<bool(FIntPoint)> InPredicate)
    {
        CellBlocked = MoveTemp(InPredicate);
    }
    bool IsBlockedAt(const FVector& Location) const;

    /** 由 GameMode 注入：被木箱挡住时尝试推箱，返回 true 表示箱子已让开、可以继续走 */
    void SetCellPushHandler(FXiaomaHerdsmanPushDelegate InHandler)
    {
        CellPush = MoveTemp(InHandler);
    }

    /** 由 GameMode 注入：某一格是不是冰面（关卡数据里配的 ice） */
    void SetIcePredicate(TFunction<bool(FIntPoint)> InPredicate)
    {
        IceCell = MoveTemp(InPredicate);
    }
    /** 正在冰上滑行：输入被锁死，撞到东西或滑出冰面才停 */
    bool IsSliding() const { return bSliding; }

    void SetOnCellChanged(FXiaomaHerdsmanCellDelegate InDelegate) { OnCellChanged = InDelegate; }
    /**
     * 由 GameMode 注入：刚开始滑行时广播一次。滑行是一整段不可控的位移，
     * 玩家按 Z 只能退出到踩上冰面那一刻，所以要在这里补一个撤销点。
     */
    void SetOnSlideBegin(FXiaomaHerdsmanSlideDelegate InDelegate) { OnSlideBegin = InDelegate; }
    virtual void Tick(float DeltaSeconds) override;

private:
    void BuildVisuals();
    void UpdateCurrentCell();
    void UpdateCamera(float DeltaSeconds);

    static FVector CellToWorld(FIntPoint Cell);
    static FIntPoint WorldToRawCell(const FVector& Location);
    /** 目标点能走就返回 true；被挡住时问一次推箱回调，箱子让开也算能走 */
    bool CanEnterOrPush(const FVector& TargetLocation, FIntPoint StepDelta);

    bool IsIceAt(FIntPoint Cell) const;
    /** 踩上冰面：按输入的主轴锁定滑行方向，之后不再听玩家输入 */
    void BeginSlide(FVector2D Dir, const FVector& Location);
    void EndSlide();
    /** 滑行结束可能停在格子边上，强制同步一次所在格，别让高亮留在上一格 */
    void SyncCurrentCellNow();

    UPROPERTY()
    TObjectPtr<USpringArmComponent> CameraBoom;

    UPROPERTY()
    TObjectPtr<UCameraComponent> FollowCamera;

    UPROPERTY()
    TObjectPtr<USceneComponent> VisualRoot;

    FIntPoint CurrentCell = FIntPoint(5, 0);
    /** 进入当前格之前所在的那一格（见 GetPreviousCell 的说明） */
    FIntPoint PreviousCell = FIntPoint(5, 0);
    FVector2D PendingInput = FVector2D::ZeroVector;
    bool bOverviewCamera = false;
    bool bMovementEnabled = true;
    float BaseMoveSpeed = 340.f;
    float BoostRemaining = 0.f;
    float WalkCycle = 0.f;
    FXiaomaHerdsmanCellDelegate OnCellChanged;
    FXiaomaHerdsmanSlideDelegate OnSlideBegin;
    TFunction<bool(FIntPoint)> CellBlocked;
    FXiaomaHerdsmanPushDelegate CellPush;
    TFunction<bool(FIntPoint)> IceCell;

    /** 滑行：方向锁在世界轴的正负四向之一，滑出冰面或撞墙才结束 */
    bool bSliding = false;
    FVector2D SlideDir = FVector2D::ZeroVector;
};
