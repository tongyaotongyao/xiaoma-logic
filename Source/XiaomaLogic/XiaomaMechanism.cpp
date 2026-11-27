#include "XiaomaMechanism.h"
#include "XiaomaPalette.h"

#include "Components/SceneComponent.h"
#include "Components/StaticMeshComponent.h"
#include "Engine/StaticMesh.h"
#include "Materials/MaterialInstanceDynamic.h"
#include "Materials/MaterialInterface.h"

namespace
{
/** 格子边长，与棋盘、主角保持一致 */
constexpr float MechCellStep = 112.f;

/** 符文颜色：金黄 / 青蓝 / 紫 / 米白，与六色地板都能区分开 */
const FColor RuneColours[] = {
    FColor(255, 205, 70),
    FColor(86, 190, 214),
    FColor(170, 126, 224),
    FColor(242, 240, 232),
};

UStaticMeshComponent* AddMeshPart(AActor* Owner, USceneComponent* Parent, const TCHAR* Name,
    UStaticMesh* Mesh, const FVector& Location, const FVector& Scale, const FColor& Colour,
    UMaterialInstanceDynamic** OutMaterial)
{
    // SetupChest 在运行时调用，不能用 CreateDefaultSubobject，只能 NewObject + 手动注册。
    // 同一只 Actor 会建多个同类部件（符文圆盘/印记），名字必须唯一，否则会触发断言。
    const FName UniqueName = MakeUniqueObjectName(Owner, UStaticMeshComponent::StaticClass(), FName(Name));
    UStaticMeshComponent* Comp = NewObject<UStaticMeshComponent>(Owner, UniqueName);
    Comp->SetupAttachment(Parent);
    Comp->SetStaticMesh(Mesh);
    Comp->SetRelativeLocation(Location);
    Comp->SetRelativeScale3D(Scale);
    Comp->SetCollisionEnabled(ECollisionEnabled::NoCollision);
    UMaterialInterface* BaseMat = LoadObject<UMaterialInterface>(nullptr,
        TEXT("/Engine/BasicShapes/BasicShapeMaterial.BasicShapeMaterial"));
    if (BaseMat)
    {
        UMaterialInstanceDynamic* Mat = UMaterialInstanceDynamic::Create(BaseMat, Owner);
        Mat->SetVectorParameterValue(TEXT("Color"), FLinearColor(Colour));
        Comp->SetMaterial(0, Mat);
        if (OutMaterial)
        {
            *OutMaterial = Mat;
        }
    }
    Comp->RegisterComponent();
    Owner->AddInstanceComponent(Comp);
    return Comp;
}
}

AXiaomaMechanism::AXiaomaMechanism()
{
    PrimaryActorTick.bCanEverTick = false;
}

AXiaomaChest::AXiaomaChest()
{
    PrimaryActorTick.bCanEverTick = false;
    SceneRoot = CreateDefaultSubobject<USceneComponent>(TEXT("Root"));
    RootComponent = SceneRoot;

    BaseMesh = CreateDefaultSubobject<UStaticMeshComponent>(TEXT("BaseMesh"));
    BaseMesh->SetupAttachment(SceneRoot);
    BaseMesh->SetCollisionEnabled(ECollisionEnabled::NoCollision);

    LidMesh = CreateDefaultSubobject<UStaticMeshComponent>(TEXT("LidMesh"));
    LidMesh->SetupAttachment(SceneRoot);
    LidMesh->SetCollisionEnabled(ECollisionEnabled::NoCollision);

    GemMesh = CreateDefaultSubobject<UStaticMeshComponent>(TEXT("GemMesh"));
    GemMesh->SetupAttachment(SceneRoot);
    GemMesh->SetCollisionEnabled(ECollisionEnabled::NoCollision);
}

