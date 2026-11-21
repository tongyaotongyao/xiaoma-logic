#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Character.h"
#include "XiaomaHerdsman.generated.h"

class USpringArmComponent;
class UCameraComponent;
class UStaticMeshComponent;

DECLARE_DELEGATE_OneParam(FXiaomaHerdsmanCellDelegate, FIntPoint /*NewCell*/);

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

    void SetOnCellChanged(FXiaomaHerdsmanCellDelegate InDelegate) { OnCellChanged = InDelegate; }
    virtual void Tick(float DeltaSeconds) override;

private:
    void BuildVisuals();
    void UpdateCurrentCell();
    void UpdateCamera(float DeltaSeconds);

    static FVector CellToWorld(FIntPoint Cell);
    static FIntPoint WorldToRawCell(const FVector& Location);

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
};
