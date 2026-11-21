#include "XiaomaGameMode.h"

#include "XiaomaBoardActor.h"
#include "XiaomaHerdsman.h"
#include "XiaomaHUDWidget.h"
#include "XiaomaMechanism.h"
#include "XiaomaPickup.h"
#include "XiaomaPlayerController.h"
#include "XiaomaSaveGame.h"
#include "XiaomaSummonWheel.h"
#include "Audio.h"
#include "Camera/CameraActor.h"
#include "Camera/CameraComponent.h"
#include "Components/AudioComponent.h"
#include "Engine/Engine.h"
#include "GameFramework/GameUserSettings.h"
#include "HAL/PlatformTime.h"
#include "Kismet/GameplayStatics.h"
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
    if (Progress)
    {
        Progress->MigrateLevelRevision();
    }

    Board = GetWorld()->SpawnActor<AXiaomaBoardActor>(AXiaomaBoardActor::StaticClass());
    BoardCamera = GetWorld()->SpawnActor<ACameraActor>(TableCamera,
        (TableFocus - TableCamera).Rotation());
    if (BoardCamera)
    {
        BoardCamera->GetCameraComponent()->SetFieldOfView(55.f);
    }

    Herdsman = GetWorld()->SpawnActor<AXiaomaHerdsman>(AXiaomaHerdsman::StaticClass());
    if (Herdsman)
    {
        Herdsman->SetOnCellChanged(FXiaomaHerdsmanCellDelegate::CreateUObject(
            this, &AXiaomaGameMode::OnHerdsmanCellChanged));
        Herdsman->SetActorHiddenInGame(true);
        Herdsman->SetMovementEnabled(false);
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
        SummonWheel = CreateWidget<UXiaomaSummonWheel>(Controller, UXiaomaSummonWheel::StaticClass());
        if (SummonWheel)
        {
            SummonWheel->OnSelectColour.BindUObject(this, &AXiaomaGameMode::OnWheelSelectColour);
            SummonWheel->OnRecall.BindUObject(this, &AXiaomaGameMode::OnWheelRecall);
            SummonWheel->OnClose.BindUObject(this, &AXiaomaGameMode::CloseSummonWheel);
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
int32 AXiaomaGameMode::GetParSteps() const
{
    const FXiaomaLevel* Level = GetCurrentLevel();
    return Level ? Level->Adventure.ParSteps : 22;
}
bool AXiaomaGameMode::IsSpeedBoostActive() const
{
    return Herdsman && Herdsman->IsSpeedBoostActive();
}
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
            Progress->Lives = CurrentLives;
        }
        UGameplayStatics::SaveGameToSlot(Progress, SaveSlot, 0);
    }
}

void AXiaomaGameMode::SetScreen(EXiaomaScreen NewScreen)
{
    Screen = NewScreen;
    if (bWheelOpen)
    {
        CloseSummonWheel();
    }
    const bool bMenu = NewScreen == EXiaomaScreen::Menu;
    const bool bPlaying = NewScreen == EXiaomaScreen::Playing;

    if (Herdsman)
    {
        Herdsman->SetActorHiddenInGame(!bPlaying && NewScreen != EXiaomaScreen::Paused
            && NewScreen != EXiaomaScreen::Won && NewScreen != EXiaomaScreen::Lost);
        Herdsman->SetMovementEnabled(bPlaying);
    }

    APlayerController* Controller = UGameplayStatics::GetPlayerController(this, 0);
    if (Controller)
    {
        if (bPlaying && Herdsman)
        {
            Controller->SetViewTargetWithBlend(Herdsman, 0.25f);
        }
        else if (BoardCamera)
        {
            const FVector Framing = bMenu ? FVector(235.f, 0.f, 0.f) : FVector::ZeroVector;
            BoardCamera->SetActorLocationAndRotation(TableCamera + Framing, (TableFocus - TableCamera).Rotation());
            Controller->SetViewTargetWithBlend(BoardCamera, 0.25f);
        }
    }

    if (Board && bMenu && !Levels.IsEmpty())
    {
        const FXiaomaLevel& Preview = Levels[0];
        TArray<uint8> PreviewCells;
        PreviewCells.Init(0, FXiaomaPuzzle::CellCount);
        Board->BuildBoard(Preview);
        Board->RefreshBoard(Preview, PreviewCells);
        Board->SetHighlightedCell(INDEX_NONE);
    }
    if (GameWidget)
    {
        GameWidget->Refresh();
    }
}

