#pragma once

#include "CoreMinimal.h"
#include "GameFramework/SaveGame.h"
#include "XiaomaSaveGame.generated.h"

UCLASS()
class XIAOMALOGIC_API UXiaomaSaveGame : public USaveGame
{
    GENERATED_BODY()

public:
    // Missing in older saves: defaults to zero, so old in-progress boards are discarded.
    UPROPERTY(SaveGame)
    int32 LevelRevision = 0;

    void MigrateLevelRevision()
    {
        if (LevelRevision == 4) return;
        bHasActiveRound = false;
        ActiveCells.Reset();
        ActiveHintsUsed = 0;
        ActiveWrongAttempts = 0;
        LevelRevision = 4;
    }

    UPROPERTY(SaveGame)
    int32 UnlockedLevel = 1;

    UPROPERTY(SaveGame)
    TArray<int32> StarsByLevel;

    UPROPERTY(SaveGame)
    int32 Lives = 2;

    UPROPERTY(SaveGame)
    int32 Affection = 0;

    UPROPERTY(SaveGame)
    int32 WinStreak = 0;

    UPROPERTY(SaveGame)
    int32 BestStreak = 0;

    UPROPERTY(SaveGame)
    bool bMusicEnabled = true;

    UPROPERTY(SaveGame)
    bool bSoundEnabled = true;

    /** 0: 长按打开轮盘松手确认, 1: 按一下切换轮盘, 2: 直接放当前格颜色 */
    UPROPERTY(SaveGame)
    uint8 SummonMode = 0;

    /** 0: 按一下 Tab 切换全局/跟随, 1: 按住 Tab 看全局 */
    UPROPERTY(SaveGame)
    uint8 TabCameraMode = 0;

    UPROPERTY(SaveGame)
    FName KeyInteract = FName("E");

    UPROPERTY(SaveGame)
    FName KeyMark = FName("F");

    UPROPERTY(SaveGame)
    FName KeyOverview = FName("Tab");

    UPROPERTY(SaveGame)
    FName KeyHint = FName("H");

    UPROPERTY(SaveGame)
    FName KeyUndo = FName("Z");

    UPROPERTY(SaveGame)
    bool bHasActiveRound = false;

    UPROPERTY(SaveGame)
    int32 ActiveLevel = 1;

    UPROPERTY(SaveGame)
    TArray<uint8> ActiveCells;

    UPROPERTY(SaveGame)
    int32 ActiveHintsUsed = 0;

    UPROPERTY(SaveGame)
    int32 ActiveWrongAttempts = 0;
};
