#include "XiaomaGameMode.h"

#include "XiaomaBoardActor.h"
#include "XiaomaHUDWidget.h"
#include "XiaomaPlayerController.h"
#include "XiaomaSaveGame.h"
#include "Audio.h"
#include "Camera/CameraActor.h"
#include "Camera/CameraComponent.h"
#include "Components/AudioComponent.h"
#include "Kismet/GameplayStatics.h"
#include "HAL/PlatformTime.h"
#include "Misc/FileHelper.h"
#include "Misc/Paths.h"
#include "Sound/SoundWaveProcedural.h"
#include "TimerManager.h"

namespace
{
const FString SaveSlot = TEXT("XiaomaLogicProgress");
const FVector TableCamera(280.f, -470.f, 1200.f);
const FVector TableFocus(280.f, 280.f, 0.f);
}

AXiaomaGameMode::AXiaomaGameMode()
{
    PlayerControllerClass = AXiaomaPlayerController::StaticClass();
    DefaultPawnClass = nullptr;
    HUDClass = nullptr;
}

void AXiaomaGameMode::BeginPlay()
{
    Super::BeginPlay();
    FString LoadError;
    if (!FXiaomaPuzzle::LoadLevels(Levels, LoadError))
    {
        StatusMessage = LoadError;
        UE_LOG(LogTemp, Error, TEXT("%s"), *LoadError);
    }
    Progress = Cast<UXiaomaSaveGame>(UGameplayStatics::LoadGameFromSlot(SaveSlot, 0));
    if (!Progress)
    {
        Progress = Cast<UXiaomaSaveGame>(UGameplayStatics::CreateSaveGameObject(UXiaomaSaveGame::StaticClass()));
    }
    if (Progress && Progress->StarsByLevel.Num() != 25)
    {
        Progress->StarsByLevel.SetNumZeroed(25);
    }

    Board = GetWorld()->SpawnActor<AXiaomaBoardActor>(AXiaomaBoardActor::StaticClass());
    BoardCamera = GetWorld()->SpawnActor<ACameraActor>(TableCamera,
        (TableFocus - TableCamera).Rotation());
    if (BoardCamera)
    {
        BoardCamera->GetCameraComponent()->SetFieldOfView(55.f);
    }
    APlayerController* Controller = UGameplayStatics::GetPlayerController(this, 0);
    if (Controller && BoardCamera)
    {
        Controller->SetViewTarget(BoardCamera);
    }
    if (Controller)
    {
        GameWidget = CreateWidget<UXiaomaHUDWidget>(Controller, UXiaomaHUDWidget::StaticClass());
        if (GameWidget)
        {
            GameWidget->SetGame(this);
            GameWidget->Refresh();
            GameWidget->AddToViewport();
        }
    }
    SetScreen(EXiaomaScreen::Menu);
}

const FXiaomaLevel* AXiaomaGameMode::GetCurrentLevel() const
{
    return Levels.IsValidIndex(CurrentLevelIndex) ? &Levels[CurrentLevelIndex] : nullptr;
}

int32 AXiaomaGameMode::GetUnlockedLevel() const { return Progress ? Progress->UnlockedLevel : 1; }
int32 AXiaomaGameMode::GetContinueLevel() const
{
    return Progress && Progress->bHasActiveRound ? Progress->ActiveLevel : GetUnlockedLevel();
}
int32 AXiaomaGameMode::GetLives() const { return Progress ? Progress->Lives : 2; }
int32 AXiaomaGameMode::GetAffection() const { return Progress ? Progress->Affection : 0; }
int32 AXiaomaGameMode::GetWinStreak() const { return Progress ? Progress->WinStreak : 0; }
int32 AXiaomaGameMode::GetBestStreak() const { return Progress ? Progress->BestStreak : 0; }
int32 AXiaomaGameMode::GetLevelStars(int32 LevelNumber) const
{
    return Progress && Progress->StarsByLevel.IsValidIndex(LevelNumber - 1)
        ? Progress->StarsByLevel[LevelNumber - 1] : 0;
}
int32 AXiaomaGameMode::GetCurrentStars() const { return GetLevelStars(GetLevelNumber()); }