EXiaomaSummonMode AXiaomaGameMode::GetSummonMode() const
{
    const uint8 Val = Progress ? Progress->SummonMode : 0;
    return Val <= 2 ? static_cast<EXiaomaSummonMode>(Val) : EXiaomaSummonMode::HoldWheel;
}

void AXiaomaGameMode::CycleSummonMode()
{
    if (Progress)
    {
        Progress->SummonMode = (Progress->SummonMode + 1) % 3;
        SaveProgress();
        if (GameWidget) GameWidget->Refresh();
    }
}

EXiaomaTabCameraMode AXiaomaGameMode::GetTabCameraMode() const
{
    const uint8 Val = Progress ? Progress->TabCameraMode : 0;
    return Val == 1 ? EXiaomaTabCameraMode::Hold : EXiaomaTabCameraMode::Toggle;
}

void AXiaomaGameMode::CycleTabCameraMode()
{
    if (Progress)
    {
        Progress->TabCameraMode = (Progress->TabCameraMode + 1) % 2;
        bOverviewToggleState = false;
        if (Herdsman) Herdsman->SetOverviewCamera(false);
        SaveProgress();
        if (GameWidget) GameWidget->Refresh();
    }
}

void AXiaomaGameMode::StartRebind(EXiaomaBindAction Action)
{
    PendingRebind = Action;
    if (GameWidget) GameWidget->Refresh();
}

void AXiaomaGameMode::CancelRebind()
{
    PendingRebind = EXiaomaBindAction::None;
    if (GameWidget) GameWidget->Refresh();
}

void AXiaomaGameMode::FinishRebind(const FKey& NewKey)
{
    if (!Progress || PendingRebind == EXiaomaBindAction::None || !NewKey.IsValid())
    {
        PendingRebind = EXiaomaBindAction::None;
        return;
    }
    const FName KeyName = NewKey.GetFName();
    switch (PendingRebind)
    {
    case EXiaomaBindAction::Interact: Progress->KeyInteract = KeyName; break;
    case EXiaomaBindAction::Mark:     Progress->KeyMark = KeyName; break;
    case EXiaomaBindAction::Overview: Progress->KeyOverview = KeyName; break;
    case EXiaomaBindAction::Hint:     Progress->KeyHint = KeyName; break;
    case EXiaomaBindAction::Undo:     Progress->KeyUndo = KeyName; break;
    default: break;
    }
    PendingRebind = EXiaomaBindAction::None;
    SaveProgress();
    if (GameWidget) GameWidget->Refresh();
}

void AXiaomaGameMode::ResetDefaultKeys()
{
    if (Progress)
    {
        Progress->KeyInteract = EKeys::E.GetFName();
        Progress->KeyMark = EKeys::F.GetFName();
        Progress->KeyOverview = EKeys::Tab.GetFName();
        Progress->KeyHint = EKeys::H.GetFName();
        Progress->KeyUndo = EKeys::Z.GetFName();
        PendingRebind = EXiaomaBindAction::None;
        SaveProgress();
        if (GameWidget) GameWidget->Refresh();
    }
}

