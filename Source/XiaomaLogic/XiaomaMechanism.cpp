#include "XiaomaMechanism.h"
#include "XiaomaPalette.h"

#include "Components/SceneComponent.h"
#include "Components/StaticMeshComponent.h"
#include "Engine/StaticMesh.h"
#include "Materials/MaterialInstanceDynamic.h"
#include "Materials/MaterialInterface.h"

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
    if (Cell == Def.Cell && bUnlocked && !bClaimed)
    {
        TryClaim();
    }
}

bool AXiaomaChest::TryInteract(FIntPoint Cell)
{
    if (Cell != Def.Cell || bClaimed)
    {
        return false;
    }
    if (bUnlocked)
    {
        TryClaim();
        return true;
    }
    // 未解锁时反馈条件，仍消费一次 E 交互避免弹轮盘遮挡提示
    return true;
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
    if (GemMaterial)
    {
        FLinearColor GemColor = FLinearColor(FColor(140, 140, 140));
        if (bUnlocked && !bClaimed)
        {
            GemColor = FLinearColor(FColor(255, 215, 70));
        }
        else if (Def.RuleType == EXiaomaChestRuleType::Region && Def.RuleIndex >= 0 && Def.RuleIndex < 6)
        {
            GemColor = XiaomaPalette::Regions[Def.RuleIndex];
        }
        GemMaterial->SetVectorParameterValue(TEXT("Color"), GemColor);
    }
    if (LidMesh)
    {
        LidMesh->SetRelativeLocation(bUnlocked ? FVector(0.f, 14.f, 42.f) : FVector(0.f, 0.f, 35.f));
        LidMesh->SetRelativeRotation(bUnlocked ? FRotator(0.f, 0.f, -40.f) : FRotator::ZeroRotator);
    }
    if (GemMesh)
    {
        GemMesh->SetVisibility(!bClaimed);
    }
    SetActorHiddenInGame(bClaimed);
}

void AXiaomaChest::SaveState(TArray<uint8>& Out) const
{
    Out.Reset();
    Out.Add(bUnlocked ? 1 : 0);
    Out.Add(bClaimed ? 1 : 0);
}

void AXiaomaChest::LoadState(const TArray<uint8>& In)
{
    if (In.Num() >= 2)
    {
        bUnlocked = In[0] != 0;
        bClaimed = In[1] != 0;
        RefreshVisuals();
    }
}
