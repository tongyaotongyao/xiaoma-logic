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

/** 地板顶面在 z≈8（与围栏、冰面同一套高度基准）；金线贴在地板缝里，只露一点点 */
constexpr float MechFloorTopZ = 8.f;
constexpr float MechHintLineZ = MechFloorTopZ + 1.6f;
/** 截面：宽 5 正好嵌进 4.5 宽的地板缝，高 3.5，走在上面也不会被看成障碍 */
constexpr float MechHintLineThickness = 0.05f;
constexpr float MechHintLineHeight = 0.035f;

/** 六色草皮的名字，与 HUD 图例、召唤轮盘保持一致 */
const TCHAR* RegionColourNames[6] = {
    TEXT("蓝色"), TEXT("棕色"), TEXT("粉色"),
    TEXT("红色"), TEXT("黄色"), TEXT("薄荷色")
};

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
    bClaimed = false;
    StageIndex = 0;
    // 校验工具保证至少一段，运行时再兜一层，避免空数组导致后续全部越界
    if (Def.Stages.Num() == 0)
    {
        Def.Stages.Add(FXiaomaChestStage());
    }
    StageProgress.Init(0, Def.Stages.Num());
    StageVisuals.Reset();
    StageVisuals.SetNum(Def.Stages.Num());
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

    // 每段的部件一次建好：后续段先压暗放着，轮到它再点亮，
    // 这样玩家从头就能看见"这关有几道锁"，也不至于提前踩到却毫无反馈。
    for (int32 Index = 0; Index < Def.Stages.Num(); ++Index)
    {
        BuildStageVisuals(Index);
    }

    RefreshVisuals();
}

void AXiaomaChest::OnBoardChanged(const FXiaomaLevel& Level, const TArray<uint8>& Cells)
{
    // 只有规则开箱看棋盘；符文、石碑、推箱各有自己的触发路径。
    // 少了这道判断，推箱宝箱会拿默认的 RuleType 去套行规则，摆对某一行就自己开了。
    const FXiaomaChestStage* Stage = GetActiveStage();
    if (!Stage || Stage->PuzzleType != EXiaomaChestPuzzleType::Rule)
    {
        return;
    }
    if (bClaimed || Cells.Num() != FXiaomaPuzzle::CellCount)
    {
        return;
    }
    bool bSatisfied = false;
    const EXiaomaChestRuleType RuleType = Stage->RuleType;
    const int32 RuleIndex = Stage->RuleIndex;
    if (RuleType == EXiaomaChestRuleType::Row)
    {
        const int32 SolCol = Level.SolutionColumns.IsValidIndex(RuleIndex)
            ? Level.SolutionColumns[RuleIndex] : -1;
        if (SolCol >= 0)
        {
            bSatisfied = Cells[RuleIndex * FXiaomaPuzzle::Size + SolCol] == 2;
        }
    }
    else if (RuleType == EXiaomaChestRuleType::Column)
    {
        for (int32 Row = 0; Row < FXiaomaPuzzle::Size; ++Row)
        {
            if (Level.SolutionColumns.IsValidIndex(Row) && Level.SolutionColumns[Row] == RuleIndex)
            {
                bSatisfied = Cells[Row * FXiaomaPuzzle::Size + RuleIndex] == 2;
                break;
            }
        }
    }
    else
    {
        for (int32 Row = 0; Row < FXiaomaPuzzle::Size; ++Row)
        {
            const int32 Col = Level.SolutionColumns.IsValidIndex(Row) ? Level.SolutionColumns[Row] : -1;
            if (Col >= 0 && Level.RegionAt(Row, Col) == RuleIndex)
            {
                bSatisfied = Cells[Row * FXiaomaPuzzle::Size + Col] == 2;
                break;
            }
        }
    }

    if (bSatisfied && !IsStageComplete(StageIndex))
    {
        CompleteActiveStage();
    }
}

