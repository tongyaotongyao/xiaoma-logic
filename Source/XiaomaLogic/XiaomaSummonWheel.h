#pragma once

#include "CoreMinimal.h"
#include "Blueprint/UserWidget.h"
#include "Components/Button.h"
#include "XiaomaSummonWheel.generated.h"

DECLARE_DELEGATE_OneParam(FXiaomaWheelSelectDelegate, int32 /*ColourIndex*/);
DECLARE_DELEGATE(FXiaomaWheelActionDelegate);

class UXiaomaSummonWheel;

UCLASS()
class XIAOMALOGIC_API UXiaomaWheelButton : public UButton
{
    GENERATED_BODY()

public:
    int32 ColourIndex = -1;
    TWeakObjectPtr<UXiaomaSummonWheel> OwnerWheel;
    void BindClick();

private:
    UFUNCTION()
    void OnClickedInternal();
};

/**
 * 召唤轮盘：按 E 打开，鼠标选择 6 色小马或请回当前格小马。
 */
UCLASS()
class XIAOMALOGIC_API UXiaomaSummonWheel : public UUserWidget
{
    GENERATED_BODY()

public:
    void OpenWheel(const TArray<int32>& PlacedCellByRegion, bool bHasPonyAtCurrentCell, int32 CurrentRegion);
    void HandleButtonClick(int32 ColourIndex);
    /** 松开按键时确认当前鼠标悬停的扇区或关闭轮盘 */
    void ConfirmHoveredSelection();

    FXiaomaWheelSelectDelegate OnSelectColour;
    FXiaomaWheelActionDelegate OnRecall;
    FXiaomaWheelActionDelegate OnClose;

protected:
    virtual FReply NativeOnMouseButtonDown(const FGeometry& InGeometry, const FPointerEvent& InMouseEvent) override;

private:
    UFUNCTION()
    void OnCenterClicked();

    UPROPERTY()
    TArray<TObjectPtr<UXiaomaWheelButton>> SectorButtons;

    UPROPERTY()
    TObjectPtr<UButton> CenterButton;

    bool bHasPonyHere = false;
};
