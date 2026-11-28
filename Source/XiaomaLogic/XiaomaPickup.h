#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "XiaomaPuzzle.h"
#include "XiaomaPickup.generated.h"

class UStaticMeshComponent;

DECLARE_DELEGATE_TwoParams(FXiaomaPickupCollectedDelegate, EXiaomaItemType /*ItemType*/, FIntPoint /*Cell*/);

/**
 * 场上可拾取道具（苹果、胡萝卜、四叶草、提示星），支持悬浮旋转与撤销恢复。
 */
UCLASS()
class XIAOMALOGIC_API AXiaomaPickup : public AActor
{
    GENERATED_BODY()

public:
    AXiaomaPickup();

    void SetupPickup(const FXiaomaItemDef& InDef, const FVector& WorldLocation);
    void SetOnCollected(FXiaomaPickupCollectedDelegate InDelegate) { OnCollected = InDelegate; }

    bool IsCollected() const { return bCollected; }
    void SetCollected(bool bInCollected);
    FIntPoint GetCell() const { return Def.Cell; }
    EXiaomaItemType GetItemType() const { return Def.Type; }

    void OnHerdsmanEnterCell(FIntPoint Cell);
    virtual void Tick(float DeltaSeconds) override;

private:
    FXiaomaItemDef Def;
    bool bCollected = false;
    /** 正在发拾取通知：通知期间状态还是“没捡”，防止重入重复发奖励 */
    bool bCollecting = false;
    float ElapsedTime = 0.f;
    FXiaomaPickupCollectedDelegate OnCollected;

    UPROPERTY()
    TObjectPtr<USceneComponent> SceneRoot;

    UPROPERTY()
    TObjectPtr<UStaticMeshComponent> BodyMesh;

    UPROPERTY()
    TObjectPtr<UStaticMeshComponent> AccentMesh;
};
