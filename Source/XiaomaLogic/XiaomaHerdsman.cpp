#include "XiaomaHerdsman.h"

#include "Camera/CameraComponent.h"
#include "Components/CapsuleComponent.h"
#include "Components/StaticMeshComponent.h"
#include "Engine/StaticMesh.h"
#include "GameFramework/CharacterMovementComponent.h"
#include "GameFramework/SpringArmComponent.h"
#include "Materials/MaterialInstanceDynamic.h"
#include "Materials/MaterialInterface.h"

namespace
{
constexpr float CellStep = 112.f;
constexpr float BoardMinXY = -36.f;
constexpr float BoardMaxXY = 5.f * CellStep + 36.f;
constexpr float HerdsmanPlaneZ = 52.f;
}

AXiaomaHerdsman::AXiaomaHerdsman()
{
    PrimaryActorTick.bCanEverTick = true;

    GetCapsuleComponent()->InitCapsuleSize(22.f, 44.f);
    GetCapsuleComponent()->SetCollisionEnabled(ECollisionEnabled::NoCollision);

    bUseControllerRotationYaw = false;
    if (UCharacterMovementComponent* Move = GetCharacterMovement())
    {
        Move->GravityScale = 0.f;
        Move->MaxWalkSpeed = BaseMoveSpeed;
        Move->MaxAcceleration = 2400.f;
        Move->BrakingDecelerationWalking = 2400.f;
        Move->bOrientRotationToMovement = false;
    }

    VisualRoot = CreateDefaultSubobject<USceneComponent>(TEXT("VisualRoot"));
    VisualRoot->SetupAttachment(RootComponent);
    VisualRoot->SetRelativeLocation(FVector(0.f, 0.f, -44.f));

    CameraBoom = CreateDefaultSubobject<USpringArmComponent>(TEXT("CameraBoom"));
    CameraBoom->SetupAttachment(RootComponent);
    CameraBoom->SetUsingAbsoluteRotation(true);
    CameraBoom->SetRelativeRotation(FRotator(-56.f, 90.f, 0.f));
    CameraBoom->TargetArmLength = 760.f;
    CameraBoom->bDoCollisionTest = false;
    CameraBoom->bEnableCameraLag = true;
    CameraBoom->CameraLagSpeed = 10.f;

    FollowCamera = CreateDefaultSubobject<UCameraComponent>(TEXT("FollowCamera"));
    FollowCamera->SetupAttachment(CameraBoom, USpringArmComponent::SocketName);
    FollowCamera->SetFieldOfView(54.f);

    BuildVisuals();
}

void AXiaomaHerdsman::BuildVisuals()
{
    UStaticMesh* Sphere = LoadObject<UStaticMesh>(nullptr, TEXT("/Engine/BasicShapes/Sphere.Sphere"));
    UStaticMesh* Cone = LoadObject<UStaticMesh>(nullptr, TEXT("/Engine/BasicShapes/Cone.Cone"));
    UStaticMesh* Cylinder = LoadObject<UStaticMesh>(nullptr, TEXT("/Engine/BasicShapes/Cylinder.Cylinder"));
    UMaterialInterface* BaseMat = LoadObject<UMaterialInterface>(nullptr,
        TEXT("/Engine/BasicShapes/BasicShapeMaterial.BasicShapeMaterial"));
    if (!Sphere || !Cone || !Cylinder || !BaseMat)
    {
        return;
    }

    auto AddPart = [&](const TCHAR* Name, UStaticMesh* InMesh, const FVector& Loc,
        const FVector& Scale, const FColor& Colour)
    {
        UStaticMeshComponent* Comp = CreateDefaultSubobject<UStaticMeshComponent>(Name);
        Comp->SetupAttachment(VisualRoot);
        Comp->SetStaticMesh(InMesh);
        Comp->SetRelativeLocation(Loc);
        Comp->SetRelativeScale3D(Scale);
        Comp->SetCollisionEnabled(ECollisionEnabled::NoCollision);
        UMaterialInstanceDynamic* Mat = UMaterialInstanceDynamic::Create(BaseMat, this);
        Mat->SetVectorParameterValue(TEXT("Color"), FLinearColor(Colour));
        Comp->SetMaterial(0, Mat);
        return Comp;
    };

    // 小精灵身体、头部、草帽和两侧小脚
    AddPart(TEXT("Body"), Cone, FVector(0.f, 0.f, 24.f), FVector(0.32f, 0.32f, 0.42f), FColor(88, 174, 146));
    AddPart(TEXT("Head"), Sphere, FVector(0.f, 0.f, 52.f), FVector(0.28f, 0.28f, 0.28f), FColor(255, 229, 204));
    AddPart(TEXT("HatBrim"), Cylinder, FVector(0.f, 0.f, 62.f), FVector(0.42f, 0.42f, 0.03f), FColor(219, 173, 98));
    AddPart(TEXT("HatTop"), Cone, FVector(0.f, 0.f, 75.f), FVector(0.24f, 0.24f, 0.26f), FColor(186, 134, 68));
    AddPart(TEXT("EyeL"), Sphere, FVector(11.f, -5.f, 53.f), FVector(0.045f), FColor(40, 36, 34));
    AddPart(TEXT("EyeR"), Sphere, FVector(11.f, 5.f, 53.f), FVector(0.045f), FColor(40, 36, 34));
}

