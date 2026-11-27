#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Character.h"
#include "XiaomaHerdsman.generated.h"

class USpringArmComponent;
class UCameraComponent;
class UStaticMeshComponent;

DECLARE_DELEGATE_OneParam(FXiaomaHerdsmanCellDelegate, FIntPoint /*NewCell*/);
DECLARE_DELEGATE_RetVal_TwoParams(bool, FXiaomaHerdsmanPushDelegate, FIntPoint /*挡住去路的格*/, FIntPoint /*推动方向，每轴 -1/0/1*/);

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

    void SetOnCellChanged(FXiaomaHerdsmanCellDelegate InDelegate) { OnCellChanged = InDelegate; }
    virtual void Tick(float DeltaSeconds) override;

private:
    void BuildVisuals();
    void UpdateCurrentCell();
    void UpdateCamera(float DeltaSeconds);

    static FVector CellToWorld(FIntPoint Cell);
    static FIntPoint WorldToRawCell(const FVector& Location);
    /** 目标点能走就返回 true；被挡住时问一次推箱回调，箱子让开也算能走 */
    bool CanEnterOrPush(const FVector& TargetLocation, FIntPoint StepDelta);

    UPROPERTY()
    TObjectPtr<USpringArmComponent> CameraBoom;

    UPROPERTY()
    TObjectPtr<UCameraComponent> FollowCamera;

    UPROPERTY()
    TObjectPtr<USceneComponent> VisualRoot;

    FIntPoint CurrentCell = FIntPoint(5, 0);
    FVector2D PendingInput = FVector2D::ZeroVector;
    bool bOverviewCamera = false;
    bool bMovementEnabled = true;
    float BaseMoveSpeed = 340.f;
    float BoostRemaining = 0.f;
    float WalkCycle = 0.f;
    FXiaomaHerdsmanCellDelegate OnCellChanged;
    TFunction<bool(FIntPoint)> CellBlocked;
    FXiaomaHerdsmanPushDelegate CellPush;
};
