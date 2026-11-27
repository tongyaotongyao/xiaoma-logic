#include "XiaomaGameMode.h"

#include "XiaomaBoardActor.h"
#include "XiaomaHerdsman.h"
#include "XiaomaHUDWidget.h"
#include "XiaomaMechanism.h"
#include "XiaomaPatroller.h"
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
/** 走格子触发的存盘最小间隔：玩家连续跑动时最多每两秒写一次盘 */
constexpr double RoundSaveIntervalSeconds = 2.0;
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
        // 轮盘里只有当前格颜色放得上去，等于让玩家自己挑错误答案；默认改成直接放当前色
        Progress->MigrateSummonMode();
        // 解锁进度被回退或存档异常时，续局关卡不能越过已经解锁的范围
        Progress->ActiveLevel = FMath::Clamp(Progress->ActiveLevel, 1, FMath::Max(1, Progress->UnlockedLevel));
        // 关卡数据改过（机关、宝箱谜题或道具布局变了）时，旧续局里的机关状态会错位，整局丢弃
        if (Progress->bHasActiveRound
            && (!Levels.IsValidIndex(Progress->ActiveLevel - 1)
                || Progress->ActiveLevelFingerprint
                    != FXiaomaPuzzle::AdventureFingerprint(Levels[Progress->ActiveLevel - 1])))
        {
            UE_LOG(LogTemp, Warning, TEXT("续局与当前关卡布局不一致，已丢弃进行中的对局"));
            Progress->ClearActiveRound();
        }
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

void AXiaomaGameMode::EndPlay(const EEndPlayReason::Type EndPlayReason)
{
    // 直接关窗口时不会有暂停/回菜单，只能在这里补一次落盘；没脏就不重复写
    if (Screen == EXiaomaScreen::Playing && Progress && Progress->bHasActiveRound && bRoundDirty)
    {
        SaveProgress();
    }
    Super::EndPlay(EndPlayReason);
}

const FXiaomaLevel* AXiaomaGameMode::GetCurrentLevel() const
{
    return Levels.IsValidIndex(CurrentLevelIndex) ? &Levels[CurrentLevelIndex] : nullptr;
}

bool AXiaomaGameMode::IsCellBlocked(FIntPoint Cell) const
{
    for (const AXiaomaMechanism* Mech : Mechanisms)
    {
        if (Mech && Mech->BlocksCell(Cell))
        {
            return true;
        }
    }
    return false;
}

int32 AXiaomaGameMode::GetUnlockedLevel() const { return Progress ? Progress->UnlockedLevel : 1; }
int32 AXiaomaGameMode::GetContinueLevel() const
{
    return Progress ? Progress->GetContinueLevel(Levels.Num()) : 1;
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
    if (!Progress)
    {
        return;
    }
    CaptureActiveRound();
    bRoundDirty = false;
    LastRoundSaveAt = FPlatformTime::Seconds();
    UGameplayStatics::SaveGameToSlot(Progress, SaveSlot, 0);
}

bool AXiaomaGameMode::CanResumeActiveRound(const FXiaomaLevel& Level) const
{
    return Progress && Progress->CanResume(Level);
}

