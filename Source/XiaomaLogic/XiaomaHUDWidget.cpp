#include "XiaomaHUDWidget.h"

#include "XiaomaGameMode.h"
#include "XiaomaPalette.h"
#include "Blueprint/WidgetTree.h"
#include "Brushes/SlateRoundedBoxBrush.h"
#include "Components/Border.h"
#include "Components/CanvasPanel.h"
#include "Components/CanvasPanelSlot.h"
#include "Components/TextBlock.h"
#include "Components/UniformGridPanel.h"
#include "Components/VerticalBox.h"
#include "Components/VerticalBoxSlot.h"
#include "Kismet/KismetSystemLibrary.h"

namespace
{
const FLinearColor Paper(FColor(255, 249, 233));
const FLinearColor Ink(FColor(55, 52, 42));
const FLinearColor Muted(FColor(103, 96, 75));
const FLinearColor Green(FColor(37, 83, 64));
const FLinearColor CreamLine(FColor(226, 210, 176));
const FLinearColor Gold(FColor(175, 97, 42));

void StyleButton(UButton& Button, bool bPrimary)
{
    FButtonStyle Style;
    Style.SetNormal(FSlateRoundedBoxBrush(
        bPrimary ? Green : FLinearColor(FColor(245, 235, 211)), 12.f));
    Style.SetHovered(FSlateRoundedBoxBrush(
        bPrimary ? FLinearColor(FColor(48, 107, 79)) : FLinearColor(FColor(236, 221, 187)), 12.f));
    Style.SetPressed(FSlateRoundedBoxBrush(
        bPrimary ? FLinearColor(FColor(27, 62, 50)) : CreamLine, 12.f));
    Style.SetDisabled(FSlateRoundedBoxBrush(FLinearColor(FColor(233, 227, 211)), 12.f));
    Style.SetNormalPadding(FMargin(11.f, 9.f));
    Style.SetPressedPadding(FMargin(11.f, 10.f, 11.f, 8.f));
    Button.SetStyle(Style);
}
}

void UXiaomaHUDWidget::SetGame(AXiaomaGameMode* InGame)
{
    Game = InGame;
}

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
    if (FontSize >= 24)
    {
        Font.TypefaceFontName = TEXT("Bold");
    }
    Label->SetFont(Font);
    Label->SetColorAndOpacity(FSlateColor(Colour));
    Label->SetAutoWrapText(true);
    UVerticalBoxSlot* BoxSlot = Parent->AddChildToVerticalBox(Label);
    BoxSlot->SetPadding(FMargin(0.f, 4.f));
    return Label;
}

UButton* UXiaomaHUDWidget::AddButton(UVerticalBox* Parent, const FString& Text, bool bPrimary)
{
    UButton* Button = WidgetTree->ConstructWidget<UButton>();
    StyleButton(*Button, bPrimary);
    UTextBlock* Label = WidgetTree->ConstructWidget<UTextBlock>();
    Label->SetText(FText::FromString(Text));
    Label->SetJustification(ETextJustify::Center);
    FSlateFontInfo Font = Label->GetFont();
    Font.Size = bPrimary ? 21 : 18;
    Label->SetFont(Font);
    Label->SetColorAndOpacity(FSlateColor(bPrimary ? Paper : Ink));
    Button->AddChild(Label);
    UVerticalBoxSlot* BoxSlot = Parent->AddChildToVerticalBox(Button);
    BoxSlot->SetPadding(FMargin(0.f, 3.f));
    return Button;
}

