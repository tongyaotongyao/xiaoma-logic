#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "XiaomaIceField.generated.h"

class AXiaomaBoardActor;
class UStaticMeshComponent;

/**
 * 冰面（M4）：只负责把 levels.json 里配的 ice 格子画出来，不参与任何状态。
 * - 每块冰是一片浅蓝圆盘，相邻冰格之间用细横条连起来，看起来是一条整冰道；
 * - 不挡路也不挡点击：主角照样走进去，鼠标射线照样打得到下面的地板；
 * - 真正的"滑"在 AXiaomaHerdsman 里，GameMode 只把「这一格是不是冰」告诉它。
 */
UCLASS()
class XIAOMALOGIC_API AXiaomaIceField : public AActor
{
    GENERATED_BODY()

public:
    AXiaomaIceField();

    void SetupIce(const TArray<FIntPoint>& Cells, const AXiaomaBoardActor* InBoard);

private:
    UStaticMeshComponent* AddPart(const TCHAR* BaseName, UStaticMesh* Mesh,
        const FVector& Location, const FVector& Scale, const FColor& Colour);

    /** 格子在冰道里能否朝某个格子方向连出去 */
    bool HasIceAt(const TArray<FIntPoint>& Cells, FIntPoint Cell) const;

    UPROPERTY()
    TObjectPtr<USceneComponent> SceneRoot;

    UPROPERTY()
    TArray<TObjectPtr<UStaticMeshComponent>> Parts;
};