FKey AXiaomaGameMode::GetKeyForAction(EXiaomaBindAction Action) const
{
    if (!Progress)
    {
        switch (Action)
        {
        case EXiaomaBindAction::Interact: return EKeys::E;
        case EXiaomaBindAction::Mark:     return EKeys::F;
        case EXiaomaBindAction::Overview: return EKeys::Tab;
        case EXiaomaBindAction::Hint:     return EKeys::H;
        case EXiaomaBindAction::Undo:     return EKeys::Z;
        default: return EKeys::Invalid;
        }
    }
    FName Name = NAME_None;
    switch (Action)
    {
    case EXiaomaBindAction::Interact: Name = Progress->KeyInteract; break;
    case EXiaomaBindAction::Mark:     Name = Progress->KeyMark; break;
    case EXiaomaBindAction::Overview: Name = Progress->KeyOverview; break;
    case EXiaomaBindAction::Hint:     Name = Progress->KeyHint; break;
    case EXiaomaBindAction::Undo:     Name = Progress->KeyUndo; break;
    default: break;
    }
    const FKey Result(Name);
    return Result.IsValid() ? Result : EKeys::E;
}

void AXiaomaGameMode::ShowMenu() { SetScreen(EXiaomaScreen::Menu); }
void AXiaomaGameMode::ShowLevelSelect() { SetScreen(EXiaomaScreen::LevelSelect); }
void AXiaomaGameMode::ShowSettings()
{
    if (Screen != EXiaomaScreen::Settings)
    {
        PendingRebind = EXiaomaBindAction::None;
        ReturnFromSettingsScreen = Screen;
        SetScreen(EXiaomaScreen::Settings);
    }
}
void AXiaomaGameMode::CloseSettings()
{
    PendingRebind = EXiaomaBindAction::None;
    SetScreen(ReturnFromSettingsScreen == EXiaomaScreen::Paused ? EXiaomaScreen::Paused : EXiaomaScreen::Menu);
}
bool AXiaomaGameMode::IsFullscreen() const
{
    if (GEngine)
    {
        if (const UGameUserSettings* Settings = GEngine->GetGameUserSettings())
        {
            return Settings->GetFullscreenMode() != EWindowMode::Windowed;
        }
    }
    return false;
}
void AXiaomaGameMode::ToggleFullscreen()
{
    if (!GEngine)
    {
        return;
    }
    if (UGameUserSettings* Settings = GEngine->GetGameUserSettings())
    {
        if (Settings->GetFullscreenMode() == EWindowMode::Windowed)
        {
            const FIntPoint DesktopRes = Settings->GetDesktopResolution();
            if (DesktopRes.X > 0 && DesktopRes.Y > 0)
            {
                Settings->SetScreenResolution(DesktopRes);
            }
            Settings->SetFullscreenMode(EWindowMode::WindowedFullscreen);
        }
        else
        {
            Settings->SetScreenResolution(FIntPoint(1280, 720));
            Settings->SetFullscreenMode(EWindowMode::Windowed);
        }
        Settings->ApplySettings(false);
        Settings->SaveSettings();
    }
    if (GameWidget)
    {
        GameWidget->Refresh();
    }
}
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
        BoardInputReadyAt = FPlatformTime::Seconds() + 0.20;
        SetScreen(EXiaomaScreen::Playing);
    }
}

void AXiaomaGameMode::ClearAdventureActors()
{
    for (AXiaomaMechanism* Mech : Mechanisms)
    {
        if (Mech) Mech->Destroy();
    }
    Mechanisms.Reset();

    for (AXiaomaPickup* Pickup : Pickups)
    {
        if (Pickup) Pickup->Destroy();
    }
    Pickups.Reset();
}

