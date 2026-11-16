#include "XiaomaHUDWidget.h"

#include "XiaomaGameMode.h"
#include "Blueprint/WidgetTree.h"
#include "Components/Border.h"
#include "Components/CanvasPanel.h"
#include "Components/CanvasPanelSlot.h"
#include "Components/TextBlock.h"
#include "Components/UniformGridPanel.h"
#include "Components/VerticalBox.h"
#include "Components/VerticalBoxSlot.h"
#include "Kismet/KismetSystemLibrary.h"

void UXiaomaLevelButton::BindLevel()
{
    OnClicked.AddDynamic(this, &UXiaomaLevelButton::OnLevelClicked);
}

void UXiaomaLevelButton::OnLevelClicked()
{
    if (OwnerWidget.IsValid())
    {
        OwnerWidget->SelectLevel(LevelNumber);
    }
}

void UXiaomaHUDWidget::NativeConstruct()
{
    Super::NativeConstruct();
    Refresh();
}

UTextBlock* UXiaomaHUDWidget::AddText(UVerticalBox* Parent, const FString& Text,
    int32 FontSize, FLinearColor Colour)
{
    UTextBlock* Label = WidgetTree->ConstructWidget<UTextBlock>();
    Label->SetText(FText::FromString(Text));
    FSlateFontInfo Font = Label->GetFont();
    Font.Size = FontSize;
    Label->SetFont(Font);
    Label->SetColorAndOpacity(FSlateColor(Colour));
    Label->SetAutoWrapText(true);
    UVerticalBoxSlot* Slot = Parent->AddChildToVerticalBox(Label);
    Slot->SetPadding(FMargin(4.f, 6.f));
    return Label;
}

UButton* UXiaomaHUDWidget::AddButton(UVerticalBox* Parent, const FString& Text)
{
    UButton* Button = WidgetTree->ConstructWidget<UButton>();
    Button->SetBackgroundColor(FLinearColor(0.32f, 0.60f, 0.72f, 1.f));
    UTextBlock* Label = WidgetTree->ConstructWidget<UTextBlock>();
    Label->SetText(FText::FromString(Text));
    Label->SetJustification(ETextJustify::Center);
    FSlateFontInfo Font = Label->GetFont();
    Font.Size = 20;
    Label->SetFont(Font);
    Label->SetColorAndOpacity(FSlateColor(FLinearColor::White));
    Button->AddChild(Label);
    UVerticalBoxSlot* Slot = Parent->AddChildToVerticalBox(Button);
    Slot->SetPadding(FMargin(4.f, 5.f));
    return Button;
}

UVerticalBox* UXiaomaHUDWidget::MakePanel(UCanvasPanel* Root,
    const FVector2D& Position, const FVector2D& Size, bool bCenter)
{
    UBorder* Border = WidgetTree->ConstructWidget<UBorder>();
    Border->SetBrushColor(FLinearColor(0.06f, 0.10f, 0.17f, 0.93f));
    Border->SetPadding(FMargin(16.f));
    UCanvasPanelSlot* Slot = Root->AddChildToCanvas(Border);
    if (bCenter)
    {
        Slot->SetAnchors(FAnchors(0.5f, 0.5f));
        Slot->SetAlignment(FVector2D(0.5f, 0.5f));
    }
    else
    {
        Slot->SetAnchors(FAnchors(0.f, 0.f));
    }
    Slot->SetPosition(Position);
    Slot->SetSize(Size);
    UVerticalBox* Panel = WidgetTree->ConstructWidget<UVerticalBox>();
    Border->AddChild(Panel);
    return Panel;
}

void UXiaomaHUDWidget::BuildMenu(UCanvasPanel* Root)
{
    UVerticalBox* Panel = MakePanel(Root, FVector2D::ZeroVector, FVector2D(430.f, 390.f), true);
    AddText(Panel, TEXT("小马逻辑"), 42, FLinearColor(1.f, 0.82f, 0.47f));
    AddText(Panel, TEXT("在六色棋盘上找出全部小马"), 21);
    if (Game.IsValid())
    {
        AddText(Panel, FString::Printf(TEXT("已解锁第 %d 关 · 好感度 %d"),
            Game->GetUnlockedLevel(), Game->GetAffection()), 18);
    }
    AddButton(Panel, TEXT("继续游戏"))->OnClicked.AddDynamic(this, &UXiaomaHUDWidget::OnPlay);
    AddButton(Panel, TEXT("选择关卡"))->OnClicked.AddDynamic(this, &UXiaomaHUDWidget::OnLevels);
    AddButton(Panel, TEXT("退出游戏"))->OnClicked.AddDynamic(this, &UXiaomaHUDWidget::OnQuit);
}