UVerticalBox* UXiaomaHUDWidget::MakePanel(UCanvasPanel* Root,
    const FVector2D& Position, const FVector2D& Size,
    const FVector2D& Anchor, const FVector2D& Alignment)
{
    UBorder* Shadow = WidgetTree->ConstructWidget<UBorder>();
    Shadow->SetBrush(FSlateRoundedBoxBrush(FLinearColor(FColor(35, 30, 20, 45)), 21.f));
    Shadow->SetVisibility(ESlateVisibility::HitTestInvisible);
    UCanvasPanelSlot* ShadowSlot = Root->AddChildToCanvas(Shadow);
    ShadowSlot->SetAnchors(FAnchors(Anchor.X, Anchor.Y));
    ShadowSlot->SetAlignment(Alignment);
    ShadowSlot->SetPosition(Position + FVector2D(0.f, 6.f));
    ShadowSlot->SetSize(Size);

    UBorder* Border = WidgetTree->ConstructWidget<UBorder>();
    Border->SetBrush(FSlateRoundedBoxBrush(Paper, 21.f, CreamLine, 1.f));
    Border->SetPadding(FMargin(24.f, 19.f));
    Border->SetVisibility(ESlateVisibility::SelfHitTestInvisible);
    UCanvasPanelSlot* CanvasSlot = Root->AddChildToCanvas(Border);
    CanvasSlot->SetAnchors(FAnchors(Anchor.X, Anchor.Y));
    CanvasSlot->SetAlignment(Alignment);
    CanvasSlot->SetPosition(Position);
    CanvasSlot->SetSize(Size);
    UVerticalBox* Panel = WidgetTree->ConstructWidget<UVerticalBox>();
    Border->AddChild(Panel);
    return Panel;
}

void UXiaomaHUDWidget::BuildMenu(UCanvasPanel* Root)
{
    UVerticalBox* Panel = MakePanel(Root, FVector2D(92.f, 0.f), FVector2D(446.f, 432.f),
        FVector2D(0.f, 0.5f), FVector2D(0.f, 0.5f));
    AddText(Panel, TEXT("小马逻辑"), 50, Green);
    AddText(Panel, TEXT("在六色棋盘上找出全部小马"), 19, Muted);
    if (Game.IsValid())
    {
        AddText(Panel, FString::Printf(TEXT("已解锁第 %d 关 · 好感度 %d"),
            Game->GetUnlockedLevel(), Game->GetAffection()), 16, Gold);
    }
    // 按钮直接写出要进哪一关：既省掉玩家的猜测，也让「继续到哪」这个行为本身可核对
    const FString PlayLabel = Game.IsValid()
        ? FString::Printf(TEXT("继续游戏（第 %d 关）"), Game->GetContinueLevel())
        : FString(TEXT("继续游戏"));
    AddButton(Panel, PlayLabel, true)->OnClicked.AddDynamic(this, &UXiaomaHUDWidget::OnPlay);
    AddButton(Panel, TEXT("选择关卡"))->OnClicked.AddDynamic(this, &UXiaomaHUDWidget::OnLevels);
    AddButton(Panel, TEXT("设置"))->OnClicked.AddDynamic(this, &UXiaomaHUDWidget::OnSettings);
    AddButton(Panel, TEXT("退出游戏"))->OnClicked.AddDynamic(this, &UXiaomaHUDWidget::OnQuit);
}

void UXiaomaHUDWidget::BuildLevelSelect(UCanvasPanel* Root)
{
    UVerticalBox* Panel = MakePanel(Root, FVector2D::ZeroVector, FVector2D(490.f, 422.f),
        FVector2D(0.5f), FVector2D(0.5f));
    AddText(Panel, TEXT("选择关卡"), 34, Green);
    UUniformGridPanel* Grid = WidgetTree->ConstructWidget<UUniformGridPanel>();
    Grid->SetSlotPadding(FMargin(4.f));
    Grid->SetMinDesiredSlotWidth(74.f);
    Grid->SetMinDesiredSlotHeight(46.f);
    UVerticalBoxSlot* GridSlot = Panel->AddChildToVerticalBox(Grid);
    GridSlot->SetPadding(FMargin(0.f, 12.f));
    for (int32 Number = 1; Number <= 25; ++Number)
    {
        UXiaomaLevelButton* Button = WidgetTree->ConstructWidget<UXiaomaLevelButton>();
        Button->LevelNumber = Number;
        Button->OwnerWidget = this;
        Button->SetIsEnabled(Game.IsValid() && Number <= Game->GetUnlockedLevel());
        const bool bUnlocked = Button->GetIsEnabled();
        StyleButton(*Button, bUnlocked);
        UTextBlock* Label = WidgetTree->ConstructWidget<UTextBlock>();
        Label->SetJustification(ETextJustify::Center);
        Label->SetText(FText::FromString(FString::Printf(TEXT("%02d  %s"), Number,
            Game.IsValid() && Game->GetLevelStars(Number) > 0 ? TEXT("★") : TEXT(""))));
        Label->SetColorAndOpacity(FSlateColor(bUnlocked ? Paper : Muted));
        Button->AddChild(Label);
        Button->BindLevel();
        Grid->AddChildToUniformGrid(Button, (Number - 1) / 5, (Number - 1) % 5);
    }
    AddButton(Panel, TEXT("返回主菜单"))->OnClicked.AddDynamic(this, &UXiaomaHUDWidget::OnMenu);
}

