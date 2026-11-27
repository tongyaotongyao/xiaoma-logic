#pragma once

#include "CoreMinimal.h"
#include "XiaomaMechanism.h"
#include "XiaomaPatroller.generated.h"

class AXiaomaBoardActor;
class AXiaomaHerdsman;
class AXiaomaPatroller;

DECLARE_DELEGATE_OneParam(FXiaomaPatrollerHitDelegate, AXiaomaPatroller* /*Patroller*/);

/**
 * 巡逻捣蛋鬼（M4）：沿 levels.json 给出的路线在格子中心之间匀速往返。
 * - 撞到主角：由 GameMode 扣 1 命并把主角弹回本关起点；
 * - 前方格放着 X 标记：视为路障，掉头；
 * - 撤销或读档时回到路线起点重新走（策划案 4 节要求）。
 */
UCLASS()
class XIAOMALOGIC_API AXiaomaPatroller : public AXiaomaMechanism
{
    GENERATED_BODY()

public:
    AXiaomaPatroller();

    void SetupPatroller(const FXiaomaPatrolDef& InDef, const AXiaomaBoardActor* InBoard,
        AXiaomaHerdsman* InHerdsman);
    void SetOnHitHerdsman(FXiaomaPatrollerHitDelegate InDelegate) { OnHitHerdsman = InDelegate; }

    /** 暂停、菜单、结算时停下，避免在暂停界面里继续撞人 */
    void SetPatrolEnabled(bool bEnabledIn);

    /** 离当前格最近的那一格 */
    FIntPoint GetCurrentCell() const;

    void ResetToStart();

    virtual void Tick(float DeltaSeconds) override;
    virtual void OnBoardChanged(const FXiaomaLevel& Level, const TArray<uint8>& Cells) override;
    virtual void LoadState(const TArray<uint8>& In) override;

private:
    void BuildVisuals();
    void UpdateTransform();
    bool IsUpcomingCellMarked() const;
    void CheckHerdsmanHit();
    FVector CellLocation(FIntPoint Cell) const;

    FXiaomaPatrolDef Def;

    TWeakObjectPtr<const AXiaomaBoardActor> Board;
    TWeakObjectPtr<AXiaomaHerdsman> Herdsman;

    /** 最近一次棋盘快照，只用来识别 X 标记 */
    TArray<uint8> BoardCells;

    /** 沿路线的行程，单位是格；0 = 起点，Num()-1 = 终点 */
    float TravelDistance = 0.f;
    int32 MoveDirection = 1;
    float HitCooldown = 0.f;
    bool bPatrolEnabled = true;

    FXiaomaPatrollerHitDelegate OnHitHerdsman;

    UPROPERTY()
    TObjectPtr<USceneComponent> SceneRoot;

    UPROPERTY()
    TObjectPtr<UStaticMeshComponent> BodyMesh;

    UPROPERTY()
    TObjectPtr<UStaticMeshComponent> HatMesh;

    UPROPERTY()
    TObjectPtr<UStaticMeshComponent> EarLeftMesh;

    UPROPERTY()
    TObjectPtr<UStaticMeshComponent> EarRightMesh;

    UPROPERTY()
    TObjectPtr<UStaticMeshComponent> EyeLeftMesh;

    UPROPERTY()
    TObjectPtr<UStaticMeshComponent> EyeRightMesh;

    UPROPERTY()
    TObjectPtr<UMaterialInstanceDynamic> BodyMaterial;
};
