#include "XiaomaBridge.h"

#include "Components/SceneComponent.h"
#include "Components/StaticMeshComponent.h"
#include "Engine/StaticMesh.h"
#include "Materials/MaterialInstanceDynamic.h"
#include "Materials/MaterialInterface.h"

namespace
{
/** 地板顶面在 z≈8；木板顶面做到 13，和冰面同高，不会把主角的脚埋进去 */
constexpr float PlankCenterZ = 10.f;
constexpr float PlankThickness = 0.06f;
constexpr float PitZ = 8.6f;

const FColor PlankWood(146, 96, 52);
const FColor PlankTrim(112, 72, 38);
const FColor PitDark(38, 31, 24);
const FColor PitInner(18, 14, 11);
}

AXiaomaBridge::AXiaomaBridge()
{
    PrimaryActorTick.bCanEverTick = false;
    SceneRoot = CreateDefaultSubobject<USceneComponent>(TEXT("Root"));
    RootComponent = SceneRoot;

    // 完好的桥：三条横板，板间留缝，远看像一座小木桥
    const TCHAR* PlankNames[] = { TEXT("PlankA"), TEXT("PlankB"), TEXT("PlankC") };
    for (const TCHAR* PlankName : PlankNames)
    {
        UStaticMeshComponent* Plank = CreateDefaultSubobject<UStaticMeshComponent>(PlankName);
        Plank->SetupAttachment(SceneRoot);
        Plank->SetCollisionEnabled(ECollisionEnabled::NoCollision);
        PlankMeshes.Add(Plank);
    }

    // 塌陷后的坑：外圈深色 + 内圈更暗，制造一点深度的错觉（不做真凹面）
    UStaticMeshComponent* PitOuter = CreateDefaultSubobject<UStaticMeshComponent>(TEXT("PitOuter"));
    PitOuter->SetupAttachment(SceneRoot);
    PitOuter->SetCollisionEnabled(ECollisionEnabled::NoCollision);
    PitOuter->SetCastShadow(false);
    PitMeshes.Add(PitOuter);

    UStaticMeshComponent* PitInnerComp = CreateDefaultSubobject<UStaticMeshComponent>(TEXT("PitInner"));
    PitInnerComp->SetupAttachment(SceneRoot);
    PitInnerComp->SetCollisionEnabled(ECollisionEnabled::NoCollision);
    PitInnerComp->SetCastShadow(false);
    PitMeshes.Add(PitInnerComp);
}

void AXiaomaBridge::AddPart(const TCHAR* BaseName, UStaticMeshComponent*& OutComp, const FVector& Location,
    const FVector& Scale, const FColor& Colour, bool bCastShadow)
{
    OutComp = NewObject<UStaticMeshComponent>(this,
        MakeUniqueObjectName(this, UStaticMeshComponent::StaticClass(), FName(BaseName)));
    OutComp->SetupAttachment(SceneRoot);
    OutComp->SetRelativeLocation(Location);
    OutComp->SetRelativeScale3D(Scale);
    OutComp->SetCollisionEnabled(ECollisionEnabled::NoCollision);
    OutComp->SetCastShadow(bCastShadow);

    UMaterialInterface* BaseMat = LoadObject<UMaterialInterface>(nullptr,
        TEXT("/Engine/BasicShapes/BasicShapeMaterial.BasicShapeMaterial"));
    if (BaseMat)
    {
        UMaterialInstanceDynamic* Mat = UMaterialInstanceDynamic::Create(BaseMat, this);
        Mat->SetVectorParameterValue(TEXT("Color"), FLinearColor(Colour));
        OutComp->SetMaterial(0, Mat);
    }
    OutComp->RegisterComponent();
    AddInstanceComponent(OutComp);
}

