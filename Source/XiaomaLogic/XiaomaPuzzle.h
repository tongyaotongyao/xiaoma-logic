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

/** 关卡中场上道具配置 */
struct FXiaomaItemDef
{
    EXiaomaItemType Type = EXiaomaItemType::Apple;
    FIntPoint Cell = FIntPoint(0, 0);
};

/** 关卡中宝箱配置（M2 先落地规则开箱） */
struct FXiaomaChestDef
{
    FIntPoint Cell = FIntPoint(0, 0);
    EXiaomaItemType Reward = EXiaomaItemType::Hint;
    EXiaomaChestRuleType RuleType = EXiaomaChestRuleType::Row;
    int32 RuleIndex = 0;
};

/** 关卡中的冒险模式扩展数据 */
struct FXiaomaAdventureData
{
    FIntPoint StartCell = FIntPoint(5, 0);
    int32 ParSteps = 22;
    int32 StartLives = 3;
    TArray<FXiaomaItemDef> Items;
    TArray<FXiaomaChestDef> Chests;
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

private:
    static void SearchSolutions(const FXiaomaLevel& Level, int32 Row, int32 UsedColumns,
        int32 UsedRegions, int32 PreviousColumn, int32 Limit, int32& Found);
};
