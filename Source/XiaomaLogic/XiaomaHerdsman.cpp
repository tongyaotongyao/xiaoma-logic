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
/** 冰上滑行比走路快一点，读起来才像"打滑"，又不至于看不清自己停在哪 */
constexpr float IceSpeedScale = 1.2f;

/**
 * 世界轴输入 → 格子增量。
 * 棋盘映射是「列号每加一世界 X 减一格、行号每加一世界 Y 减一格」，
 * 所以沿世界 +X 走等于列号 -1、沿世界 +Y 走等于行号 -1。
 * 这两个轴很容易写反（写反后箱子会朝左手的垂直方向跑），集中在这里换算。
 */
FIntPoint WorldAxisToCellDelta(int32 WorldX, int32 WorldY)
{
    return FIntPoint(-WorldY, -WorldX);
}
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

bool AXiaomaHerdsman::IsBlockedAt(const FVector& Location) const
{
    return CellBlocked ? CellBlocked(WorldToRawCell(Location)) : false;
}

bool AXiaomaHerdsman::CanEnterOrPush(const FVector& TargetLocation, FIntPoint StepDelta)
{
    if (!IsBlockedAt(TargetLocation))
    {
        return true;
    }
    // 挡路的如果是木箱，问 GameMode 能不能推开；推开了这一帧就照常前进
    if (!CellPush.IsBound() || (StepDelta.X == 0 && StepDelta.Y == 0))
    {
        return false;
    }
    return CellPush.Execute(WorldToRawCell(TargetLocation), StepDelta);
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
    // 传送不是"走过来"：来路跟着对齐，否则下一步会拿传送前那一格当上桥格
    PreviousCell = CurrentCell;
    SetActorLocation(CellToWorld(CurrentCell));
    PendingInput = FVector2D::ZeroVector;
    // 死亡重生、撤销、读档都是直接把主角搬回格心，别把上一段的滑行状态带过来
    bSliding = false;
    SlideDir = FVector2D::ZeroVector;
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
        // 死亡、暂停、结算时不能还带着惯性往前滑
        bSliding = false;
        SlideDir = FVector2D::ZeroVector;
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
        // 先记来路再赋值：广播期间各机关读 PreviousCell 拿到的都是"进入这一格之前"的位置
        PreviousCell = CurrentCell;
        CurrentCell = Raw;
        OnCellChanged.ExecuteIfBound(CurrentCell);
    }
}

void AXiaomaHerdsman::SyncCurrentCellNow()
{
    const FIntPoint Raw = WorldToRawCell(GetActorLocation());
    if (Raw == CurrentCell)
    {
        return;
    }
    PreviousCell = CurrentCell;
    CurrentCell = Raw;
    OnCellChanged.ExecuteIfBound(CurrentCell);
}

bool AXiaomaHerdsman::IsIceAt(FIntPoint Cell) const
{
    return IceCell ? IceCell(Cell) : false;
}

void AXiaomaHerdsman::BeginSlide(FVector2D Dir, const FVector& Location)
{
    // 斜着踩上冰面也只能滑一个轴：先看输入哪个分量更大；正好 45° 时看玩家
    // 已经越过格心更多的那一轴，免得贴着格子边走反而横着滑出去
    bool bSlideAlongX;
    if (!FMath::IsNearlyEqual(FMath::Abs(Dir.X), FMath::Abs(Dir.Y), 0.15f))
    {
        bSlideAlongX = FMath::Abs(Dir.X) > FMath::Abs(Dir.Y);
    }
    else
    {
        const FVector Center = CellToWorld(WorldToRawCell(Location));
        bSlideAlongX = FMath::Abs(Location.X - Center.X) >= FMath::Abs(Location.Y - Center.Y);
    }
    SlideDir = bSlideAlongX ? FVector2D(FMath::Sign(Dir.X), 0.f) : FVector2D(0.f, FMath::Sign(Dir.Y));
    bSliding = !SlideDir.IsNearlyZero();
    if (bSliding)
    {
        // 先让 GameMode 存下“踩上冰面”这一刻的快照，玩家按 Z 才有得退
        OnSlideBegin.ExecuteIfBound();
    }
}

