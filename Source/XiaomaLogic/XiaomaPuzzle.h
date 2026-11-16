#pragma once

#include "CoreMinimal.h"

struct FXiaomaLevel
{
    int32 Id = 0;
    TArray<int32> Regions; // 36 row-major values in [0, 5]
    TArray<FIntPoint> Givens; // X=row, Y=column
    TArray<int32> SolutionColumns; // one column per row

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