void AXiaomaGameMode::SpawnAdventureActors()
{
    ClearAdventureActors();
    const FXiaomaLevel* Level = GetCurrentLevel();
    if (!Level || !Board)
    {
        return;
    }
    for (const FXiaomaChestDef& ChestDef : Level->Adventure.Chests)
    {
        AXiaomaChest* Chest = GetWorld()->SpawnActor<AXiaomaChest>(AXiaomaChest::StaticClass());
        if (Chest)
        {
            Chest->SetupChest(ChestDef, Board->GetCellWorldLocation(ChestDef.Cell));
            Chest->SetOnOpened(FXiaomaChestOpenedDelegate::CreateUObject(
                this, &AXiaomaGameMode::OnChestOpened));
            Mechanisms.Add(Chest);
        }
    }
    for (const FXiaomaItemDef& ItemDef : Level->Adventure.Items)
    {
        AXiaomaPickup* Pickup = GetWorld()->SpawnActor<AXiaomaPickup>(AXiaomaPickup::StaticClass());
        if (Pickup)
        {
            Pickup->SetupPickup(ItemDef, Board->GetCellWorldLocation(ItemDef.Cell));
            Pickup->SetOnCollected(FXiaomaPickupCollectedDelegate::CreateUObject(
                this, &AXiaomaGameMode::OnPickupCollected));
            Pickups.Add(Pickup);
        }
    }
    NotifyMechanismsBoardChanged();
}

void AXiaomaGameMode::StartLevel(int32 LevelNumber)
{
    if (!Levels.IsValidIndex(LevelNumber - 1) || LevelNumber > GetUnlockedLevel())
    {
        return;
    }
    CurrentLevelIndex = LevelNumber - 1;
    const FXiaomaLevel& Level = Levels[CurrentLevelIndex];
    const bool bResume = Progress->bHasActiveRound && Progress->ActiveLevel == LevelNumber
        && Progress->ActiveCells.Num() == FXiaomaPuzzle::CellCount && Progress->Lives > 0;

    HintsUsed = bResume ? Progress->ActiveHintsUsed : 0;
    WrongAttempts = bResume ? Progress->ActiveWrongAttempts : 0;
    CurrentLives = bResume ? FMath::Clamp(Progress->Lives, 1, 5) : Level.Adventure.StartLives;
    StepCount = 0;
    HintTokens = 0;
    PettedMask = 0;
    bHasClover = false;
    bOverviewToggleState = false;
    if (Herdsman) Herdsman->SetOverviewCamera(false);

    if (bResume)
    {
        Cells = Progress->ActiveCells;
        UndoHistory.Reset();
    }
    else
    {
        ResetBoard();
    }

    if (Board)
    {
        Board->BuildBoard(Level);
    }
    SpawnAdventureActors();

    if (Herdsman)
    {
        Herdsman->TeleportToCell(Level.Adventure.StartCell);
        if (Board)
        {
            Board->SetHighlightedCell(Herdsman->GetCurrentCellIndex());
        }
    }

    StatusMessage = TEXT("WASD 移动，E 打开召唤轮盘或开箱，F 标记排除，Tab 看全局");
    BoardInputReadyAt = FPlatformTime::Seconds() + 0.25;
    SetScreen(EXiaomaScreen::Playing);
    RefreshBoardAndUI();
    SaveProgress();
}

void AXiaomaGameMode::ResetBoard()
{
    Cells.Init(0, FXiaomaPuzzle::CellCount);
    UndoHistory.Reset();
}

void AXiaomaGameMode::PushUndoSnapshot()
{
    FXiaomaUndoSnapshot Snap;
    Snap.Cells = Cells;
    Snap.HerdsmanCell = Herdsman ? Herdsman->GetCurrentCell() : FIntPoint(5, 0);
    Snap.StepCount = StepCount;
    Snap.Lives = CurrentLives;
    Snap.HintTokens = HintTokens;
    Snap.bHasClover = bHasClover;
    for (const AXiaomaPickup* Pickup : Pickups)
    {
        Snap.PickupCollected.Add(Pickup ? Pickup->IsCollected() : false);
    }
    for (const AXiaomaMechanism* Mech : Mechanisms)
    {
        TArray<uint8> State;
        if (Mech) Mech->SaveState(State);
        Snap.MechanismStates.Add(MoveTemp(State));
    }
    UndoHistory.Add(MoveTemp(Snap));
}