bool AXiaomaGameMode::IsSoundEnabled() const
{
    return Progress && Progress->bSoundEnabled;
}

void AXiaomaGameMode::ToggleSound()
{
    if (Progress)
    {
        Progress->bSoundEnabled = !Progress->bSoundEnabled;
        SaveProgress();
        if (GameWidget)
        {
            GameWidget->Refresh();
        }
    }
}

void AXiaomaGameMode::PlayFeedback(const TCHAR* Name)
{
    if (!IsSoundEnabled())
    {
        return;
    }

    const FString Path = FPaths::ProjectContentDir() / TEXT("ImportSource/Audio")
        / (FString(Name) + TEXT(".wav"));
    TArray<uint8> File;
    FWaveModInfo Info;
    if (!FFileHelper::LoadFileToArray(File, *Path) || !Info.ReadWaveInfo(File.GetData(), File.Num())
        || *Info.pFormatTag != 1 || *Info.pBitsPerSample != 16 || *Info.pChannels != 1
        || *Info.pSamplesPerSec == 0 || Info.SampleDataSize == 0)
    {
        UE_LOG(LogTemp, Warning, TEXT("Could not read feedback WAV: %s"), *Path);
        return;
    }

    USoundWaveProcedural* Sound = NewObject<USoundWaveProcedural>(this);
    Sound->SetSampleRate(*Info.pSamplesPerSec);
    Sound->NumChannels = 1;
    const float Duration = static_cast<float>(Info.SampleDataSize) / (2.f * *Info.pSamplesPerSec);
    Sound->Duration = Duration;
    Sound->QueueAudio(Info.SampleDataStart, Info.SampleDataSize);
    if (UAudioComponent* Audio = UGameplayStatics::SpawnSound2D(this, Sound))
    {
        FTimerHandle StopTimer;
        GetWorldTimerManager().SetTimer(StopTimer,
            FTimerDelegate::CreateUObject(Audio, &UAudioComponent::Stop), Duration + 0.10f, false);
    }
}

void AXiaomaGameMode::SaveProgress()
{
    if (Progress)
    {
        if (Screen == EXiaomaScreen::Playing && CurrentLevelIndex >= 0)
        {
            Progress->bHasActiveRound = true;
            Progress->ActiveLevel = CurrentLevelIndex + 1;
            Progress->ActiveCells = Cells;
            Progress->ActiveHintsUsed = HintsUsed;
            Progress->ActiveWrongAttempts = WrongAttempts;
        }
        UGameplayStatics::SaveGameToSlot(Progress, SaveSlot, 0);
    }
}

void AXiaomaGameMode::SetScreen(EXiaomaScreen NewScreen)
{
    Screen = NewScreen;
    const bool bMenu = NewScreen == EXiaomaScreen::Menu;
    if (BoardCamera)
    {
        const FVector Framing = bMenu ? FVector(235.f, 0.f, 0.f) : FVector::ZeroVector;
        BoardCamera->SetActorLocationAndRotation(TableCamera + Framing, (TableFocus - TableCamera).Rotation());
    }
    if (Board && bMenu && !Levels.IsEmpty())
    {
        const FXiaomaLevel& Preview = Levels[0];
        TArray<uint8> PreviewCells;
        PreviewCells.Init(0, FXiaomaPuzzle::CellCount);
        for (const FIntPoint& Given : Preview.Givens)
        {
            PreviewCells[Given.X * FXiaomaPuzzle::Size + Given.Y] = 2;
        }
        Board->BuildBoard(Preview);
        Board->RefreshBoard(Preview, PreviewCells);
    }
    if (GameWidget)
    {
        GameWidget->Refresh();
    }
}

void AXiaomaGameMode::ShowMenu() { SetScreen(EXiaomaScreen::Menu); }
void AXiaomaGameMode::ShowLevelSelect() { SetScreen(EXiaomaScreen::LevelSelect); }
void AXiaomaGameMode::PauseGame()
{
    if (Screen == EXiaomaScreen::Playing)
    {
        SetScreen(EXiaomaScreen::Paused);
    }
}
void AXiaomaGameMode::ResumeGame()
{
    if (Screen == EXiaomaScreen::Paused)
    {
        BoardInputReadyAt = FPlatformTime::Seconds() + 0.25;
        SetScreen(EXiaomaScreen::Playing);
    }
}
void AXiaomaGameMode::ToggleMarkMode()
{
    bMarkMode = !bMarkMode;
    RefreshBoardAndUI();
}