void AXiaomaChest::SetupChest(const FXiaomaChestDef& InDef, const FVector& WorldLocation)
{
    Def = InDef;
    bUnlocked = false;
    bClaimed = false;
    SetActorLocation(WorldLocation);

    UStaticMesh* Cube = LoadObject<UStaticMesh>(nullptr, TEXT("/Engine/BasicShapes/Cube.Cube"));
    UStaticMesh* Sphere = LoadObject<UStaticMesh>(nullptr, TEXT("/Engine/BasicShapes/Sphere.Sphere"));
    UMaterialInterface* BaseMat = LoadObject<UMaterialInterface>(nullptr,
        TEXT("/Engine/BasicShapes/BasicShapeMaterial.BasicShapeMaterial"));

    if (Cube && Sphere && BaseMat)
    {
        BaseMesh->SetStaticMesh(Cube);
        BaseMesh->SetRelativeLocation(FVector(0.f, 0.f, 20.f));
        BaseMesh->SetRelativeScale3D(FVector(0.46f, 0.34f, 0.24f));

        LidMesh->SetStaticMesh(Cube);
        LidMesh->SetRelativeLocation(FVector(0.f, 0.f, 35.f));
        LidMesh->SetRelativeScale3D(FVector(0.48f, 0.36f, 0.08f));

        GemMesh->SetStaticMesh(Sphere);
        GemMesh->SetRelativeLocation(FVector(0.f, 0.f, 48.f));
        GemMesh->SetRelativeScale3D(FVector(0.16f));

        BaseMaterial = UMaterialInstanceDynamic::Create(BaseMat, this);
        BaseMaterial->SetVectorParameterValue(TEXT("Color"), FLinearColor(FColor(145, 92, 52)));
        BaseMesh->SetMaterial(0, BaseMaterial);

        UMaterialInstanceDynamic* LidMat = UMaterialInstanceDynamic::Create(BaseMat, this);
        LidMat->SetVectorParameterValue(TEXT("Color"), FLinearColor(FColor(219, 173, 74)));
        LidMesh->SetMaterial(0, LidMat);

        GemMaterial = UMaterialInstanceDynamic::Create(BaseMat, this);
        BaseMesh->SetCastShadow(true);
        GemMesh->SetMaterial(0, GemMaterial);
    }

    if (Def.PuzzleType == EXiaomaChestPuzzleType::Runes)
    {
        BuildRuneVisuals();
    }
    else if (Def.PuzzleType == EXiaomaChestPuzzleType::Stele)
    {
        BuildSteleVisuals();
    }

    RefreshVisuals();
}

void AXiaomaChest::OnBoardChanged(const FXiaomaLevel& Level, const TArray<uint8>& Cells)
{
    if (bClaimed || Cells.Num() != FXiaomaPuzzle::CellCount)
    {
        return;
    }
    bool bSatisfied = false;
    if (Def.RuleType == EXiaomaChestRuleType::Row)
    {
        const int32 SolCol = Level.SolutionColumns.IsValidIndex(Def.RuleIndex)
            ? Level.SolutionColumns[Def.RuleIndex] : -1;
        if (SolCol >= 0)
        {
            bSatisfied = Cells[Def.RuleIndex * FXiaomaPuzzle::Size + SolCol] == 2;
        }
    }
    else if (Def.RuleType == EXiaomaChestRuleType::Column)
    {
        for (int32 Row = 0; Row < FXiaomaPuzzle::Size; ++Row)
        {
            if (Level.SolutionColumns.IsValidIndex(Row) && Level.SolutionColumns[Row] == Def.RuleIndex)
            {
                bSatisfied = Cells[Row * FXiaomaPuzzle::Size + Def.RuleIndex] == 2;
                break;
            }
        }
    }
    else
    {
        for (int32 Row = 0; Row < FXiaomaPuzzle::Size; ++Row)
        {
            const int32 Col = Level.SolutionColumns.IsValidIndex(Row) ? Level.SolutionColumns[Row] : -1;
            if (Col >= 0 && Level.RegionAt(Row, Col) == Def.RuleIndex)
            {
                bSatisfied = Cells[Row * FXiaomaPuzzle::Size + Col] == 2;
                break;
            }
        }
    }

    if (bSatisfied && !bUnlocked)
    {
        bUnlocked = true;
        RefreshVisuals();
    }
}

void AXiaomaChest::OnHerdsmanEnterCell(FIntPoint Cell)
{
    if (bClaimed)
    {
        return;
    }
    if (Def.PuzzleType == EXiaomaChestPuzzleType::Runes)
    {
        HandleRuneStep(Cell);
    }
    if (Cell == Def.Cell && bUnlocked && !bClaimed)
    {
        TryClaim();
    }
}