void AXiaomaChest::OnHerdsmanEnterCell(FIntPoint Cell)
{
    if (bClaimed)
    {
        return;
    }
    const FXiaomaChestStage* Stage = GetActiveStage();
    if (Stage && Stage->PuzzleType == EXiaomaChestPuzzleType::Runes)
    {
        HandleRuneStep(Cell);
    }
    else if (IsFutureRuneCell(Cell))
    {
        // 踩到后面才轮到的符文：给个明确反馈，别让玩家以为踩了没反应
        OnChestNotice.ExecuteIfBound(TEXT("这个符文还没醒——先把前面那道锁打开"), false);
    }
    if (Cell == Def.Cell && IsUnlocked() && !bClaimed)
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
    // 任一阶段的石碑格：按 E 读线索（已完成阶段的石碑也还能念，当参考）
    if (const FXiaomaChestStage* SteleStage = FindSteleStage(Cell))
    {
        const FXiaomaChestStage* Active = GetActiveStage();
        if (SteleStage == Active && !SteleStage->ClueText.IsEmpty())
        {
            // 只有轮到这一段时，"读过"才记进度
            if (StageProgress.IsValidIndex(StageIndex) && StageProgress[StageIndex] < 1)
            {
                StageProgress[StageIndex] = 1;
            }
            RefreshVisuals();
        }
        return true;
    }
    if (Cell != Def.Cell)
    {
        return false;
    }
    const FXiaomaChestStage* Stage = GetActiveStage();
    // 石碑段：读过线索才能挖，挖出来这一段就完成
    if (Stage && Stage->PuzzleType == EXiaomaChestPuzzleType::Stele)
    {
        const uint8 Progress = StageProgress.IsValidIndex(StageIndex) ? StageProgress[StageIndex] : 0;
        if (Progress < 2)
        {
            if (Progress >= 1)
            {
                // 先挖出来再推进段，提示里带上"挖出了宝箱"
                CompleteActiveStage(TEXT("挖出了宝箱"));
            }
            else
            {
                OnChestNotice.ExecuteIfBound(TEXT("这里埋着东西，但还不知道埋在哪——先去找石碑"), false);
            }
            return true;
        }
    }
    if (IsUnlocked())
    {
        TryClaim();
        return true;
    }
    // 未解锁时反馈条件，仍消费一次 E 交互避免弹轮盘遮挡提示
    return true;
}

void AXiaomaChest::HandleRuneStep(FIntPoint Cell)
{
    const FXiaomaChestStage* Stage = GetActiveStage();
    if (!Stage || Stage->PuzzleType != EXiaomaChestPuzzleType::Runes
        || Stage->RuneOrder.Num() == 0 || !StageProgress.IsValidIndex(StageIndex))
    {
        return;
    }
    const int32 Index = Stage->RuneOrder.IndexOfByKey(Cell);
    if (Index == INDEX_NONE)
    {
        return;
    }
    const int32 Progress = StageProgress[StageIndex];
    if (Index == Progress)
    {
        ++StageProgress[StageIndex];
        if (StageProgress[StageIndex] >= Stage->RuneOrder.Num())
        {
            CompleteActiveStage();
            return;
        }
    }
    else if (Index > Progress)
    {
        // 踩错顺序：这一段已经亮的全部熄灭
        StageProgress[StageIndex] = 0;
        OnChestNotice.ExecuteIfBound(TEXT("顺序错了，符文全部熄灭"), false);
    }
    // Index < Progress 表示重复踩已经亮过的符文，忽略
    RefreshRuneVisuals(StageIndex);
    RefreshVisuals();
}

bool AXiaomaChest::IsBuried() const
{
    const FXiaomaChestStage* Stage = GetActiveStage();
    if (!Stage || bClaimed || Stage->PuzzleType != EXiaomaChestPuzzleType::Stele)
    {
        return false;
    }
    const uint8 Progress = StageProgress.IsValidIndex(StageIndex) ? StageProgress[StageIndex] : 0;
    return Progress < 2;
}

const FXiaomaChestStage* AXiaomaChest::GetActiveStage() const
{
    return Def.Stages.IsValidIndex(StageIndex) ? &Def.Stages[StageIndex] : nullptr;
}

const FXiaomaChestStage* AXiaomaChest::FindSteleStage(FIntPoint Cell) const
{
    for (const FXiaomaChestStage& Stage : Def.Stages)
    {
        if (Stage.PuzzleType == EXiaomaChestPuzzleType::Stele && Stage.SteleCell == Cell)
        {
            return &Stage;
        }
    }
    return nullptr;
}

bool AXiaomaChest::IsPushStageActive() const
{
    const FXiaomaChestStage* Stage = GetActiveStage();
    return Stage && Stage->PuzzleType == EXiaomaChestPuzzleType::Push;
}

FIntPoint AXiaomaChest::GetActivePlate() const
{
    const FXiaomaChestStage* Stage = GetActiveStage();
    return Stage && Stage->PuzzleType == EXiaomaChestPuzzleType::Push
        ? Stage->PushPlate : FIntPoint(-1, -1);
}

bool AXiaomaChest::IsFutureRuneCell(FIntPoint Cell) const
{
    for (int32 Index = StageIndex + 1; Index < Def.Stages.Num(); ++Index)
    {
        const FXiaomaChestStage& Stage = Def.Stages[Index];
        if (Stage.PuzzleType == EXiaomaChestPuzzleType::Runes && Stage.RuneOrder.Contains(Cell))
        {
            return true;
        }
    }
    return false;
}