void AXiaomaGameMode::StartLevel(int32 LevelNumber)
{
    if (!Levels.IsValidIndex(LevelNumber - 1) || LevelNumber > GetUnlockedLevel())
    {
        return;
    }
    CurrentLevelIndex = LevelNumber - 1;
    const bool bResume = Progress->bHasActiveRound && Progress->ActiveLevel == LevelNumber
        && Progress->ActiveCells.Num() == FXiaomaPuzzle::CellCount && Progress->Lives > 0;
    HintsUsed = bResume ? Progress->ActiveHintsUsed : 0;
    WrongAttempts = bResume ? Progress->ActiveWrongAttempts : 0;
    bMarkMode = false;
    if (bResume)
    {
        Cells = Progress->ActiveCells;
        UndoHistory.Reset();
    }
    else
    {
        Progress->Lives = 2;
        ResetBoard();
    }
    if (Board)
    {
        Board->BuildBoard(Levels[CurrentLevelIndex]);
    }
    StatusMessage = TEXT("每行、每列、每个颜色区域各放一匹小马");
    BoardInputReadyAt = FPlatformTime::Seconds() + 0.25;
    SetScreen(EXiaomaScreen::Playing);
    RefreshBoardAndUI();
    SaveProgress();
}

void AXiaomaGameMode::ResetBoard()
{
    Cells.Init(0, FXiaomaPuzzle::CellCount);
    UndoHistory.Reset();
    if (const FXiaomaLevel* Level = GetCurrentLevel())
    {
        for (const FIntPoint& Given : Level->Givens)
        {
            Cells[Given.X * FXiaomaPuzzle::Size + Given.Y] = 2;
        }
    }
}

bool AXiaomaGameMode::IsCorrectCell(int32 Index) const
{
    const FXiaomaLevel* Level = GetCurrentLevel();
    const int32 Row = Index / FXiaomaPuzzle::Size;
    return Level && Level->SolutionColumns.IsValidIndex(Row)
        && Level->SolutionColumns[Row] == Index % FXiaomaPuzzle::Size;
}

void AXiaomaGameMode::LoseLife()
{
    if (!Progress)
    {
        return;
    }
    ++WrongAttempts;
    PlayFeedback(TEXT("Error"));
    Progress->Lives = FMath::Max(0, Progress->Lives - 1);
    if (Progress->Lives == 0)
    {
        Progress->WinStreak = 0;
        Progress->bHasActiveRound = false;
        StatusMessage = TEXT("生命值耗尽，再试一次吧");
        SetScreen(EXiaomaScreen::Lost);
    }
    else
    {
        StatusMessage = TEXT("这里不是小马的位置，失去一条生命");
    }
    SaveProgress();
}

void AXiaomaGameMode::ActivateCell(int32 Index, bool bForceMark)
{
    if (Screen != EXiaomaScreen::Playing || !Cells.IsValidIndex(Index)
        || FPlatformTime::Seconds() < BoardInputReadyAt)
    {
        return;
    }
    const FXiaomaLevel* Level = GetCurrentLevel();
    if (!Level || Level->IsGiven(Index / 6, Index % 6))
    {
        return;
    }
    bool bWrongCell = false;
    if (bForceMark || bMarkMode)
    {
        UndoHistory.Add(Cells);
        Cells[Index] = Cells[Index] == 1 ? 0 : 1;
        StatusMessage = TEXT("已更新排除标记");
    }
    else if (Cells[Index] == 2)
    {
        UndoHistory.Add(Cells);
        Cells[Index] = 0;
        StatusMessage = TEXT("已移除小马");
    }
    else if (IsCorrectCell(Index))
    {
        UndoHistory.Add(Cells);
        Cells[Index] = 2;
        StatusMessage = TEXT("找到一匹小马！");
        PlayFeedback(TEXT("Place"));
    }
    else
    {
        bWrongCell = true;
        LoseLife();
    }
    RefreshBoardAndUI();
    if (Board)
    {
        Board->FlashCell(Index, bWrongCell);
    }
    if (Screen == EXiaomaScreen::Playing)
    {
        SaveProgress();
    }
}