void AXiaomaHerdsman::EndSlide()
{
    if (!bSliding)
    {
        return;
    }
    bSliding = false;
    SlideDir = FVector2D::ZeroVector;
    SyncCurrentCellNow();
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

    const float Speed = BaseMoveSpeed * (bSliding ? IceSpeedScale : 1.f)
        * (BoostRemaining > 0.f ? 1.5f : 1.f);
    FVector2D Dir = bMovementEnabled ? PendingInput : FVector2D::ZeroVector;
    if (Dir.SizeSquared() > 1.f)
    {
        Dir.Normalize();
    }
    // 滑行期间玩家的输入不生效：方向锁死在踩上冰面那一刻
    if (bSliding)
    {
        Dir = SlideDir;
    }

    if (!Dir.IsNearlyZero())
    {
        const FVector Loc = GetActorLocation();
        FVector Next = Loc;

        // 滑行时不推箱：冰面上撞到木箱就停下，免得一口气推好几格
        const bool bAllowPush = !bSliding;
        auto CanStep = [this, bAllowPush](const FVector& Target, FIntPoint StepDelta)
        {
            return bAllowPush ? CanEnterOrPush(Target, StepDelta) : !IsBlockedAt(Target);
        };

        // 分轴推进：某一轴被机关挡住时，另一轴仍可沿墙滑行
        const int32 StepX = FMath::RoundToInt(FMath::Sign(Dir.X));
        const int32 StepY = FMath::RoundToInt(FMath::Sign(Dir.Y));
        const FVector TryX(Loc.X + Dir.X * Speed * DeltaSeconds, Loc.Y, HerdsmanPlaneZ);
        if (CanStep(TryX, WorldAxisToCellDelta(StepX, 0)))
        {
            Next.X = FMath::Clamp(TryX.X, BoardMinXY, BoardMaxXY);
        }
        const FVector TryY(Next.X, Loc.Y + Dir.Y * Speed * DeltaSeconds, HerdsmanPlaneZ);
        if (CanStep(TryY, WorldAxisToCellDelta(0, StepY)))
        {
            Next.Y = FMath::Clamp(TryY.Y, BoardMinXY, BoardMaxXY);
        }

        const bool bMoved = !Next.Equals(Loc, 0.01f);
        if (bMoved)
        {
            Next.Z = HerdsmanPlaneZ;
            SetActorLocation(Next);
        }
        else if (bSliding)
        {
            // 顶死了：滑行到此为止，玩家重新拿回控制权
            EndSlide();
        }

        const float TargetYaw = FMath::RadiansToDegrees(FMath::Atan2(Dir.Y, Dir.X));
        const FRotator NewRot = FMath::RInterpTo(GetActorRotation(), FRotator(0.f, TargetYaw, 0.f), DeltaSeconds, 14.f);
        SetActorRotation(NewRot);

        if (bMoved)
        {
            WalkCycle += DeltaSeconds * (BoostRemaining > 0.f ? 15.f : 11.f);
            if (VisualRoot)
            {
                // 滑行时几乎不颠：脚下是冰，跳着滑反而看着别扭
                const float Hop = FMath::Abs(FMath::Sin(WalkCycle)) * (bSliding ? 2.f : 7.f);
                VisualRoot->SetRelativeLocation(FVector(0.f, 0.f, -44.f + Hop));
            }
            UpdateCurrentCell();

            // 冰面判定看脚下这一格，而且不用带 12% 缓冲的 CurrentCell：
            // 一踩上去就得开始滑，不能等走进 62% 才反应
            if (IsIceAt(WorldToRawCell(Next)))
            {
                if (!bSliding)
                {
                    BeginSlide(Dir, Next);
                }
            }
            else if (bSliding)
            {
                EndSlide();
            }
        }
        else if (VisualRoot)
        {
            // 顶住了障碍：停下脚步，别原地空踩
            const FVector Cur = VisualRoot->GetRelativeLocation();
            VisualRoot->SetRelativeLocation(FMath::VInterpTo(Cur, FVector(0.f, 0.f, -44.f), DeltaSeconds, 12.f));
        }
    }
    else if (VisualRoot)
    {
        const FVector Cur = VisualRoot->GetRelativeLocation();
        VisualRoot->SetRelativeLocation(FMath::VInterpTo(Cur, FVector(0.f, 0.f, -44.f), DeltaSeconds, 12.f));
    }

    UpdateCamera(DeltaSeconds);
}