void AXiaomaGameMode::OnHerdsmanCellChanged(FIntPoint NewCell)
{
    if (Screen != EXiaomaScreen::Playing)
    {
        return;
    }
    ++StepCount;
    if (Board)
    {
        Board->SetHighlightedCell(NewCell.X * FXiaomaPuzzle::Size + NewCell.Y);
    }
    for (AXiaomaMechanism* Mech : Mechanisms)
    {
        if (Mech) Mech->OnHerdsmanEnterCell(NewCell);
    }
    for (AXiaomaPickup* Pickup : Pickups)
    {
        if (Pickup) Pickup->OnHerdsmanEnterCell(NewCell);
    }
    if (GameWidget)
    {
        GameWidget->Refresh();
    }
}

void AXiaomaGameMode::InteractPressed()
{
    if (Screen != EXiaomaScreen::Playing || !Herdsman || FPlatformTime::Seconds() < BoardInputReadyAt)
    {
        return;
    }
    if (bWheelOpen)
    {
        if (GetSummonMode() == EXiaomaSummonMode::ToggleWheel)
        {
            CloseSummonWheel();
        }
        return;
    }

    const FIntPoint Cell = Herdsman->GetCurrentCell();
    for (AXiaomaMechanism* Mech : Mechanisms)
    {
        if (Mech && Mech->TryInteract(Cell))
        {
            if (const AXiaomaChest* Chest = Cast<AXiaomaChest>(Mech))
            {
                if (!Chest->IsUnlocked() && !Chest->IsClaimed())
                {
                    const FXiaomaChestDef& Def = Chest->GetDef();
                    const TCHAR* RuleText = Def.RuleType == EXiaomaChestRuleType::Row ? TEXT("对应行")
                        : (Def.RuleType == EXiaomaChestRuleType::Column ? TEXT("对应列") : TEXT("对应颜色区域"));
                    StatusMessage = FString::Printf(TEXT("宝箱上锁中：在%s第 %d 格放对小马即可开启"),
                        RuleText, Def.RuleIndex + 1);
                    if (GameWidget) GameWidget->Refresh();
                }
            }
            return;
        }
    }

    const FXiaomaLevel* Level = GetCurrentLevel();
    if (!Level)
    {
        return;
    }
    const int32 Idx = Cell.X * FXiaomaPuzzle::Size + Cell.Y;

    // 模式 2：直接放置或请回当前地板颜色的小马
    if (GetSummonMode() == EXiaomaSummonMode::DirectRegion)
    {
        if (Cells[Idx] == 2)
        {
            OnWheelRecall();
        }
        else
        {
            const int32 RegionColour = Level->Regions[Idx];
            bool bAlreadyPlacedElsewhere = false;
            for (int32 I = 0; I < Cells.Num(); ++I)
            {
                if (I != Idx && Cells[I] == 2 && Level->Regions[I] == RegionColour)
                {
                    bAlreadyPlacedElsewhere = true;
                    break;
                }
            }
            if (bAlreadyPlacedElsewhere)
            {
                if (Board) Board->RejectStandbyPony(RegionColour);
                PlayFeedback(TEXT("Error"));
                StatusMessage = TEXT("该颜色的小马已在棋盘上，请先请回");
                if (GameWidget) GameWidget->Refresh();
            }
            else
            {
                OnWheelSelectColour(RegionColour);
            }
        }
        return;
    }

    if (!SummonWheel)
    {
        return;
    }
    TArray<int32> PlacedCellByRegion;
    PlacedCellByRegion.Init(INDEX_NONE, FXiaomaPuzzle::Size);
    for (int32 I = 0; I < Cells.Num(); ++I)
    {
        if (Cells[I] == 2)
        {
            PlacedCellByRegion[Level->Regions[I]] = I;
        }
    }

    bWheelOpen = true;
    Herdsman->SetMovementEnabled(false);
    SummonWheel->OpenWheel(PlacedCellByRegion, Cells[Idx] == 2, Level->Regions[Idx]);
    if (!SummonWheel->IsInViewport())
    {
        SummonWheel->AddToViewport(10);
    }
}

