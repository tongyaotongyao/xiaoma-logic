#include "XiaomaPuzzle.h"

#include "Dom/JsonObject.h"
#include "Misc/FileHelper.h"
#include "Misc/Paths.h"
#include "Serialization/JsonReader.h"
#include "Serialization/JsonSerializer.h"

namespace
{
bool ParseIntPoint(const TArray<TSharedPtr<FJsonValue>>& Pair, FIntPoint& OutPoint)
{
    if (Pair.Num() != 2)
    {
        return false;
    }
    const int32 Row = static_cast<int32>(Pair[0]->AsNumber());
    const int32 Col = static_cast<int32>(Pair[1]->AsNumber());
    if (Row < 0 || Row >= FXiaomaPuzzle::Size || Col < 0 || Col >= FXiaomaPuzzle::Size)
    {
        return false;
    }
    OutPoint = FIntPoint(Row, Col);
    return true;
}

EXiaomaItemType ParseItemType(const FString& Name)
{
    if (Name.Equals(TEXT("apple"), ESearchCase::IgnoreCase)) return EXiaomaItemType::Apple;
    if (Name.Equals(TEXT("carrot"), ESearchCase::IgnoreCase)) return EXiaomaItemType::Carrot;
    if (Name.Equals(TEXT("clover"), ESearchCase::IgnoreCase)) return EXiaomaItemType::Clover;
    return EXiaomaItemType::Hint;
}
}

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
        for (int32 Colour = 0; Colour < Size; ++Colour)
        {
            const int32 RegionSize = Level.Regions.FilterByPredicate(
                [Colour](int32 Value) { return Value == Colour; }).Num();
            if (RegionSize < 3 || RegionSize > 9)
            {
                OutError = FString::Printf(TEXT("Level %d has an unbalanced region"), Level.Id);
                return false;
            }
        }
        TArray<uint8> Answer;
        Answer.Init(0, CellCount);
        for (int32 Row = 0; Row < Size; ++Row)
        {
            const int32 Col = Level.SolutionColumns[Row];
            if (Col < 0 || Col >= Size)
            {
                OutError = TEXT("Invalid solution column");
                return false;
            }
            Answer[Row * Size + Col] = 2;
        }
        FString AnswerMessage;
        if (CheckBoard(Level, Answer, AnswerMessage) != EXiaomaBoardResult::Solved)
        {
            OutError = FString::Printf(TEXT("Level %d has an incorrect stored solution"), Level.Id);
            return false;
        }

        // 解析冒险模式扩展字段（未配置时按关卡序号给默认生命与起点）
        Level.Adventure.StartLives = Level.Id <= 10 ? 3 : 2;
        Level.Adventure.StartCell = FIntPoint(5, 0);
        Level.Adventure.ParSteps = 22;
        const TSharedPtr<FJsonObject>* AdventureObj = nullptr;
        if (Object->TryGetObjectField(TEXT("adventure"), AdventureObj) && AdventureObj && (*AdventureObj).IsValid())
        {
            const TSharedPtr<FJsonObject>& Adv = *AdventureObj;
            const TArray<TSharedPtr<FJsonValue>>* StartArr = nullptr;
            if (Adv->TryGetArrayField(TEXT("start"), StartArr) && StartArr)
            {
                ParseIntPoint(*StartArr, Level.Adventure.StartCell);
            }
            int32 ParValue = 0;
            if (Adv->TryGetNumberField(TEXT("par"), ParValue) && ParValue > 0)
            {
                Level.Adventure.ParSteps = ParValue;
            }
            int32 LivesValue = 0;
            if (Adv->TryGetNumberField(TEXT("lives"), LivesValue) && LivesValue > 0)
            {
                Level.Adventure.StartLives = FMath::Clamp(LivesValue, 1, 5);
            }
            const TArray<TSharedPtr<FJsonValue>>* ItemsArr = nullptr;
            if (Adv->TryGetArrayField(TEXT("items"), ItemsArr) && ItemsArr)
            {
                for (const TSharedPtr<FJsonValue>& ItemVal : *ItemsArr)
                {
                    const TSharedPtr<FJsonObject> ItemObj = ItemVal->AsObject();
                    if (!ItemObj.IsValid()) continue;
                    const TArray<TSharedPtr<FJsonValue>>* CellArr = nullptr;
                    FIntPoint Cell;
                    if (ItemObj->TryGetArrayField(TEXT("cell"), CellArr) && CellArr && ParseIntPoint(*CellArr, Cell))
                    {
                        FXiaomaItemDef Def;
                        FString TypeStr;
                        ItemObj->TryGetStringField(TEXT("type"), TypeStr);
                        Def.Type = ParseItemType(TypeStr);
                        Def.Cell = Cell;
                        Level.Adventure.Items.Add(Def);
                    }
                }
            }
            const TArray<TSharedPtr<FJsonValue>>* ChestsArr = nullptr;
            if (Adv->TryGetArrayField(TEXT("chests"), ChestsArr) && ChestsArr)
            {
                for (const TSharedPtr<FJsonValue>& ChestVal : *ChestsArr)
                {
                    const TSharedPtr<FJsonObject> ChestObj = ChestVal->AsObject();
                    if (!ChestObj.IsValid()) continue;
                    const TArray<TSharedPtr<FJsonValue>>* CellArr = nullptr;
                    FIntPoint Cell;
                    if (ChestObj->TryGetArrayField(TEXT("cell"), CellArr) && CellArr && ParseIntPoint(*CellArr, Cell))
                    {
                        FXiaomaChestDef Def;
                        Def.Cell = Cell;
                        FString RewardStr;
                        if (ChestObj->TryGetStringField(TEXT("reward"), RewardStr))
                        {
                            Def.Reward = ParseItemType(RewardStr);
                        }
                        const TSharedPtr<FJsonObject>* PuzzleObj = nullptr;
                        if (ChestObj->TryGetObjectField(TEXT("puzzle"), PuzzleObj) && PuzzleObj && (*PuzzleObj).IsValid())
                        {
                            const TSharedPtr<FJsonObject>& Puzzle = *PuzzleObj;

                            FString PuzzleTypeStr;
                            Puzzle->TryGetStringField(TEXT("type"), PuzzleTypeStr);
                            if (PuzzleTypeStr.Equals(TEXT("runes"), ESearchCase::IgnoreCase))
                            {
                                Def.PuzzleType = EXiaomaChestPuzzleType::Runes;
                            }
                            else if (PuzzleTypeStr.Equals(TEXT("stele"), ESearchCase::IgnoreCase))
                            {
                                Def.PuzzleType = EXiaomaChestPuzzleType::Stele;
                            }
                            else if (PuzzleTypeStr.Equals(TEXT("push"), ESearchCase::IgnoreCase))
                            {
                                Def.PuzzleType = EXiaomaChestPuzzleType::Push;
                            }
                            else
                            {
                                Def.PuzzleType = EXiaomaChestPuzzleType::Rule;
                            }

                            FString RuleTarget;
                            if (Puzzle->TryGetStringField(TEXT("target"), RuleTarget))
                            {
                                if (RuleTarget.Equals(TEXT("col"), ESearchCase::IgnoreCase)
                                    || RuleTarget.Equals(TEXT("column"), ESearchCase::IgnoreCase))
                                {
                                    Def.RuleType = EXiaomaChestRuleType::Column;
                                }
                                else if (RuleTarget.Equals(TEXT("region"), ESearchCase::IgnoreCase))
                                {
                                    Def.RuleType = EXiaomaChestRuleType::Region;
                                }
                                else
                                {
                                    Def.RuleType = EXiaomaChestRuleType::Row;
                                }
                            }
                            int32 IndexVal = 0;
                            if (Puzzle->TryGetNumberField(TEXT("index"), IndexVal))
                            {
                                Def.RuleIndex = FMath::Clamp(IndexVal, 0, Size - 1);
                            }

                            // 符文顺序
                            const TArray<TSharedPtr<FJsonValue>>* OrderArr = nullptr;
                            if (Puzzle->TryGetArrayField(TEXT("order"), OrderArr) && OrderArr)
                            {
                                for (const TSharedPtr<FJsonValue>& StepVal : *OrderArr)
                                {
                                    const TArray<TSharedPtr<FJsonValue>>* StepArr = nullptr;
                                    FIntPoint Step;
                                    if (StepVal->TryGetArray(StepArr) && StepArr && ParseIntPoint(*StepArr, Step))
                                    {
                                        Def.RuneOrder.Add(Step);
                                    }
                                }
                            }

                            // 石碑线索与埋藏格
                            const TSharedPtr<FJsonObject>* ClueObj = nullptr;
                            if (Puzzle->TryGetObjectField(TEXT("clue"), ClueObj) && ClueObj && (*ClueObj).IsValid())
                            {
                                (*ClueObj)->TryGetStringField(TEXT("text"), Def.ClueText);
                                const TArray<TSharedPtr<FJsonValue>>* ClueCellArr = nullptr;
                                FIntPoint ClueCell;
                                if ((*ClueObj)->TryGetArrayField(TEXT("cell"), ClueCellArr) && ClueCellArr
                                    && ParseIntPoint(*ClueCellArr, ClueCell))
                                {
                                    Def.SteleCell = ClueCell;
                                }
                            }
                            const TArray<TSharedPtr<FJsonValue>>* AnswerArr = nullptr;
                            FIntPoint AnswerCell;
                            if (Puzzle->TryGetArrayField(TEXT("answer"), AnswerArr) && AnswerArr
                                && ParseIntPoint(*AnswerArr, AnswerCell))
                            {
                                Def.SteleAnswer = AnswerCell;
                            }
                        }
                        Level.Adventure.Chests.Add(Def);
                    }
                }
            }

            // 静态障碍
            const TArray<TSharedPtr<FJsonValue>>* ObstaclesArr = nullptr;
            if (Adv->TryGetArrayField(TEXT("obstacles"), ObstaclesArr) && ObstaclesArr)
            {
                for (const TSharedPtr<FJsonValue>& ObstacleVal : *ObstaclesArr)
                {
                    const TArray<TSharedPtr<FJsonValue>>* ObstacleCellArr = nullptr;
                    FIntPoint ObstacleCell;
                    if (ObstacleVal->TryGetArray(ObstacleCellArr) && ObstacleCellArr
                        && ParseIntPoint(*ObstacleCellArr, ObstacleCell))
                    {
                        Level.Adventure.Obstacles.Add(ObstacleCell);
                    }
                }
            }
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

int32 FXiaomaPuzzle::AdventureFingerprint(const FXiaomaLevel& Level)
{
    // FNV-1a，只求稳定、不要求分布性质
    uint32 Hash = 2166136261u;
    auto Mix = [&Hash](int32 Value)
    {
        Hash ^= static_cast<uint32>(Value);
        Hash *= 16777619u;
    };

    Mix(Level.Id);

    Mix(Level.Adventure.Obstacles.Num());
    for (const FIntPoint& Cell : Level.Adventure.Obstacles)
    {
        Mix(Cell.X);
        Mix(Cell.Y);
    }

    Mix(Level.Adventure.Chests.Num());
    for (const FXiaomaChestDef& Chest : Level.Adventure.Chests)
    {
        Mix(Chest.Cell.X);
        Mix(Chest.Cell.Y);
        Mix(static_cast<int32>(Chest.Reward));
        Mix(static_cast<int32>(Chest.PuzzleType));
        Mix(Chest.RuleIndex);
        Mix(Chest.RuneOrder.Num());
        for (const FIntPoint& Rune : Chest.RuneOrder)
        {
            Mix(Rune.X);
            Mix(Rune.Y);
        }
        Mix(Chest.SteleCell.X);
        Mix(Chest.SteleCell.Y);
        Mix(Chest.SteleAnswer.X);
        Mix(Chest.SteleAnswer.Y);
    }

    Mix(Level.Adventure.Items.Num());
    for (const FXiaomaItemDef& Item : Level.Adventure.Items)
    {
        Mix(static_cast<int32>(Item.Type));
        Mix(Item.Cell.X);
        Mix(Item.Cell.Y);
    }

    return static_cast<int32>(Hash & 0x7fffffffu);
}