bool AXiaomaChest::TryInteract(FIntPoint Cell)
{
    if (bClaimed)
    {
        return false;
    }
    // 石碑格：按 E 读线索（读过再按仍然给提示，避免误开轮盘）
    if (Def.PuzzleType == EXiaomaChestPuzzleType::Stele && Cell == Def.SteleCell)
    {
        bSteleRead = true;
        RefreshVisuals();
        return true;
    }
    if (Cell != Def.Cell)
    {
        return false;
    }
    // 石碑宝箱：读过线索才能挖
    if (Def.PuzzleType == EXiaomaChestPuzzleType::Stele && !bDug)
    {
        if (bSteleRead)
        {
            bDug = true;
            bUnlocked = true;
            RefreshVisuals();
            OnChestNotice.ExecuteIfBound(TEXT("挖出了宝箱，再按一次 E 领取"), true);
        }
        else
        {
            OnChestNotice.ExecuteIfBound(TEXT("这里埋着东西，但还不知道埋在哪——先去找石碑"), false);
        }
        return true;
    }
    if (bUnlocked)
    {
        TryClaim();
        return true;
    }
    // 未解锁时反馈条件，仍消费一次 E 交互避免弹轮盘遮挡提示
    return true;
}

void AXiaomaChest::HandleRuneStep(FIntPoint Cell)
{
    if (bUnlocked || Def.RuneOrder.Num() == 0)
    {
        return;
    }
    const int32 Index = Def.RuneOrder.IndexOfByKey(Cell);
    if (Index == INDEX_NONE)
    {
        return;
    }
    if (Index == RuneProgress)
    {
        ++RuneProgress;
        if (RuneProgress >= Def.RuneOrder.Num())
        {
            bUnlocked = true;
            RefreshVisuals();
            OnChestNotice.ExecuteIfBound(TEXT("符文全部点亮，宝箱打开了"), true);
            return;
        }
    }
    else if (Index > RuneProgress)
    {
        // 踩错顺序：已经亮的全部熄灭
        RuneProgress = 0;
        OnChestNotice.ExecuteIfBound(TEXT("顺序错了，符文全部熄灭"), false);
        RefreshVisuals();
    }
    // Index < RuneProgress 表示重复踩已经亮过的符文，忽略
    RefreshRuneVisuals();
    RefreshVisuals();
}

bool AXiaomaChest::IsBuried() const
{
    return Def.PuzzleType == EXiaomaChestPuzzleType::Stele && !bDug && !bClaimed;
}

FVector AXiaomaChest::OffsetFromChest(FIntPoint Cell) const
{
    // 与棋盘一致：列号每加一，X 减一格；行号每加一，Y 减一格
    return FVector((Def.Cell.Y - Cell.Y) * MechCellStep,
        (Def.Cell.X - Cell.X) * MechCellStep, 0.f);
}

void AXiaomaChest::BuildRuneVisuals()
{
    UStaticMesh* Cylinder = LoadObject<UStaticMesh>(nullptr, TEXT("/Engine/BasicShapes/Cylinder.Cylinder"));
    UStaticMesh* Cube = LoadObject<UStaticMesh>(nullptr, TEXT("/Engine/BasicShapes/Cube.Cube"));
    UStaticMesh* Sphere = LoadObject<UStaticMesh>(nullptr, TEXT("/Engine/BasicShapes/Sphere.Sphere"));
    if (!Cylinder || !Cube || !Sphere || !SceneRoot)
    {
        return;
    }

    const int32 Count = FMath::Min(Def.RuneOrder.Num(), 4);

    // 盖子上方一排小宝石：从左到右就是踩亮顺序
    for (int32 Index = 0; Index < Count; ++Index)
    {
        UMaterialInstanceDynamic* Mat = nullptr;
        UStaticMeshComponent* Gem = AddMeshPart(this, SceneRoot, TEXT("OrderGem"),
            Sphere, FVector((Index - (Count - 1) * 0.5f) * 12.f, -14.f, 54.f),
            FVector(0.085f), RuneColours[Index % 4], &Mat);
        OrderGems.Add(Gem);
    }

    // 场上的符文格：一块圆盘 + 中间一个菱形印记，两者共用材质，一起变亮变暗
    for (int32 Index = 0; Index < Count; ++Index)
    {
        const FVector Offset = OffsetFromChest(Def.RuneOrder[Index]);
        UMaterialInstanceDynamic* Mat = nullptr;
        UStaticMeshComponent* Disc = AddMeshPart(this, SceneRoot, TEXT("RuneDisc"),
            Cylinder, Offset + FVector(0.f, 0.f, 17.f), FVector(0.30f, 0.30f, 0.02f),
            RuneColours[Index % 4], &Mat);
        UStaticMeshComponent* Glyph = AddMeshPart(this, SceneRoot, TEXT("RuneGlyph"),
            Cube, Offset + FVector(0.f, 0.f, 19.f), FVector(0.15f, 0.15f, 0.015f),
            RuneColours[Index % 4], nullptr);
        Glyph->SetRelativeRotation(FRotator(0.f, 45.f, 0.f));
        if (Mat)
        {
            Glyph->SetMaterial(0, Mat);
        }
        RuneDiscs.Add(Disc);
        RuneGlyphs.Add(Glyph);
        RuneMaterials.Add(Mat);
    }
}

