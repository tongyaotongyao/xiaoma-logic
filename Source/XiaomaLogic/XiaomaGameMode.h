#pragma once

#include "CoreMinimal.h"
#include "GameFramework/GameModeBase.h"
#include "XiaomaPuzzle.h"
#include "XiaomaGameMode.generated.h"

class AXiaomaBoardActor;
class ACameraActor;
class AXiaomaHerdsman;
class AXiaomaMechanism;
class AXiaomaPickup;
class UXiaomaHUDWidget;
class UXiaomaSummonWheel;
class UXiaomaSaveGame;

enum class EXiaomaScreen : uint8
{
    Menu,
    LevelSelect,
    Settings,
    Playing,
    Paused,
    Won,
    Lost
};

enum class EXiaomaSummonMode : uint8
{
    HoldWheel = 0,
    ToggleWheel = 1,
    DirectRegion = 2
};

enum class EXiaomaTabCameraMode : uint8
{
    Toggle = 0,
    Hold = 1
};

enum class EXiaomaBindAction : uint8
{
    None = 0,
    Interact,
    Mark,
    Overview,
    Hint,
    Undo
};

/** 每次操作前的撤销快照（包含棋盘、主角位置、道具与机关状态） */
struct FXiaomaUndoSnapshot
{
    TArray<uint8> Cells;
    FIntPoint HerdsmanCell = FIntPoint(5, 0);
    int32 StepCount = 0;
    int32 Lives = 3;
    int32 HintTokens = 0;
    bool bHasClover = false;
    TArray<bool> PickupCollected;
    TArray<TArray<uint8>> MechanismStates;
};

UCLASS()
class XIAOMALOGIC_API AXiaomaGameMode : public AGameModeBase
{
    GENERATED_BODY()

public:
    AXiaomaGameMode();
    virtual void BeginPlay() override;
    virtual void EndPlay(const EEndPlayReason::Type EndPlayReason) override;

    void ShowMenu();
    void ShowLevelSelect();
    void ShowSettings();
    void CloseSettings();
    void ToggleFullscreen();
    bool IsFullscreen() const;
    void CycleSummonMode();
    EXiaomaSummonMode GetSummonMode() const;
    void CycleTabCameraMode();
    EXiaomaTabCameraMode GetTabCameraMode() const;
    void StartRebind(EXiaomaBindAction Action);
    void CancelRebind();
    void FinishRebind(const FKey& NewKey);
    void ResetDefaultKeys();
    EXiaomaBindAction GetPendingRebind() const { return PendingRebind; }
    FKey GetKeyForAction(EXiaomaBindAction Action) const;
    void StartLevel(int32 LevelNumber);
    void PauseGame();
    void ResumeGame();
    void NextLevel();
    void RestartLevel();

    /** 冒险版核心交互接口 */
    void InteractPressed();
    void InteractReleased();
    void ToggleMarkAtCurrentCell();
    void CloseSummonWheel();
    void OverviewPressed();
    void OverviewReleased();
    void PetPony(int32 ColourIndex);
    bool IsSummonWheelOpen() const { return bWheelOpen; }

    void Undo();
    void UseHint();
    void SubmitAnswer();
    void ToggleSound();
    bool IsSoundEnabled() const;

    EXiaomaScreen GetScreen() const { return Screen; }
    int32 GetLevelNumber() const { return CurrentLevelIndex + 1; }
    int32 GetUnlockedLevel() const;
    int32 GetContinueLevel() const;
    int32 GetLives() const { return CurrentLives; }
    int32 GetStepCount() const { return StepCount; }
    int32 GetParSteps() const;
    int32 GetHintTokens() const { return HintTokens; }
    bool HasClover() const { return bHasClover; }
    bool IsSpeedBoostActive() const;
    int32 GetAffection() const;
    int32 GetWinStreak() const;
    int32 GetBestStreak() const;
    int32 GetLevelStars(int32 LevelNumber) const;
    int32 GetCurrentStars() const;
    int32 GetHintCount() const { return HintsUsed; }
    const FString& GetStatusMessage() const { return StatusMessage; }
    AXiaomaBoardActor* GetBoard() const { return Board; }
    AXiaomaHerdsman* GetHerdsman() const { return Herdsman; }
    const FXiaomaLevel* GetCurrentLevel() const;

