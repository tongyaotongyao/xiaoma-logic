#include "XiaomaIceField.h"
#include "XiaomaBoardActor.h"

#include "Components/SceneComponent.h"
#include "Components/StaticMeshComponent.h"
#include "Engine/StaticMesh.h"
#include "Materials/MaterialInstanceDynamic.h"
#include "Materials/MaterialInterface.h"

namespace
{
/** 地板顶面在 z≈8，格子边长 112 */
constexpr float IceCellStep = 112.f;
/** 冰面比地板高一点点：太厚会把小马和主角的脚埋进去 */
constexpr float IceTopZ = 13.f;
constexpr float IceThickness = 5.f;
constexpr float IceCenterZ = IceTopZ - IceThickness * 0.5f;

const FColor IceBlue(152, 206, 232);
const FColor IceSheen(226, 246, 253);
}

AXiaomaIceField::AXiaomaIceField()
{
    PrimaryActorTick.bCanEverTick = false;
    SceneRoot = CreateDefaultSubobject<USceneComponent>(TEXT("Root"));
    RootComponent = SceneRoot;
}

UStaticMeshComponent* AXiaomaIceField::AddPart(const TCHAR* BaseName, UStaticMesh* Mesh,
    const FVector& Location, const FVector& Scale, const FColor& Colour)
{
    // 运行时建件：同一种部件会有很多个，名字必须唯一，否则 RegisterComponent 会断言
    const FName UniqueName = MakeUniqueObjectName(this, UStaticMeshComponent::StaticClass(), FName(BaseName));
    UStaticMeshComponent* Comp = NewObject<UStaticMeshComponent>(this, UniqueName);
    Comp->SetupAttachment(SceneRoot);
    Comp->SetStaticMesh(Mesh);
    Comp->SetRelativeLocation(Location);
    Comp->SetRelativeScale3D(Scale);
    // 冰面只是装饰：不挡主角、不挡鼠标射线、不投影
    Comp->SetCollisionEnabled(ECollisionEnabled::NoCollision);
    Comp->SetCastShadow(false);

    UMaterialInterface* BaseMat = LoadObject<UMaterialInterface>(nullptr,
        TEXT("/Engine/BasicShapes/BasicShapeMaterial.BasicShapeMaterial"));
    if (BaseMat)
    {
        UMaterialInstanceDynamic* Mat = UMaterialInstanceDynamic::Create(BaseMat, this);
        Mat->SetVectorParameterValue(TEXT("Color"), FLinearColor(Colour));
        Comp->SetMaterial(0, Mat);
    }
    Comp->RegisterComponent();
    AddInstanceComponent(Comp);
    Parts.Add(Comp);
    return Comp;
}

bool AXiaomaIceField::HasIceAt(const TArray<FIntPoint>& Cells, FIntPoint Cell) const
{
    return Cells.Contains(Cell);
}

void AXiaomaIceField::SetupIce(const TArray<FIntPoint>& Cells, const AXiaomaBoardActor* InBoard)
{
    if (!InBoard || Cells.Num() == 0)
    {
        return;
    }
    UStaticMesh* Cylinder = LoadObject<UStaticMesh>(nullptr, TEXT("/Engine/BasicShapes/Cylinder.Cylinder"));
    UStaticMesh* Cube = LoadObject<UStaticMesh>(nullptr, TEXT("/Engine/BasicShapes/Cube.Cube"));
    if (!Cylinder || !Cube)
    {
        return;
    }

    // 整块冰跟着棋盘走，这样每格的位置直接用相对坐标算，不必再逐格减世界原点
    SetActorLocation(InBoard->GetActorLocation());

    for (const FIntPoint& Cell : Cells)
    {
        const FVector Center = InBoard->GetCellWorldLocation(Cell) - GetActorLocation()
            + FVector(0.f, 0.f, IceCenterZ);

        // 主冰面：略小于一格的圆盘，四角露出地板，读得出"这是一块冰"而不是新地板
        AddPart(TEXT("IceDisc"), Cylinder, Center,
            FVector(0.94f, 0.94f, IceThickness / 100.f), IceBlue);
        // 中心高光：一层更亮更薄的圆片，让冰面看起来是透的
        AddPart(TEXT("IceSheen"), Cylinder, Center + FVector(0.f, 0.f, IceThickness * 0.45f),
            FVector(0.46f, 0.46f, 0.012f), IceSheen);

        // 相邻冰格之间补一条细冰条，把圆盘连成一条完整的冰道
        // （列号 +1 → 世界 X -112；行号 +1 → 世界 Y -112）
        if (HasIceAt(Cells, FIntPoint(Cell.X, Cell.Y + 1)))
        {
            AddPart(TEXT("IceLinkX"), Cube, Center + FVector(-IceCellStep * 0.5f, 0.f, 0.f),
                FVector(IceCellStep / 100.f, 0.34f, IceThickness / 100.f), IceBlue);
        }
        if (HasIceAt(Cells, FIntPoint(Cell.X + 1, Cell.Y)))
        {
            AddPart(TEXT("IceLinkY"), Cube, Center + FVector(0.f, -IceCellStep * 0.5f, 0.f),
                FVector(0.34f, IceCellStep / 100.f, IceThickness / 100.f), IceBlue);
        }
    }
}