int32 AXiaomaChest::MaxProgressForStage(const FXiaomaChestStage& Stage)
{
    if (Stage.PuzzleType == EXiaomaChestPuzzleType::Runes)
    {
        return FMath::Max(1, Stage.RuneOrder.Num());
    }
    if (Stage.PuzzleType == EXiaomaChestPuzzleType::Stele)
    {
        return 2;
    }
    return 1;
}

bool AXiaomaChest::IsStageComplete(int32 Index) const
{
    if (!Def.Stages.IsValidIndex(Index) || !StageProgress.IsValidIndex(Index))
    {
        return false;
    }
    return StageProgress[Index] >= MaxProgressForStage(Def.Stages[Index]);
}

void AXiaomaChest::CompleteActiveStage(const FString& Prefix)
{
    if (!Def.Stages.IsValidIndex(StageIndex))
    {
        return;
    }
    const FXiaomaChestStage& Stage = Def.Stages[StageIndex];
    if (StageProgress.IsValidIndex(StageIndex))
    {
        StageProgress[StageIndex] = static_cast<uint8>(MaxProgressForStage(Stage));
    }
    ++StageIndex;
    RefreshVisuals();

    FString Message;
    if (StageIndex < Def.Stages.Num())
    {
        const FString Next = BuildPrompt();
        Message = Next.IsEmpty()
            ? FString::Printf(TEXT("第 %d/%d 道锁开了"), StageIndex, Def.Stages.Num())
            : FString::Printf(TEXT("第 %d/%d 道锁开了——%s"), StageIndex, Def.Stages.Num(), *Next);
    }
    else
    {
        Message = TEXT("最后一道锁开了，宝箱打开了");
    }
    if (!Prefix.IsEmpty())
    {
        Message = FString::Printf(TEXT("%s——%s"), *Prefix, *Message);
    }
    OnChestNotice.ExecuteIfBound(Message, true);
}

FVector AXiaomaChest::OffsetFromChest(FIntPoint Cell) const
{
    // 与棋盘一致：列号每加一，X 减一格；行号每加一，Y 减一格
    return FVector((Def.Cell.Y - Cell.Y) * MechCellStep,
        (Def.Cell.X - Cell.X) * MechCellStep, 0.f);
}

