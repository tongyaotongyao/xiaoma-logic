#include "XiaomaSummonWheel.h"
#include "XiaomaPalette.h"

#include "Blueprint/WidgetTree.h"
#include "Brushes/SlateRoundedBoxBrush.h"
#include "Components/Border.h"
#include "Components/CanvasPanel.h"
#include "Components/CanvasPanelSlot.h"
#include "Components/TextBlock.h"

namespace
{
const TCHAR* WheelColourNames[6] = {
    TEXT("蓝色小马"),
    TEXT("棕色小马"),
    TEXT("粉色小马"),
    TEXT("红色小马"),
    TEXT("黄色小马"),
    TEXT("薄荷小马")
};
}

void UXiaomaWheelButton::BindClick()
{
    OnClicked.AddDynamic(this, &UXiaomaWheelButton::OnClickedInternal);
}

void UXiaomaWheelButton::OnClickedInternal()
{
    if (OwnerWheel.IsValid())
    {
        OwnerWheel->HandleButtonClick(ColourIndex);
    }
}

void UXiaomaSummonWheel::OpenWheel(const TArray<int32>& PlacedCellByRegion,
    bool bHasPonyAtCurrentCell, int32 CurrentRegion)
{
    bHasPonyHere = bHasPonyAtCurrentCell;
    if (!WidgetTree)
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
    SectorButtons.Reset();
    CenterButton = nullptr;

    // 半透明底环提示
    UBorder* Backdrop = WidgetTree->ConstructWidget<UBorder>();
    Backdrop->SetBrush(FSlateRoundedBoxBrush(FLinearColor(0.08f, 0.09f, 0.08f, 0.45f), 200.f));
    Backdrop->SetVisibility(ESlateVisibility::HitTestInvisible);
    UCanvasPanelSlot* BackSlot = Root->AddChildToCanvas(Backdrop);
    BackSlot->SetAnchors(FAnchors(0.5f, 0.5f));
    BackSlot->SetAlignment(FVector2D(0.5f, 0.5f));
    BackSlot->SetSize(FVector2D(390.f, 390.f));

    // 环形排布 6 个色区扇区按钮
    const float Radius = 136.f;
    for (int32 Colour = 0; Colour < 6; ++Colour)
    {
        const float AngleRad = FMath::DegreesToRadians(-90.f + Colour * 60.f);
        const FVector2D Offset(FMath::Cos(AngleRad) * Radius, FMath::Sin(AngleRad) * Radius);
        const int32 PlacedIdx = PlacedCellByRegion.IsValidIndex(Colour) ? PlacedCellByRegion[Colour] : INDEX_NONE;
        const bool bAlreadyPlaced = PlacedIdx != INDEX_NONE;

        UXiaomaWheelButton* Btn = WidgetTree->ConstructWidget<UXiaomaWheelButton>();
        Btn->ColourIndex = Colour;
        Btn->OwnerWheel = this;
        Btn->SetIsEnabled(!bAlreadyPlaced);

        const FLinearColor BaseCol = XiaomaPalette::Regions[Colour];
        const FLinearColor NormalCol = bAlreadyPlaced
            ? FLinearColor(0.35f, 0.35f, 0.35f, 0.75f)
            : BaseCol;
        const FLinearColor HoverCol = FMath::Lerp(BaseCol, FLinearColor::White, 0.25f);
        const FLinearColor PressCol = FMath::Lerp(BaseCol, FLinearColor::Black, 0.15f);

        FButtonStyle Style;
        Style.SetNormal(FSlateRoundedBoxBrush(NormalCol, 24.f,
            Colour == CurrentRegion ? FLinearColor::White : FLinearColor(0.2f, 0.2f, 0.2f, 0.5f),
            Colour == CurrentRegion ? 3.f : 1.f));
        Style.SetHovered(FSlateRoundedBoxBrush(HoverCol, 24.f, FLinearColor::White, 3.f));
        Style.SetPressed(FSlateRoundedBoxBrush(PressCol, 24.f));
        Style.SetDisabled(FSlateRoundedBoxBrush(NormalCol, 24.f));
        Btn->SetStyle(Style);

        UTextBlock* Label = WidgetTree->ConstructWidget<UTextBlock>();
        FString Text = WheelColourNames[Colour];
        if (bAlreadyPlaced)
        {
            Text += FString::Printf(TEXT("\n(%d,%d)"), PlacedIdx / 6 + 1, PlacedIdx % 6 + 1);
        }
        Label->SetText(FText::FromString(Text));
        Label->SetJustification(ETextJustify::Center);
        FSlateFontInfo Font = Label->GetFont();
        Font.Size = 15;
        Font.TypefaceFontName = TEXT("Bold");
        Label->SetFont(Font);
        Label->SetColorAndOpacity(FSlateColor(bAlreadyPlaced ? FLinearColor(0.85f, 0.85f, 0.85f) : FLinearColor(0.14f, 0.13f, 0.10f)));
        Btn->AddChild(Label);
        Btn->BindClick();

        UCanvasPanelSlot* BtnSlot = Root->AddChildToCanvas(Btn);
        BtnSlot->SetAnchors(FAnchors(0.5f, 0.5f));
        BtnSlot->SetAlignment(FVector2D(0.5f, 0.5f));
        BtnSlot->SetPosition(Offset);
        BtnSlot->SetSize(FVector2D(104.f, 76.f));
        SectorButtons.Add(Btn);
    }

    // 中央按钮：当前格有小马时显示“请回”，否则显示“取消”
    UButton* CenterBtn = WidgetTree->ConstructWidget<UButton>();
    CenterButton = CenterBtn;
    FButtonStyle CenterStyle;
    const FLinearColor CenterCol = bHasPonyAtCurrentCell
        ? FLinearColor(FColor(219, 108, 102))
        : FLinearColor(FColor(245, 235, 211));
    CenterStyle.SetNormal(FSlateRoundedBoxBrush(CenterCol, 42.f));
    CenterStyle.SetHovered(FSlateRoundedBoxBrush(FMath::Lerp(CenterCol, FLinearColor::White, 0.2f), 42.f));
    CenterStyle.SetPressed(FSlateRoundedBoxBrush(FMath::Lerp(CenterCol, FLinearColor::Black, 0.15f), 42.f));
    CenterBtn->SetStyle(CenterStyle);

    UTextBlock* CenterLabel = WidgetTree->ConstructWidget<UTextBlock>();
    CenterLabel->SetText(FText::FromString(bHasPonyAtCurrentCell ? TEXT("请回") : TEXT("取消")));
    CenterLabel->SetJustification(ETextJustify::Center);
    FSlateFontInfo CFont = CenterLabel->GetFont();
    CFont.Size = 16;
    CFont.TypefaceFontName = TEXT("Bold");
    CenterLabel->SetFont(CFont);
    CenterLabel->SetColorAndOpacity(FSlateColor(bHasPonyAtCurrentCell ? FLinearColor::White : FLinearColor(FColor(55, 52, 42))));
    CenterBtn->AddChild(CenterLabel);
    CenterBtn->OnClicked.AddDynamic(this, &UXiaomaSummonWheel::OnCenterClicked);

    UCanvasPanelSlot* CenterSlot = Root->AddChildToCanvas(CenterBtn);
    CenterSlot->SetAnchors(FAnchors(0.5f, 0.5f));
    CenterSlot->SetAlignment(FVector2D(0.5f, 0.5f));
    CenterSlot->SetSize(FVector2D(92.f, 92.f));
}

