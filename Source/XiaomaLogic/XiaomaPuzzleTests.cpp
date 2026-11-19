#if WITH_DEV_AUTOMATION_TESTS

#include "XiaomaPuzzle.h"
#include "XiaomaSaveGame.h"
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

        TestTrue(Prefix + TEXT(" no initial ponies"), Level.Givens.IsEmpty());
        TArray<uint8> Cells;
        Cells.Init(0, FXiaomaPuzzle::CellCount);
        for (const FIntPoint& Given : Level.Givens)
        {
            Cells[Given.X * FXiaomaPuzzle::Size + Given.Y] = 2;
        }
        FString Message;
        TestTrue(Prefix + TEXT(" unfinished board"),
            FXiaomaPuzzle::CheckBoard(Level, Cells, Message) == EXiaomaBoardResult::Incomplete);

        for (int32 Step = 0; Step < FXiaomaPuzzle::Size; ++Step)
        {
            const FIntPoint Hint = FXiaomaPuzzle::FindHint(Level, Cells);
            TestTrue(Prefix + TEXT(" hint in bounds"), Hint.X >= 0 && Hint.X < 6 && Hint.Y >= 0 && Hint.Y < 6);
            if (Hint.X < 0 || Hint.X >= 6 || Hint.Y < 0 || Hint.Y >= 6) return false;
            TestEqual(Prefix + TEXT(" hint matches solution"), Hint.Y, Level.SolutionColumns[Hint.X]);
            TestEqual(Prefix + TEXT(" hint is new"), Cells[Hint.X * 6 + Hint.Y], uint8(0));
            Cells[Hint.X * 6 + Hint.Y] = 2;
        }
        TestTrue(Prefix + TEXT(" solved board"),
            FXiaomaPuzzle::CheckBoard(Level, Cells, Message) == EXiaomaBoardResult::Solved);

        const int32 ExtraColumn = (Level.SolutionColumns[0] + 1) % FXiaomaPuzzle::Size;
        Cells[ExtraColumn] = 2;
        TestTrue(Prefix + TEXT(" seventh pony invalid"),
            FXiaomaPuzzle::CheckBoard(Level, Cells, Message) == EXiaomaBoardResult::Invalid);
    }
    UXiaomaSaveGame* Save = NewObject<UXiaomaSaveGame>();
    Save->LevelRevision = 3; // Previous balanced release.
    Save->UnlockedLevel = 4;
    Save->Affection = 15;
    Save->StarsByLevel = {3, 2, 3};
    Save->bHasActiveRound = true;
    Save->ActiveCells.Init(2, 36);
    Save->MigrateLevelRevision();
    TestFalse(TEXT("Old board not resumed"), Save->bHasActiveRound);
    TestTrue(TEXT("Old cells discarded"), Save->ActiveCells.IsEmpty());
    TestEqual(TEXT("Unlocked levels preserved"), Save->UnlockedLevel, 4);
    TestEqual(TEXT("Affection preserved"), Save->Affection, 15);
    TestEqual(TEXT("Stars preserved"), Save->StarsByLevel[1], 2);
    Save->bHasActiveRound = true;
    Save->ActiveCells.Init(0, 36);
    Save->ActiveCells[3] = 2;
    Save->MigrateLevelRevision();
    TestTrue(TEXT("New round still resumable"), Save->bHasActiveRound);
    TestEqual(TEXT("New player move preserved"), Save->ActiveCells[3], uint8(2));
    return true;
}

#endif