void AXiaomaChest::BuildStageVisuals(int32 StageNum)
{
    if (!Def.Stages.IsValidIndex(StageNum) || !StageVisuals.IsValidIndex(StageNum) || !SceneRoot)
    {
        return;
    }
    const FXiaomaChestStage& Stage = Def.Stages[StageNum];
    FXiaomaChestStageVisuals& V = Visuals(StageNum);

    UStaticMesh* Cylinder = LoadObject<UStaticMesh>(nullptr, TEXT("/Engine/BasicShapes/Cylinder.Cylinder"));
    UStaticMesh* Cube = LoadObject<UStaticMesh>(nullptr, TEXT("/Engine/BasicShapes/Cube.Cube"));
    UStaticMesh* Sphere = LoadObject<UStaticMesh>(nullptr, TEXT("/Engine/BasicShapes/Sphere.Sphere"));

    if (Stage.PuzzleType == EXiaomaChestPuzzleType::Runes)
    {
        if (!Cylinder || !Cube || !Sphere)
        {
            return;
        }
        const int32 Count = FMath::Min(Stage.RuneOrder.Num(), 4);

        // 盖子上方一排小宝石：从左到右就是踩亮顺序
        for (int32 Index = 0; Index < Count; ++Index)
        {
            UMaterialInstanceDynamic* Mat = nullptr;
            UStaticMeshComponent* Gem = AddMeshPart(this, SceneRoot, TEXT("OrderGem"),
                Sphere, FVector((Index - (Count - 1) * 0.5f) * 12.f, -14.f, 54.f),
                FVector(0.085f), RuneColours[Index % 4], &Mat);
            V.OrderGems.Add(Gem);
        }

        // 场上的符文格：一块圆盘 + 中间一个菱形印记，两者共用材质，一起变亮变暗
        for (int32 Index = 0; Index < Count; ++Index)
        {
            const FVector Offset = OffsetFromChest(Stage.RuneOrder[Index]);
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
            V.RuneDiscs.Add(Disc);
            V.RuneGlyphs.Add(Glyph);
            V.RuneMaterials.Add(Mat);
        }
    }
    else if (Stage.PuzzleType == EXiaomaChestPuzzleType::Stele)
    {
        if (!Cube || !Sphere || Stage.SteleCell.X < 0)
        {
            return;
        }
        const FVector Offset = OffsetFromChest(Stage.SteleCell);
        V.SteleBase = AddMeshPart(this, SceneRoot, TEXT("SteleBase"), Cube,
            Offset + FVector(0.f, 0.f, 12.f), FVector(0.30f, 0.22f, 0.07f), FColor(146, 143, 136), nullptr);
        V.SteleSlab = AddMeshPart(this, SceneRoot, TEXT("SteleSlab"), Cube,
            Offset + FVector(0.f, 0.f, 44.f), FVector(0.17f, 0.09f, 0.40f), FColor(180, 177, 168), nullptr);
        V.SteleGlyph = AddMeshPart(this, SceneRoot, TEXT("SteleGlyph"), Sphere,
            Offset + FVector(0.f, 0.f, 74.f), FVector(0.09f, 0.09f, 0.13f), FColor(255, 214, 96), nullptr);

        // 埋藏点提示：读过石碑后才亮起，告诉玩家挖哪里
        UMaterialInstanceDynamic* MarkerMaterial = nullptr;
        V.DigMarker = AddMeshPart(this, SceneRoot, TEXT("DigMarker"), Cube,
            FVector(0.f, 0.f, 17.f), FVector(0.60f, 0.60f, 0.015f), FColor(255, 214, 120), &MarkerMaterial);
        V.DigMarker->SetRelativeRotation(FRotator(0.f, 45.f, 0.f));
        V.DigMaterial = MarkerMaterial;
    }
    else if (Stage.PuzzleType == EXiaomaChestPuzzleType::Push)
    {
        if (!Cylinder || !Cube || Stage.PushPlate.X < 0)
        {
            return;
        }
        // 压力板画在宝箱 Actor 下、按格差偏移，所以箱子推动时它不会跟着跑。
        // 高度必须踩在地板顶面上：地板顶面约在 z = 8（小马就摆在 z = 10），
        // 原先画在 z = 2、厚 4 个单位，整块埋在地板里，玩家根本看不到。
        const FVector Offset = OffsetFromChest(Stage.PushPlate);
        UMaterialInstanceDynamic* PadMaterial = nullptr;
        UMaterialInstanceDynamic* GlowMaterial = nullptr;

        // 底盘：一块厚圆饼，压住时整块由冷灰转暖色
        V.PlatePad = AddMeshPart(this, SceneRoot, TEXT("PlatePad"), Cylinder,
            Offset + FVector(0.f, 0.f, 9.f), FVector(0.86f, 0.86f, 0.14f), FColor(92, 96, 104), &PadMaterial);

        // 四角铆钉：让这一格一眼看出是机关，而不是地板的深色贴图
        for (int32 Bolt = 0; Bolt < 4; ++Bolt)
        {
            const float SignX = (Bolt & 1) ? 1.f : -1.f;
            const float SignY = (Bolt & 2) ? 1.f : -1.f;
            AddMeshPart(this, SceneRoot, TEXT("PlateBolt"), Cube,
                Offset + FVector(SignX * 24.f, SignY * 24.f, 16.f),
                FVector(0.06f, 0.06f, 0.06f), FColor(58, 62, 70), nullptr);
        }

        // 中央指示片：没压住是暗铜色，压住后变亮，兼作「箱子该推到哪儿」的目标点
        V.PlateGlow = AddMeshPart(this, SceneRoot, TEXT("PlateGlow"), Cylinder,
            Offset + FVector(0.f, 0.f, 16.6f), FVector(0.52f, 0.52f, 0.04f), FColor(96, 78, 46), &GlowMaterial);

        V.PlateMaterial = PadMaterial;
        V.PlateGlowMaterial = GlowMaterial;
        if (V.PlatePad) V.PlatePad->SetCastShadow(true);
        if (V.PlateGlow) V.PlateGlow->SetCastShadow(false);
    }
}

void AXiaomaChest::RefreshRuneVisuals(int32 StageNum)
{
    if (!Def.Stages.IsValidIndex(StageNum) || !StageVisuals.IsValidIndex(StageNum))
    {
        return;
    }
    const FXiaomaChestStage& Stage = Def.Stages[StageNum];
    if (Stage.PuzzleType != EXiaomaChestPuzzleType::Runes)
    {
        return;
    }
    FXiaomaChestStageVisuals& V = Visuals(StageNum);
    // 已经做完的段整片点亮；当前段按进度点亮；还没轮到的段全暗，表示"未激活"
    const bool bDone = StageNum < StageIndex;
    const int32 Progress = StageProgress.IsValidIndex(StageNum) ? StageProgress[StageNum] : 0;
    for (int32 Index = 0; Index < V.RuneMaterials.Num(); ++Index)
    {
        const bool bLit = bDone || (StageNum == StageIndex && Index < Progress);
        FLinearColor Colour(RuneColours[Index % 4]);
        if (!bLit)
        {
            Colour *= 0.30f;
        }
        if (V.RuneMaterials[Index])
        {
            V.RuneMaterials[Index]->SetVectorParameterValue(TEXT("Color"), Colour);
        }
        if (V.RuneDiscs.IsValidIndex(Index) && V.RuneDiscs[Index])
        {
            const FVector Loc = V.RuneDiscs[Index]->GetRelativeLocation();
            V.RuneDiscs[Index]->SetRelativeLocation(FVector(Loc.X, Loc.Y, bLit ? 18.2f : 17.f));
        }
        if (V.RuneGlyphs.IsValidIndex(Index) && V.RuneGlyphs[Index])
        {
            const FVector Loc = V.RuneGlyphs[Index]->GetRelativeLocation();
            V.RuneGlyphs[Index]->SetRelativeLocation(FVector(Loc.X, Loc.Y, bLit ? 20.2f : 19.f));
        }
    }
}