void AXiaomaGameMode::CaptureActiveRound()
{
    if (!Progress || Screen != EXiaomaScreen::Playing || !Levels.IsValidIndex(CurrentLevelIndex))
    {
        return;
    }
    const FXiaomaLevel& Level = Levels[CurrentLevelIndex];
    Progress->bHasActiveRound = true;
    Progress->ActiveLevel = CurrentLevelIndex + 1;
    Progress->ActiveCells = Cells;
    Progress->ActiveStepCount = StepCount;
    Progress->ActiveHintsUsed = HintsUsed;
    Progress->ActiveWrongAttempts = WrongAttempts;
    Progress->ActiveHintTokens = HintTokens;
    Progress->ActivePettedMask = PettedMask;
    Progress->ActiveHasClover = bHasClover;
    Progress->ActiveHerdsmanCell = Herdsman ? Herdsman->GetCurrentCell() : Level.Adventure.StartCell;
    Progress->ActiveLevelFingerprint = FXiaomaPuzzle::AdventureFingerprint(Level);
    Progress->Lives = CurrentLives;

    Progress->ActivePickupCollected.Reset();
    for (const AXiaomaPickup* Pickup : Pickups)
    {
        Progress->ActivePickupCollected.Add(Pickup && Pickup->IsCollected());
    }

    TArray<TArray<uint8>> States;
    States.Reserve(Mechanisms.Num());
    for (const AXiaomaMechanism* Mech : Mechanisms)
    {
        TArray<uint8> State;
        if (Mech)
        {
            Mech->SaveState(State);
        }
        States.Add(MoveTemp(State));
    }
    FXiaomaMechanismStateCodec::Encode(States, Progress->ActiveMechanismStates);
}

void AXiaomaGameMode::RestoreActiveRound(const FXiaomaLevel& Level)
{
    if (!Progress)
    {
        return;
    }
    StepCount = FMath::Max(0, Progress->ActiveStepCount);
    HintTokens = FMath::Max(0, Progress->ActiveHintTokens);
    PettedMask = Progress->ActivePettedMask;
    bHasClover = Progress->ActiveHasClover;

    TArray<TArray<uint8>> States;
    if (!FXiaomaMechanismStateCodec::Decode(Progress->ActiveMechanismStates, States))
    {
        // 状态流损坏：整关按新开局处理，不要恢复出一半正常一半诡异的机关
        UE_LOG(LogTemp, Warning, TEXT("机关状态数据损坏，本关按新开局处理"));
        Progress->ClearActiveRound();
        ResetBoard();
        StepCount = 0;
        HintTokens = 0;
        PettedMask = 0;
        bHasClover = false;
        CurrentLives = Level.Adventure.StartLives;
        Progress->Lives = CurrentLives;
        for (AXiaomaPickup* Pickup : Pickups)
        {
            if (Pickup) Pickup->SetCollected(false);
        }
        if (Herdsman)
        {
            Herdsman->TeleportToCell(Level.Adventure.StartCell);
        }
        // 棋盘刚被清空，让规则宝箱跟着回到未解锁状态
        NotifyMechanismsBoardChanged();
        return;
    }

    for (int32 Index = 0; Index < Pickups.Num() && Index < Progress->ActivePickupCollected.Num(); ++Index)
    {
        if (Pickups[Index])
        {
            Pickups[Index]->SetCollected(Progress->ActivePickupCollected[Index]);
        }
    }
    for (int32 Index = 0; Index < Mechanisms.Num() && Index < States.Num(); ++Index)
    {
        if (Mechanisms[Index])
        {
            Mechanisms[Index]->LoadState(States[Index]);
        }
    }
    // 木箱位置是从存档读回来的，压力板视觉按它重算一遍
    RefreshPushPlates(false);
}