void AXiaomaGameMode::Undo()
{
    if (Screen != EXiaomaScreen::Playing || UndoHistory.IsEmpty())
    {
        return;
    }
    Cells = UndoHistory.Pop();
    StatusMessage = TEXT("已撤销上一步");
    RefreshBoardAndUI();
    SaveProgress();
}

void AXiaomaGameMode::UseHint()
{
    if (Screen != EXiaomaScreen::Playing)
    {
        return;
    }
    const FXiaomaLevel* Level = GetCurrentLevel();
    if (!Level)
    {
        return;
    }
    const FIntPoint Cell = FXiaomaPuzzle::FindHint(*Level, Cells);
    if (Cell.X >= 0)
    {
        UndoHistory.Add(Cells);
        Cells[Cell.X * FXiaomaPuzzle::Size + Cell.Y] = 2;
        ++HintsUsed;
        PlayFeedback(TEXT("Place"));
        StatusMessage = FString::Printf(TEXT("提示：第 %d 行，第 %d 列"), Cell.X + 1, Cell.Y + 1);
        RefreshBoardAndUI();
        SaveProgress();
    }
}

void AXiaomaGameMode::SubmitAnswer()
{
    if (Screen != EXiaomaScreen::Playing || !GetCurrentLevel())
    {
        return;
    }
    const EXiaomaBoardResult Result = FXiaomaPuzzle::CheckBoard(*GetCurrentLevel(), Cells, StatusMessage);
    if (Result == EXiaomaBoardResult::Invalid)
    {
        LoseLife();
    }
    else if (Result == EXiaomaBoardResult::Solved)
    {
        const int32 Stars = (HintsUsed == 0 && WrongAttempts == 0) ? 3
            : (HintsUsed + WrongAttempts <= 2 ? 2 : 1);
        Progress->StarsByLevel[CurrentLevelIndex] = FMath::Max(
            Progress->StarsByLevel[CurrentLevelIndex], Stars);
        Progress->UnlockedLevel = FMath::Min(25,
            FMath::Max(Progress->UnlockedLevel, CurrentLevelIndex + 2));
        Progress->Affection += 5;
        ++Progress->WinStreak;
        PlayFeedback(TEXT("Win"));
        Progress->BestStreak = FMath::Max(Progress->BestStreak, Progress->WinStreak);
        Progress->bHasActiveRound = false;
        StatusMessage = FString::Printf(TEXT("通关！获得 %d 颗星，好感度 +5"), Stars);
        SetScreen(EXiaomaScreen::Won);
        if (Board)
        {
            Board->Celebrate();
        }
        SaveProgress();
    }
    RefreshBoardAndUI();
}

void AXiaomaGameMode::RestartLevel()
{
    if (!GetCurrentLevel())
    {
        return;
    }
    Progress->Lives = 2;
    Progress->WinStreak = 0;
    HintsUsed = 0;
    WrongAttempts = 0;
    ResetBoard();
    Progress->bHasActiveRound = false;
    StatusMessage = TEXT("已重开当前关卡");
    BoardInputReadyAt = FPlatformTime::Seconds() + 0.25;
    SetScreen(EXiaomaScreen::Playing);
    RefreshBoardAndUI();
    SaveProgress();
}

void AXiaomaGameMode::NextLevel()
{
    if (CurrentLevelIndex + 1 < Levels.Num())
    {
        StartLevel(CurrentLevelIndex + 2);
    }
    else
    {
        ShowLevelSelect();
    }
}

void AXiaomaGameMode::RefreshBoardAndUI()
{
    if (Board && GetCurrentLevel())
    {
        Board->RefreshBoard(*GetCurrentLevel(), Cells);
    }
    if (GameWidget)
    {
        GameWidget->Refresh();
    }
}
