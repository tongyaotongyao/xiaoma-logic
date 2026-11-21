#include "XiaomaBoardActor.h"
#include "XiaomaPalette.h"

#include "Components/SceneComponent.h"
#include "Components/StaticMeshComponent.h"
#include "Materials/MaterialInstanceDynamic.h"
#include "Materials/MaterialInterface.h"
#include "Engine/StaticMesh.h"

namespace
{
constexpr float HopDuration = 0.38f;
constexpr float TravelDuration = 0.38f;
constexpr float RejectDuration = 0.45f;
constexpr float FlashDuration = 0.28f;
const TCHAR* PonyAssetPaths[6] = {
    TEXT("/Game/Ponies/Pony_Blue.Pony_Blue"),
    TEXT("/Game/Ponies/Pony_Brown.Pony_Brown"),
    TEXT("/Game/Ponies/Pony_Pink.Pony_Pink"),
    TEXT("/Game/Ponies/Pony_Red.Pony_Red"),
    TEXT("/Game/Ponies/Pony_Yellow.Pony_Yellow"),
    TEXT("/Game/Ponies/Pony_Mint.Pony_Mint")
};
}

AXiaomaBoardActor::AXiaomaBoardActor()
{
    PrimaryActorTick.bCanEverTick = true;
    PrimaryActorTick.bStartWithTickEnabled = false;
    SceneRoot = CreateDefaultSubobject<USceneComponent>(TEXT("Root"));
    RootComponent = SceneRoot;
}

FVector AXiaomaBoardActor::GetStandbyLocation(int32 ColourIndex) const
{
    return FVector(30.f + ColourIndex * 100.f, 815.f, 10.f);
}

FVector AXiaomaBoardActor::GetCellWorldLocation(FIntPoint Cell) const
{
    const int32 Row = FMath::Clamp(Cell.X, 0, FXiaomaPuzzle::Size - 1);
    const int32 Column = FMath::Clamp(Cell.Y, 0, FXiaomaPuzzle::Size - 1);
    return GetActorLocation() + FVector((FXiaomaPuzzle::Size - 1 - Column) * 112.f,
        (FXiaomaPuzzle::Size - 1 - Row) * 112.f, 0.f);
}