FString AXiaomaChest::BuildPrompt() const
{
    if (bClaimed)
    {
        return FString();
    }
    if (IsUnlocked())
    {
        return FString::Printf(TEXT("宝箱已经打开，按 E 领取%s"),
            Def.Reward == EXiaomaItemType::Hint ? TEXT("提示星") : TEXT("奖励"));
    }
    const FXiaomaChestStage* Stage = GetActiveStage();
    if (!Stage)
    {
        return FString();
    }
    FString Body;
    const int32 Progress = StageProgress.IsValidIndex(StageIndex) ? StageProgress[StageIndex] : 0;
    if (Stage->PuzzleType == EXiaomaChestPuzzleType::Runes)
    {
        Body = FString::Printf(TEXT("按宝箱上方的颜色顺序踩亮符文（%d/%d）"),
            Progress, Stage->RuneOrder.Num());
    }
    else if (Stage->PuzzleType == EXiaomaChestPuzzleType::Stele)
    {
        Body = Progress >= 1
            ? FString(TEXT("按石碑的线索走到发光的位置，按 E 挖开"))
            : FString(TEXT("宝箱埋在土里：先去场上的石碑按 E 读线索"));
    }
    else if (Stage->PuzzleType == EXiaomaChestPuzzleType::Push)
    {
        Body = FString(TEXT("把场上的木箱推到铁制压力板上"));
    }
    else if (Stage->RuleType == EXiaomaChestRuleType::Row)
    {
        // RuleIndex 是行号，不是"这一行里的第几格"。旧文案写成"第 x 格"会被读成列号，
        // 玩家照着数一定数错，所以统一说"把这一行的马摆对"。
        Body = FString::Printf(TEXT("把第 %d 行的马摆对即可开启"), Stage->RuleIndex + 1);
    }
    else if (Stage->RuleType == EXiaomaChestRuleType::Column)
    {
        Body = FString::Printf(TEXT("把第 %d 列的马摆对即可开启"), Stage->RuleIndex + 1);
    }
    else
    {
        // 色区没有"第几"的说法，直接报颜色（宝石这时也已经染成同一个色，两边对得上）
        const TCHAR* ColourName = Stage->RuleIndex >= 0 && Stage->RuleIndex < UE_ARRAY_COUNT(RegionColourNames)
            ? RegionColourNames[Stage->RuleIndex] : TEXT("对应颜色");
        Body = FString::Printf(TEXT("把%s草皮上的马摆对即可开启"), ColourName);
    }
    // 多段宝箱把「第几道锁」写在前面，玩家一眼知道还剩几步
    if (Def.Stages.Num() > 1)
    {
        return FString::Printf(TEXT("第 %d/%d 道锁：%s"), StageIndex + 1, Def.Stages.Num(), *Body);
    }
    return FString::Printf(TEXT("宝箱上锁中：%s"), *Body);
}

void AXiaomaChest::UnlockByMechanism()
{
    // 只有当前段是推箱时才认这块压力板；不是的话说明这把锁另有解法
    const FXiaomaChestStage* Stage = GetActiveStage();
    if (!Stage || Stage->PuzzleType != EXiaomaChestPuzzleType::Push || bClaimed)
    {
        return;
    }
    if (!IsStageComplete(StageIndex))
    {
        CompleteActiveStage();
    }
}

void AXiaomaChest::SetPlatePressed(bool bPressed)
{
    // 压力板只在推箱段显示，别的段没有这块盘，直接跳过
    const FXiaomaChestStage* Stage = GetActiveStage();
    if (!Stage || Stage->PuzzleType != EXiaomaChestPuzzleType::Push
        || !StageVisuals.IsValidIndex(StageIndex))
    {
        return;
    }
    FXiaomaChestStageVisuals& V = Visuals(StageIndex);
    V.bPlatePressed = bPressed;
    if (V.PlateMaterial)
    {
        // 压住时铁盘变暖色，没压住是冷灰
        V.PlateMaterial->SetVectorParameterValue(TEXT("Color"),
            bPressed ? FLinearColor(FColor(226, 182, 92)) : FLinearColor(FColor(92, 96, 104)));
    }
    if (V.PlateGlowMaterial)
    {
        // 中央指示片常亮，只有暗/亮两档：不压住时也要能看见压力板在哪
        V.PlateGlowMaterial->SetVectorParameterValue(TEXT("Color"),
            bPressed ? FLinearColor(FColor(255, 214, 96)) : FLinearColor(FColor(96, 78, 46)));
    }
}

