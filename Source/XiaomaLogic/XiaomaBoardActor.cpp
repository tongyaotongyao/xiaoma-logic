#include "XiaomaBoardActor.h"

#include "Components/SceneComponent.h"
#include "Components/StaticMeshComponent.h"
#include "Components/TextRenderComponent.h"
#include "Materials/MaterialInstanceDynamic.h"
#include "Materials/MaterialInterface.h"
#include "Engine/StaticMesh.h"

namespace
{
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
    PrimaryActorTick.bCanEverTick = false;
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
    UStaticMesh* Sphere = LoadObject<UStaticMesh>(nullptr, TEXT("/Engine/BasicShapes/Sphere.Sphere"));
    UMaterialInterface* BaseMaterial = LoadObject<UMaterialInterface>(nullptr,
        TEXT("/Engine/BasicShapes/BasicShapeMaterial.BasicShapeMaterial"));
    if (!Cube || !Sphere || !BaseMaterial)
    {
        UE_LOG(LogTemp, Error, TEXT("Could not load engine basic meshes/material"));
        return;
    }
    FallbackPonyMesh = Sphere;

    for (int32 Index = 0; Index < FXiaomaPuzzle::CellCount; ++Index)
    {
        const int32 Row = Index / FXiaomaPuzzle::Size;
        const int32 Column = Index % FXiaomaPuzzle::Size;
        const FVector Position(Row * 112.f, Column * 112.f, 0.f);

        UStaticMeshComponent* Tile = NewObject<UStaticMeshComponent>(this);
        Tile->SetStaticMesh(Cube);
        Tile->SetupAttachment(SceneRoot);
        Tile->SetRelativeLocation(Position);
        Tile->SetRelativeScale3D(FVector(1.04f, 1.04f, 0.16f));
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
        Pony->SetCollisionEnabled(ECollisionEnabled::NoCollision);
        Pony->RegisterComponent();
        AddInstanceComponent(Pony);
        Pony->SetVisibility(false);
        Ponies.Add(Pony);

        UTextRenderComponent* Mark = NewObject<UTextRenderComponent>(this);
        Mark->SetupAttachment(SceneRoot);
        Mark->SetRelativeLocation(Position + FVector(0.f, 0.f, 13.f));
        Mark->SetRelativeRotation(FRotator(90.f, 0.f, 0.f));
        Mark->SetHorizontalAlignment(EHTA_Center);
        Mark->SetVerticalAlignment(EVRTA_TextCenter);
        Mark->SetWorldSize(58.f);
        Mark->SetText(FText::FromString(TEXT("X")));
        Mark->SetTextRenderColor(FColor(45, 48, 72));
        Mark->RegisterComponent();
        AddInstanceComponent(Mark);
        Mark->SetVisibility(false);
        Marks.Add(Mark);
    }
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
        TileMaterials[Index]->SetVectorParameterValue(TEXT("Color"), Colour);
        UStaticMesh* Character = LoadObject<UStaticMesh>(nullptr,
            PonyAssetPaths[Level.Regions[Index]]);
        Ponies[Index]->SetStaticMesh(Character ? Character : FallbackPonyMesh);
        Ponies[Index]->SetRelativeLocation(Tiles[Index]->GetRelativeLocation() + FVector(0.f, 0.f, Character ? 10.f : 52.f));
        Ponies[Index]->SetRelativeScale3D(Character ? FVector(0.45f) : FVector(0.36f));
        Ponies[Index]->SetVisibility(Cells[Index] == 2);
        Marks[Index]->SetVisibility(Cells[Index] == 1);
    }
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