void AXiaomaGameMode::InteractReleased()
{
    if (bWheelOpen && GetSummonMode() == EXiaomaSummonMode::HoldWheel && SummonWheel)
    {
        SummonWheel->ConfirmHoveredSelection();
    }
}

void AXiaomaGameMode::CloseSummonWheel()
{
    bWheelOpen = false;
    if (SummonWheel && SummonWheel->IsInViewport())
    {
        SummonWheel->RemoveFromParent();
    }
    if (Herdsman && Screen == EXiaomaScreen::Playing)
    {
        Herdsman->SetMovementEnabled(true);
    }
}

void AXiaomaGameMode::OnWheelSelectColour(int32 ColourIndex)
{
    CloseSummonWheel();
    const FXiaomaLevel* Level = GetCurrentLevel();
    if (!Level || !Herdsman || Screen != EXiaomaScreen::Playing)
    {
        return;
    }
    const int32 Idx = Herdsman->GetCurrentCellIndex();
    if (!Cells.IsValidIndex(Idx))
    {
        return;
    }
    if (Level->Regions[Idx] != ColourIndex)
    {
        if (Board)
        {
            Board->RejectStandbyPony(ColourIndex);
        }
        PlayFeedback(TEXT("Error"));
        StatusMessage = TEXT("颜色与色区不符，小马摇头拒绝入驻");
        if (GameWidget) GameWidget->Refresh();
        return;
    }

    PushUndoSnapshot();
    Cells[Idx] = 2;
    PlayFeedback(TEXT("Place"));
    StatusMessage = TEXT("已召唤小马入驻当前格");
    NotifyMechanismsBoardChanged();
    RefreshBoardAndUI();
    SaveProgress();
}

void AXiaomaGameMode::OnWheelRecall()
{
    CloseSummonWheel();
    if (!Herdsman || Screen != EXiaomaScreen::Playing)
    {
        return;
    }
    const int32 Idx = Herdsman->GetCurrentCellIndex();
    if (Cells.IsValidIndex(Idx) && Cells[Idx] == 2)
    {
        PushUndoSnapshot();
        Cells[Idx] = 0;
        StatusMessage = TEXT("已请回小马到待命区");
        NotifyMechanismsBoardChanged();
        RefreshBoardAndUI();
        SaveProgress();
    }
}

void AXiaomaGameMode::ToggleMarkAtCurrentCell()
{
    if (Screen != EXiaomaScreen::Playing || !Herdsman || bWheelOpen
        || FPlatformTime::Seconds() < BoardInputReadyAt)
    {
        return;
    }
    const int32 Idx = Herdsman->GetCurrentCellIndex();
    if (!Cells.IsValidIndex(Idx) || Cells[Idx] == 2)
    {
        return;
    }
    PushUndoSnapshot();
    Cells[Idx] = Cells[Idx] == 1 ? 0 : 1;
    StatusMessage = Cells[Idx] == 1 ? TEXT("已放置 X 排除标记") : TEXT("已清除 X 排除标记");
    RefreshBoardAndUI();
    SaveProgress();
}

void AXiaomaGameMode::OverviewPressed()
{
    if (!Herdsman || Screen != EXiaomaScreen::Playing)
    {
        return;
    }
    if (GetTabCameraMode() == EXiaomaTabCameraMode::Toggle)
    {
        bOverviewToggleState = !bOverviewToggleState;
        Herdsman->SetOverviewCamera(bOverviewToggleState);
    }
    else
    {
        Herdsman->SetOverviewCamera(true);
    }
}

void AXiaomaGameMode::OverviewReleased()
{
    if (Herdsman && Screen == EXiaomaScreen::Playing && GetTabCameraMode() == EXiaomaTabCameraMode::Hold)
    {
        Herdsman->SetOverviewCamera(false);
    }
}