void AXiaomaChest::TryClaim()
{
    if (!IsUnlocked() || bClaimed || bClaiming)
    {
        return;
    }
    // 先通知、后置位：GameMode 在回调里拍撤销快照，快照必须记成“还没开”。
    // 反过来做的话，撤销会把奖励退回去、宝箱却是空的，奖励就白丢了。
    bClaiming = true;
    FString DummyMsg;
    OnChestOpened.ExecuteIfBound(Def.Reward, Def.Cell, DummyMsg);
    bClaiming = false;
    bClaimed = true;
    RefreshVisuals();
}

void AXiaomaChest::RefreshLineHint()
{
    // 只有「规则开箱 + 指定某一行/列」需要在地板上指出这条线：
    // 色区块已经靠宝石变色指路，符文/石碑/推箱根本没有"线"这回事。
    const FXiaomaChestStage* Stage = GetActiveStage();
    const bool bWantLine = Stage && !bClaimed
        && Stage->PuzzleType == EXiaomaChestPuzzleType::Rule
        && Stage->RuleType != EXiaomaChestRuleType::Region
        && Stage->RuleIndex >= 0 && Stage->RuleIndex < FXiaomaPuzzle::Size;

    if (!bWantLine)
    {
        for (const TObjectPtr<UStaticMeshComponent>& Rail : LineRails)
        {
            if (Rail)
            {
                Rail->SetVisibility(false);
            }
        }
        return;
    }

    if (LineRails.Num() == 0)
    {
        UStaticMesh* Cube = LoadObject<UStaticMesh>(nullptr, TEXT("/Engine/BasicShapes/Cube.Cube"));
        if (!Cube || !SceneRoot)
        {
            return;
        }
        // 两条线一次建好，之后换行换列只改位置与缩放，组件数不变
        const FColor LineColour(226, 180, 82);
        for (int32 Side = 0; Side < 2; ++Side)
        {
            UStaticMeshComponent* Rail = AddMeshPart(this, SceneRoot, TEXT("LineHintRail"), Cube,
                FVector::ZeroVector, FVector(1.f), LineColour, nullptr);
            // 组合宝箱换段时要挪位置，静态组件运行时移动会报警，先改成可移动
            Rail->SetMobility(EComponentMobility::Movable);
            Rail->SetCastShadow(false);
            LineRails.Add(Rail);
        }
    }

    const int32 Index = Stage->RuleIndex;
    const bool bRow = Stage->RuleType == EXiaomaChestRuleType::Row;
    // 行号/列号每加一，世界 Y/X 反而减一格：用"宝箱格 - 目标格"算偏移，与 OffsetFromChest 同一套
    const FVector Center = bRow
        ? FVector((Def.Cell.Y - 2.5f) * MechCellStep, (Def.Cell.X - Index) * MechCellStep, MechHintLineZ)
        : FVector((Def.Cell.Y - Index) * MechCellStep, (Def.Cell.X - 2.5f) * MechCellStep, MechHintLineZ);
    const FVector LineScale = bRow
        ? FVector(MechCellStep * 6.f / 100.f, MechHintLineThickness, MechHintLineHeight)
        : FVector(MechHintLineThickness, MechCellStep * 6.f / 100.f, MechHintLineHeight);
    // 两条线贴在这一行/列的两侧边缘（±半格），把整条线夹在中间
    const FVector SideOffset = bRow
        ? FVector(0.f, MechCellStep * 0.5f, 0.f)
        : FVector(MechCellStep * 0.5f, 0.f, 0.f);

    for (int32 Side = 0; Side < LineRails.Num(); ++Side)
    {
        UStaticMeshComponent* Rail = LineRails[Side];
        if (!Rail)
        {
            continue;
        }
        Rail->SetRelativeLocation(Center + (Side == 0 ? SideOffset : -SideOffset));
        Rail->SetRelativeScale3D(LineScale);
        Rail->SetVisibility(true);
    }
}