void UXiaomaHUDWidget::BuildSettings(UCanvasPanel* Root)
{
    UVerticalBox* Panel = MakePanel(Root, FVector2D::ZeroVector, FVector2D(460.f, 640.f),
        FVector2D(0.5f), FVector2D(0.5f));
    AddText(Panel, TEXT("设置"), 32, Green);

    const bool bFull = Game.IsValid() && Game->IsFullscreen();
    AddButton(Panel, bFull ? TEXT("显示模式：全屏") : TEXT("显示模式：窗口"))
        ->OnClicked.AddDynamic(this, &UXiaomaHUDWidget::OnToggleFullscreen);
    AddButton(Panel, Game.IsValid() && Game->IsSoundEnabled() ? TEXT("音效：开") : TEXT("音效：关"))
        ->OnClicked.AddDynamic(this, &UXiaomaHUDWidget::OnSound);

    const EXiaomaSummonMode SMode = Game.IsValid() ? Game->GetSummonMode() : EXiaomaSummonMode::HoldWheel;
    const TCHAR* SText = SMode == EXiaomaSummonMode::HoldWheel ? TEXT("召唤方式：长按轮盘（松手确认）")
        : (SMode == EXiaomaSummonMode::ToggleWheel ? TEXT("召唤方式：点击切换轮盘") : TEXT("召唤方式：直接放当前色"));
    AddButton(Panel, SText)->OnClicked.AddDynamic(this, &UXiaomaHUDWidget::OnCycleSummonMode);

    const EXiaomaTabCameraMode CMode = Game.IsValid() ? Game->GetTabCameraMode() : EXiaomaTabCameraMode::Toggle;
    AddButton(Panel, CMode == EXiaomaTabCameraMode::Toggle ? TEXT("视角切换：按一下切换") : TEXT("视角切换：按住看全局"))
        ->OnClicked.AddDynamic(this, &UXiaomaHUDWidget::OnCycleTabCameraMode);

    AddText(Panel, TEXT("键位设置（点击后按键盘修改）"), 16, Gold);
    const EXiaomaBindAction Pending = Game.IsValid() ? Game->GetPendingRebind() : EXiaomaBindAction::None;
    auto KeyLabel = [&](const TCHAR* Label, EXiaomaBindAction Act)
    {
        if (Pending == Act)
        {
            return FString::Printf(TEXT("%s：请按新按键..."), Label);
        }
        const FString KeyStr = Game.IsValid() ? Game->GetKeyForAction(Act).GetDisplayName().ToString() : TEXT("-");
        return FString::Printf(TEXT("%s：%s"), Label, *KeyStr);
    };
    AddButton(Panel, KeyLabel(TEXT("召唤 / 交互"), EXiaomaBindAction::Interact))
        ->OnClicked.AddDynamic(this, &UXiaomaHUDWidget::OnRebindInteract);
    AddButton(Panel, KeyLabel(TEXT("排除标记"), EXiaomaBindAction::Mark))
        ->OnClicked.AddDynamic(this, &UXiaomaHUDWidget::OnRebindMark);
    AddButton(Panel, KeyLabel(TEXT("全局 / 跟随视角"), EXiaomaBindAction::Overview))
        ->OnClicked.AddDynamic(this, &UXiaomaHUDWidget::OnRebindOverview);
    AddButton(Panel, KeyLabel(TEXT("使用提示星"), EXiaomaBindAction::Hint))
        ->OnClicked.AddDynamic(this, &UXiaomaHUDWidget::OnRebindHint);
    AddButton(Panel, KeyLabel(TEXT("撤销一步"), EXiaomaBindAction::Undo))
        ->OnClicked.AddDynamic(this, &UXiaomaHUDWidget::OnRebindUndo);

    AddButton(Panel, TEXT("恢复默认按键"))->OnClicked.AddDynamic(this, &UXiaomaHUDWidget::OnResetKeys);
    AddButton(Panel, TEXT("返回"), true)->OnClicked.AddDynamic(this, &UXiaomaHUDWidget::OnCloseSettings);
}