void AXiaomaChest::BuildSteleVisuals()
{
    UStaticMesh* Cube = LoadObject<UStaticMesh>(nullptr, TEXT("/Engine/BasicShapes/Cube.Cube"));
    UStaticMesh* Sphere = LoadObject<UStaticMesh>(nullptr, TEXT("/Engine/BasicShapes/Sphere.Sphere"));
    if (!Cube || !Sphere || !SceneRoot || Def.SteleCell.X < 0)
    {
        return;
    }

    const FVector Offset = OffsetFromChest(Def.SteleCell);
    SteleBase = AddMeshPart(this, SceneRoot, TEXT("SteleBase"), Cube,
        Offset + FVector(0.f, 0.f, 12.f), FVector(0.30f, 0.22f, 0.07f), FColor(146, 143, 136), nullptr);
    SteleSlab = AddMeshPart(this, SceneRoot, TEXT("SteleSlab"), Cube,
        Offset + FVector(0.f, 0.f, 44.f), FVector(0.17f, 0.09f, 0.40f), FColor(180, 177, 168), nullptr);
    SteleGlyph = AddMeshPart(this, SceneRoot, TEXT("SteleGlyph"), Sphere,
        Offset + FVector(0.f, 0.f, 74.f), FVector(0.09f, 0.09f, 0.13f), FColor(255, 214, 96), nullptr);

    // 埋藏点提示：读过石碑后才亮起，告诉玩家挖哪里
    UMaterialInstanceDynamic* MarkerMaterial = nullptr;
    DigMarker = AddMeshPart(this, SceneRoot, TEXT("DigMarker"), Cube,
        FVector(0.f, 0.f, 17.f), FVector(0.60f, 0.60f, 0.015f), FColor(255, 214, 120), &MarkerMaterial);
    DigMarker->SetRelativeRotation(FRotator(0.f, 45.f, 0.f));
    DigMaterial = MarkerMaterial;
}

void AXiaomaChest::RefreshRuneVisuals()
{
    if (Def.PuzzleType != EXiaomaChestPuzzleType::Runes)
    {
        return;
    }
    for (int32 Index = 0; Index < RuneMaterials.Num(); ++Index)
    {
        const bool bLit = bUnlocked || Index < RuneProgress;
        FLinearColor Colour(RuneColours[Index % 4]);
        if (!bLit)
        {
            Colour *= 0.30f;
        }
        if (RuneMaterials[Index])
        {
            RuneMaterials[Index]->SetVectorParameterValue(TEXT("Color"), Colour);
        }
        if (RuneDiscs.IsValidIndex(Index) && RuneDiscs[Index])
        {
            const FVector Loc = RuneDiscs[Index]->GetRelativeLocation();
            RuneDiscs[Index]->SetRelativeLocation(FVector(Loc.X, Loc.Y, bLit ? 18.2f : 17.f));
        }
        if (RuneGlyphs.IsValidIndex(Index) && RuneGlyphs[Index])
        {
            const FVector Loc = RuneGlyphs[Index]->GetRelativeLocation();
            RuneGlyphs[Index]->SetRelativeLocation(FVector(Loc.X, Loc.Y, bLit ? 20.2f : 19.f));
        }
    }
}