void AXiaomaChest::RefreshVisuals()
{
    const bool bBuried = IsBuried();
    const bool bShowChest = !bClaimed && !bBuried;
    const bool bAllStagesDone = IsUnlocked();
    const FXiaomaChestStage* ActiveStage = GetActiveStage();

    if (GemMaterial)
    {
        FLinearColor GemColor = FLinearColor(FColor(140, 140, 140));
        if (bAllStagesDone && !bClaimed)
        {
            GemColor = FLinearColor(FColor(255, 215, 70));
        }
        else if (ActiveStage && ActiveStage->PuzzleType == EXiaomaChestPuzzleType::Rule
            && ActiveStage->RuleType == EXiaomaChestRuleType::Region
            && ActiveStage->RuleIndex >= 0 && ActiveStage->RuleIndex < 6)
        {
            GemColor = XiaomaPalette::Regions[ActiveStage->RuleIndex];
        }
        GemMaterial->SetVectorParameterValue(TEXT("Color"), GemColor);
    }
    if (BaseMesh)
    {
        BaseMesh->SetVisibility(bShowChest);
    }
    if (LidMesh)
    {
        LidMesh->SetRelativeLocation(bAllStagesDone ? FVector(0.f, 14.f, 42.f) : FVector(0.f, 0.f, 35.f));
        LidMesh->SetRelativeRotation(bAllStagesDone ? FRotator(0.f, 0.f, -40.f) : FRotator::ZeroRotator);
        LidMesh->SetVisibility(bShowChest);
    }
    if (GemMesh)
    {
        GemMesh->SetVisibility(bShowChest && !bClaimed);
    }

    // 逐段刷：顺序宝石只显示当前段；符文已做的整片亮、当前段按进度亮、后续段全暗
    for (int32 Index = 0; Index < Def.Stages.Num() && StageVisuals.IsValidIndex(Index); ++Index)
    {
        FXiaomaChestStageVisuals& V = Visuals(Index);
        const bool bIsActive = Index == StageIndex;
        for (const TObjectPtr<UStaticMeshComponent>& Gem : V.OrderGems)
        {
            if (Gem)
            {
                Gem->SetVisibility(bShowChest && bIsActive);
            }
        }
        RefreshRuneVisuals(Index);
        if (V.DigMarker)
        {
            // 埋藏点提示：石碑读过、还没挖出来，且正好轮到这个石碑段
            const bool bRead = StageProgress.IsValidIndex(Index) && StageProgress[Index] == 1;
            V.DigMarker->SetVisibility(bIsActive && bRead && !bClaimed);
        }
        if (V.SteleGlyph)
        {
            const bool bLit = Index <= StageIndex;
            V.SteleGlyph->SetRelativeScale3D(bLit ? FVector(0.09f, 0.09f, 0.13f) : FVector(0.06f));
        }
    }
    // 石碑本体一直留在场上，所以不能整只 Actor 隐藏，只能逐个部件控制
    SetActorHiddenInGame(false);
    RefreshLineHint();
}

void AXiaomaChest::SaveState(TArray<uint8>& Out) const
{
    // 格式：[版本 2][已完成段数][是否领取][每段进度...]
    // 首字节写 2 而不是 0/1，是为了和旧格式（首字节是 bUnlocked 的布尔值）区分开。
    Out.Reset();
    Out.Add(2);
    Out.Add(static_cast<uint8>(FMath::Clamp(StageIndex, 0, 255)));
    Out.Add(bClaimed ? 1 : 0);
    for (int32 Index = 0; Index < Def.Stages.Num(); ++Index)
    {
        Out.Add(StageProgress.IsValidIndex(Index) ? StageProgress[Index] : 0);
    }
}

