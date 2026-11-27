#include "XiaomaPatroller.h"

#include "XiaomaBoardActor.h"
#include "XiaomaHerdsman.h"

#include "Components/SceneComponent.h"
#include "Components/StaticMeshComponent.h"
#include "Engine/StaticMesh.h"
#include "Materials/MaterialInstanceDynamic.h"
#include "Materials/MaterialInterface.h"

namespace
{
/** 撞人后短暂免疫，避免主角被弹回起点后又被同一只连扣 */
constexpr float HitCooldownSeconds = 1.0f;
/** 掉头时离开整点一点点，否则下一帧会再次判定同一格而抖动 */
constexpr float TurnNudge = 0.02f;
/** 6 色小马的配色里没有「坏家伙」，捣蛋鬼自己用一套深紫 */
const FLinearColor PatrollerBodyColour(0.34f, 0.22f, 0.44f);
const FLinearColor PatrollerHatColour(0.20f, 0.13f, 0.28f);
}

AXiaomaPatroller::AXiaomaPatroller()
{
    PrimaryActorTick.bCanEverTick = true;
    PrimaryActorTick.bStartWithTickEnabled = false;

    SceneRoot = CreateDefaultSubobject<USceneComponent>(TEXT("Root"));
    RootComponent = SceneRoot;

    auto MakePart = [this](const TCHAR* Name)
    {
        UStaticMeshComponent* Part = CreateDefaultSubobject<UStaticMeshComponent>(Name);
        Part->SetupAttachment(SceneRoot);
        Part->SetCollisionEnabled(ECollisionEnabled::NoCollision);
        Part->SetCastShadow(false);
        return Part;
    };

    BodyMesh = MakePart(TEXT("Body"));
    HatMesh = MakePart(TEXT("Hat"));
    EarLeftMesh = MakePart(TEXT("EarLeft"));
    EarRightMesh = MakePart(TEXT("EarRight"));
    EyeLeftMesh = MakePart(TEXT("EyeLeft"));
    EyeRightMesh = MakePart(TEXT("EyeRight"));
}

void AXiaomaPatroller::SetupPatroller(const FXiaomaPatrolDef& InDef, const AXiaomaBoardActor* InBoard,
    AXiaomaHerdsman* InHerdsman)
{
    Def = InDef;
    Board = InBoard;
    Herdsman = InHerdsman;

    BuildVisuals();
    ResetToStart();
    SetActorTickEnabled(Def.Path.Num() >= 2);
}

void AXiaomaPatroller::SetPatrolEnabled(bool bEnabledIn)
{
    bPatrolEnabled = bEnabledIn;
    SetActorTickEnabled(bPatrolEnabled && Def.Path.Num() >= 2);
}

FVector AXiaomaPatroller::CellLocation(FIntPoint Cell) const
{
    if (Board.IsValid())
    {
        return Board->GetCellWorldLocation(Cell);
    }
    return GetActorLocation();
}

void AXiaomaPatroller::BuildVisuals()
{
    UStaticMesh* Sphere = LoadObject<UStaticMesh>(nullptr, TEXT("/Engine/BasicShapes/Sphere.Sphere"));
    UMaterialInterface* BaseMat = LoadObject<UMaterialInterface>(nullptr,
        TEXT("/Engine/BasicShapes/BasicShapeMaterial.BasicShapeMaterial"));
    if (!Sphere || !BaseMat)
    {
        return;
    }

    BodyMaterial = UMaterialInstanceDynamic::Create(BaseMat, this);
    BodyMaterial->SetVectorParameterValue(TEXT("Color"), PatrollerBodyColour);
    UMaterialInstanceDynamic* HatMaterial = UMaterialInstanceDynamic::Create(BaseMat, this);
    HatMaterial->SetVectorParameterValue(TEXT("Color"), PatrollerHatColour);
    UMaterialInstanceDynamic* EyeMaterial = UMaterialInstanceDynamic::Create(BaseMat, this);
    EyeMaterial->SetVectorParameterValue(TEXT("Color"), FLinearColor(0.97f, 0.96f, 0.92f));

    // 圆滚滚的身体：直径约 55cm，底部离地 5cm
    BodyMesh->SetStaticMesh(Sphere);
    BodyMesh->SetRelativeLocation(FVector(0.f, 0.f, 32.f));
    BodyMesh->SetRelativeScale3D(FVector(0.56f, 0.56f, 0.50f));
    BodyMesh->SetMaterial(0, BodyMaterial);
    BodyMesh->SetCastShadow(true);

    // 小尖帽
    HatMesh->SetStaticMesh(Sphere);
    HatMesh->SetRelativeLocation(FVector(0.f, 0.f, 58.f));
    HatMesh->SetRelativeScale3D(FVector(0.20f, 0.20f, 0.24f));
    HatMesh->SetMaterial(0, HatMaterial);

    // 两只尖耳朵
    for (UStaticMeshComponent* Ear : {EarLeftMesh, EarRightMesh})
    {
        Ear->SetStaticMesh(Sphere);
        Ear->SetRelativeScale3D(FVector(0.11f, 0.11f, 0.17f));
        Ear->SetMaterial(0, HatMaterial);
        Ear->SetRelativeRotation(FRotator(0.f, 0.f, 22.f));
    }
    if (EarLeftMesh)
    {
        EarLeftMesh->SetRelativeLocation(FVector(-15.f, 0.f, 50.f));
    }
    if (EarRightMesh)
    {
        EarRightMesh->SetRelativeLocation(FVector(15.f, 0.f, 50.f));
    }

    // 眼睛朝 +X（UpdateTransform 会把整只转向移动方向）
    for (UStaticMeshComponent* Eye : {EyeLeftMesh, EyeRightMesh})
    {
        Eye->SetStaticMesh(Sphere);
        Eye->SetRelativeScale3D(FVector(0.10f, 0.10f, 0.10f));
        Eye->SetMaterial(0, EyeMaterial);
    }
    if (EyeLeftMesh)
    {
        EyeLeftMesh->SetRelativeLocation(FVector(25.f, -11.f, 40.f));
    }
    if (EyeRightMesh)
    {
        EyeRightMesh->SetRelativeLocation(FVector(25.f, 11.f, 40.f));
    }
}

