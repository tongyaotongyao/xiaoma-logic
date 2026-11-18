#include "XiaomaBoardActor.h"

#include "Components/SceneComponent.h"
#include "Components/StaticMeshComponent.h"
#include "Materials/MaterialInstanceDynamic.h"
#include "Materials/MaterialInterface.h"
#include "Engine/StaticMesh.h"

namespace
{
constexpr float HopDuration = 0.38f;
constexpr float FlashDuration = 0.28f;
const FLinearColor RegionColours[6] = {
    FLinearColor(0.29f, 0.58f, 0.94f),
    FLinearColor(0.82f, 0.53f, 0.34f),
    FLinearColor(0.96f, 0.48f, 0.70f),
    FLinearColor(0.88f, 0.36f, 0.42f),
    FLinearColor(0.94f, 0.78f, 0.30f),
    FLinearColor(0.53f, 0.82f, 0.69f)
};
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
    MarkMaterial->SetVectorParameterValue(TEXT("Color"), FLinearColor(0.10f, 0.16f, 0.24f));

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
        Tile->SetRelativeScale3D(FVector(1.04f, 1.04f, 0.16f));
        Tile->SetCastShadow(false);
        Tile->SetCollisionEnabled(ECollisionEnabled::QueryOnly);
        Tile->SetCollisionResponseToAllChannels(ECR_Ignore);
        Tile->SetCollisionResponseToChannel(ECC_Visibility, ECR_Block);
        Tile->RegisterComponent();
        AddInstanceComponent(Tile);
        Tiles.Add(Tile);

        UMaterialInstanceDynamic* Material = UMaterialInstanceDynamic::Create(BaseMaterial, this);
        Material->SetVectorParameterValue(TEXT("Color"), RegionColours[Level.Regions[Index]]);
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
        Pony->SetCollisionEnabled(ECollisionEnabled::NoCollision);
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
    HopTime.Init(0.f, Tiles.Num());
    PonyRestPositions.SetNum(Tiles.Num());
    TileColours.SetNum(Tiles.Num());
}

void AXiaomaBoardActor::RefreshBoard(const FXiaomaLevel& Level, const TArray<uint8>& Cells)
{
    if (Cells.Num() != FXiaomaPuzzle::CellCount || Tiles.Num() != FXiaomaPuzzle::CellCount)
    {
        return;
    }
    for (int32 Index = 0; Index < Cells.Num(); ++Index)
    {
        const bool bGiven = Level.IsGiven(Index / 6, Index % 6);
        FLinearColor Colour = RegionColours[Level.Regions[Index]];
        if (bGiven)
        {
            Colour *= 1.25f;
        }
        else if (Cells[Index] == 1)
        {
            Colour *= 0.65f;
        }
        TileColours[Index] = Colour;
        TileMaterials[Index]->SetVectorParameterValue(TEXT("Color"), Colour);
        UStaticMesh* Character = LoadObject<UStaticMesh>(nullptr,
            PonyAssetPaths[Level.Regions[Index]]);
        Ponies[Index]->SetStaticMesh(Character ? Character : FallbackPonyMesh.Get());
        PonyRestPositions[Index] = Tiles[Index]->GetRelativeLocation()
            + FVector(0.f, 0.f, Character ? 10.f : 52.f);
        Ponies[Index]->SetRelativeLocation(PonyRestPositions[Index]);
        Ponies[Index]->SetRelativeScale3D(Character ? FVector(0.45f) : FVector(0.36f));
        const bool bShowPony = Cells[Index] == 2;
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
