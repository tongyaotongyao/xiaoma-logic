#pragma once

#include "CoreMinimal.h"
#include "GameFramework/GameModeBase.h"
#include "XiaomaPuzzle.h"
#include "XiaomaGameMode.generated.h"

class AXiaomaBoardActor;
class ACameraActor;
class UXiaomaHUDWidget;
class UXiaomaSaveGame;

enum class EXiaomaScreen : uint8
{
    Menu,
    LevelSelect,
    Playing,
    Paused,
    Won,
    Lost
};

UCLASS()
class XIAOMALOGIC_API AXiaomaGameMode : public AGameModeBase
{
    GENERATED_BODY()

public:
    AXiaomaGameMode();
    virtual void BeginPlay() override;

    void ShowMenu();
    void ShowLevelSelect();
    void StartLevel(int32 LevelNumber);
    void PauseGame();
    void ResumeGame();
    void NextLevel();
    void RestartLevel();
    void ToggleMarkMode();
    void ActivateCell(int32 Index, bool bForceMark = false);
    void Undo();
    void UseHint();
    void SubmitAnswer();

    EXiaomaScreen GetScreen() const { return Screen; }
    int32 GetLevelNumber() const { return CurrentLevelIndex + 1; }
    int32 GetUnlockedLevel() const;
    int32 GetContinueLevel() const;
    int32 GetLives() const;
    int32 GetAffection() const;
    int32 GetWinStreak() const;
    int32 GetBestStreak() const;
    int32 GetLevelStars(int32 LevelNumber) const;
    int32 GetCurrentStars() const;
    int32 GetHintCount() const { return HintsUsed; }
    bool IsMarkMode() const { return bMarkMode; }
    const FString& GetStatusMessage() const { return StatusMessage; }
    AXiaomaBoardActor* GetBoard() const { return Board; }
    const FXiaomaLevel* GetCurrentLevel() const;

private:
    void SetScreen(EXiaomaScreen NewScreen);
    void RefreshBoardAndUI();
    void SaveProgress();
    void ResetBoard();
    bool IsCorrectCell(int32 Index) const;
    void LoseLife();

    TArray<FXiaomaLevel> Levels;
    TArray<uint8> Cells; // 0 empty, 1 marked, 2 pony
    TArray<TArray<uint8>> UndoHistory;
    int32 CurrentLevelIndex = -1;
    int32 HintsUsed = 0;
    int32 WrongAttempts = 0;
    bool bMarkMode = false;
    EXiaomaScreen Screen = EXiaomaScreen::Menu;
    FString StatusMessage;

    UPROPERTY()
    TObjectPtr<AXiaomaBoardActor> Board;

    UPROPERTY()
    TObjectPtr<ACameraActor> BoardCamera;

    UPROPERTY()
    TObjectPtr<UXiaomaHUDWidget> GameWidget;

    UPROPERTY()
    TObjectPtr<UXiaomaSaveGame> Progress;
};
