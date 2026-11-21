#include "XiaomaPlayerController.h"

#include "XiaomaBoardActor.h"
#include "XiaomaGameMode.h"
#include "XiaomaHerdsman.h"
#include "Components/InputComponent.h"
#include "GameFramework/PlayerInput.h"
#include "InputCoreTypes.h"
#include "Kismet/GameplayStatics.h"

AXiaomaPlayerController::AXiaomaPlayerController()
{
    bShowMouseCursor = true;
    bEnableClickEvents = true;
    bEnableMouseOverEvents = true;
}

void AXiaomaPlayerController::BeginPlay()
{
    Super::BeginPlay();
    FInputModeGameAndUI Mode;
    Mode.SetHideCursorDuringCapture(false);
    SetInputMode(Mode);
}

void AXiaomaPlayerController::SetupInputComponent()
{
    Super::SetupInputComponent();
    InputComponent->BindAction(TEXT("Place"), IE_Pressed, this, &AXiaomaPlayerController::OnLeftClick);
    InputComponent->BindAction(TEXT("Mark"), IE_Pressed, this, &AXiaomaPlayerController::OnRightClick);
    InputComponent->BindAction(TEXT("RestartLevel"), IE_Pressed, this, &AXiaomaPlayerController::Restart);
    InputComponent->BindAction(TEXT("SubmitAnswer"), IE_Pressed, this, &AXiaomaPlayerController::Submit);
    InputComponent->BindAction(TEXT("EscapeMenu"), IE_Pressed, this, &AXiaomaPlayerController::Escape);
}

bool AXiaomaPlayerController::InputKey(const FInputKeyParams& Params)
{
    AXiaomaGameMode* Game = Cast<AXiaomaGameMode>(UGameplayStatics::GetGameMode(this));
    if (Game)
    {
        // 设置界面等待绑定新按键
        if (Game->GetPendingRebind() != EXiaomaBindAction::None && Params.Event == IE_Pressed)
        {
            if (Params.Key == EKeys::Escape)
            {
                Game->CancelRebind();
                return true;
            }
            if (!Params.Key.IsMouseButton() && !Params.Key.IsModifierKey() && Params.Key.IsValid())
            {
                Game->FinishRebind(Params.Key);
                return true;
            }
        }

        if (Game->GetScreen() == EXiaomaScreen::Playing)
        {
            if (Params.Key == Game->GetKeyForAction(EXiaomaBindAction::Interact))
            {
                if (Params.Event == IE_Pressed)
                {
                    Game->InteractPressed();
                    return true;
                }
                if (Params.Event == IE_Released)
                {
                    Game->InteractReleased();
                    return true;
                }
            }
            else if (Params.Key == Game->GetKeyForAction(EXiaomaBindAction::Overview))
            {
                if (Params.Event == IE_Pressed)
                {
                    Game->OverviewPressed();
                    return true;
                }
                if (Params.Event == IE_Released)
                {
                    Game->OverviewReleased();
                    return true;
                }
            }
            else if (Params.Event == IE_Pressed)
            {
                if (Params.Key == Game->GetKeyForAction(EXiaomaBindAction::Mark))
                {
                    Game->ToggleMarkAtCurrentCell();
                    return true;
                }
                if (Params.Key == Game->GetKeyForAction(EXiaomaBindAction::Hint))
                {
                    Game->UseHint();
                    return true;
                }
                if (Params.Key == Game->GetKeyForAction(EXiaomaBindAction::Undo))
                {
                    Game->Undo();
                    return true;
                }
            }
        }
    }
    return Super::InputKey(Params);
}

void AXiaomaPlayerController::PlayerTick(float DeltaTime)
{
    Super::PlayerTick(DeltaTime);
    AXiaomaGameMode* Game = Cast<AXiaomaGameMode>(UGameplayStatics::GetGameMode(this));
    if (!Game || Game->GetScreen() != EXiaomaScreen::Playing)
    {
        return;
    }
    if (AXiaomaHerdsman* Herdsman = Game->GetHerdsman())
    {
        float Forward = 0.f;
        float Right = 0.f;
        if (IsInputKeyDown(EKeys::W) || IsInputKeyDown(EKeys::Up)) Forward += 1.f;
        if (IsInputKeyDown(EKeys::S) || IsInputKeyDown(EKeys::Down)) Forward -= 1.f;
        // UE 左手系面向 +Y 时，屏幕右侧为 -X、左侧为 +X
        if (IsInputKeyDown(EKeys::D) || IsInputKeyDown(EKeys::Right)) Right -= 1.f;
        if (IsInputKeyDown(EKeys::A) || IsInputKeyDown(EKeys::Left)) Right += 1.f;
        Herdsman->MoveForwardInput(Forward);
        Herdsman->MoveRightInput(Right);
    }
}

void AXiaomaPlayerController::OnLeftClick()
{
    AXiaomaGameMode* Game = Cast<AXiaomaGameMode>(UGameplayStatics::GetGameMode(this));
    if (!Game || !Game->GetBoard() || Game->GetScreen() != EXiaomaScreen::Playing || Game->IsSummonWheelOpen())
    {
        return;
    }
    // 鼠标左键不再放马，用于轻抚待命区或棋盘上的小马
    FHitResult Hit;
    if (GetHitResultUnderCursor(ECC_Visibility, false, Hit))
    {
        const int32 Colour = Game->GetBoard()->FindPonyColour(Hit.GetComponent());
        if (Colour != INDEX_NONE)
        {
            Game->PetPony(Colour);
        }
    }
}

void AXiaomaPlayerController::OnRightClick()
{
    AXiaomaGameMode* Game = Cast<AXiaomaGameMode>(UGameplayStatics::GetGameMode(this));
    if (Game && Game->IsSummonWheelOpen())
    {
        Game->CloseSummonWheel();
    }
}

void AXiaomaPlayerController::Restart()
{
    if (AXiaomaGameMode* Game = Cast<AXiaomaGameMode>(UGameplayStatics::GetGameMode(this))) Game->RestartLevel();
}
void AXiaomaPlayerController::Submit()
{
    if (AXiaomaGameMode* Game = Cast<AXiaomaGameMode>(UGameplayStatics::GetGameMode(this))) Game->SubmitAnswer();
}
void AXiaomaPlayerController::Escape()
{
    if (AXiaomaGameMode* Game = Cast<AXiaomaGameMode>(UGameplayStatics::GetGameMode(this)))
    {
        if (Game->GetPendingRebind() != EXiaomaBindAction::None)
        {
            Game->CancelRebind();
            return;
        }
        if (Game->IsSummonWheelOpen())
        {
            Game->CloseSummonWheel();
            return;
        }
        if (Game->GetScreen() == EXiaomaScreen::Settings) Game->CloseSettings();
        else if (Game->GetScreen() == EXiaomaScreen::Playing) Game->PauseGame();
        else if (Game->GetScreen() == EXiaomaScreen::Paused) Game->ResumeGame();
    }
}
