#include "XiaomaGate.h"

#include "XiaomaBoardActor.h"
#include "XiaomaPalette.h"

#include "Components/SceneComponent.h"
#include "Components/StaticMeshComponent.h"
#include "Engine/StaticMesh.h"
#include "Materials/MaterialInstanceDynamic.h"
#include "Materials/MaterialInterface.h"

namespace
{
/** 格子边长 112，地板顶面 z≈8（与冰面、木桥同一套高度基准） */
constexpr float CellStep = 112.f;
constexpr float FloorTopZ = 8.f;

/** 横栏中心高度与截面：栏杆总高约 28，到主角膝盖以上，一眼看得出是"过不去" */
constexpr float RailZ = 22.f;
constexpr float RailThickness = 0.09f;
/** 立柱：插在地板里往上顶，撑住横栏 */
constexpr float PostHeight = 30.f;
constexpr float PostZ = FloorTopZ + PostHeight * 0.5f;

/** 开关踏板：底座贴地，按钮踩下去要明显下沉 */
constexpr float PadZ = FloorTopZ + 3.f;
constexpr float ButtonUpZ = FloorTopZ + 10.f;
constexpr float ButtonDownZ = FloorTopZ + 5.5f;

FColor Shade(const FColor& Colour, float Factor)
{
    return FColor(
        static_cast<uint8>(FMath::Clamp(Colour.R * Factor, 0.f, 255.f)),
        static_cast<uint8>(FMath::Clamp(Colour.G * Factor, 0.f, 255.f)),
        static_cast<uint8>(FMath::Clamp(Colour.B * Factor, 0.f, 255.f)));
}
}

AXiaomaGate::AXiaomaGate()
{
    PrimaryActorTick.bCanEverTick = false;
    SceneRoot = CreateDefaultSubobject<USceneComponent>(TEXT("Root"));
    RootComponent = SceneRoot;
}

UStaticMeshComponent* AXiaomaGate::AddPart(const TCHAR* BaseName, UStaticMesh* Mesh,
    const FVector& Location, const FVector& Scale, const FColor& Colour,
    TObjectPtr<UMaterialInstanceDynamic>* OutMaterial)
{
    // 同一种部件会有很多个，名字必须唯一，否则 RegisterComponent 会断言
    const FName UniqueName = MakeUniqueObjectName(this, UStaticMeshComponent::StaticClass(), FName(BaseName));
    UStaticMeshComponent* Comp = NewObject<UStaticMeshComponent>(this, UniqueName);
    Comp->SetupAttachment(SceneRoot);
    Comp->SetStaticMesh(Mesh);
    Comp->SetRelativeLocation(Location);
    Comp->SetRelativeScale3D(Scale);
    // 阻挡由 BlocksCell 负责，网格不参与碰撞：否则主角会卡在栏杆的物理体上
    Comp->SetCollisionEnabled(ECollisionEnabled::NoCollision);
    Comp->SetCastShadow(false);

    UMaterialInterface* BaseMat = LoadObject<UMaterialInterface>(nullptr,
        TEXT("/Engine/BasicShapes/BasicShapeMaterial.BasicShapeMaterial"));
    if (BaseMat)
    {
        UMaterialInstanceDynamic* Mat = UMaterialInstanceDynamic::Create(BaseMat, this);
        Mat->SetVectorParameterValue(TEXT("Color"), FLinearColor(Colour));
        Comp->SetMaterial(0, Mat);
        if (OutMaterial)
        {
            *OutMaterial = Mat;
        }
    }
    Comp->RegisterComponent();
    AddInstanceComponent(Comp);
    return Comp;
}

