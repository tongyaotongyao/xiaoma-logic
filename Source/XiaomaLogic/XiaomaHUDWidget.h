#pragma once

#include "CoreMinimal.h"
#include "Blueprint/UserWidget.h"
#include "Components/Button.h"
#include "XiaomaHUDWidget.generated.h"

class AXiaomaGameMode;
class UCanvasPanel;
class UTextBlock;
class UVerticalBox;

UCLASS()
class XIAOMALOGIC_API UXiaomaLevelButton : public UButton
{
    GENERATED_BODY()

public:
    int32 LevelNumber = 1;
    TWeakObjectPtr<class UXiaomaHUDWidget> OwnerWidget;
    void BindLevel();

private:
    UFUNCTION()
    void OnLevelClicked();
};

UCLASS()
class XIAOMALOGIC_API UXiaomaHUDWidget : public UUserWidget
{
    GENERATED_BODY()

public:
    void SetGame(AXiaomaGameMode* InGame);
    void Refresh();
    void SelectLevel(int32 Number);

protected:
    virtual void NativeConstruct() override;

private:
    UTextBlock* AddText(UVerticalBox* Parent, const FString& Text, int32 FontSize = 20,
        FLinearColor Colour = FLinearColor(FColor(55, 52, 42)));
    UButton* AddButton(UVerticalBox* Parent, const FString& Text, bool bPrimary = false);
    UVerticalBox* MakePanel(UCanvasPanel* Root, const FVector2D& Position,
        const FVector2D& Size, const FVector2D& Anchor, const FVector2D& Alignment);
    void BuildMenu(UCanvasPanel* Root);
    void BuildLevelSelect(UCanvasPanel* Root);
    void BuildSettings(UCanvasPanel* Root);
    void BuildGameHUD(UCanvasPanel* Root);
    void BuildOverlay(UCanvasPanel* Root, const FString& Title, const FString& Detail);

    UFUNCTION() void OnPlay();
    UFUNCTION() void OnLevels();
    UFUNCTION() void OnMenu();
    UFUNCTION() void OnResume();
    UFUNCTION() void OnPause();
    UFUNCTION() void OnUndo();
    UFUNCTION() void OnRestart();
    UFUNCTION() void OnHint();
    UFUNCTION() void OnSubmit();
    UFUNCTION() void OnNext();
    UFUNCTION() void OnSettings();
    UFUNCTION() void OnCloseSettings();
    UFUNCTION() void OnToggleFullscreen();
    UFUNCTION() void OnCycleSummonMode();
    UFUNCTION() void OnCycleTabCameraMode();
    UFUNCTION() void OnRebindInteract();
    UFUNCTION() void OnRebindMark();
    UFUNCTION() void OnRebindOverview();
    UFUNCTION() void OnRebindHint();
    UFUNCTION() void OnRebindUndo();
    UFUNCTION() void OnResetKeys();
    UFUNCTION() void OnQuit();
    UFUNCTION() void OnSound();

    TWeakObjectPtr<AXiaomaGameMode> Game;
};