void AXiaomaPatroller::ResetToStart()
{
    TravelDistance = 0.f;
    MoveDirection = 1;
    HitCooldown = 0.f;
    UpdateTransform();
}

void AXiaomaPatroller::UpdateTransform()
{
    if (Def.Path.Num() < 2)
    {
        return;
    }
    const float Total = static_cast<float>(Def.Path.Num() - 1);
    const float Clamped = FMath::Clamp(TravelDistance, 0.f, Total);
    const int32 SegmentIndex = FMath::Clamp(FMath::FloorToInt(Clamped), 0, Def.Path.Num() - 2);
    const float Alpha = Clamped - SegmentIndex;

    const FIntPoint From = Def.Path[SegmentIndex];
    const FIntPoint To = Def.Path[SegmentIndex + 1];
    SetActorLocation(FMath::Lerp(CellLocation(From), CellLocation(To), Alpha));

    // 格子坐标是 (行, 列)，世界坐标 X 随列减小、Y 随行减小，所以方向要取反
    const FIntPoint Delta = MoveDirection >= 0 ? (To - From) : (From - To);
    if (Delta.X != 0 || Delta.Y != 0)
    {
        const float WorldDeltaX = -static_cast<float>(Delta.Y);
        const float WorldDeltaY = -static_cast<float>(Delta.X);
        const float Yaw = FMath::RadiansToDegrees(FMath::Atan2(WorldDeltaY, WorldDeltaX));
        SetActorRotation(FRotator(0.f, Yaw, 0.f));
    }
}

bool AXiaomaPatroller::IsUpcomingCellMarked() const
{
    if (Def.Path.Num() < 2 || BoardCells.Num() != FXiaomaPuzzle::CellCount)
    {
        return false;
    }
    // 紧接着要踏入的那一格
    const int32 Upcoming = MoveDirection > 0
        ? FMath::CeilToInt(TravelDistance + KINDA_SMALL_NUMBER)
        : FMath::FloorToInt(TravelDistance - KINDA_SMALL_NUMBER);
    if (!Def.Path.IsValidIndex(Upcoming))
    {
        return false;
    }
    const FIntPoint Cell = Def.Path[Upcoming];
    const int32 Index = Cell.X * FXiaomaPuzzle::Size + Cell.Y;
    return BoardCells.IsValidIndex(Index) && BoardCells[Index] == 1;
}

void AXiaomaPatroller::Tick(float DeltaSeconds)
{
    Super::Tick(DeltaSeconds);
    if (!bPatrolEnabled || Def.Path.Num() < 2 || !Board.IsValid())
    {
        return;
    }
    if (HitCooldown > 0.f)
    {
        HitCooldown = FMath::Max(0.f, HitCooldown - DeltaSeconds);
    }

    const float Total = static_cast<float>(Def.Path.Num() - 1);
    TravelDistance += MoveDirection * Def.Speed * DeltaSeconds;

    // 到端点折返：把越过端点的那段距离折回来，卡顿时会来回折几次
    while (TravelDistance > Total || TravelDistance < 0.f)
    {
        if (TravelDistance > Total)
        {
            TravelDistance = Total - (TravelDistance - Total);
            MoveDirection = -1;
        }
        else
        {
            TravelDistance = -TravelDistance;
            MoveDirection = 1;
        }
    }

    // 前方格放着 X 标记：路障，掉头
    if (IsUpcomingCellMarked())
    {
        MoveDirection = -MoveDirection;
        TravelDistance = FMath::Clamp(TravelDistance + MoveDirection * TurnNudge, 0.f, Total);
    }

    UpdateTransform();
    CheckHerdsmanHit();
}

void AXiaomaPatroller::CheckHerdsmanHit()
{
    if (HitCooldown > 0.f || !Herdsman.IsValid())
    {
        return;
    }
    if (GetCurrentCell() != Herdsman->GetCurrentCell())
    {
        return;
    }
    HitCooldown = HitCooldownSeconds;
    OnHitHerdsman.ExecuteIfBound(this);
}

FIntPoint AXiaomaPatroller::GetCurrentCell() const
{
    if (Def.Path.Num() == 0)
    {
        return FIntPoint(-1, -1);
    }
    if (Def.Path.Num() == 1)
    {
        return Def.Path[0];
    }
    const float Total = static_cast<float>(Def.Path.Num() - 1);
    const float Clamped = FMath::Clamp(TravelDistance, 0.f, Total);
    const int32 SegmentIndex = FMath::Clamp(FMath::FloorToInt(Clamped), 0, Def.Path.Num() - 2);
    const float Alpha = Clamped - SegmentIndex;
    // 走过半格才算进到下一格，避免贴边来回跳
    const int32 CellIndex = Alpha < 0.5f ? SegmentIndex : SegmentIndex + 1;
    return Def.Path.IsValidIndex(CellIndex) ? Def.Path[CellIndex] : Def.Path.Last();
}

void AXiaomaPatroller::OnBoardChanged(const FXiaomaLevel& Level, const TArray<uint8>& Cells)
{
    BoardCells = Cells;
}

void AXiaomaPatroller::LoadState(const TArray<uint8>& In)
{
    // 撤销与读档都让捣蛋鬼从路线起点重新走（策划案 4 节）
    ResetToStart();
}