void AXiaomaGate::SetupGate(const FXiaomaGateDef& InDef, const TArray<FIntPoint>& InRegionCells,
    const AXiaomaBoardActor* InBoard)
{
    Region = InDef.Region;
    SwitchCell = InDef.SwitchCell;
    RegionCells.Reset();
    for (const FIntPoint& Cell : InRegionCells)
    {
        RegionCells.Add(Cell);
    }
    bOpen = false;

    if (!InBoard || RegionCells.Num() == 0)
    {
        return;
    }

    UStaticMesh* Cube = LoadObject<UStaticMesh>(nullptr, TEXT("/Engine/BasicShapes/Cube.Cube"));
    UStaticMesh* Cylinder = LoadObject<UStaticMesh>(nullptr, TEXT("/Engine/BasicShapes/Cylinder.Cylinder"));
    if (!Cube || !Cylinder)
    {
        return;
    }

    // 整道围栏跟着棋盘走，各段栏杆直接用"格心世界坐标 - 棋盘原点"当相对位置
    SetActorLocation(InBoard->GetActorLocation());

    const FColor RegionColour = Region >= 0 && Region < UE_ARRAY_COUNT(XiaomaPalette::Regions)
        ? XiaomaPalette::Regions[Region].ToFColor(true)
        : FColor(150, 150, 150);
    // 木头压暗一点，免得和同色地砖糊成一片
    const FColor Wood = Shade(RegionColour, 0.72f);
    const FColor WoodDark = Shade(RegionColour, 0.54f);

    // 区域边界 = 邻居不在本区域的每一条边（棋盘外也算区域外）
    const FIntPoint EdgeDirs[] = {
        FIntPoint(-1, 0), FIntPoint(1, 0), FIntPoint(0, -1), FIntPoint(0, 1)
    };
    for (const FIntPoint& Cell : RegionCells)
    {
        const FVector Center = InBoard->GetCellWorldLocation(Cell) - GetActorLocation();
        for (const FIntPoint& Dir : EdgeDirs)
        {
            if (RegionCells.Contains(FIntPoint(Cell.X + Dir.X, Cell.Y + Dir.Y)))
            {
                continue;
            }
            // 列号 +1 → 世界 X -112；行号 +1 → 世界 Y -112，所以偏移方向要取反
            FVector Offset;
            FVector RailScale;
            if (Dir.X != 0)
            {
                Offset = FVector(0.f, Dir.X > 0 ? -CellStep * 0.5f : CellStep * 0.5f, 0.f);
                RailScale = FVector(CellStep / 100.f, RailThickness, RailThickness);
            }
            else
            {
                Offset = FVector(Dir.Y > 0 ? -CellStep * 0.5f : CellStep * 0.5f, 0.f, 0.f);
                RailScale = FVector(RailThickness, CellStep / 100.f, RailThickness);
            }

            RailMeshes.Add(AddPart(TEXT("GateRail"), Cube, Center + Offset + FVector(0.f, 0.f, RailZ),
                RailScale, Wood));
            RailMeshes.Add(AddPart(TEXT("GatePost"), Cube, Center + Offset + FVector(0.f, 0.f, PostZ),
                FVector(0.10f, 0.10f, PostHeight / 100.f), WoodDark));
        }
    }

    // 开关：贴地底座 + 一颗可以踩下去的圆柱按钮，颜色就是它守着的色区
    SwitchPadCenter = InBoard->GetCellWorldLocation(SwitchCell) - GetActorLocation();
    SwitchColour = RegionColour;
    SwitchPad = AddPart(TEXT("GatePad"), Cube, SwitchPadCenter + FVector(0.f, 0.f, PadZ),
        FVector(0.86f, 0.86f, 0.06f), Wood);
    SwitchButton = AddPart(TEXT("GateButton"), Cylinder,
        SwitchPadCenter + FVector(0.f, 0.f, ButtonUpZ),
        FVector(0.56f, 0.56f, 0.10f), RegionColour, &SwitchMaterial);

    RefreshVisuals();
}

bool AXiaomaGate::BlocksCell(FIntPoint Cell) const
{
    return !bOpen && RegionCells.Contains(Cell);
}

void AXiaomaGate::OnHerdsmanEnterCell(FIntPoint Cell)
{
    if (bOpen || Cell != SwitchCell)
    {
        return;
    }
    // 先广播再开门：这一刻门还是关着的，调用方压下的快照才能把门复原
    OnOpened.ExecuteIfBound(this);
    Open();
}

void AXiaomaGate::Open()
{
    if (bOpen)
    {
        return;
    }
    bOpen = true;
    RefreshVisuals();
}

void AXiaomaGate::ForceOpen()
{
    Open();
}

void AXiaomaGate::SaveState(TArray<uint8>& Out) const
{
    Out.Reset();
    Out.Add(bOpen ? 1 : 0);
}

void AXiaomaGate::LoadState(const TArray<uint8>& In)
{
    bOpen = In.Num() > 0 && In[0] != 0;
    RefreshVisuals();
}

void AXiaomaGate::RefreshVisuals()
{
    // 开门就把栏杆藏起来：矮栏杆已经"落进地里"，只剩踏板变暗作为走过的痕迹
    for (UStaticMeshComponent* Rail : RailMeshes)
    {
        if (Rail)
        {
            Rail->SetVisibility(!bOpen);
        }
    }
    if (SwitchButton)
    {
        SwitchButton->SetRelativeLocation(SwitchPadCenter
            + FVector(0.f, 0.f, bOpen ? ButtonDownZ : ButtonUpZ));
    }
    if (SwitchMaterial)
    {
        // 未踩时是鲜亮的色区色；踩下去压暗，和下沉的按钮一起表示"这道门已经用掉了"
        SwitchMaterial->SetVectorParameterValue(TEXT("Color"),
            FLinearColor(bOpen ? Shade(SwitchColour, 0.55f) : SwitchColour));
    }
}
