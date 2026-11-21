#include "XiaomaPickup.h"

#include "Components/SceneComponent.h"
#include "Components/StaticMeshComponent.h"
#include "Engine/StaticMesh.h"
#include "Materials/MaterialInstanceDynamic.h"
#include "Materials/MaterialInterface.h"

AXiaomaPickup::AXiaomaPickup()
{
    PrimaryActorTick.bCanEverTick = true;
    SceneRoot = CreateDefaultSubobject<USceneComponent>(TEXT("Root"));
    RootComponent = SceneRoot;

    BodyMesh = CreateDefaultSubobject<UStaticMeshComponent>(TEXT("BodyMesh"));
    BodyMesh->SetupAttachment(SceneRoot);
    BodyMesh->SetCollisionEnabled(ECollisionEnabled::NoCollision);

    AccentMesh = CreateDefaultSubobject<UStaticMeshComponent>(TEXT("AccentMesh"));
    AccentMesh->SetupAttachment(BodyMesh);
    AccentMesh->SetCollisionEnabled(ECollisionEnabled::NoCollision);
}

void AXiaomaPickup::SetupPickup(const FXiaomaItemDef& InDef, const FVector& WorldLocation)
{
    Def = InDef;
    bCollected = false;
    SetActorLocation(WorldLocation);
    SetActorHiddenInGame(false);

    UStaticMesh* Sphere = LoadObject<UStaticMesh>(nullptr, TEXT("/Engine/BasicShapes/Sphere.Sphere"));
    UStaticMesh* Cone = LoadObject<UStaticMesh>(nullptr, TEXT("/Engine/BasicShapes/Cone.Cone"));
    UStaticMesh* Cylinder = LoadObject<UStaticMesh>(nullptr, TEXT("/Engine/BasicShapes/Cylinder.Cylinder"));
    UMaterialInterface* BaseMat = LoadObject<UMaterialInterface>(nullptr,
        TEXT("/Engine/BasicShapes/BasicShapeMaterial.BasicShapeMaterial"));

    if (!Sphere || !BaseMat)
    {
        return;
    }

    UMaterialInstanceDynamic* BodyMat = UMaterialInstanceDynamic::Create(BaseMat, this);
    UMaterialInstanceDynamic* AccentMat = UMaterialInstanceDynamic::Create(BaseMat, this);

    BodyMesh->SetRelativeLocation(FVector(0.f, 0.f, 28.f));
    BodyMesh->SetRelativeRotation(FRotator::ZeroRotator);

    if (Def.Type == EXiaomaItemType::Apple)
    {
        BodyMesh->SetStaticMesh(Sphere);
        BodyMesh->SetRelativeScale3D(FVector(0.24f, 0.24f, 0.22f));
        BodyMat->SetVectorParameterValue(TEXT("Color"), FLinearColor(FColor(225, 62, 62)));

        AccentMesh->SetStaticMesh(Cylinder ? Cylinder : Sphere);
        AccentMesh->SetRelativeLocation(FVector(0.f, 0.f, 52.f));
        AccentMesh->SetRelativeScale3D(FVector(0.18f, 0.18f, 0.35f));
        AccentMat->SetVectorParameterValue(TEXT("Color"), FLinearColor(FColor(78, 165, 84)));
    }
    else if (Def.Type == EXiaomaItemType::Carrot)
    {
        BodyMesh->SetStaticMesh(Cone ? Cone : Sphere);
        BodyMesh->SetRelativeScale3D(FVector(0.16f, 0.16f, 0.34f));
        BodyMesh->SetRelativeRotation(FRotator(180.f, 0.f, 0.f));
        BodyMat->SetVectorParameterValue(TEXT("Color"), FLinearColor(FColor(246, 134, 42)));

        AccentMesh->SetStaticMesh(Cylinder ? Cylinder : Sphere);
        AccentMesh->SetRelativeLocation(FVector(0.f, 0.f, -55.f));
        AccentMesh->SetRelativeScale3D(FVector(0.35f, 0.35f, 0.28f));
        AccentMat->SetVectorParameterValue(TEXT("Color"), FLinearColor(FColor(84, 185, 88)));
    }
    else if (Def.Type == EXiaomaItemType::Clover)
    {
        BodyMesh->SetStaticMesh(Cylinder ? Cylinder : Sphere);
        BodyMesh->SetRelativeScale3D(FVector(0.28f, 0.28f, 0.08f));
        BodyMat->SetVectorParameterValue(TEXT("Color"), FLinearColor(FColor(76, 198, 114)));

        AccentMesh->SetStaticMesh(Sphere);
        AccentMesh->SetRelativeLocation(FVector(0.f, 0.f, 65.f));
        AccentMesh->SetRelativeScale3D(FVector(0.36f, 0.36f, 0.9f));
        AccentMat->SetVectorParameterValue(TEXT("Color"), FLinearColor(FColor(152, 240, 165)));
    }
    else
    {
        BodyMesh->SetStaticMesh(Sphere);
        BodyMesh->SetRelativeScale3D(FVector(0.24f));
        BodyMat->SetVectorParameterValue(TEXT("Color"), FLinearColor(FColor(255, 215, 70)));

        AccentMesh->SetStaticMesh(Sphere);
        AccentMesh->SetRelativeLocation(FVector::ZeroVector);
        AccentMesh->SetRelativeScale3D(FVector(0.5f));
        AccentMat->SetVectorParameterValue(TEXT("Color"), FLinearColor::White);
    }

    BodyMesh->SetMaterial(0, BodyMat);
    AccentMesh->SetMaterial(0, AccentMat);
}

void AXiaomaPickup::SetCollected(bool bInCollected)
{
    bCollected = bInCollected;
    SetActorHiddenInGame(bCollected);
    SetActorTickEnabled(!bCollected);
}

void AXiaomaPickup::OnHerdsmanEnterCell(FIntPoint Cell)
{
    if (!bCollected && Cell == Def.Cell)
    {
        SetCollected(true);
        OnCollected.ExecuteIfBound(Def.Type, Def.Cell);
    }
}

void AXiaomaPickup::Tick(float DeltaSeconds)
{
    Super::Tick(DeltaSeconds);
    if (bCollected)
    {
        return;
    }
    ElapsedTime += DeltaSeconds;
    const float BobZ = 28.f + 5.f * FMath::Sin(ElapsedTime * 3.2f);
    BodyMesh->SetRelativeLocation(FVector(0.f, 0.f, BobZ));
    AddActorLocalRotation(FRotator(0.f, 75.f * DeltaSeconds, 0.f));
}