void UXiaomaHUDWidget::BuildLevelSelect(UCanvasPanel* Root)
{
    UVerticalBox* Panel = MakePanel(Root, FVector2D::ZeroVector, FVector2D(620.f, 600.f), true);
    AddText(Panel, TEXT("选择关卡"), 34, FLinearColor(1.f, 0.82f, 0.47f));
    UUniformGridPanel* Grid = WidgetTree->ConstructWidget<UUniformGridPanel>();
    UVerticalBoxSlot* GridSlot = Panel->AddChildToVerticalBox(Grid);
    GridSlot->SetPadding(FMargin(0.f, 12.f));
    for (int32 Number = 1; Number <= 25; ++Number)
    {
        UXiaomaLevelButton* Button = WidgetTree->ConstructWidget<UXiaomaLevelButton>();
        Button->LevelNumber = Number;
        Button->OwnerWidget = this;
        Button->SetIsEnabled(Game.IsValid() && Number <= Game->GetUnlockedLevel());
        Button->SetBackgroundColor(Number <= (Game.IsValid() ? Game->GetUnlockedLevel() : 0)
            ? FLinearColor(0.30f, 0.55f, 0.69f) : FLinearColor(0.30f, 0.32f, 0.37f));
        UTextBlock* Label = WidgetTree->ConstructWidget<UTextBlock>();
        Label->SetJustification(ETextJustify::Center);
        Label->SetText(FText::FromString(FString::Printf(TEXT("%02d  %s"), Number,
            Game.IsValid() && Game->GetLevelStars(Number) > 0 ? TEXT("★") : TEXT(""))));
        Label->SetColorAndOpacity(FSlateColor(FLinearColor::White));
        Button->AddChild(Label);
        Button->BindLevel();
        Grid->AddChildToUniformGrid(Button, (Number - 1) / 5, (Number - 1) % 5);
    }
    AddButton(Panel, TEXT("返回主菜单"))->OnClicked.AddDynamic(this, &UXiaomaHUDWidget::OnMenu);
}

void UXiaomaHUDWidget::BuildGameHUD(UCanvasPanel* Root)
{
    if (!Game.IsValid())
    {
        return;
    }
    UVerticalBox* Panel = MakePanel(Root, FVector2D(18.f, 18.f), FVector2D(280.f, 640.f), false);
    AddText(Panel, FString::Printf(TEXT("第 %d / 25 关"), Game->GetLevelNumber()), 28,
        FLinearColor(1.f, 0.82f, 0.47f));
    AddText(Panel, FString::Printf(TEXT("生命  %d / 2"), Game->GetLives()), 19);
    AddText(Panel, FString::Printf(TEXT("本关星星  %d / 3"), Game->GetCurrentStars()), 18);
    AddText(Panel, FString::Printf(TEXT("好感度  %d"), Game->GetAffection()), 18);
    AddText(Panel, FString::Printf(TEXT("连胜  %d  ·  最佳 %d"),
        Game->GetWinStreak(), Game->GetBestStreak()), 18);
    AddText(Panel, Game->GetStatusMessage(), 17, FLinearColor(0.78f, 0.92f, 1.f));
    AddButton(Panel, Game->IsMarkMode() ? TEXT("标记模式：开") : TEXT("标记模式：关"))
        ->OnClicked.AddDynamic(this, &UXiaomaHUDWidget::OnMarkMode);
    AddButton(Panel, TEXT("撤销  Z"))->OnClicked.AddDynamic(this, &UXiaomaHUDWidget::OnUndo);
    AddButton(Panel, TEXT("重开  R"))->OnClicked.AddDynamic(this, &UXiaomaHUDWidget::OnRestart);
    AddButton(Panel, TEXT("提示  H"))->OnClicked.AddDynamic(this, &UXiaomaHUDWidget::OnHint);
    AddButton(Panel, TEXT("提交答案  Enter"))->OnClicked.AddDynamic(this, &UXiaomaHUDWidget::OnSubmit);
    AddButton(Panel, TEXT("暂停  Esc"))->OnClicked.AddDynamic(this, &UXiaomaHUDWidget::OnPause);
}

