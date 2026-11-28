#pragma once

#include "CoreMinimal.h"
#include "GameFramework/GameModeBase.h"
#include "XiaomaPuzzle.h"
#include "XiaomaGameMode.generated.h"

class AXiaomaBoardActor;
class ACameraActor;
class AXiaomaHerdsman;
class AXiaomaIceField;
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
    /** 历史为空时（撞鬼弹回、刚开局、刚清盘）撤销键点了也没用，HUD 据此置灰 */
    bool CanUndo() const { return UndoHistory.Num() > 0; }
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
    /** 本关宝箱总数与已开数量：三星的第三颗星看「有没有集齐」，HUD 也显示它 */
    int32 GetChestCount() const;
    int32 GetOpenedChestCount() const;
    const FString& GetStatusMessage() const { return StatusMessage; }
    AXiaomaBoardActor* GetBoard() const { return Board; }
    AXiaomaHerdsman* GetHerdsman() const { return Herdsman; }
    const FXiaomaLevel* GetCurrentLevel() const;

    /** 这一格当前是否被机关封住（障碍、未打开的围栏等） */
    bool IsCellBlocked(FIntPoint Cell) const;

    /** 这一格是不是冰面：踩上去主角会一直滑到滑出冰面或撞墙 */
    bool IsIceCell(FIntPoint Cell) const;

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
    /** 刚踩上冰面：滑行这一段玩家中途控不了，先存一个撤销点 */
    void OnHerdsmanSlideBegin();
    /** 刚踩上木桥：桥还完好，只给提示，不压撤销点 */
    void OnBridgeStepped(class AXiaomaBridge* Bridge);
    /** 木桥即将塌陷（桥此刻还完好）：在塌陷前压一条撤销快照，按 Z 能把桥修好 */
    void OnBridgeCollapsing(class AXiaomaBridge* Bridge);
    /** 直接把主角搬走（撤销/读档/挨撞）之后，把"人还在不在桥上"同步给各座桥 */
    void SyncBridgeOccupancy();
    /** 踩到围栏开关：此刻门还关着，先压一条"没开门"的快照，再让门打开 */
    void OnGateOpened(class AXiaomaGate* Gate);
    /** 兜底：人已经在栏内、门却关着（存档不一致）时直接开门，别把人卡死在原地 */
    void EnsureGateConsistency();
    void OnWheelSelectColour(int32 ColourIndex);
    void OnWheelRecall();
    void OnChestOpened(EXiaomaItemType Reward, FIntPoint Cell, FString& OutMessage);
    void OnChestNotice(const FString& Message, bool bPositive);
    void OnPickupCollected(EXiaomaItemType ItemType, FIntPoint Cell);
    /** 巡逻捣蛋鬼撞到主角：扣 1 命并弹回本关起点 */
    void OnPatrollerHitHerdsman(class AXiaomaPatroller* Patroller);
    /**
     * 主角撞上木箱时由主角回调进来：能推就把箱子挪一格并返回 true（这一帧放行）。
     * BoxCell 是被挡住的那一格，StepDelta 是推动方向（每轴 -1/0/1）。
     */
    bool TryPushBoxAtCell(FIntPoint BoxCell, FIntPoint StepDelta);
    /** 木箱能不能进入这一格：越界、障碍、别的箱子、宝箱/石碑、没捡的道具都算不行 */
    bool CanBoxEnter(FIntPoint Cell, const class AXiaomaMechanism* IgnoredBox) const;
    /** 同步压力板视觉，必要时解锁推箱宝箱；bAnnounce 为真时弹提示与音效 */
    void RefreshPushPlates(bool bAnnounce);
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

    /** 冰面装饰：没有状态，所以不进 Mechanisms，单独持有以便换关时清掉 */
    UPROPERTY()
    TObjectPtr<AXiaomaIceField> IceField;

    UPROPERTY()
    TObjectPtr<UXiaomaHUDWidget> GameWidget;

    UPROPERTY()
    TObjectPtr<UXiaomaSummonWheel> SummonWheel;

    UPROPERTY()
    TObjectPtr<UXiaomaSaveGame> Progress;
};
