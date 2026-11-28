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
    // 存档迁移：4 版及更早的进行中棋盘全部失效，长期进度保留
    UXiaomaSaveGame* Save = NewObject<UXiaomaSaveGame>();
    Save->LevelRevision = 4;
    Save->UnlockedLevel = 4;
    Save->Affection = 15;
    Save->StarsByLevel = {3, 2, 3};
    Save->bHasActiveRound = true;
    Save->ActiveLevel = 3;
    Save->ActiveCells.Init(2, 36);
    Save->ActiveStepCount = 12;
    Save->ActiveHerdsmanCell = FIntPoint(2, 3);
    Save->ActiveMechanismStates = {5, 1, 0, 0, 0, 0};
    Save->MigrateLevelRevision();
    TestEqual(TEXT("Revision upgraded"), Save->LevelRevision, XiaomaSave::LevelRevision);
    TestFalse(TEXT("Old board not resumed"), Save->bHasActiveRound);
    TestTrue(TEXT("Old cells discarded"), Save->ActiveCells.IsEmpty());
    TestTrue(TEXT("Old mechanism states discarded"), Save->ActiveMechanismStates.IsEmpty());
    TestTrue(TEXT("Old herdsman cell discarded"), Save->ActiveHerdsmanCell.X < 0);
    TestEqual(TEXT("Old step count discarded"), Save->ActiveStepCount, 0);
    TestEqual(TEXT("Unlocked levels preserved"), Save->UnlockedLevel, 4);
    TestEqual(TEXT("Affection preserved"), Save->Affection, 15);
    TestEqual(TEXT("Stars preserved"), Save->StarsByLevel[1], 2);

    // 已经是当前版本的续局必须原样保留
    Save->bHasActiveRound = true;
    Save->ActiveCells.Init(0, 36);
    Save->ActiveCells[3] = 2;
    Save->ActiveStepCount = 7;
    Save->MigrateLevelRevision();
    TestTrue(TEXT("New round still resumable"), Save->bHasActiveRound);
    TestEqual(TEXT("New player move preserved"), Save->ActiveCells[3], uint8(2));
    TestEqual(TEXT("New step count preserved"), Save->ActiveStepCount, 7);

    // 机关状态的扁平编解码
    TArray<TArray<uint8>> States;
    States.Add(TArray<uint8>()); // 障碍没有状态
    States.Add({1, 0, 2, 1, 0}); // 符文宝箱：已开、未领、进度 2、未读碑、未挖
    TArray<uint8> Blob;
    FXiaomaMechanismStateCodec::Encode(States, Blob);
    TestEqual(TEXT("Encoded size"), Blob.Num(), 1 + 1 + 5);

    TArray<TArray<uint8>> Decoded;
    TestTrue(TEXT("States decode"), FXiaomaMechanismStateCodec::Decode(Blob, Decoded));
    TestEqual(TEXT("State count"), Decoded.Num(), 2);
    TestEqual(TEXT("Empty state stays empty"), Decoded[0].Num(), 0);
    TestEqual(TEXT("Chest state round-trip"), Decoded[1].Num(), 5);
    TestEqual(TEXT("Rune progress round-trip"), Decoded[1][2], uint8(2));

    // 坏数据必须整份拒绝，不能恢复出一半
    TArray<uint8> Truncated = Blob;
    Truncated.Pop();
    TestFalse(TEXT("Truncated states rejected"), FXiaomaMechanismStateCodec::Decode(Truncated, Decoded));
    TestEqual(TEXT("Rejected decode clears output"), Decoded.Num(), 0);

    // 布局指纹：稳定、区分关卡、随机关变化
    const int32 Fingerprint6 = FXiaomaPuzzle::AdventureFingerprint(Levels[5]);
    TestEqual(TEXT("Fingerprint stable"), FXiaomaPuzzle::AdventureFingerprint(Levels[5]), Fingerprint6);
    TestNotEqual(TEXT("Fingerprint separates levels"), FXiaomaPuzzle::AdventureFingerprint(Levels[6]), Fingerprint6);
    FXiaomaLevel Modified = Levels[5];
    Modified.Adventure.Obstacles.Add(FIntPoint(5, 5));
    TestNotEqual(TEXT("Fingerprint tracks obstacles"), FXiaomaPuzzle::AdventureFingerprint(Modified), Fingerprint6);

    // 冰面：第 16 关起才配，且会进指纹（地形改了旧续局就不能再用）
    for (int32 Index = 0; Index < 15; ++Index)
    {
        TestTrue(FString::Printf(TEXT("Level %d has no ice"), Levels[Index].Id),
            Levels[Index].Adventure.Ice.IsEmpty());
    }
    TestEqual(TEXT("Level 16 ice cells parsed"), Levels[15].Adventure.Ice.Num(), 4);
    TestEqual(TEXT("Level 17 ice cells parsed"), Levels[16].Adventure.Ice.Num(), 6);
    FXiaomaLevel Iced = Levels[5];
    Iced.Adventure.Ice.Add(FIntPoint(5, 5));
    TestNotEqual(TEXT("Fingerprint tracks ice"), FXiaomaPuzzle::AdventureFingerprint(Iced), Fingerprint6);

    // 木桥：第 18 关起才配；桥的数量与顺序决定存档里机关下标，所以必须进指纹
    for (int32 Index = 0; Index < 17; ++Index)
    {
        TestTrue(FString::Printf(TEXT("Level %d has no bridges"), Levels[Index].Id),
            Levels[Index].Adventure.Bridges.IsEmpty());
    }
    TestEqual(TEXT("Level 18 bridge cells parsed"), Levels[17].Adventure.Bridges.Num(), 1);
    TestEqual(TEXT("Level 19 bridge cells parsed"), Levels[18].Adventure.Bridges.Num(), 2);
    TestEqual(TEXT("Level 20 bridge cells parsed"), Levels[19].Adventure.Bridges.Num(), 1);
    FXiaomaLevel Bridged = Levels[5];
    Bridged.Adventure.Bridges.Add(FIntPoint(5, 5));
    TestNotEqual(TEXT("Fingerprint tracks bridges"), FXiaomaPuzzle::AdventureFingerprint(Bridged), Fingerprint6);

    // 色区围栏：第 21 关起才配；开门状态按 Mechanisms 下标进存档，区域与开关格也必须进指纹
    for (int32 Index = 0; Index < 20; ++Index)
    {
        TestTrue(FString::Printf(TEXT("Level %d has no gates"), Levels[Index].Id),
            Levels[Index].Adventure.Gates.IsEmpty());
    }
    TestEqual(TEXT("Level 21 gate count"), Levels[20].Adventure.Gates.Num(), 1);
    if (Levels[20].Adventure.Gates.Num() == 1)
    {
        const FXiaomaGateDef& Gate = Levels[20].Adventure.Gates[0];
        TestEqual(TEXT("Level 21 gate region"), Gate.Region, 2);
        TestEqual(TEXT("Level 21 gate switch row"), Gate.SwitchCell.X, 3);
        TestEqual(TEXT("Level 21 gate switch column"), Gate.SwitchCell.Y, 3);
        TestTrue(TEXT("Level 21 gate switch is outside its region"),
            Levels[20].RegionAt(Gate.SwitchCell.X, Gate.SwitchCell.Y) != Gate.Region);
    }
    FXiaomaLevel Gated = Levels[5];
    FXiaomaGateDef ExtraGate;
    ExtraGate.Region = 1;
    ExtraGate.SwitchCell = FIntPoint(3, 3);
    Gated.Adventure.Gates.Add(ExtraGate);
    TestNotEqual(TEXT("Fingerprint tracks gates"), FXiaomaPuzzle::AdventureFingerprint(Gated), Fingerprint6);
    // 区域或开关格一改，存档里那个开门状态就配错格子了，所以两者都要进指纹
    FXiaomaLevel OtherRegion = Gated;
    OtherRegion.Adventure.Gates[0].Region = 2;
    TestNotEqual(TEXT("Fingerprint tracks gate region"),
        FXiaomaPuzzle::AdventureFingerprint(OtherRegion), FXiaomaPuzzle::AdventureFingerprint(Gated));
    FXiaomaLevel OtherSwitch = Gated;
    OtherSwitch.Adventure.Gates[0].SwitchCell = FIntPoint(4, 4);
    TestNotEqual(TEXT("Fingerprint tracks gate switch"),
        FXiaomaPuzzle::AdventureFingerprint(OtherSwitch), FXiaomaPuzzle::AdventureFingerprint(Gated));

    // 续局判定：关卡、格数、生命、布局指纹任一条不对就不续
    UXiaomaSaveGame* Round = NewObject<UXiaomaSaveGame>();
    Round->MigrateLevelRevision();
    const FXiaomaLevel& Level6 = Levels[5];
    TestFalse(TEXT("Fresh save cannot resume"), Round->CanResume(Level6));

    Round->bHasActiveRound = true;
    Round->ActiveLevel = Level6.Id;
    Round->ActiveCells.Init(0, FXiaomaPuzzle::CellCount);
    Round->ActiveLevelFingerprint = FXiaomaPuzzle::AdventureFingerprint(Level6);
    Round->Lives = 3;
    TestTrue(TEXT("Matching round resumes"), Round->CanResume(Level6));

    Round->ActiveLevelFingerprint = 12345;
    TestFalse(TEXT("Stale layout cannot resume"), Round->CanResume(Level6));
    Round->ActiveLevelFingerprint = FXiaomaPuzzle::AdventureFingerprint(Level6);

    Round->ActiveCells.SetNum(35);
    TestFalse(TEXT("Truncated board cannot resume"), Round->CanResume(Level6));
    Round->ActiveCells.Init(0, FXiaomaPuzzle::CellCount);

    Round->Lives = 0;
    TestFalse(TEXT("Dead round cannot resume"), Round->CanResume(Level6));
    Round->Lives = 3;

    Round->ActiveLevel = Level6.Id + 1;
    TestFalse(TEXT("Other level cannot resume"), Round->CanResume(Level6));
    Round->ActiveLevel = Level6.Id;
    TestTrue(TEXT("Restored round resumes again"), Round->CanResume(Level6));

    // 「继续游戏」进哪一关：有续局回续局，否则回最后玩过的关，不能拿解锁进度顶替
    UXiaomaSaveGame* Nav = NewObject<UXiaomaSaveGame>();
    TestEqual(TEXT("Fresh save continues at level 1"), Nav->GetContinueLevel(25), 1);

    // 老存档没有 LastPlayedLevel（0），只能退回解锁进度
    Nav->UnlockedLevel = 25;
    TestEqual(TEXT("Legacy save falls back to unlocked"), Nav->GetContinueLevel(25), 25);

    // 玩到第 4 关就退出：不能跳到第 25 关
    Nav->LastPlayedLevel = 4;
    TestEqual(TEXT("Continue returns last played level"), Nav->GetContinueLevel(25), 4);

    // 通关第 4 关后退出：落在第 5 关
    Nav->LastPlayedLevel = 5;
    TestEqual(TEXT("Cleared level moves continue to next"), Nav->GetContinueLevel(25), 5);

    // 未解锁的关不能直接进
    Nav->UnlockedLevel = 6;
    Nav->LastPlayedLevel = 9;
    TestEqual(TEXT("Continue clamps to unlocked"), Nav->GetContinueLevel(25), 6);
    TestEqual(TEXT("Continue clamps to level count"), Nav->GetContinueLevel(5), 5);

    // 有续局时以续局为准
    Nav->UnlockedLevel = 6;
    Nav->LastPlayedLevel = 3;
    Nav->bHasActiveRound = true;
    Nav->ActiveLevel = 5;
    TestEqual(TEXT("Active round wins over last played"), Nav->GetContinueLevel(25), 5);
    return true;
}

#endif
