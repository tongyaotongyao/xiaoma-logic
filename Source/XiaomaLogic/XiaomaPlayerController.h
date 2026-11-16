#pragma once

#include "CoreMinimal.h"
#include "GameFramework/PlayerController.h"
#include "XiaomaPlayerController.generated.h"

UCLASS()
class XIAOMALOGIC_API AXiaomaPlayerController : public APlayerController
{
    GENERATED_BODY()

public:
    AXiaomaPlayerController();
    virtual void BeginPlay() override;
    virtual void SetupInputComponent() override;

private:
    void ClickCell();
    void MarkCell();
    void UndoMove();
    void Restart();
    void Hint();
    void Submit();
    void Escape();
};
