#pragma once

#include "CoreMinimal.h"

/** 冒险模式可拾取与宝箱奖励道具类型 */
enum class EXiaomaItemType : uint8
{
    Hint,
    Apple,
    Carrot,
    Clover
};

/** 规则开箱的触发条件类型 */
enum class EXiaomaChestRuleType : uint8
{
    Row,
    Column,
    Region
};

/** 开箱谜题类型：规则开箱已落地，符文与石碑随 M3 加入，推箱留到 M4 */
enum class EXiaomaChestPuzzleType : uint8
{
    Rule,
    Runes,
    Stele,
    Push
};

/** 关卡中场上道具配置 */
struct FXiaomaItemDef
{
    EXiaomaItemType Type = EXiaomaItemType::Apple;
    FIntPoint Cell = FIntPoint(0, 0);
};

/** 关卡中宝箱配置 */
struct FXiaomaChestDef
{
    FIntPoint Cell = FIntPoint(0, 0);
    EXiaomaItemType Reward = EXiaomaItemType::Hint;
    EXiaomaChestPuzzleType PuzzleType = EXiaomaChestPuzzleType::Rule;

    /** 规则开箱：绑定哪一行/列/色区 */
    EXiaomaChestRuleType RuleType = EXiaomaChestRuleType::Row;
    int32 RuleIndex = 0;

    /** 符文宝箱：按此顺序踩亮这些格才会开箱 */
    TArray<FIntPoint> RuneOrder;

    /** 石碑宝箱：石碑立在 SteleCell，读到的提示是 ClueText，宝箱埋在 SteleAnswer */
    FIntPoint SteleCell = FIntPoint(-1, -1);
    FString ClueText;
    FIntPoint SteleAnswer = FIntPoint(-1, -1);

    /**
     * 推箱宝箱：压力板所在格。压力板只记在这里，木箱只记自己的初始格——
     * 同一个坐标写两处迟早会对不上，校验工具负责检查「确实有箱子能推到这块板」。
     */
    FIntPoint PushPlate = FIntPoint(-1, -1);
};

/** 推箱木箱：初始占据一格，主角撞上去整格推动 */
struct FXiaomaBoxDef
{
    FIntPoint Cell = FIntPoint(0, 0);
};

/** 巡逻捣蛋鬼：沿 Path 的格子中心匀速往返，Speed 单位为格/秒 */
struct FXiaomaPatrolDef
{
    TArray<FIntPoint> Path;
    float Speed = 1.5f;
};

/** 关卡中的冒险模式扩展数据 */
struct FXiaomaAdventureData
{
    FIntPoint StartCell = FIntPoint(5, 0);
    int32 ParSteps = 22;
    int32 StartLives = 3;
    TArray<FXiaomaItemDef> Items;
    TArray<FXiaomaChestDef> Chests;
    /** 静态障碍：占据整格，主角不能进入，也不能召唤小马 */
    TArray<FIntPoint> Obstacles;
    /** 巡逻捣蛋鬼：撞到主角扣 1 命，遇到 X 标记掉头 */
    TArray<FXiaomaPatrolDef> Patrols;
    /** 推箱木箱：占据整格，主角撞上去会推着走 */
    TArray<FXiaomaBoxDef> Boxes;
};

struct FXiaomaLevel
{
    int32 Id = 0;
    TArray<int32> Regions; // 36 row-major values in [0, 5]
    TArray<FIntPoint> Givens; // X=row, Y=column
    TArray<int32> SolutionColumns; // one column per row
    FXiaomaAdventureData Adventure;

    int32 RegionAt(int32 Row, int32 Column) const { return Regions[Row * 6 + Column]; }
    bool IsGiven(int32 Row, int32 Column) const;
};

enum class EXiaomaBoardResult : uint8
{
    Incomplete,
    Invalid,
    Solved
};

class XIAOMALOGIC_API FXiaomaPuzzle
{
public:
    static constexpr int32 Size = 6;
    static constexpr int32 CellCount = Size * Size;

    static bool LoadLevels(TArray<FXiaomaLevel>& OutLevels, FString& OutError);
    static int32 CountSolutions(const FXiaomaLevel& Level, int32 Limit = 2);
    static EXiaomaBoardResult CheckBoard(const FXiaomaLevel& Level, const TArray<uint8>& Cells, FString& OutMessage);
    static FIntPoint FindHint(const FXiaomaLevel& Level, const TArray<uint8>& Cells);

    /**
     * 冒险布局指纹：只覆盖会影响「续局状态配位」的字段（机关、宝箱谜题、道具位置）。
     * 关卡数据一改，旧存档里的机关状态就可能错位，靠它判定是否还能续局。
     */
    static int32 AdventureFingerprint(const FXiaomaLevel& Level);

private:
    static void SearchSolutions(const FXiaomaLevel& Level, int32 Row, int32 UsedColumns,
        int32 UsedRegions, int32 PreviousColumn, int32 Limit, int32& Found);
};