FString AXiaomaChest::BuildPrompt() const
{
    if (bClaimed)
    {
        return FString();
    }
    if (Def.PuzzleType == EXiaomaChestPuzzleType::Runes)
    {
        if (bUnlocked)
        {
            return TEXT("符文全部点亮，按 E 领取提示星");
        }
        return FString::Printf(TEXT("符文宝箱上锁中：按宝箱上方的颜色顺序踩亮符文（%d/%d）"),
            RuneProgress, Def.RuneOrder.Num());
    }
    if (Def.PuzzleType == EXiaomaChestPuzzleType::Stele)
    {
        if (bDug)
        {
            return TEXT("宝箱已经挖出来了，按 E 领取提示星");
        }
        if (bSteleRead)
        {
            return TEXT("按石碑的线索走到发光的位置，按 E 挖开");
        }
        return TEXT("宝箱埋在土里：先去场上的石碑按 E 读线索");
    }
    const TCHAR* RuleText = Def.RuleType == EXiaomaChestRuleType::Row ? TEXT("对应行")
        : (Def.RuleType == EXiaomaChestRuleType::Column ? TEXT("对应列") : TEXT("对应颜色区域"));
    return FString::Printf(TEXT("宝箱上锁中：在%s第 %d 格放对小马即可开启"), RuleText, Def.RuleIndex + 1);
}

void AXiaomaChest::TryClaim()
{
    if (!bUnlocked || bClaimed)
    {
        return;
    }
    bClaimed = true;
    RefreshVisuals();
    FString DummyMsg;
    OnChestOpened.ExecuteIfBound(Def.Reward, Def.Cell, DummyMsg);
}

void AXiaomaChest::RefreshVisuals()
{
    const bool bBuried = IsBuried();
    const bool bShowChest = !bClaimed && !bBuried;

    if (GemMaterial)
    {
        FLinearColor GemColor = FLinearColor(FColor(140, 140, 140));
        if (bUnlocked && !bClaimed)
        {
            GemColor = FLinearColor(FColor(255, 215, 70));
        }
        else if (Def.PuzzleType == EXiaomaChestPuzzleType::Rule
            && Def.RuleType == EXiaomaChestRuleType::Region
            && Def.RuleIndex >= 0 && Def.RuleIndex < 6)
        {
            GemColor = XiaomaPalette::Regions[Def.RuleIndex];
        }
        GemMaterial->SetVectorParameterValue(TEXT("Color"), GemColor);
    }
    if (BaseMesh)
    {
        BaseMesh->SetVisibility(bShowChest);
    }
    if (LidMesh)
    {
        LidMesh->SetRelativeLocation(bUnlocked ? FVector(0.f, 14.f, 42.f) : FVector(0.f, 0.f, 35.f));
        LidMesh->SetRelativeRotation(bUnlocked ? FRotator(0.f, 0.f, -40.f) : FRotator::ZeroRotator);
        LidMesh->SetVisibility(bShowChest);
    }
    if (GemMesh)
    {
        GemMesh->SetVisibility(bShowChest && !bClaimed);
    }
    // 顺序提示只在符文宝箱还没打开时显示
    for (const TObjectPtr<UStaticMeshComponent>& Gem : OrderGems)
    {
        if (Gem)
        {
            Gem->SetVisibility(bShowChest && !bUnlocked);
        }
    }
    RefreshRuneVisuals();
    // 埋藏点提示：读过石碑、还没挖出来时亮着
    if (DigMarker)
    {
        DigMarker->SetVisibility(bBuried && bSteleRead);
    }
    // 石碑本体一直留在场上，所以不能整只 Actor 隐藏，只能逐个部件控制
    SetActorHiddenInGame(false);
}

void AXiaomaChest::SaveState(TArray<uint8>& Out) const
{
    Out.Reset();
    Out.Add(bUnlocked ? 1 : 0);
    Out.Add(bClaimed ? 1 : 0);
    Out.Add(static_cast<uint8>(FMath::Clamp(RuneProgress, 0, 255)));
    Out.Add(bSteleRead ? 1 : 0);
    Out.Add(bDug ? 1 : 0);
}

void AXiaomaChest::LoadState(const TArray<uint8>& In)
{
    if (In.Num() >= 2)
    {
        bUnlocked = In[0] != 0;
        bClaimed = In[1] != 0;
        RuneProgress = In.Num() >= 3 ? In[2] : 0;
        bSteleRead = In.Num() >= 4 && In[3] != 0;
        bDug = In.Num() >= 5 && In[4] != 0;
        RefreshVisuals();
    }
}