void AXiaomaBridge::SetupBridge(const FIntPoint& InCell, const FVector& WorldLocation,
    const AXiaomaBoardActor* InBoard, bool bAlongColumns)
{
    BridgeCell = InCell;
    ApproachCell = InCell;
    bRunsAlongColumns = bAlongColumns;
    SetActorLocation(WorldLocation);

    UStaticMesh* Cube = LoadObject<UStaticMesh>(nullptr, TEXT("/Engine/BasicShapes/Cube.Cube"));
    UStaticMesh* Cylinder = LoadObject<UStaticMesh>(nullptr, TEXT("/Engine/BasicShapes/Cylinder.Cylinder"));
    if (!Cube || !Cylinder)
    {
        return;
    }

    // 板条垂直于行进方向：桥沿列铺（东西向）时板条长边朝南北
    const bool bSlatsLongInY = bRunsAlongColumns;
    for (int32 Index = 0; Index < PlankMeshes.Num(); ++Index)
    {
        UStaticMeshComponent* Plank = PlankMeshes[Index];
        if (!Plank)
        {
            continue;
        }
        const float Offset = (Index - 1) * 30.f;
        const FVector Location = bSlatsLongInY
            ? FVector(Offset, 0.f, PlankCenterZ)
            : FVector(0.f, Offset, PlankCenterZ);
        const FVector Scale = bSlatsLongInY
            ? FVector(0.24f, 0.92f, PlankThickness)
            : FVector(0.92f, 0.24f, PlankThickness);
        Plank->SetStaticMesh(Cube);
        Plank->SetRelativeLocation(Location);
        Plank->SetRelativeScale3D(Scale);

        UMaterialInterface* BaseMat = LoadObject<UMaterialInterface>(nullptr,
            TEXT("/Engine/BasicShapes/BasicShapeMaterial.BasicShapeMaterial"));
        if (BaseMat)
        {
            UMaterialInstanceDynamic* Mat = UMaterialInstanceDynamic::Create(BaseMat, this);
            Mat->SetVectorParameterValue(TEXT("Color"),
                FLinearColor(Index == 1 ? PlankTrim : PlankWood));
            Plank->SetMaterial(0, Mat);
        }
    }

    // 坑洞：两块贴地圆盘，外圈浅一点、内圈接近全黑
    if (PitMeshes.Num() == 2 && PitMeshes[0] && PitMeshes[1])
    {
        PitMeshes[0]->SetStaticMesh(Cylinder);
        PitMeshes[0]->SetRelativeLocation(FVector(0.f, 0.f, PitZ));
        PitMeshes[0]->SetRelativeScale3D(FVector(0.94f, 0.94f, 0.012f));
        PitMeshes[1]->SetStaticMesh(Cylinder);
        PitMeshes[1]->SetRelativeLocation(FVector(0.f, 0.f, PitZ + 0.8f));
        PitMeshes[1]->SetRelativeScale3D(FVector(0.78f, 0.78f, 0.012f));

        UMaterialInterface* BaseMat = LoadObject<UMaterialInterface>(nullptr,
            TEXT("/Engine/BasicShapes/BasicShapeMaterial.BasicShapeMaterial"));
        if (BaseMat)
        {
            UMaterialInstanceDynamic* OuterMat = UMaterialInstanceDynamic::Create(BaseMat, this);
            OuterMat->SetVectorParameterValue(TEXT("Color"), FLinearColor(PitDark));
            PitMeshes[0]->SetMaterial(0, OuterMat);
            UMaterialInstanceDynamic* InnerMat = UMaterialInstanceDynamic::Create(BaseMat, this);
            InnerMat->SetVectorParameterValue(TEXT("Color"), FLinearColor(PitInner));
            PitMeshes[1]->SetMaterial(0, InnerMat);
        }
    }

    bCollapsed = false;
    bHerdsmanOn = false;
    RefreshVisuals();
}

bool AXiaomaBridge::BlocksCell(FIntPoint Cell) const
{
    return bCollapsed && Cell == BridgeCell;
}

void AXiaomaBridge::OnHerdsmanEnterCell(FIntPoint Cell)
{
    if (Cell == BridgeCell)
    {
        if (!bCollapsed && !bHerdsmanOn)
        {
            // 这一刻桥还完好：先压撤销快照，等主角离开才真塌，所以按 Z 能回来
            bHerdsmanOn = true;
            OnStepped.ExecuteIfBound(this);
        }
        return;
    }
    if (bHerdsmanOn)
    {
        bHerdsmanOn = false;
        if (!bCollapsed)
        {
            // 先广播再塌：这一刻桥还是完好的，调用方压下的快照才能把桥复原
            OnCollapsing.ExecuteIfBound(this);
            Collapse();
        }
    }
}

void AXiaomaBridge::SyncHerdsmanCell(FIntPoint Cell)
{
    // 只同步"人在不在桥上"，不触发塌陷：撤销回到桥上时桥应该还在
    bHerdsmanOn = !bCollapsed && Cell == BridgeCell;
}

void AXiaomaBridge::Collapse()
{
    if (bCollapsed)
    {
        return;
    }
    bCollapsed = true;
    RefreshVisuals();
}

void AXiaomaBridge::SaveState(TArray<uint8>& Out) const
{
    Out.Reset();
    Out.Add(bCollapsed ? 1 : 0);
}

void AXiaomaBridge::LoadState(const TArray<uint8>& In)
{
    bCollapsed = In.Num() > 0 && In[0] != 0;
    bHerdsmanOn = false;
    RefreshVisuals();
}

void AXiaomaBridge::RefreshVisuals()
{
    for (UStaticMeshComponent* Plank : PlankMeshes)
    {
        if (Plank)
        {
            Plank->SetVisibility(!bCollapsed);
        }
    }
    for (UStaticMeshComponent* Pit : PitMeshes)
    {
        if (Pit)
        {
            Pit->SetVisibility(bCollapsed);
        }
    }
}