void AXiaomaGameMode::SetScreen(EXiaomaScreen NewScreen)
{
    // 离开进行中状态（暂停、通关、失败、回菜单）之前先落盘，避免最近几格移动丢掉。
    // 通关/失败时 bHasActiveRound 已被置为 false，这里不会把已结束的对局又写回去。
    if (Screen == EXiaomaScreen::Playing && NewScreen != EXiaomaScreen::Playing
        && Progress && Progress->bHasActiveRound)
    {
        SaveProgress();
    }
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

    // 暂停、菜单、结算时捣蛋鬼也停住，否则会在暂停界面里继续走动甚至撞人
    for (AXiaomaMechanism* Mech : Mechanisms)
    {
        if (AXiaomaPatroller* Patroller = Cast<AXiaomaPatroller>(Mech))
        {
            Patroller->SetPatrolEnabled(bPlaying);
        }
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
    for (const FIntPoint& ObstacleCell : Level->Adventure.Obstacles)
    {
        AXiaomaObstacle* Obstacle = GetWorld()->SpawnActor<AXiaomaObstacle>(AXiaomaObstacle::StaticClass());
        if (Obstacle)
        {
            // 按格子序号轮换外观，让同一关里不出现一模一样的石头
            const int32 Variant = ObstacleCell.X * FXiaomaPuzzle::Size + ObstacleCell.Y;
            Obstacle->SetupObstacle(ObstacleCell, Board->GetCellWorldLocation(ObstacleCell), Variant);
            Mechanisms.Add(Obstacle);
        }
    }
    for (const FXiaomaChestDef& ChestDef : Level->Adventure.Chests)
    {
        AXiaomaChest* Chest = GetWorld()->SpawnActor<AXiaomaChest>(AXiaomaChest::StaticClass());
        if (Chest)
        {
            Chest->SetupChest(ChestDef, Board->GetCellWorldLocation(ChestDef.Cell));
            Chest->SetOnOpened(FXiaomaChestOpenedDelegate::CreateUObject(
                this, &AXiaomaGameMode::OnChestOpened));
            Chest->SetOnNotice(FXiaomaChestNoticeDelegate::CreateUObject(
                this, &AXiaomaGameMode::OnChestNotice));
            Mechanisms.Add(Chest);
        }
    }
    // 木箱排在宝箱之后：撤销/读档按 Mechanisms 下标配位，顺序必须与存档一致
    for (const FXiaomaBoxDef& BoxDef : Level->Adventure.Boxes)
    {
        AXiaomaPushBox* Box = GetWorld()->SpawnActor<AXiaomaPushBox>(AXiaomaPushBox::StaticClass());
        if (Box)
        {
            Box->SetupBox(BoxDef.Cell, Board->GetCellWorldLocation(BoxDef.Cell));
            Mechanisms.Add(Box);
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
    for (const FXiaomaPatrolDef& PatrolDef : Level->Adventure.Patrols)
    {
        AXiaomaPatroller* Patroller = GetWorld()->SpawnActor<AXiaomaPatroller>(AXiaomaPatroller::StaticClass());
        if (Patroller)
        {
            Patroller->SetupPatroller(PatrolDef, Board, Herdsman);
            Patroller->SetOnHitHerdsman(FXiaomaPatrollerHitDelegate::CreateUObject(
                this, &AXiaomaGameMode::OnPatrollerHitHerdsman));
            Mechanisms.Add(Patroller);
        }
    }
    NotifyMechanismsBoardChanged();
    RefreshPushPlates(false);
}

void AXiaomaGameMode::StartLevel(int32 LevelNumber)
{
    if (!Levels.IsValidIndex(LevelNumber - 1) || LevelNumber > GetUnlockedLevel())
    {
        return;
    }
    CurrentLevelIndex = LevelNumber - 1;
    const FXiaomaLevel& Level = Levels[CurrentLevelIndex];
    const bool bResume = CanResumeActiveRound(Level);
    // 「继续游戏」要回到玩家真正玩到的地方，不能靠解锁进度猜
    Progress->LastPlayedLevel = LevelNumber;

    HintsUsed = bResume ? Progress->ActiveHintsUsed : 0;
    WrongAttempts = bResume ? Progress->ActiveWrongAttempts : 0;
    CurrentLives = bResume ? FMath::Clamp(Progress->Lives, 1, 5) : Level.Adventure.StartLives;
    StepCount = 0;
    HintTokens = 0;
    PettedMask = 0;
    bHasClover = false;
    bOverviewToggleState = false;
    bRoundDirty = false;
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

    // 续局时回到上次站的位置，而不是一律回起点
    FIntPoint SpawnCell = Level.Adventure.StartCell;
    if (bResume && Progress->ActiveHerdsmanCell.X >= 0 && Progress->ActiveHerdsmanCell.Y >= 0)
    {
        SpawnCell = Progress->ActiveHerdsmanCell;
    }

    if (Herdsman)
    {
        Herdsman->SetCellBlockedPredicate([this](FIntPoint Cell) { return IsCellBlocked(Cell); });
        Herdsman->SetCellPushHandler(FXiaomaHerdsmanPushDelegate::CreateUObject(
            this, &AXiaomaGameMode::TryPushBoxAtCell));
        Herdsman->TeleportToCell(SpawnCell);
        if (Board)
        {
            Board->SetHighlightedCell(Herdsman->GetCurrentCellIndex());
        }
    }

    if (bResume)
    {
        // 机关与道具是在 SpawnAdventureActors 里新建的，必须等它们都就位后再贴存档状态
        RestoreActiveRound(Level);
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
    // 走格子会改步数、踩亮符文、拾取道具，这些都不经过其他存档点，只能在这里标脏后节流落盘
    bRoundDirty = true;
    if (FPlatformTime::Seconds() - LastRoundSaveAt >= RoundSaveIntervalSeconds)
    {
        SaveProgress();
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
                const FXiaomaChestDef& Def = Chest->GetDef();
                bool bShowPrompt = false;
                if (!Chest->IsClaimed()
                    && Def.PuzzleType == EXiaomaChestPuzzleType::Stele
                    && Cell == Def.SteleCell && !Def.ClueText.IsEmpty())
                {
                    // 站在石碑上按 E：念出线索，代替轮盘
                    StatusMessage = FString::Printf(TEXT("石碑：%s"), *Def.ClueText);
                    bShowPrompt = true;
                }
                else if (!Chest->IsClaimed())
                {
                    const FString Prompt = Chest->BuildPrompt();
                    if (!Prompt.IsEmpty())
                    {
                        StatusMessage = Prompt;
                        bShowPrompt = true;
                    }
                }
                if (bShowPrompt && GameWidget)
                {
                    GameWidget->Refresh();
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

    // 障碍格既不能站人也不能放马，轮盘直接不给开
    if (IsCellBlocked(Cell))
    {
        PlayFeedback(TEXT("Error"));
        StatusMessage = TEXT("这一格被障碍占住了，换个格子再召唤");
        if (GameWidget) GameWidget->Refresh();
        return;
    }

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
    if (IsCellBlocked(Herdsman->GetCurrentCell()))
    {
        PlayFeedback(TEXT("Error"));
        StatusMessage = TEXT("这一格被障碍占住了，小马进不来");
        if (GameWidget) GameWidget->Refresh();
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
    // 捣蛋鬼要立刻知道标记变了：X 对它是路障
    NotifyMechanismsBoardChanged();
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

void AXiaomaGameMode::OnChestNotice(const FString& Message, bool bPositive)
{
    StatusMessage = Message;
    PlayFeedback(bPositive ? TEXT("Place") : TEXT("Error"));
    if (GameWidget)
    {
        GameWidget->Refresh();
    }
}

void AXiaomaGameMode::OnPickupCollected(EXiaomaItemType ItemType, FIntPoint Cell)
{
    PushUndoSnapshot();
    GrantItemReward(ItemType, TEXT("拾取道具"));
}

void AXiaomaGameMode::OnPatrollerHitHerdsman(AXiaomaPatroller* Patroller)
{
    if (Screen != EXiaomaScreen::Playing || !Herdsman)
    {
        return;
    }
    const FXiaomaLevel* Level = GetCurrentLevel();
    if (!Level)
    {
        return;
    }
    // 先弹回起点再扣命：LoseLife 会立即落盘，存档里记的应该是弹回之后的位置
    Herdsman->TeleportToCell(Level->Adventure.StartCell);
    LoseLife(TEXT("被捣蛋鬼撞到了，弹回起点并失去一条生命"));
}

bool AXiaomaGameMode::CanBoxEnter(FIntPoint Cell, const AXiaomaMechanism* IgnoredBox) const
{
    if (Cell.X < 0 || Cell.X >= FXiaomaPuzzle::Size || Cell.Y < 0 || Cell.Y >= FXiaomaPuzzle::Size)
    {
        return false;
    }
    // 宝箱和石碑本身不阻挡走人，但木箱压上去会盖住它们，得单独排掉
    if (const FXiaomaLevel* Level = GetCurrentLevel())
    {
        for (const FXiaomaChestDef& Chest : Level->Adventure.Chests)
        {
            if (Chest.Cell == Cell || Chest.SteleCell == Cell)
            {
                return false;
            }
        }
    }
    for (const AXiaomaMechanism* Mech : Mechanisms)
    {
        if (Mech && Mech != IgnoredBox && Mech->BlocksCell(Cell))
        {
            return false;
        }
    }
    // 没捡的道具被压住就再也拿不到了，直接不许推过去
    for (const AXiaomaPickup* Pickup : Pickups)
    {
        if (Pickup && !Pickup->IsCollected() && Pickup->GetCell() == Cell)
        {
            return false;
        }
    }
    return true;
}

bool AXiaomaGameMode::TryPushBoxAtCell(FIntPoint BoxCell, FIntPoint StepDelta)
{
    if (Screen != EXiaomaScreen::Playing || !Board)
    {
        return false;
    }
    AXiaomaPushBox* Box = nullptr;
    for (AXiaomaMechanism* Mech : Mechanisms)
    {
        AXiaomaPushBox* Candidate = Cast<AXiaomaPushBox>(Mech);
        if (Candidate && Candidate->GetCell() == BoxCell)
        {
            Box = Candidate;
            break;
        }
    }
    if (!Box)
    {
        return false;
    }
    const FIntPoint Target = BoxCell + StepDelta;
    if (!CanBoxEnter(Target, Box))
    {
        return false;
    }
    // 推箱会改机关状态，先压快照：此时主角还在箱子后面，撤销能完整回到推之前
    PushUndoSnapshot();
    Box->MoveToCell(Target, Board->GetCellWorldLocation(Target));
    PlayFeedback(TEXT("Place"));
    RefreshPushPlates(true);
    bRoundDirty = true;
    if (GameWidget) GameWidget->Refresh();
    return true;
}

void AXiaomaGameMode::RefreshPushPlates(bool bAnnounce)
{
    const FXiaomaLevel* Level = GetCurrentLevel();
    if (!Level)
    {
        return;
    }
    for (AXiaomaMechanism* Mech : Mechanisms)
    {
        AXiaomaChest* Chest = Cast<AXiaomaChest>(Mech);
        if (!Chest || Chest->GetDef().PuzzleType != EXiaomaChestPuzzleType::Push)
        {
            continue;
        }
        const FIntPoint Plate = Chest->GetDef().PushPlate;
        bool bPressed = false;
        for (const AXiaomaMechanism* Other : Mechanisms)
        {
            const AXiaomaPushBox* Box = Cast<AXiaomaPushBox>(Other);
            if (Box && Box->GetCell() == Plate)
            {
                bPressed = true;
                break;
            }
        }
        Chest->SetPlatePressed(bPressed);
        const bool bWasLocked = !Chest->IsUnlocked() && !Chest->IsClaimed();
        if (bPressed && bWasLocked)
        {
            // 一次性锁存：之后把箱子推走也不会重新上锁
            Chest->UnlockByMechanism();
            if (bAnnounce)
            {
                PlayFeedback(TEXT("Win"));
                StatusMessage = TEXT("木箱压上了压力板，宝箱弹开了——走过去按 E 领取");
            }
        }
    }
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
    // Cells 被整体换掉了，捣蛋鬼缓存的标记格也要跟着更新
    NotifyMechanismsBoardChanged();
    // 木箱可能被撤回到压力板之外，压力板视觉要跟着回去
    RefreshPushPlates(false);
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
        // 通关后「继续游戏」落到下一关；最后一关则停在最后一关
        Progress->LastPlayedLevel = FMath::Min(Levels.Num(), CurrentLevelIndex + 2);
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