    /** 这一格当前是否被机关封住（障碍、未打开的围栏等） */
    bool IsCellBlocked(FIntPoint Cell) const;

private:
    void SetScreen(EXiaomaScreen NewScreen);
    void RefreshBoardAndUI();
    void SaveProgress();
    /** 把当前进行中的对局状态写进 Progress（只在 Playing 且关卡有效时生效） */
    void CaptureActiveRound();
    /** 从 Progress 恢复进行中的对局状态；必须在 SpawnAdventureActors 之后调用 */
    void RestoreActiveRound(const FXiaomaLevel& Level);
    /** 存档里的续局是否还能用：关卡、格子数、生命、布局指纹都要对得上 */
    bool CanResumeActiveRound(const FXiaomaLevel& Level) const;
    void ResetBoard();
    void SpawnAdventureActors();
    void ClearAdventureActors();
    void PushUndoSnapshot();
    void OnHerdsmanCellChanged(FIntPoint NewCell);
    void OnWheelSelectColour(int32 ColourIndex);
    void OnWheelRecall();
    void OnChestOpened(EXiaomaItemType Reward, FIntPoint Cell, FString& OutMessage);
    void OnChestNotice(const FString& Message, bool bPositive);
    void OnPickupCollected(EXiaomaItemType ItemType, FIntPoint Cell);
    /** 巡逻捣蛋鬼撞到主角：扣 1 命并弹回本关起点 */
    void OnPatrollerHitHerdsman(class AXiaomaPatroller* Patroller);
    void GrantItemReward(EXiaomaItemType ItemType, const TCHAR* SourceLabel);
    void NotifyMechanismsBoardChanged();
    void LoseLife(const FString& Reason);
    void PlayFeedback(const TCHAR* Name);

    TArray<FXiaomaLevel> Levels;
    TArray<uint8> Cells; // 0 空, 1 标记 X, 2 小马
    TArray<FXiaomaUndoSnapshot> UndoHistory;
    int32 CurrentLevelIndex = -1;
    int32 CurrentLives = 3;
    int32 StepCount = 0;
    int32 HintTokens = 0;
    int32 HintsUsed = 0;
    int32 WrongAttempts = 0;
    int32 PettedMask = 0;
    bool bHasClover = false;
    bool bWheelOpen = false;
    bool bOverviewToggleState = false;
    EXiaomaBindAction PendingRebind = EXiaomaBindAction::None;
    double BoardInputReadyAt = 0.0;
    /** 自上次落盘以来走格子改动过状态，等待节流写盘 */
    bool bRoundDirty = false;
    double LastRoundSaveAt = 0.0;
    EXiaomaScreen Screen = EXiaomaScreen::Menu;
    EXiaomaScreen ReturnFromSettingsScreen = EXiaomaScreen::Menu;
    FString StatusMessage;

    UPROPERTY()
    TObjectPtr<AXiaomaBoardActor> Board;

    UPROPERTY()
    TObjectPtr<ACameraActor> BoardCamera;

    UPROPERTY()
    TObjectPtr<AXiaomaHerdsman> Herdsman;

    UPROPERTY()
    TArray<TObjectPtr<AXiaomaMechanism>> Mechanisms;

    UPROPERTY()
    TArray<TObjectPtr<AXiaomaPickup>> Pickups;

    UPROPERTY()
    TObjectPtr<UXiaomaHUDWidget> GameWidget;

    UPROPERTY()
    TObjectPtr<UXiaomaSummonWheel> SummonWheel;

    UPROPERTY()
    TObjectPtr<UXiaomaSaveGame> Progress;
};
