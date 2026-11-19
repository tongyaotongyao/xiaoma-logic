#include "XiaomaPuzzle.h"

#include "Dom/JsonObject.h"
#include "Misc/FileHelper.h"
#include "Misc/Paths.h"
#include "Serialization/JsonReader.h"
#include "Serialization/JsonSerializer.h"

bool FXiaomaLevel::IsGiven(int32 Row, int32 Column) const
{
    return Givens.Contains(FIntPoint(Row, Column));
}

bool FXiaomaPuzzle::LoadLevels(TArray<FXiaomaLevel>& OutLevels, FString& OutError)
{
    OutLevels.Reset();
    const FString Path = FPaths::ProjectContentDir() / TEXT("Levels/levels.json");
    FString Text;
    if (!FFileHelper::LoadFileToString(Text, *Path))
    {
        OutError = FString::Printf(TEXT("Cannot load levels: %s"), *Path);
        return false;
    }
    TSharedPtr<FJsonObject> Root;
    const TSharedRef<TJsonReader<>> Reader = TJsonReaderFactory<>::Create(Text);
    if (!FJsonSerializer::Deserialize(Reader, Root) || !Root.IsValid()
        || Root->GetIntegerField(TEXT("size")) != Size)
    {
        OutError = TEXT("Invalid level JSON header");
        return false;
    }

    const TArray<TSharedPtr<FJsonValue>>* Entries = nullptr;
    if (!Root->TryGetArrayField(TEXT("levels"), Entries) || Entries == nullptr || Entries->Num() != 25)
    {
        OutError = TEXT("Expected exactly 25 levels");
        return false;
    }
    for (const TSharedPtr<FJsonValue>& Entry : *Entries)
    {
        const TSharedPtr<FJsonObject> Object = Entry->AsObject();
        if (!Object.IsValid())
        {
            OutError = TEXT("Invalid level object");
            return false;
        }
        FXiaomaLevel Level;
        Level.Id = Object->GetIntegerField(TEXT("id"));
        for (const TSharedPtr<FJsonValue>& RowValue : Object->GetArrayField(TEXT("regions")))
        {
            const TArray<TSharedPtr<FJsonValue>>& Row = RowValue->AsArray();
            if (Row.Num() != Size)
            {
                OutError = TEXT("Invalid region row size");
                return false;
            }
            for (const TSharedPtr<FJsonValue>& Cell : Row)
            {
                const int32 Colour = static_cast<int32>(Cell->AsNumber());
                if (Colour < 0 || Colour >= Size)
                {
                    OutError = TEXT("Invalid region colour");
                    return false;
                }
                Level.Regions.Add(Colour);
            }
        }
        for (const TSharedPtr<FJsonValue>& GivenValue : Object->GetArrayField(TEXT("givens")))
        {
            const TArray<TSharedPtr<FJsonValue>>& Pair = GivenValue->AsArray();
            if (Pair.Num() != 2)
            {
                OutError = TEXT("Invalid given cell");
                return false;
            }
            Level.Givens.Add(FIntPoint(static_cast<int32>(Pair[0]->AsNumber()),
                static_cast<int32>(Pair[1]->AsNumber())));
        }
        for (const TSharedPtr<FJsonValue>& Column : Object->GetArrayField(TEXT("solution")))
        {
            Level.SolutionColumns.Add(static_cast<int32>(Column->AsNumber()));
        }
        if (!Level.Givens.IsEmpty() || Level.Regions.Num() != CellCount || Level.SolutionColumns.Num() != Size
            || CountSolutions(Level) != 1)
        {
            OutError = FString::Printf(TEXT("Level %d is malformed or not unique"), Level.Id);
            return false;
        }
        OutLevels.Add(MoveTemp(Level));
    }
    return true;
}

int32 FXiaomaPuzzle::CountSolutions(const FXiaomaLevel& Level, int32 Limit)
{
    if (Level.Regions.Num() != CellCount)
    {
        return 0;
    }
    int32 Found = 0;
    SearchSolutions(Level, 0, 0, 0, -10, FMath::Max(1, Limit), Found);
    return Found;
}