void UXiaomaSummonWheel::HandleButtonClick(int32 ColourIndex)
{
    OnSelectColour.ExecuteIfBound(ColourIndex);
}

void UXiaomaSummonWheel::ConfirmHoveredSelection()
{
    for (UXiaomaWheelButton* Btn : SectorButtons)
    {
        if (Btn && Btn->GetIsEnabled() && Btn->IsHovered())
        {
            OnSelectColour.ExecuteIfBound(Btn->ColourIndex);
            return;
        }
    }
    if (bHasPonyHere && CenterButton && CenterButton->IsHovered())
    {
        OnRecall.ExecuteIfBound();
        return;
    }
    OnClose.ExecuteIfBound();
}

void UXiaomaSummonWheel::OnCenterClicked()
{
    if (bHasPonyHere)
    {
        OnRecall.ExecuteIfBound();
    }
    else
    {
        OnClose.ExecuteIfBound();
    }
}

FReply UXiaomaSummonWheel::NativeOnMouseButtonDown(const FGeometry& InGeometry, const FPointerEvent& InMouseEvent)
{
    if (InMouseEvent.GetEffectingButton() == EKeys::RightMouseButton)
    {
        OnClose.ExecuteIfBound();
        return FReply::Handled();
    }
    return Super::NativeOnMouseButtonDown(InGeometry, InMouseEvent);
}