void AXiaomaGameMode::PetPony(int32 ColourIndex)
{
    if (Screen != EXiaomaScreen::Playing || ColourIndex < 0 || ColourIndex >= 6)
    {
        return;
    }
    if (Board)
    {
        Board->PetPony(ColourIndex);
    }
    PlayFeedback(TEXT("Place"));
    const int32 Bit = (1 << ColourIndex);
    if ((PettedMask & Bit) == 0 && Progress)
    {
        PettedMask |= Bit;
        ++Progress->Affection;
        StatusMessage = TEXT("轻抚了小马，好感度 +1");
        SaveProgress();
    }
    else
    {
        StatusMessage = TEXT("小马开心地跳了一下");
    }
    if (GameWidget)
    {
        GameWidget->Refresh();
    }
}

void AXiaomaGameMode::NotifyMechanismsBoardChanged()
{
    const FXiaomaLevel* Level = GetCurrentLevel();
    if (!Level)
    {
        return;
    }
    for (AXiaomaMechanism* Mech : Mechanisms)
    {
        if (Mech)
        {
            Mech->OnBoardChanged(*Level, Cells);
        }
    }
}

void AXiaomaGameMode::GrantItemReward(EXiaomaItemType ItemType, const TCHAR* SourceLabel)
{
    switch (ItemType)
    {
    case EXiaomaItemType::Hint:
        ++HintTokens;
        StatusMessage = FString::Printf(TEXT("%s：获得 1 颗提示星（按 H 使用）"), SourceLabel);
        break;
    case EXiaomaItemType::Apple:
        CurrentLives = FMath::Min(5, CurrentLives + 1);
        StatusMessage = FString::Printf(TEXT("%s：获得苹果，生命 +1"), SourceLabel);
        break;
    case EXiaomaItemType::Carrot:
        if (Herdsman) Herdsman->SetSpeedBoost(3.0f);
        StatusMessage = FString::Printf(TEXT("%s：获得胡萝卜，3 秒内移速提升！"), SourceLabel);
        break;
    case EXiaomaItemType::Clover:
        bHasClover = true;
        StatusMessage = FString::Printf(TEXT("%s：获得四叶草，可抵挡下一次扣命"), SourceLabel);
        break;
    }
    PlayFeedback(TEXT("Place"));
    if (GameWidget)
    {
        GameWidget->Refresh();
    }
    SaveProgress();
}

void AXiaomaGameMode::OnChestOpened(EXiaomaItemType Reward, FIntPoint Cell, FString& OutMessage)
{
    PushUndoSnapshot();
    GrantItemReward(Reward, TEXT("开启宝箱"));
}

void AXiaomaGameMode::OnPickupCollected(EXiaomaItemType ItemType, FIntPoint Cell)
{
    PushUndoSnapshot();
    GrantItemReward(ItemType, TEXT("拾取道具"));
}

void AXiaomaGameMode::LoseLife(const FString& Reason)
{
    if (!Progress)
    {
        return;
    }
    if (bHasClover)
    {
        bHasClover = false;
        PlayFeedback(TEXT("Place"));
        StatusMessage = TEXT("四叶草生效，抵挡了一次扣命！");
        if (GameWidget) GameWidget->Refresh();
        SaveProgress();
        return;
    }
    ++WrongAttempts;
    PlayFeedback(TEXT("Error"));
    CurrentLives = FMath::Max(0, CurrentLives - 1);
    Progress->Lives = CurrentLives;
    if (CurrentLives == 0)
    {
        Progress->WinStreak = 0;
        Progress->bHasActiveRound = false;
        StatusMessage = TEXT("生命值耗尽，再试一次吧");
        SetScreen(EXiaomaScreen::Lost);
    }
    else
    {
        StatusMessage = Reason;
    }
    SaveProgress();
}