FVector AXiaomaHerdsman::CellToWorld(FIntPoint Cell)
{
    return FVector((5 - Cell.Y) * CellStep, (5 - Cell.X) * CellStep, HerdsmanPlaneZ);
}

FIntPoint AXiaomaHerdsman::WorldToRawCell(const FVector& Location)
{
    const int32 Row = FMath::Clamp(FMath::RoundToInt(5.f - Location.Y / CellStep), 0, 5);
    const int32 Col = FMath::Clamp(FMath::RoundToInt(5.f - Location.X / CellStep), 0, 5);
    return FIntPoint(Row, Col);
}

void AXiaomaHerdsman::TeleportToCell(FIntPoint Cell)
{
    CurrentCell = FIntPoint(FMath::Clamp(Cell.X, 0, 5), FMath::Clamp(Cell.Y, 0, 5));
    SetActorLocation(CellToWorld(CurrentCell));
    PendingInput = FVector2D::ZeroVector;
}

void AXiaomaHerdsman::SetOverviewCamera(bool bEnable)
{
    bOverviewCamera = bEnable;
}

void AXiaomaHerdsman::SetMovementEnabled(bool bEnabled)
{
    bMovementEnabled = bEnabled;
    if (!bEnabled)
    {
        PendingInput = FVector2D::ZeroVector;
    }
}

void AXiaomaHerdsman::SetSpeedBoost(float DurationSeconds)
{
    BoostRemaining = FMath::Max(BoostRemaining, DurationSeconds);
}

void AXiaomaHerdsman::MoveForwardInput(float Value)
{
    PendingInput.Y = Value;
}

void AXiaomaHerdsman::MoveRightInput(float Value)
{
    PendingInput.X = Value;
}

void AXiaomaHerdsman::UpdateCurrentCell()
{
    const FVector Loc = GetActorLocation();
    const FIntPoint Raw = WorldToRawCell(Loc);
    if (Raw == CurrentCell)
    {
        return;
    }
    // 使用约 12% 缓冲带防止在格子交界处来回抖动触发步数
    const FVector CurrentCenter = CellToWorld(CurrentCell);
    const float Dx = FMath::Abs(Loc.X - CurrentCenter.X);
    const float Dy = FMath::Abs(Loc.Y - CurrentCenter.Y);
    if (Dx > CellStep * 0.62f || Dy > CellStep * 0.62f)
    {
        CurrentCell = Raw;
        OnCellChanged.ExecuteIfBound(CurrentCell);
    }
}

void AXiaomaHerdsman::UpdateCamera(float DeltaSeconds)
{
    if (!CameraBoom)
    {
        return;
    }
    const float TargetArm = bOverviewCamera ? 1240.f : 760.f;
    CameraBoom->TargetArmLength = FMath::FInterpTo(CameraBoom->TargetArmLength, TargetArm, DeltaSeconds, 8.f);

    const FVector BoardCenter(280.f, 280.f, HerdsmanPlaneZ);
    const FVector TargetWorld = bOverviewCamera ? BoardCenter : GetActorLocation();
    const FVector Offset = TargetWorld - GetActorLocation();
    const FVector CurrentOffset = CameraBoom->GetRelativeLocation();
    CameraBoom->SetRelativeLocation(FMath::VInterpTo(CurrentOffset, Offset, DeltaSeconds, 8.f));
}

void AXiaomaHerdsman::Tick(float DeltaSeconds)
{
    Super::Tick(DeltaSeconds);

    if (BoostRemaining > 0.f)
    {
        BoostRemaining = FMath::Max(0.f, BoostRemaining - DeltaSeconds);
    }

    const float Speed = BaseMoveSpeed * (BoostRemaining > 0.f ? 1.5f : 1.0f);
    FVector2D Dir = bMovementEnabled ? PendingInput : FVector2D::ZeroVector;
    if (Dir.SizeSquared() > 1.f)
    {
        Dir.Normalize();
    }

    if (!Dir.IsNearlyZero())
    {
        FVector Loc = GetActorLocation();
        Loc.X = FMath::Clamp(Loc.X + Dir.X * Speed * DeltaSeconds, BoardMinXY, BoardMaxXY);
        Loc.Y = FMath::Clamp(Loc.Y + Dir.Y * Speed * DeltaSeconds, BoardMinXY, BoardMaxXY);
        Loc.Z = HerdsmanPlaneZ;
        SetActorLocation(Loc);

        const float TargetYaw = FMath::RadiansToDegrees(FMath::Atan2(Dir.Y, Dir.X));
        const FRotator NewRot = FMath::RInterpTo(GetActorRotation(), FRotator(0.f, TargetYaw, 0.f), DeltaSeconds, 14.f);
        SetActorRotation(NewRot);

        WalkCycle += DeltaSeconds * (BoostRemaining > 0.f ? 15.f : 11.f);
        if (VisualRoot)
        {
            const float Hop = FMath::Abs(FMath::Sin(WalkCycle)) * 7.f;
            VisualRoot->SetRelativeLocation(FVector(0.f, 0.f, -44.f + Hop));
        }
        UpdateCurrentCell();
    }
    else if (VisualRoot)
    {
        const FVector Cur = VisualRoot->GetRelativeLocation();
        VisualRoot->SetRelativeLocation(FMath::VInterpTo(Cur, FVector(0.f, 0.f, -44.f), DeltaSeconds, 12.f));
    }

    UpdateCamera(DeltaSeconds);
}
