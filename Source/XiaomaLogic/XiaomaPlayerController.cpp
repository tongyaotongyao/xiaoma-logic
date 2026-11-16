#include "XiaomaPlayerController.h"

#include "XiaomaBoardActor.h"
#include "XiaomaGameMode.h"
#include "Components/InputComponent.h"
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
    InputComponent->BindAction(TEXT("Place"), IE_Pressed, this, &AXiaomaPlayerController::ClickCell);
    InputComponent->BindAction(TEXT("Mark"), IE_Pressed, this, &AXiaomaPlayerController::MarkCell);
    InputComponent->BindAction(TEXT("UndoMove"), IE_Pressed, this, &AXiaomaPlayerController::UndoMove);
    InputComponent->BindAction(TEXT("RestartLevel"), IE_Pressed, this, &AXiaomaPlayerController::Restart);
    InputComponent->BindAction(TEXT("UseHint"), IE_Pressed, this, &AXiaomaPlayerController::Hint);
    InputComponent->BindAction(TEXT("SubmitAnswer"), IE_Pressed, this, &AXiaomaPlayerController::Submit);
    InputComponent->BindAction(TEXT("EscapeMenu"), IE_Pressed, this, &AXiaomaPlayerController::Escape);
}

void AXiaomaPlayerController::ClickCell()
{
    AXiaomaGameMode* Game = Cast<AXiaomaGameMode>(UGameplayStatics::GetGameMode(this));
    if (!Game || !Game->GetBoard())
    {
        return;
    }
    FHitResult Hit;
    if (GetHitResultUnderCursor(ECC_Visibility, false, Hit))
    {
        const int32 Index = Game->GetBoard()->FindCell(Hit.GetComponent());
        if (Index != INDEX_NONE)
        {
            Game->ActivateCell(Index);
        }
    }
}

void AXiaomaPlayerController::MarkCell()
{
    AXiaomaGameMode* Game = Cast<AXiaomaGameMode>(UGameplayStatics::GetGameMode(this));
    if (!Game || !Game->GetBoard())
    {
        return;
    }
    FHitResult Hit;
    if (GetHitResultUnderCursor(ECC_Visibility, false, Hit))
    {
        const int32 Index = Game->GetBoard()->FindCell(Hit.GetComponent());
        if (Index != INDEX_NONE)
        {
            Game->ActivateCell(Index, true);
        }
    }
}

void AXiaomaPlayerController::UndoMove()
{
    if (AXiaomaGameMode* Game = Cast<AXiaomaGameMode>(UGameplayStatics::GetGameMode(this))) Game->Undo();
}
void AXiaomaPlayerController::Restart()
{
    if (AXiaomaGameMode* Game = Cast<AXiaomaGameMode>(UGameplayStatics::GetGameMode(this))) Game->RestartLevel();
}
void AXiaomaPlayerController::Hint()
{
    if (AXiaomaGameMode* Game = Cast<AXiaomaGameMode>(UGameplayStatics::GetGameMode(this))) Game->UseHint();
}
void AXiaomaPlayerController::Submit()
{
    if (AXiaomaGameMode* Game = Cast<AXiaomaGameMode>(UGameplayStatics::GetGameMode(this))) Game->SubmitAnswer();
}
void AXiaomaPlayerController::Escape()
{
    if (AXiaomaGameMode* Game = Cast<AXiaomaGameMode>(UGameplayStatics::GetGameMode(this)))
    {
        if (Game->GetScreen() == EXiaomaScreen::Playing) Game->PauseGame();
        else if (Game->GetScreen() == EXiaomaScreen::Paused) Game->ResumeGame();
    }
}
