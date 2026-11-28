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

/**
 * 宝箱谜题的一段。组合宝箱（JSON 里的 `puzzle.then`）就是多段按顺序完成：
 * 前一段做完，下一段才开始响应，最后一段完成宝箱才真正打开。
 */
struct FXiaomaChestStage
{
    EXiaomaChestPuzzleType PuzzleType = EXiaomaChestPuzzleType::Rule;

    /** 规则开箱：绑定哪一行/列/色区 */
    EXiaomaChestRuleType RuleType = EXiaomaChestRuleType::Row;
    int32 RuleIndex = 0;

    /** 符文宝箱：按此顺序踩亮这些格才会完成这一段 */
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

/** 关卡中宝箱配置：一个格子 + 至少一段谜题 */
struct FXiaomaChestDef
{
    FIntPoint Cell = FIntPoint(0, 0);
    EXiaomaItemType Reward = EXiaomaItemType::Hint;

    /** 按顺序完成，Stages.Num() >= 1；没有 `then` 的宝箱只有一段 */
    TArray<FXiaomaChestStage> Stages;

    int32 NumStages() const { return Stages.Num(); }
    const FXiaomaChestStage& StageAt(int32 Index) const { return Stages[Index]; }
};

/** 推箱木箱：初始占据一格，主角撞上去整格推动 */
struct FXiaomaBoxDef
{
    FIntPoint Cell = FIntPoint(0, 0);
};

/**
 * 色区围栏：把某个色区四周封住，主角进不去区域内的格子。
 * 踩下区域外的开关后永久打开，不能手动关上（撤销可以把「没开门」的状态退回来）。
 *
 * 它是唯一允许盖住正解格的阻碍——关卡设计的意图就是「什么时候进这个色区」，
 * 开关摆得远一点，绕路的代价就写进 par 里。
 */
struct FXiaomaGateDef
{
    /** 被围住的色区编号（0~5） */
    int32 Region = 0;
    /** 开关格：必须在区域外，初始关闭时主角才踩得到 */
    FIntPoint SwitchCell = FIntPoint(-1, -1);
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
    /**
     * 冰面：可以正常走进去，但踩上去就再也站不住——会沿进入方向一直滑，
     * 直到滑出冰面（落在后面第一块普通地板）或撞上墙/障碍（停在最后一块冰上）。
     * 冰面上不放道具、宝箱、木箱，校验工具会检查。
     */
    TArray<FIntPoint> Ice;
    /**
     * 塌陷木桥：初始完好可通行，主角走过一次就塌成坑、不能再进入。
     * 每座桥是一个机关（`AXiaomaBridge`），状态要进撤销快照与存档，
     * 所以解析出来的只是初始格，运行时会额外生成 actor。
     */
    TArray<FIntPoint> Bridges;
    /**
     * 色区围栏：把整个色区四周封住，踩下区域外的开关后永久打开。
     * 每道围栏是一个机关（`AXiaomaGate`），开/关状态要进撤销快照与存档。
     */
    TArray<FXiaomaGateDef> Gates;
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
