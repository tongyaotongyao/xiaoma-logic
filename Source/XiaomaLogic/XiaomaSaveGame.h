#pragma once

#include "CoreMinimal.h"
#include "GameFramework/SaveGame.h"
#include "XiaomaPuzzle.h"
#include "XiaomaSaveGame.generated.h"

/**
 * 进行中存档的格式版本。
 * 只要改动进行中关卡的字段含义或数量，就必须 +1：旧版本里的进行中棋盘会被丢弃，
 * 解锁进度、星级、好感度、连胜与设置全部保留。
 * 4 -> 5：补齐主角位置、步数、提示星、已拾取道具、四叶草与各机关状态（见 ADVENTURE_DESIGN 6.3）。
 */
namespace XiaomaSave
{
    constexpr int32 LevelRevision = 5;
}

/**
 * 机关状态的扁平编解码。
 * UPROPERTY 不支持嵌套容器，所以存档里按「1 字节长度 + 内容」依次拼接每个机关的状态。
 * 长度上限 255 足够：宝箱目前 5 字节，障碍 0 字节。
 */
struct XIAOMALOGIC_API FXiaomaMechanismStateCodec
{
    static constexpr int32 MaxStateLength = 255;

    static void Encode(const TArray<TArray<uint8>>& States, TArray<uint8>& Out)
    {
        Out.Reset();
        for (const TArray<uint8>& State : States)
        {
            const int32 Length = FMath::Min(State.Num(), MaxStateLength);
            Out.Add(static_cast<uint8>(Length));
            Out.Append(State.GetData(), Length);
        }
    }

    /** 数据被截断或长度非法时返回 false 并清空 Out，调用方应当整局丢弃续局而不是局部恢复 */
    static bool Decode(const TArray<uint8>& In, TArray<TArray<uint8>>& Out)
    {
        Out.Reset();
        int32 Offset = 0;
        while (Offset < In.Num())
        {
            const int32 Length = In[Offset++];
            if (Offset + Length > In.Num())
            {
                Out.Reset();
                return false;
            }
            TArray<uint8> State;
            State.Append(In.GetData() + Offset, Length);
            Out.Add(MoveTemp(State));
            Offset += Length;
        }
        return true;
    }
};

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
        if (LevelRevision == XiaomaSave::LevelRevision)
        {
            return;
        }
        ClearActiveRound();
        LevelRevision = XiaomaSave::LevelRevision;
    }

    /** 丢弃进行中的对局，但保留解锁、星级、好感度、连胜与所有设置 */
    void ClearActiveRound()
    {
        bHasActiveRound = false;
        ActiveLevel = 1;
        ActiveCells.Reset();
        ActiveStepCount = 0;
        ActiveHintsUsed = 0;
        ActiveWrongAttempts = 0;
        ActiveHintTokens = 0;
        ActivePettedMask = 0;
        ActiveHasClover = false;
        ActiveHerdsmanCell = FIntPoint(-1, -1);
        ActivePickupCollected.Reset();
        ActiveMechanismStates.Reset();
        ActiveLevelFingerprint = 0;
    }

    /**
     * 这份存档的续局还能不能用：关卡、棋盘格数、生命、布局指纹都要对得上。
     * 任一条不满足就当新开局处理，绝不让按索引恢复的机关状态错位。
     */
    bool CanResume(const FXiaomaLevel& Level) const
    {
        if (!bHasActiveRound || Lives <= 0)
        {
            return false;
        }
        if (ActiveLevel != Level.Id || ActiveCells.Num() != FXiaomaPuzzle::CellCount)
        {
            return false;
        }
        return ActiveLevelFingerprint == FXiaomaPuzzle::AdventureFingerprint(Level);
    }

    /**
     * 菜单「继续游戏」应当进入哪一关。
     * 有续局就回续局；没有就回最后玩过的那一关（通关后是下一关）；两者都没有才用解锁进度。
     * LevelCount 用于夹住关卡数，避免存档里的旧值越过当前关卡表。
     */
    int32 GetContinueLevel(int32 LevelCount = 25) const
    {
        const int32 MaxLevel = FMath::Max(1, LevelCount);
        const int32 Unlocked = FMath::Clamp(UnlockedLevel, 1, MaxLevel);
        if (bHasActiveRound)
        {
            return FMath::Clamp(ActiveLevel, 1, Unlocked);
        }
        if (LastPlayedLevel <= 0)
        {
            return Unlocked;
        }
        return FMath::Clamp(LastPlayedLevel, 1, Unlocked);
    }

    UPROPERTY(SaveGame)
    int32 UnlockedLevel = 1;

    /**
     * 最后一次实际进入过的关卡（通关时记成下一关）。
     * 0 表示老存档没有这条记录，此时「继续游戏」退回解锁进度。
     * 不能再用「解锁进度」代替它：重玩旧关或开发用全解锁时，解锁进度不等于玩家所在位置。
     */
    UPROPERTY(SaveGame)
    int32 LastPlayedLevel = 0;

    UPROPERTY(SaveGame)
    TArray<int32> StarsByLevel;

    /** 当前关卡的生命；进入新关卡时按 levels.json 的 lives 重置 */
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

    /** 已走格数，用于三星判定；必须跨会话保留，否则退出重进等于白送步数星 */
    UPROPERTY(SaveGame)
    int32 ActiveStepCount = 0;

    UPROPERTY(SaveGame)
    int32 ActiveHintsUsed = 0;

    UPROPERTY(SaveGame)
    int32 ActiveWrongAttempts = 0;

    /** 手里还没用的提示星 */
    UPROPERTY(SaveGame)
    int32 ActiveHintTokens = 0;

    /** 已抚摸过的小马位掩码，纯表现用 */
    UPROPERTY(SaveGame)
    int32 ActivePettedMask = 0;

    /** 四叶草是否还挂着（抵挡下一次扣命） */
    UPROPERTY(SaveGame)
    bool ActiveHasClover = false;

    /** 主角所在格；(-1,-1) 表示没有记录，恢复时回到关卡起点 */
    UPROPERTY(SaveGame)
    FIntPoint ActiveHerdsmanCell = FIntPoint(-1, -1);

    /** 每关道具的拾取位图，顺序与 levels.json 的 items 一致 */
    UPROPERTY(SaveGame)
    TArray<bool> ActivePickupCollected;

    /** 各机关状态，顺序与生成顺序一致（障碍 -> 宝箱 -> 道具），用 FXiaomaMechanismStateCodec 编解码 */
    UPROPERTY(SaveGame)
    TArray<uint8> ActiveMechanismStates;

    /** 关卡的冒险布局指纹；与当前关卡数据不一致时丢弃续局，防止机关状态错位 */
    UPROPERTY(SaveGame)
    int32 ActiveLevelFingerprint = 0;
};