void AXiaomaBoardActor::BuildBoard(const FXiaomaLevel& Level)
{
    if (Tiles.Num() > 0)
    {
        return;
    }
    UStaticMesh* Cube = LoadObject<UStaticMesh>(nullptr, TEXT("/Engine/BasicShapes/Cube.Cube"));
    UStaticMesh* RoundedTile = LoadObject<UStaticMesh>(nullptr, TEXT("/Game/Board/Tile.Tile"));
    UStaticMesh* Sphere = LoadObject<UStaticMesh>(nullptr, TEXT("/Engine/BasicShapes/Sphere.Sphere"));
    UMaterialInterface* BaseMaterial = LoadObject<UMaterialInterface>(nullptr,
        TEXT("/Engine/BasicShapes/BasicShapeMaterial.BasicShapeMaterial"));
    if (!Cube || !Sphere || !BaseMaterial)
    {
        UE_LOG(LogTemp, Error, TEXT("Could not load engine basic meshes/material"));
        return;
    }
    FallbackPonyMesh = Sphere;
    MarkMaterial = UMaterialInstanceDynamic::Create(BaseMaterial, this);
    MarkMaterial->SetVectorParameterValue(TEXT("Color"), FLinearColor(FColor(71, 65, 59)));

    for (int32 Index = 0; Index < FXiaomaPuzzle::CellCount; ++Index)
    {
        const int32 Row = Index / FXiaomaPuzzle::Size;
        const int32 Column = Index % FXiaomaPuzzle::Size;
        const FVector Position((FXiaomaPuzzle::Size - 1 - Column) * 112.f,
            (FXiaomaPuzzle::Size - 1 - Row) * 112.f, 0.f);

        UStaticMeshComponent* Tile = NewObject<UStaticMeshComponent>(this);
        Tile->SetStaticMesh(RoundedTile ? RoundedTile : Cube);
        Tile->SetupAttachment(SceneRoot);
        Tile->SetRelativeLocation(Position);
        Tile->SetRelativeScale3D(FVector(1.075f, 1.075f, 0.15f));
        Tile->SetCastShadow(false);
        Tile->SetCollisionEnabled(ECollisionEnabled::QueryOnly);
        Tile->SetCollisionResponseToAllChannels(ECR_Ignore);
        Tile->SetCollisionResponseToChannel(ECC_Visibility, ECR_Block);
        Tile->RegisterComponent();
        AddInstanceComponent(Tile);
        Tiles.Add(Tile);

        UMaterialInstanceDynamic* Material = UMaterialInstanceDynamic::Create(BaseMaterial, this);
        Material->SetVectorParameterValue(TEXT("Color"), XiaomaPalette::Regions[Level.Regions[Index]]);
        Tile->SetMaterial(0, Material);
        TileMaterials.Add(Material);

        UStaticMeshComponent* Pony = NewObject<UStaticMeshComponent>(this);
        UStaticMesh* Character = LoadObject<UStaticMesh>(nullptr,
            PonyAssetPaths[Level.Regions[Index]]);
        Pony->SetStaticMesh(Character ? Character : Sphere);
        Pony->SetupAttachment(SceneRoot);
        Pony->SetRelativeLocation(Position + FVector(0.f, 0.f, Character ? 10.f : 52.f));
        Pony->SetRelativeScale3D(Character ? FVector(0.45f) : FVector(0.36f));
        Pony->SetRelativeRotation(FRotator(0.f, 135.f, 0.f));
        Pony->SetCollisionEnabled(ECollisionEnabled::QueryOnly);
        Pony->SetCollisionResponseToAllChannels(ECR_Ignore);
        Pony->SetCollisionResponseToChannel(ECC_Visibility, ECR_Block);
        Pony->RegisterComponent();
        AddInstanceComponent(Pony);
        Pony->SetVisibility(false);
        Ponies.Add(Pony);

        for (const float Yaw : {45.f, -45.f})
        {
            UStaticMeshComponent* Stroke = NewObject<UStaticMeshComponent>(this);
            Stroke->SetStaticMesh(Cube);
            Stroke->SetupAttachment(SceneRoot);
            Stroke->SetRelativeLocation(Position + FVector(0.f, 0.f, 15.f));
            Stroke->SetRelativeRotation(FRotator(0.f, Yaw, 0.f));
            Stroke->SetRelativeScale3D(FVector(0.58f, 0.07f, 0.045f));
            Stroke->SetMaterial(0, MarkMaterial);
            Stroke->SetCollisionEnabled(ECollisionEnabled::NoCollision);
            Stroke->SetCastShadow(false);
            Stroke->RegisterComponent();
            AddInstanceComponent(Stroke);
            Stroke->SetVisibility(false);
            if (Yaw > 0.f)
            {
                MarkStrokesA.Add(Stroke);
            }
            else
            {
                MarkStrokesB.Add(Stroke);
            }
        }
    }

    // 在棋盘上侧木栅栏后生成 6 匹待命区小马
    for (int32 Colour = 0; Colour < 6; ++Colour)
    {
        UStaticMeshComponent* Standby = NewObject<UStaticMeshComponent>(this);
        UStaticMesh* Character = LoadObject<UStaticMesh>(nullptr, PonyAssetPaths[Colour]);
        Standby->SetStaticMesh(Character ? Character : Sphere);
        Standby->SetupAttachment(SceneRoot);
        Standby->SetRelativeLocation(GetStandbyLocation(Colour));
        Standby->SetRelativeScale3D(Character ? FVector(0.48f) : FVector(0.38f));
        Standby->SetRelativeRotation(FRotator(0.f, -90.f, 0.f));
        Standby->SetCollisionEnabled(ECollisionEnabled::QueryOnly);
        Standby->SetCollisionResponseToAllChannels(ECR_Ignore);
        Standby->SetCollisionResponseToChannel(ECC_Visibility, ECR_Block);
        Standby->RegisterComponent();
        AddInstanceComponent(Standby);
        StandbyPonies.Add(Standby);
    }

    HopTime.Init(0.f, Tiles.Num());
    PonyRestPositions.SetNum(Tiles.Num());
    TileColours.SetNum(Tiles.Num());
    RegionAtCell = Level.Regions;

    StandbyTravelTime.Init(0.f, 6);
    StandbyStartPos.SetNum(6);
    StandbyEndPos.SetNum(6);
    StandbyTargetVisible.Init(true, 6);
    StandbyRejectTime.Init(0.f, 6);
    StandbyHopTime.Init(0.f, 6);
}

