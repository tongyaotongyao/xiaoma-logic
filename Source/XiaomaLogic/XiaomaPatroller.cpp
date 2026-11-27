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
/**
 * 可走区间的宽度下限（单位格）。
 * 一个格被 X 封住后，剩下的空间可能只有半格——这种宽度下往返会变成原地打转，
 * 看起来比停住更怪，所以直接判定为「无路可走」。
 */
constexpr float MinTravelSpan = 0.75f;
/** 判断「某个 X 墙在当前位置的哪一侧」时的浮点容差 */
constexpr float BoundaryEpsilon = 1e-3f;
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
    RecomputeBounds();
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

bool AXiaomaPatroller::IsMarked(int32 Index) const
{
    if (!Def.Path.IsValidIndex(Index) || BoardCells.Num() != FXiaomaPuzzle::CellCount)
    {
        return false;
    }
    const FIntPoint Cell = Def.Path[Index];
    const int32 CellIndex = Cell.X * FXiaomaPuzzle::Size + Cell.Y;
    return BoardCells.IsValidIndex(CellIndex) && BoardCells[CellIndex] == 1;
}

void AXiaomaPatroller::RecomputeBounds()
{
    const int32 Last = Def.Path.Num() - 1;
    MinTravel = 0.f;
    MaxTravel = static_cast<float>(Last);
    bStalled = false;

    if (Last < 1)
    {
        bStalled = true;
        return;
    }

    // 被 X 标记的格整格不可进入，等价于「格心 ± 半格」这段行程是墙。
    // 从当前位置向两侧各找最近的一道墙，它们之间的部分就是它能活动的范围。
    // 注意不能用「每帧试探紧接着要踏入的那一格」：那种判定在端点附近会出现
    // 「掉头后又马上撞回端点」的极限环，表现为每帧翻转一次的抽搐。
    int32 LeftWall = INDEX_NONE;
    int32 RightWall = INDEX_NONE;
    for (int32 Index = 0; Index < Def.Path.Num(); ++Index)
    {
        if (!IsMarked(Index))
        {
            continue;
        }
        if (static_cast<float>(Index) + 0.5f <= TravelDistance + BoundaryEpsilon)
        {
            LeftWall = Index;
        }
        else if (RightWall == INDEX_NONE)
        {
            RightWall = Index;
        }
    }
    if (LeftWall != INDEX_NONE)
    {
        MinTravel = static_cast<float>(LeftWall) + 0.5f;
    }
    if (RightWall != INDEX_NONE)
    {
        MaxTravel = static_cast<float>(RightWall) - 0.5f;
    }

    // X 正好落在脚下（撞人冷却期间玩家放的）：推回最近的可站位置
    if (TravelDistance > MaxTravel)
    {
        TravelDistance = MaxTravel;
    }
    if (TravelDistance < MinTravel)
    {
        TravelDistance = MinTravel;
    }

    // 剩下的空间不足一格：往返只会变成原地打转，干脆停住
    if (MaxTravel - MinTravel < MinTravelSpan)
    {
        bStalled = true;
    }
}

void AXiaomaPatroller::Advance(float DeltaSeconds)
{
    if (bStalled)
    {
        return;
    }
    TravelDistance += MoveDirection * Def.Speed * DeltaSeconds;

    // 撞到区间边界就折返：把越过的那一段折回来，位置不会钉在界上反复触发
    if (TravelDistance >= MaxTravel)
    {
        TravelDistance = MaxTravel - (TravelDistance - MaxTravel);
        MoveDirection = -1;
    }
    else if (TravelDistance <= MinTravel)
    {
        TravelDistance = MinTravel + (MinTravel - TravelDistance);
        MoveDirection = 1;
    }
    TravelDistance = FMath::Clamp(TravelDistance, MinTravel, MaxTravel);
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

    // 停住时位置与朝向一律不动；撞人判定仍要做，站着也是一只捣蛋鬼
    if (!bStalled)
    {
        Advance(DeltaSeconds);
        UpdateTransform();
    }
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
    // 召唤小马、用提示星也会走到这里，那些并不影响 X 路障；
    // 只有标记真的变了才重算可走区间，免得停住的捣蛋鬼被无谓地唤醒又立刻停回去。
    if (Cells == BoardCells)
    {
        return;
    }
    BoardCells = Cells;
    RecomputeBounds();
    UpdateTransform();
}

void AXiaomaPatroller::LoadState(const TArray<uint8>& In)
{
    // 撤销与读档都让捣蛋鬼从路线起点重新走（策划案 4 节）
    ResetToStart();
}