AXiaomaObstacle::AXiaomaObstacle()
{
    PrimaryActorTick.bCanEverTick = false;
    SceneRoot = CreateDefaultSubobject<USceneComponent>(TEXT("Root"));
    RootComponent = SceneRoot;

    MainMesh = CreateDefaultSubobject<UStaticMeshComponent>(TEXT("MainMesh"));
    MainMesh->SetupAttachment(SceneRoot);
    MainMesh->SetCollisionEnabled(ECollisionEnabled::NoCollision);

    TrimMesh = CreateDefaultSubobject<UStaticMeshComponent>(TEXT("TrimMesh"));
    TrimMesh->SetupAttachment(SceneRoot);
    TrimMesh->SetCollisionEnabled(ECollisionEnabled::NoCollision);
}

void AXiaomaObstacle::SetupObstacle(const FIntPoint& InCell, const FVector& WorldLocation, int32 Variant)
{
    ObstacleCell = InCell;
    SetActorLocation(WorldLocation);

    UStaticMesh* Sphere = LoadObject<UStaticMesh>(nullptr, TEXT("/Engine/BasicShapes/Sphere.Sphere"));
    UStaticMesh* Cylinder = LoadObject<UStaticMesh>(nullptr, TEXT("/Engine/BasicShapes/Cylinder.Cylinder"));
    UMaterialInterface* BaseMat = LoadObject<UMaterialInterface>(nullptr,
        TEXT("/Engine/BasicShapes/BasicShapeMaterial.BasicShapeMaterial"));
    if (!Sphere || !Cylinder || !BaseMat)
    {
        return;
    }

    MainMaterial = UMaterialInstanceDynamic::Create(BaseMat, this);
    UMaterialInstanceDynamic* TrimMaterial = UMaterialInstanceDynamic::Create(BaseMat, this);

    const int32 Kind = ((Variant % 3) + 3) % 3;
    if (Kind == 0)
    {
        // 石头：一大一小两块，低矮有阴影
        MainMesh->SetStaticMesh(Sphere);
        MainMesh->SetRelativeLocation(FVector(0.f, 0.f, 22.f));
        MainMesh->SetRelativeScale3D(FVector(0.48f, 0.42f, 0.34f));
        TrimMesh->SetStaticMesh(Sphere);
        TrimMesh->SetRelativeLocation(FVector(24.f, -18.f, 10.f));
        TrimMesh->SetRelativeScale3D(FVector(0.22f));
        MainMaterial->SetVectorParameterValue(TEXT("Color"), FLinearColor(FColor(138, 134, 129)));
        TrimMaterial->SetVectorParameterValue(TEXT("Color"), FLinearColor(FColor(112, 108, 104)));
    }
    else if (Kind == 1)
    {
        // 木桩：圆柱加一圈顶面
        MainMesh->SetStaticMesh(Cylinder);
        MainMesh->SetRelativeLocation(FVector(0.f, 0.f, 26.f));
        MainMesh->SetRelativeScale3D(FVector(0.34f, 0.34f, 0.52f));
        TrimMesh->SetStaticMesh(Cylinder);
        TrimMesh->SetRelativeLocation(FVector(0.f, 0.f, 54.f));
        TrimMesh->SetRelativeScale3D(FVector(0.38f, 0.38f, 0.04f));
        MainMaterial->SetVectorParameterValue(TEXT("Color"), FLinearColor(FColor(139, 107, 69)));
        TrimMaterial->SetVectorParameterValue(TEXT("Color"), FLinearColor(FColor(96, 74, 48)));
    }
    else
    {
        // 灌木：压扁的双球
        MainMesh->SetStaticMesh(Sphere);
        MainMesh->SetRelativeLocation(FVector(0.f, 0.f, 26.f));
        MainMesh->SetRelativeScale3D(FVector(0.44f, 0.44f, 0.40f));
        TrimMesh->SetStaticMesh(Sphere);
        TrimMesh->SetRelativeLocation(FVector(-16.f, 14.f, 16.f));
        TrimMesh->SetRelativeScale3D(FVector(0.24f));
        MainMaterial->SetVectorParameterValue(TEXT("Color"), FLinearColor(FColor(109, 148, 96)));
        TrimMaterial->SetVectorParameterValue(TEXT("Color"), FLinearColor(FColor(86, 124, 76)));
    }

    MainMesh->SetMaterial(0, MainMaterial);
    TrimMesh->SetMaterial(0, TrimMaterial);
    MainMesh->SetCastShadow(true);
    TrimMesh->SetCastShadow(true);
}