void AXiaomaBoardActor::RefreshBoard(const FXiaomaLevel& Level, const TArray<uint8>& Cells)
{
    if (Cells.Num() != FXiaomaPuzzle::CellCount || Tiles.Num() != FXiaomaPuzzle::CellCount)
    {
        return;
    }
    RegionAtCell = Level.Regions;

    TArray<int32> PlacedCellByRegion;
    PlacedCellByRegion.Init(INDEX_NONE, 6);

    for (int32 Index = 0; Index < Cells.Num(); ++Index)
    {
        const int32 Colour = Level.Regions[Index];
        const bool bGiven = Level.IsGiven(Index / 6, Index % 6);
        FLinearColor TileColor = XiaomaPalette::Regions[Colour];
        if (bGiven)
        {
            TileColor = FMath::Lerp(TileColor, FLinearColor::White, 0.08f);
        }
        else if (Cells[Index] == 1)
        {
            TileColor *= 0.65f;
        }
        TileColours[Index] = TileColor;
        if (Index == HighlightedCell)
        {
            TileColor = FMath::Lerp(TileColor, FLinearColor::White, 0.28f);
        }
        TileMaterials[Index]->SetVectorParameterValue(TEXT("Color"), TileColor);

        const FVector BasePos = Tiles[Index]->GetRelativeLocation();
        Tiles[Index]->SetRelativeLocation(FVector(BasePos.X, BasePos.Y, Index == HighlightedCell ? 5.f : 0.f));

        UStaticMesh* Character = LoadObject<UStaticMesh>(nullptr, PonyAssetPaths[Colour]);
        Ponies[Index]->SetStaticMesh(Character ? Character : FallbackPonyMesh.Get());
        PonyRestPositions[Index] = FVector(BasePos.X, BasePos.Y, 0.f)
            + FVector(0.f, 0.f, Character ? 10.f : 52.f);
        Ponies[Index]->SetRelativeLocation(PonyRestPositions[Index]);
        Ponies[Index]->SetRelativeScale3D(Character ? FVector(0.45f) : FVector(0.36f));

        const bool bShowPony = Cells[Index] == 2;
        if (bShowPony)
        {
            PlacedCellByRegion[Colour] = Index;
        }
        if (bShowPony && !bGiven && !Ponies[Index]->IsVisible())
        {
            HopTime[Index] = HopDuration;
            SetActorTickEnabled(true);
        }
        else if (!bShowPony)
        {
            HopTime[Index] = 0.f;
        }
        Ponies[Index]->SetVisibility(bShowPony);
        MarkStrokesA[Index]->SetVisibility(Cells[Index] == 1);
        MarkStrokesB[Index]->SetVisibility(Cells[Index] == 1);
    }

    // 同步待命区 6 匹小马的站位与飞入/请回动画
    for (int32 Colour = 0; Colour < 6 && Colour < StandbyPonies.Num(); ++Colour)
    {
        const bool bShouldStandby = (PlacedCellByRegion[Colour] == INDEX_NONE);
        if (bShouldStandby != StandbyTargetVisible[Colour])
        {
            StandbyTargetVisible[Colour] = bShouldStandby;
            StandbyTravelTime[Colour] = TravelDuration;
            if (!bShouldStandby)
            {
                StandbyStartPos[Colour] = GetStandbyLocation(Colour);
                StandbyEndPos[Colour] = PonyRestPositions[PlacedCellByRegion[Colour]];
                StandbyPonies[Colour]->SetVisibility(true);
            }
            else
            {
                StandbyStartPos[Colour] = StandbyPonies[Colour]->GetRelativeLocation();
                StandbyEndPos[Colour] = GetStandbyLocation(Colour);
                StandbyPonies[Colour]->SetVisibility(true);
            }
            SetActorTickEnabled(true);
        }
        else if (StandbyTravelTime[Colour] <= 0.f)
        {
            StandbyPonies[Colour]->SetVisibility(bShouldStandby);
            if (bShouldStandby)
            {
                StandbyPonies[Colour]->SetRelativeLocation(GetStandbyLocation(Colour));
            }
        }
    }
}