void FXiaomaPuzzle::SearchSolutions(const FXiaomaLevel& Level, int32 Row, int32 UsedColumns,
    int32 UsedRegions, int32 PreviousColumn, int32 Limit, int32& Found)
{
    if (Found >= Limit)
    {
        return;
    }
    if (Row == Size)
    {
        ++Found;
        return;
    }
    for (int32 Column = 0; Column < Size; ++Column)
    {
        if ((UsedColumns & (1 << Column)) != 0 || FMath::Abs(Column - PreviousColumn) <= 1)
        {
            continue;
        }
        bool bMatchesGiven = true;
        for (const FIntPoint& Given : Level.Givens)
        {
            if (Given.X == Row && Given.Y != Column)
            {
                bMatchesGiven = false;
                break;
            }
        }
        if (!bMatchesGiven)
        {
            continue;
        }
        const int32 Region = Level.RegionAt(Row, Column);
        if (Region < 0 || Region >= Size || (UsedRegions & (1 << Region)) != 0)
        {
            continue;
        }
        SearchSolutions(Level, Row + 1, UsedColumns | (1 << Column),
            UsedRegions | (1 << Region), Column, Limit, Found);
    }
}

EXiaomaBoardResult FXiaomaPuzzle::CheckBoard(const FXiaomaLevel& Level,
    const TArray<uint8>& Cells, FString& OutMessage)
{
    if (Cells.Num() != CellCount)
    {
        OutMessage = TEXT("棋盘数据无效");
        return EXiaomaBoardResult::Invalid;
    }
    int32 Ponies = 0;
    for (const uint8 Cell : Cells)
    {
        Ponies += Cell == 2 ? 1 : 0;
    }
    if (Ponies < Size)
    {
        OutMessage = FString::Printf(TEXT("还需放置 %d 匹小马"), Size - Ponies);
        return EXiaomaBoardResult::Incomplete;
    }
    if (Ponies > Size)
    {
        OutMessage = TEXT("棋盘上只能放置六匹小马");
        return EXiaomaBoardResult::Invalid;
    }

    int32 Rows[Size] = {};
    int32 Columns[Size] = {};
    int32 Regions[Size] = {};
    for (int32 Row = 0; Row < Size; ++Row)
    {
        for (int32 Column = 0; Column < Size; ++Column)
        {
            if (Cells[Row * Size + Column] != 2)
            {
                continue;
            }
            ++Rows[Row];
            ++Columns[Column];
            ++Regions[Level.RegionAt(Row, Column)];
            if (Rows[Row] > 1 || Columns[Column] > 1 || Regions[Level.RegionAt(Row, Column)] > 1)
            {
                OutMessage = TEXT("同一行、列或颜色区域放了多匹小马");
                return EXiaomaBoardResult::Invalid;
            }
            for (int32 PrevRow = FMath::Max(0, Row - 1); PrevRow <= Row; ++PrevRow)
            {
                for (int32 PrevColumn = FMath::Max(0, Column - 1); PrevColumn <= Column + 1 && PrevColumn < Size; ++PrevColumn)
                {
                    if ((PrevRow != Row || PrevColumn < Column)
                        && Cells[PrevRow * Size + PrevColumn] == 2)
                    {
                        OutMessage = TEXT("有两匹小马相邻");
                        return EXiaomaBoardResult::Invalid;
                    }
                }
            }
        }
    }
    for (const FIntPoint& Given : Level.Givens)
    {
        if (!Cells.IsValidIndex(Given.X * Size + Given.Y)
            || Cells[Given.X * Size + Given.Y] != 2)
        {
            OutMessage = TEXT("预设的小马不能移除");
            return EXiaomaBoardResult::Invalid;
        }
    }
    OutMessage = TEXT("全部规则满足！");
    return EXiaomaBoardResult::Solved;
}

FIntPoint FXiaomaPuzzle::FindHint(const FXiaomaLevel& Level, const TArray<uint8>& Cells)
{
    if (Level.SolutionColumns.Num() != Size || Cells.Num() != CellCount)
    {
        return FIntPoint(-1, -1);
    }
    for (int32 Row = 0; Row < Size; ++Row)
    {
        const int32 Column = Level.SolutionColumns[Row];
        if (Column >= 0 && Column < Size && Cells[Row * Size + Column] != 2)
        {
            return FIntPoint(Row, Column);
        }
    }
    return FIntPoint(-1, -1);
}
