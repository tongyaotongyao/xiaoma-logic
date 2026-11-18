#if WITH_DEV_AUTOMATION_TESTS

#include "XiaomaPuzzle.h"
#include "Misc/AutomationTest.h"

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FXiaomaPuzzleTest,
    "XiaomaLogic.Puzzle.AllLevels",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FXiaomaPuzzleTest::RunTest(const FString& Parameters)
{
    TArray<FXiaomaLevel> Levels;
    FString Error;
    if (!FXiaomaPuzzle::LoadLevels(Levels, Error))
    {
        AddError(Error);
        return false;
    }
    TestEqual(TEXT("Level count"), Levels.Num(), 25);
    for (const FXiaomaLevel& Level : Levels)
    {
        const FString Prefix = FString::Printf(TEXT("Level %d"), Level.Id);
        TestEqual(Prefix + TEXT(" unique solution"), FXiaomaPuzzle::CountSolutions(Level), 1);

        TArray<uint8> Cells;
        Cells.Init(0, FXiaomaPuzzle::CellCount);
        for (const FIntPoint& Given : Level.Givens)
        {
            Cells[Given.X * FXiaomaPuzzle::Size + Given.Y] = 2;
        }
        FString Message;
        TestTrue(Prefix + TEXT(" unfinished board"),
            FXiaomaPuzzle::CheckBoard(Level, Cells, Message) == EXiaomaBoardResult::Incomplete);

        for (int32 Row = 0; Row < FXiaomaPuzzle::Size; ++Row)
        {
            Cells[Row * FXiaomaPuzzle::Size + Level.SolutionColumns[Row]] = 2;
        }
        TestTrue(Prefix + TEXT(" solved board"),
            FXiaomaPuzzle::CheckBoard(Level, Cells, Message) == EXiaomaBoardResult::Solved);

        const int32 ExtraColumn = (Level.SolutionColumns[0] + 1) % FXiaomaPuzzle::Size;
        Cells[ExtraColumn] = 2;
        TestTrue(Prefix + TEXT(" seventh pony invalid"),
            FXiaomaPuzzle::CheckBoard(Level, Cells, Message) == EXiaomaBoardResult::Invalid);
    }
    return true;
}

#endif