void AXiaomaBoardActor::SetHighlightedCell(int32 Index)
{
    if (HighlightedCell == Index)
    {
        return;
    }
    const int32 OldIndex = HighlightedCell;
    HighlightedCell = Index;

    auto UpdateSingleTile = [this](int32 Idx)
    {
        if (!Tiles.IsValidIndex(Idx)) return;
        const FVector BasePos = Tiles[Idx]->GetRelativeLocation();
        Tiles[Idx]->SetRelativeLocation(FVector(BasePos.X, BasePos.Y, Idx == HighlightedCell ? 5.f : 0.f));
        if (TileMaterials.IsValidIndex(Idx) && TileColours.IsValidIndex(Idx))
        {
            FLinearColor BaseCol = TileColours[Idx];
            if (Idx == HighlightedCell)
            {
                BaseCol = FMath::Lerp(BaseCol, FLinearColor::White, 0.28f);
            }
            TileMaterials[Idx]->SetVectorParameterValue(TEXT("Color"), BaseCol);
        }
    };

    UpdateSingleTile(OldIndex);
    UpdateSingleTile(HighlightedCell);
}

void AXiaomaBoardActor::RejectStandbyPony(int32 ColourIndex)
{
    if (StandbyRejectTime.IsValidIndex(ColourIndex))
    {
        StandbyRejectTime[ColourIndex] = RejectDuration;
        SetActorTickEnabled(true);
    }
}

void AXiaomaBoardActor::PetPony(int32 ColourIndex)
{
    if (StandbyPonies.IsValidIndex(ColourIndex) && StandbyPonies[ColourIndex]->IsVisible())
    {
        StandbyHopTime[ColourIndex] = HopDuration;
        SetActorTickEnabled(true);
    }
    for (int32 Index = 0; Index < Ponies.Num(); ++Index)
    {
        if (Ponies[Index]->IsVisible() && RegionAtCell.IsValidIndex(Index) && RegionAtCell[Index] == ColourIndex)
        {
            HopTime[Index] = HopDuration;
            SetActorTickEnabled(true);
        }
    }
}

int32 AXiaomaBoardActor::FindPonyColour(const UPrimitiveComponent* Component) const
{
    if (!Component)
    {
        return INDEX_NONE;
    }
    for (int32 Colour = 0; Colour < StandbyPonies.Num(); ++Colour)
    {
        if (StandbyPonies[Colour] == Component)
        {
            return Colour;
        }
    }
    for (int32 Index = 0; Index < Ponies.Num(); ++Index)
    {
        if (Ponies[Index] == Component && RegionAtCell.IsValidIndex(Index))
        {
            return RegionAtCell[Index];
        }
    }
    return INDEX_NONE;
}

void AXiaomaBoardActor::Celebrate()
{
    int32 PonyCount = 0;
    for (int32 Index = 0; Index < Ponies.Num(); ++Index)
    {
        if (Ponies[Index]->IsVisible())
        {
            HopTime[Index] = HopDuration + PonyCount++ * 0.08f;
        }
    }
    SetActorTickEnabled(true);
}

void AXiaomaBoardActor::FlashCell(int32 Index, bool bError)
{
    if (!TileMaterials.IsValidIndex(Index))
    {
        return;
    }
    if (TileMaterials.IsValidIndex(FlashIndex))
    {
        TileMaterials[FlashIndex]->SetVectorParameterValue(TEXT("Color"), TileColours[FlashIndex]);
    }
    FlashIndex = Index;
    FlashRemaining = FlashDuration;
    FlashColour = bError ? FLinearColor(1.f, 0.20f, 0.18f) : FLinearColor(1.f, 0.93f, 0.68f);
    SetActorTickEnabled(true);
}

