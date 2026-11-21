#pragma once

#include "CoreMinimal.h"
#include "GameFramework/PlayerController.h"
#include "XiaomaPlayerController.generated.h"

struct FInputKeyParams;

UCLASS()
class XIAOMALOGIC_API AXiaomaPlayerController : public APlayerController
{
    GENERATED_BODY()

public:
    AXiaomaPlayerController();
    virtual void BeginPlay() override;
    virtual void SetupInputComponent() override;
    virtual void PlayerTick(float DeltaTime) override;
    virtual bool InputKey(const FInputKeyParams& Params) override;

private:
    void OnLeftClick();
    void OnRightClick();
    void Restart();
    void Submit();
    void Escape();
};