void AXiaomaChest::LoadState(const TArray<uint8>& In)
{
    StageProgress.Init(0, Def.Stages.Num());
    if (In.Num() >= 3 && In[0] == 2)
    {
        StageIndex = FMath::Clamp<int32>(In[1], 0, Def.Stages.Num());
        bClaimed = In[2] != 0;
        for (int32 Index = 0; Index < Def.Stages.Num() && In.Num() > 3 + Index; ++Index)
        {
            StageProgress[Index] = In[3 + Index];
        }
    }
    else if (In.Num() >= 2)
    {
        // 旧格式（单段谜题）：指纹已经变了，正常不会走到这里，兜底按一段解释
        const bool bOldUnlocked = In[0] != 0;
        bClaimed = In[1] != 0;
        StageIndex = bOldUnlocked ? Def.Stages.Num() : 0;
        if (bOldUnlocked)
        {
            for (int32 Index = 0; Index < Def.Stages.Num(); ++Index)
            {
                StageProgress[Index] = static_cast<uint8>(MaxProgressForStage(Def.Stages[Index]));
            }
        }
        else if (Def.Stages.Num() > 0)
        {
            const FXiaomaChestStage& First = Def.Stages[0];
            if (First.PuzzleType == EXiaomaChestPuzzleType::Runes && In.Num() >= 3)
            {
                StageProgress[0] = In[2];
            }
            else if (First.PuzzleType == EXiaomaChestPuzzleType::Stele)
            {
                StageProgress[0] = (In.Num() >= 5 && In[4] != 0) ? 2 : ((In.Num() >= 4 && In[3] != 0) ? 1 : 0);
            }
        }
    }
    RefreshVisuals();
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

AXiaomaPushBox::AXiaomaPushBox()
{
    PrimaryActorTick.bCanEverTick = true;
    SceneRoot = CreateDefaultSubobject<USceneComponent>(TEXT("Root"));
    RootComponent = SceneRoot;

    BodyMesh = CreateDefaultSubobject<UStaticMeshComponent>(TEXT("BodyMesh"));
    BodyMesh->SetupAttachment(SceneRoot);
    BodyMesh->SetCollisionEnabled(ECollisionEnabled::NoCollision);

    TopMesh = CreateDefaultSubobject<UStaticMeshComponent>(TEXT("TopMesh"));
    TopMesh->SetupAttachment(SceneRoot);
    TopMesh->SetCollisionEnabled(ECollisionEnabled::NoCollision);
}

void AXiaomaPushBox::SetupBox(const FIntPoint& InCell, const FVector& WorldLocation)
{
    StartCell = InCell;
    SnapToCell(InCell, WorldLocation);

    UStaticMesh* Cube = LoadObject<UStaticMesh>(nullptr, TEXT("/Engine/BasicShapes/Cube.Cube"));
    UMaterialInterface* BaseMat = LoadObject<UMaterialInterface>(nullptr,
        TEXT("/Engine/BasicShapes/BasicShapeMaterial.BasicShapeMaterial"));
    if (!Cube || !BaseMat)
    {
        return;
    }

    // 边长 100 的立方体缩到 0.78 格宽、0.62 格高，底面正好贴在地板上
    BodyMesh->SetStaticMesh(Cube);
    BodyMesh->SetRelativeLocation(FVector(0.f, 0.f, 31.f));
    BodyMesh->SetRelativeScale3D(FVector(0.78f, 0.78f, 0.62f));

    TopMesh->SetStaticMesh(Cube);
    TopMesh->SetRelativeLocation(FVector(0.f, 0.f, 60.f));
    TopMesh->SetRelativeScale3D(FVector(0.84f, 0.84f, 0.08f));

    BodyMaterial = UMaterialInstanceDynamic::Create(BaseMat, this);
    BodyMaterial->SetVectorParameterValue(TEXT("Color"), FLinearColor(FColor(154, 110, 66)));
    BodyMesh->SetMaterial(0, BodyMaterial);

    TopMaterial = UMaterialInstanceDynamic::Create(BaseMat, this);
    TopMaterial->SetVectorParameterValue(TEXT("Color"), FLinearColor(FColor(112, 78, 46)));
    TopMesh->SetMaterial(0, TopMaterial);

    BodyMesh->SetCastShadow(true);
    TopMesh->SetCastShadow(true);
}

void AXiaomaPushBox::SnapToCell(const FIntPoint& InCell, const FVector& WorldLocation)
{
    BoxCell = InCell;
    SlideTarget = WorldLocation;
    bSliding = false;
    SetActorLocation(WorldLocation);
}

void AXiaomaPushBox::MoveToCell(const FIntPoint& InCell, const FVector& WorldLocation)
{
    BoxCell = InCell;
    SlideTarget = WorldLocation;
    bSliding = true;
}

void AXiaomaPushBox::Tick(float DeltaSeconds)
{
    Super::Tick(DeltaSeconds);
    if (!bSliding)
    {
        return;
    }
    const FVector Current = GetActorLocation();
    const FVector Next = FMath::VInterpTo(Current, SlideTarget, DeltaSeconds, 22.f);
    SetActorLocation(Next);
    if (FVector::DistSquared(Next, SlideTarget) < 1.f)
    {
        SetActorLocation(SlideTarget);
        bSliding = false;
    }
}

void AXiaomaPushBox::SaveState(TArray<uint8>& Out) const
{
    Out.Reset();
    if (BoxCell.X < 0 || BoxCell.Y < 0)
    {
        Out.Add(255);
        return;
    }
    Out.Add(static_cast<uint8>(BoxCell.X * FXiaomaPuzzle::Size + BoxCell.Y));
}

void AXiaomaPushBox::LoadState(const TArray<uint8>& In)
{
    if (In.Num() < 1 || In[0] >= FXiaomaPuzzle::CellCount)
    {
        return;
    }
    const int32 Index = In[0];
    const FIntPoint Cell(Index / FXiaomaPuzzle::Size, Index % FXiaomaPuzzle::Size);
    if (Cell == BoxCell)
    {
        return;
    }
    // 箱子自己不知道棋盘，用初始格与初始世界坐标推当前格的世界坐标：
    // 列号加一 X 减一格、行号加一 Y 减一格，与棋盘完全一致。
    const FVector World = SlideTarget + FVector(
        (BoxCell.Y - Cell.Y) * MechCellStep,
        (BoxCell.X - Cell.X) * MechCellStep,
        0.f);
    SnapToCell(Cell, World);
}