void UXiaomaHUDWidget::BuildOverlay(UCanvasPanel* Root,
    const FString& Title, const FString& Detail)
{
    UVerticalBox* Panel = MakePanel(Root, FVector2D::ZeroVector, FVector2D(440.f, 310.f), true);
    AddText(Panel, Title, 34, FLinearColor(1.f, 0.82f, 0.47f));
    AddText(Panel, Detail, 19);
    if (!Game.IsValid())
    {
        return;
    }
    if (Game->GetScreen() == EXiaomaScreen::Paused)
    {
        AddButton(Panel, TEXT("继续"))->OnClicked.AddDynamic(this, &UXiaomaHUDWidget::OnResume);
    }
    if (Game->GetScreen() == EXiaomaScreen::Won)
    {
        AddButton(Panel, TEXT("下一关"))->OnClicked.AddDynamic(this, &UXiaomaHUDWidget::OnNext);
    }
    if (Game->GetScreen() == EXiaomaScreen::Lost)
    {
        AddButton(Panel, TEXT("再试一次"))->OnClicked.AddDynamic(this, &UXiaomaHUDWidget::OnRestart);
    }
    AddButton(Panel, TEXT("选择关卡"))->OnClicked.AddDynamic(this, &UXiaomaHUDWidget::OnLevels);
    AddButton(Panel, TEXT("主菜单"))->OnClicked.AddDynamic(this, &UXiaomaHUDWidget::OnMenu);
}

void UXiaomaHUDWidget::Refresh()
{
    if (!WidgetTree || !Game.IsValid())
    {
        return;
    }
    UCanvasPanel* Root = Cast<UCanvasPanel>(WidgetTree->RootWidget);
    if (!Root)
    {
        Root = WidgetTree->ConstructWidget<UCanvasPanel>();
        WidgetTree->RootWidget = Root;
    }
    Root->ClearChildren();
    switch (Game->GetScreen())
    {
    case EXiaomaScreen::Menu: BuildMenu(Root); break;
    case EXiaomaScreen::LevelSelect: BuildLevelSelect(Root); break;
    case EXiaomaScreen::Playing: BuildGameHUD(Root); break;
    case EXiaomaScreen::Paused: BuildOverlay(Root, TEXT("已暂停"), TEXT("休息一下，随时继续。")); break;
    case EXiaomaScreen::Won: BuildOverlay(Root, TEXT("通关！"), Game->GetStatusMessage()); break;
    case EXiaomaScreen::Lost: BuildOverlay(Root, TEXT("再试一次"), Game->GetStatusMessage()); break;
    }
}

void UXiaomaHUDWidget::SelectLevel(int32 Number) { if (Game.IsValid()) Game->StartLevel(Number); }
void UXiaomaHUDWidget::OnPlay() { if (Game.IsValid()) Game->StartLevel(Game->GetContinueLevel()); }
void UXiaomaHUDWidget::OnLevels() { if (Game.IsValid()) Game->ShowLevelSelect(); }
void UXiaomaHUDWidget::OnMenu() { if (Game.IsValid()) Game->ShowMenu(); }
void UXiaomaHUDWidget::OnResume() { if (Game.IsValid()) Game->ResumeGame(); }
void UXiaomaHUDWidget::OnPause() { if (Game.IsValid()) Game->PauseGame(); }
void UXiaomaHUDWidget::OnUndo() { if (Game.IsValid()) Game->Undo(); }
void UXiaomaHUDWidget::OnRestart() { if (Game.IsValid()) Game->RestartLevel(); }
void UXiaomaHUDWidget::OnHint() { if (Game.IsValid()) Game->UseHint(); }
void UXiaomaHUDWidget::OnSubmit() { if (Game.IsValid()) Game->SubmitAnswer(); }
void UXiaomaHUDWidget::OnMarkMode() { if (Game.IsValid()) Game->ToggleMarkMode(); }
void UXiaomaHUDWidget::OnNext() { if (Game.IsValid()) Game->NextLevel(); }
void UXiaomaHUDWidget::OnQuit()
{
    if (APlayerController* Controller = GetOwningPlayer())
    {
        UKismetSystemLibrary::QuitGame(this, Controller, EQuitPreference::Quit, false);
    }
}