void AXiaomaGameMode::Undo()
{
    if (Screen != EXiaomaScreen::Playing || UndoHistory.IsEmpty())
    {
        return;
    }
    if (bWheelOpen)
    {
        CloseSummonWheel();
    }
    const FXiaomaUndoSnapshot Snap = UndoHistory.Pop();
    Cells = Snap.Cells;
    StepCount = Snap.StepCount;
    CurrentLives = Snap.Lives;
    HintTokens = Snap.HintTokens;
    bHasClover = Snap.bHasClover;
    if (Herdsman)
    {
        Herdsman->TeleportToCell(Snap.HerdsmanCell);
        if (Board) Board->SetHighlightedCell(Herdsman->GetCurrentCellIndex());
    }
    for (int32 I = 0; I < Pickups.Num() && I < Snap.PickupCollected.Num(); ++I)
    {
        if (Pickups[I]) Pickups[I]->SetCollected(Snap.PickupCollected[I]);
    }
    for (int32 I = 0; I < Mechanisms.Num() && I < Snap.MechanismStates.Num(); ++I)
    {
        if (Mechanisms[I]) Mechanisms[I]->LoadState(Snap.MechanismStates[I]);
    }
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
    if (HintTokens <= 0)
    {
        StatusMessage = TEXT("提示星不足，去解开场上的宝箱获取吧");
        if (GameWidget) GameWidget->Refresh();
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
        PushUndoSnapshot();
        --HintTokens;
        Cells[Cell.X * FXiaomaPuzzle::Size + Cell.Y] = 2;
        ++HintsUsed;
        PlayFeedback(TEXT("Place"));
        StatusMessage = FString::Printf(TEXT("消耗 1 颗提示星：第 %d 行，第 %d 列"), Cell.X + 1, Cell.Y + 1);
        NotifyMechanismsBoardChanged();
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
    const FXiaomaLevel* Level = GetCurrentLevel();
    const EXiaomaBoardResult Result = FXiaomaPuzzle::CheckBoard(*Level, Cells, StatusMessage);
    if (Result == EXiaomaBoardResult::Invalid)
    {
        LoseLife(StatusMessage + TEXT("，失去一条生命"));
    }
    else if (Result == EXiaomaBoardResult::Solved)
    {
        const int32 Par = Level->Adventure.ParSteps;
        int32 Stars = 1;
        if (StepCount <= Par) ++Stars;
        if (HintsUsed == 0) ++Stars;

        Progress->StarsByLevel[CurrentLevelIndex] = FMath::Max(
            Progress->StarsByLevel[CurrentLevelIndex], Stars);
        Progress->UnlockedLevel = FMath::Min(25,
            FMath::Max(Progress->UnlockedLevel, CurrentLevelIndex + 2));
        Progress->Affection += 5;
        ++Progress->WinStreak;
        PlayFeedback(TEXT("Win"));
        Progress->BestStreak = FMath::Max(Progress->BestStreak, Progress->WinStreak);
        Progress->bHasActiveRound = false;
        StatusMessage = FString::Printf(TEXT("通关！步数 %d/%d，获得 %d 颗星，好感度 +5"),
            StepCount, Par, Stars);
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
    const FXiaomaLevel* Level = GetCurrentLevel();
    if (!Level)
    {
        return;
    }
    CurrentLives = Level->Adventure.StartLives;
    Progress->Lives = CurrentLives;
    Progress->WinStreak = 0;
    StepCount = 0;
    HintTokens = 0;
    HintsUsed = 0;
    WrongAttempts = 0;
    PettedMask = 0;
    bHasClover = false;
    bOverviewToggleState = false;
    if (Herdsman) Herdsman->SetOverviewCamera(false);
    ResetBoard();
    SpawnAdventureActors();
    if (Herdsman)
    {
        Herdsman->TeleportToCell(Level->Adventure.StartCell);
        if (Board) Board->SetHighlightedCell(Herdsman->GetCurrentCellIndex());
    }
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