void AXiaomaBoardActor::Tick(float DeltaSeconds)
{
    Super::Tick(DeltaSeconds);
    bool bAnimating = false;
    for (int32 Index = 0; Index < HopTime.Num(); ++Index)
    {
        if (HopTime[Index] > 0.f)
        {
            HopTime[Index] = FMath::Max(0.f, HopTime[Index] - DeltaSeconds);
            const float Phase = 1.f - FMath::Min(1.f, HopTime[Index] / HopDuration);
            Ponies[Index]->SetRelativeLocation(PonyRestPositions[Index]
                + FVector(0.f, 0.f, 32.f * FMath::Sin(PI * Phase)));
            bAnimating |= HopTime[Index] > 0.f;
        }
    }

    for (int32 Colour = 0; Colour < StandbyPonies.Num(); ++Colour)
    {
        if (StandbyTravelTime[Colour] > 0.f)
        {
            StandbyTravelTime[Colour] = FMath::Max(0.f, StandbyTravelTime[Colour] - DeltaSeconds);
            const float T = 1.f - StandbyTravelTime[Colour] / TravelDuration;
            FVector Pos = FMath::Lerp(StandbyStartPos[Colour], StandbyEndPos[Colour], T);
            Pos.Z += 68.f * FMath::Sin(PI * T);
            StandbyPonies[Colour]->SetRelativeLocation(Pos);
            if (StandbyTravelTime[Colour] <= 0.f)
            {
                StandbyPonies[Colour]->SetRelativeLocation(GetStandbyLocation(Colour));
                StandbyPonies[Colour]->SetVisibility(StandbyTargetVisible[Colour]);
            }
            else
            {
                bAnimating = true;
            }
        }
        if (StandbyHopTime[Colour] > 0.f)
        {
            StandbyHopTime[Colour] = FMath::Max(0.f, StandbyHopTime[Colour] - DeltaSeconds);
            const float Phase = 1.f - FMath::Min(1.f, StandbyHopTime[Colour] / HopDuration);
            StandbyPonies[Colour]->SetRelativeLocation(GetStandbyLocation(Colour)
                + FVector(0.f, 0.f, 30.f * FMath::Sin(PI * Phase)));
            bAnimating |= StandbyHopTime[Colour] > 0.f;
        }
        if (StandbyRejectTime[Colour] > 0.f)
        {
            StandbyRejectTime[Colour] = FMath::Max(0.f, StandbyRejectTime[Colour] - DeltaSeconds);
            const float T = 1.f - StandbyRejectTime[Colour] / RejectDuration;
            const float ShakeYaw = -90.f + 28.f * FMath::Sin(T * PI * 4.f);
            StandbyPonies[Colour]->SetRelativeRotation(FRotator(0.f, ShakeYaw, 0.f));
            if (StandbyRejectTime[Colour] <= 0.f)
            {
                StandbyPonies[Colour]->SetRelativeRotation(FRotator(0.f, -90.f, 0.f));
            }
            else
            {
                bAnimating = true;
            }
        }
    }

    if (FlashRemaining > 0.f)
    {
        FlashRemaining = FMath::Max(0.f, FlashRemaining - DeltaSeconds);
        const FLinearColor Colour = FMath::Lerp(TileColours[FlashIndex], FlashColour,
            FlashRemaining / FlashDuration);
        TileMaterials[FlashIndex]->SetVectorParameterValue(TEXT("Color"), Colour);
        bAnimating |= FlashRemaining > 0.f;
    }
    SetActorTickEnabled(bAnimating);
}

int32 AXiaomaBoardActor::FindCell(const UPrimitiveComponent* Component) const
{
    for (int32 Index = 0; Index < Tiles.Num(); ++Index)
    {
        if (Tiles[Index] == Component)
        {
            return Index;
        }
    }
    return INDEX_NONE;
}