void UXiaomaHUDWidget::BuildGameHUD(UCanvasPanel* Root)
{
    if (!Game.IsValid())
    {
        return;
    }
    UVerticalBox* Panel = MakePanel(Root, FVector2D(22.f, 24.f), FVector2D(250.f, 520.f),
        FVector2D::ZeroVector, FVector2D::ZeroVector);
    AddText(Panel, FString::Printf(TEXT("第 %02d 关"), Game->GetLevelNumber()), 28, Green);
    // 生命不带分母：各关初始生命不同（1~10 关 3、11 关起 2），吃苹果最高到 5，
    // 原先恒写 "/5" 会让满血看起来像掉过血。
    AddText(Panel, FString::Printf(TEXT("生命 %d  ·  步数 %d/%d"),
        Game->GetLives(), Game->GetStepCount(), Game->GetParSteps()), 16, Gold);
    // 宝箱数写在这里：三星的第三颗星就是「集齐本关宝箱」，目标得让玩家看得见。
    // 第 1、2 关没有宝箱，写 "0/0" 会让人以为漏了一个，干脆显示成横杠。
    const int32 ChestTotal = Game->GetChestCount();
    const FString ChestText = ChestTotal > 0
        ? FString::Printf(TEXT("%d/%d"), Game->GetOpenedChestCount(), ChestTotal)
        : FString(TEXT("—"));
    AddText(Panel, FString::Printf(TEXT("提示星 %d  ·  宝箱 %s"),
        Game->GetHintTokens(), *ChestText), 16, Gold);
    AddText(Panel, FString::Printf(TEXT("四叶草 %s  ·  好感 %d  ·  连胜 %d"),
        Game->HasClover() ? TEXT("有") : TEXT("无"), Game->GetAffection(), Game->GetWinStreak()), 15, Muted);
    const FString KInteract = Game->GetKeyForAction(EXiaomaBindAction::Interact).GetDisplayName().ToString();
    const FString KMark = Game->GetKeyForAction(EXiaomaBindAction::Mark).GetDisplayName().ToString();
    const FString KOverview = Game->GetKeyForAction(EXiaomaBindAction::Overview).GetDisplayName().ToString();
    const FString KHint = Game->GetKeyForAction(EXiaomaBindAction::Hint).GetDisplayName().ToString();
    const FString KUndo = Game->GetKeyForAction(EXiaomaBindAction::Undo).GetDisplayName().ToString();

    UButton* UndoButton = AddButton(Panel, FString::Printf(TEXT("撤销  %s"), *KUndo));
    UndoButton->OnClicked.AddDynamic(this, &UXiaomaHUDWidget::OnUndo);
    if (!Game->CanUndo())
    {
        // 历史为空时按钮点了也没反应，干脆置灰并把字压暗，别让玩家以为按键坏了
        UndoButton->SetIsEnabled(false);
        if (UTextBlock* UndoLabel = Cast<UTextBlock>(UndoButton->GetChildAt(0)))
        {
            UndoLabel->SetColorAndOpacity(FSlateColor(Muted));
        }
    }
    AddButton(Panel, TEXT("重开  R"))->OnClicked.AddDynamic(this, &UXiaomaHUDWidget::OnRestart);
    AddButton(Panel, FString::Printf(TEXT("提示星 (%d)  %s"), Game->GetHintTokens(), *KHint))
        ->OnClicked.AddDynamic(this, &UXiaomaHUDWidget::OnHint);
    AddButton(Panel, TEXT("提交答案"), true)->OnClicked.AddDynamic(this, &UXiaomaHUDWidget::OnSubmit);
    AddButton(Panel, TEXT("暂停  Esc"))->OnClicked.AddDynamic(this, &UXiaomaHUDWidget::OnPause);

    UVerticalBox* Legend = MakePanel(Root, FVector2D(-22.f, 24.f),
        FVector2D(234.f, 415.f), FVector2D(1.f, 0.f), FVector2D(1.f, 0.f));
    AddText(Legend, TEXT("六色小马"), 24, Green);
    const TCHAR* Names[6] = {
        TEXT("蓝色"), TEXT("棕色"), TEXT("粉色"),
        TEXT("红色"), TEXT("黄色"), TEXT("薄荷色")
    };
    for (int32 Colour = 0; Colour < 6; ++Colour)
    {
        AddText(Legend, FString::Printf(TEXT("●  %s"), Names[Colour]), 16,
            FMath::Lerp(XiaomaPalette::Regions[Colour], Ink, 0.45f));
    }
    AddText(Legend, FString::Printf(TEXT("WASD 移动 · %s 召唤/开箱\n%s 标记 · %s 全局视角\n左键轻抚小马"),
        *KInteract, *KMark, *KOverview), 14, Muted);

    UVerticalBox* Status = MakePanel(Root, FVector2D(0.f, -18.f), FVector2D(480.f, 64.f),
        FVector2D(0.5f, 1.f), FVector2D(0.5f, 1.f));
    AddText(Status, Game->GetStatusMessage(), 16, Green)->SetJustification(ETextJustify::Center);
}

void UXiaomaHUDWidget::BuildOverlay(UCanvasPanel* Root,
    const FString& Title, const FString& Detail)
{
    UVerticalBox* Panel = MakePanel(Root, FVector2D::ZeroVector, FVector2D(402.f, 307.f),
        FVector2D(0.5f), FVector2D(0.5f));
    AddText(Panel, Title, 34, Green);
    AddText(Panel, Detail, 19, Muted);
    if (!Game.IsValid())
    {
        return;
    }
    if (Game->GetScreen() == EXiaomaScreen::Paused)
    {
        AddButton(Panel, TEXT("继续"), true)->OnClicked.AddDynamic(this, &UXiaomaHUDWidget::OnResume);
        AddButton(Panel, TEXT("设置"))->OnClicked.AddDynamic(this, &UXiaomaHUDWidget::OnSettings);
    }
    if (Game->GetScreen() == EXiaomaScreen::Won)
    {
        AddButton(Panel, TEXT("下一关"), true)->OnClicked.AddDynamic(this, &UXiaomaHUDWidget::OnNext);
    }
    if (Game->GetScreen() == EXiaomaScreen::Lost)
    {
        AddButton(Panel, TEXT("再试一次"), true)->OnClicked.AddDynamic(this, &UXiaomaHUDWidget::OnRestart);
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
    case EXiaomaScreen::Settings: BuildSettings(Root); break;
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
void UXiaomaHUDWidget::OnNext() { if (Game.IsValid()) Game->NextLevel(); }
void UXiaomaHUDWidget::OnSettings() { if (Game.IsValid()) Game->ShowSettings(); }
void UXiaomaHUDWidget::OnCloseSettings() { if (Game.IsValid()) Game->CloseSettings(); }
void UXiaomaHUDWidget::OnToggleFullscreen() { if (Game.IsValid()) Game->ToggleFullscreen(); }
void UXiaomaHUDWidget::OnCycleSummonMode() { if (Game.IsValid()) Game->CycleSummonMode(); }
void UXiaomaHUDWidget::OnCycleTabCameraMode() { if (Game.IsValid()) Game->CycleTabCameraMode(); }
void UXiaomaHUDWidget::OnRebindInteract() { if (Game.IsValid()) Game->StartRebind(EXiaomaBindAction::Interact); }
void UXiaomaHUDWidget::OnRebindMark() { if (Game.IsValid()) Game->StartRebind(EXiaomaBindAction::Mark); }
void UXiaomaHUDWidget::OnRebindOverview() { if (Game.IsValid()) Game->StartRebind(EXiaomaBindAction::Overview); }
void UXiaomaHUDWidget::OnRebindHint() { if (Game.IsValid()) Game->StartRebind(EXiaomaBindAction::Hint); }
void UXiaomaHUDWidget::OnRebindUndo() { if (Game.IsValid()) Game->StartRebind(EXiaomaBindAction::Undo); }
void UXiaomaHUDWidget::OnResetKeys() { if (Game.IsValid()) Game->ResetDefaultKeys(); }
void UXiaomaHUDWidget::OnSound() { if (Game.IsValid()) Game->ToggleSound(); }
void UXiaomaHUDWidget::OnQuit()
{
    if (APlayerController* Controller = GetOwningPlayer())
    {
        UKismetSystemLibrary::QuitGame(this, Controller, EQuitPreference::Quit, false);
    }
}
