
#include "VRCharacter.h"
#include "Camera/CameraComponent.h"
#include "Components/StaticMeshComponent.h"
#include "TimerManager.h"
#include "GameFramework/Controller.h"
#include "Components/CapsuleComponent.h"
#include "NavigationSystem.h"
#include "Components/PostProcessComponent.h"
#include "Materials/MaterialInstanceDynamic.h"
#include "Kismet/GameplayStatics.h"
#include "Components/SplineComponent.h"
#include "Components/SplineMeshComponent.h"
#include "HandController.h"

AVRCharacter::AVRCharacter()
{
	PrimaryActorTick.bCanEverTick = true;

	VRRoot = CreateDefaultSubobject<USceneComponent>(FName("VRRoot"));
	VRRoot->SetupAttachment(GetRootComponent());

	Camera = CreateDefaultSubobject<UCameraComponent>(FName("Camera"));
	Camera->SetupAttachment(VRRoot);

	TeleportPath = CreateDefaultSubobject<USplineComponent>(FName("TeleportPath"));
	TeleportPath->SetupAttachment(VRRoot);

	DestinationMarker = CreateDefaultSubobject<UStaticMeshComponent>(FName("DestinationMarker"));
	DestinationMarker->SetupAttachment(GetRootComponent());

	PostProcessComponent = CreateDefaultSubobject<UPostProcessComponent>(FName("PostProcessComponent"));
	PostProcessComponent->SetupAttachment(GetRootComponent());	
}

void AVRCharacter::BeginPlay()
{
	Super::BeginPlay();

	DestinationMarker->SetVisibility(false);

	if (BlinkerMaterialBase)
	{
		BlinkerMaterialInstance = UMaterialInstanceDynamic::Create(BlinkerMaterialBase, this);
		PostProcessComponent->AddOrUpdateBlendable(BlinkerMaterialInstance);
	}

	LeftController = GetWorld()->SpawnActor<AHandController>(HandControllerClass);
	
	if (LeftController)
	{
		LeftController->AttachToComponent(VRRoot, FAttachmentTransformRules::KeepRelativeTransform);
		LeftController->SetHand(EControllerHand::Left);
		LeftController->SetOwner(this);
	}

	RightController = GetWorld()->SpawnActor<AHandController>(HandControllerClass);

	if (RightController)
	{
		RightController->AttachToComponent(VRRoot, FAttachmentTransformRules::KeepRelativeTransform);
		RightController->SetHand(EControllerHand::Right);
		RightController->SetOwner(this);
	}
}

void AVRCharacter::Tick(float DeltaTime)
{
	Super::Tick(DeltaTime);

	FVector NewCameraOffset = Camera->GetComponentLocation() - GetActorLocation();
	NewCameraOffset.Z = 0;
	AddActorWorldOffset(NewCameraOffset);
	VRRoot->AddWorldOffset(-NewCameraOffset);

	UpdateDestinationMarker();
	UpdateBlinkers();
}

bool AVRCharacter::FindTeleportDestination(TArray<FVector>& OutPath, FVector& OutLocation)
{
	FVector Start = RightController->GetActorLocation();
	FVector Look = RightController->GetActorForwardVector();
	Look = Look.RotateAngleAxis(ControllerRotation, RightController->GetActorRightVector());

	FPredictProjectilePathParams Params(
		TeleportProjectileRadius,
		Start,
		Look * TeleportProjectileSpeed,
		TeleportSimulationTime,
		ECollisionChannel::ECC_Visibility,
		this
	);

	//Params.DrawDebugType = EDrawDebugTrace::ForOneFrame;
	Params.bTraceComplex = true;

	FPredictProjectilePathResult Result;

	bool bHit = UGameplayStatics::PredictProjectilePath(this, Params, Result);

	if (!bHit) return false;

	for (FPredictProjectilePathPointData PointData : Result.PathData)
	{
		OutPath.Add(PointData.Location);
	}

	FNavLocation NavLocation;

	bool bOnNavMesh = UNavigationSystemV1::GetCurrent(GetWorld())->ProjectPointToNavigation(
		Result.HitResult.Location,
		NavLocation,
		TeleportProjectionExtent
	);

	if (!bOnNavMesh) return false;

	OutLocation = NavLocation.Location;

	return bHit && bOnNavMesh;
}

void AVRCharacter::UpdateDestinationMarker()
{
	TArray<FVector> Path;
	FVector Location;
	bool bHasDestination = FindTeleportDestination(Path, Location);

	if (bHasDestination)
	{
		DestinationMarker->SetVisibility(true);
		DestinationMarker->SetWorldLocation(Location);

		DrawTeleportPath(Path);
	} 
	else
	{
		DestinationMarker->SetVisibility(false);

		TArray<FVector> EmptyPath;
		DrawTeleportPath(EmptyPath);
	}
}

FVector2D AVRCharacter::GetBlinkerCenter()
{
	FVector MovementDirection = GetVelocity().GetSafeNormal();

	if (MovementDirection.IsNearlyZero())
	{
		return FVector2D(0.5, 0.5);
	}

	FVector WorldStationaryLocation;

	if (FVector::DotProduct(Camera->GetForwardVector(), MovementDirection) > 0)
	{
		// Moving forwards
		WorldStationaryLocation = Camera->GetComponentLocation() + MovementDirection * 1000;
	}
	else
	{
		// Moving backwards
		WorldStationaryLocation = Camera->GetComponentLocation() - MovementDirection * 1000;
	}

	PlayerController = PlayerController == nullptr ? Cast<APlayerController>(Controller) : PlayerController;

	FVector2D ScreenStationaryLocation;

	if (PlayerController)
	{
		PlayerController->ProjectWorldLocationToScreen(WorldStationaryLocation, ScreenStationaryLocation);

		int32 SizeX, SizeY;
		PlayerController->GetViewportSize(SizeX, SizeY);

		ScreenStationaryLocation.X /= SizeX;
		ScreenStationaryLocation.Y /= SizeY;
	} 
	else
	{
		return FVector2D(0.5, 0.5);
	}

	return ScreenStationaryLocation;
}

void AVRCharacter::UpdateBlinkers()
{
	if (RadiusVsVelocity == nullptr) return;

	float Speed = GetVelocity().Size();
	float Radius = RadiusVsVelocity->GetFloatValue(Speed);

	if (BlinkerMaterialInstance)
	{
		BlinkerMaterialInstance->SetScalarParameterValue(TEXT("Radius"), Radius);

		FVector2D Center = GetBlinkerCenter();

		BlinkerMaterialInstance->SetVectorParameterValue(TEXT("Center"), FLinearColor(Center.X, Center.Y, 0));
	}
}

void AVRCharacter::DrawTeleportPath(const TArray<FVector>& Path)
{
	UpdateSpline(Path);

	for (USplineMeshComponent* SplineMesh : TeleportPathMeshPool)
	{
		SplineMesh->SetVisibility(false);
	}

	int32 SegmentNum = Path.Num() - 1;

	for (int32 i = 0; i < SegmentNum; ++i)
	{
		if (TeleportPathMeshPool.Num() <= i)
		{
			USplineMeshComponent* SplineMesh = NewObject<USplineMeshComponent>(this);
			SplineMesh->SetMobility(EComponentMobility::Movable);
			SplineMesh->AttachToComponent(TeleportPath, FAttachmentTransformRules::KeepRelativeTransform);
			SplineMesh->SetStaticMesh(TeleportArchMesh);
			SplineMesh->SetMaterial(0, TeleportArchMaterial);
			SplineMesh->RegisterComponent();
			
			TeleportPathMeshPool.Add(SplineMesh);
		}

		USplineMeshComponent* SplineMesh = TeleportPathMeshPool[i];
		SplineMesh->SetVisibility(true);

		FVector StartPos, StartTangent, EndPos, EndTangent;
		
		TeleportPath->GetLocalLocationAndTangentAtSplinePoint(i, StartPos, StartTangent);
		TeleportPath->GetLocalLocationAndTangentAtSplinePoint(i + 1, EndPos, EndTangent);

		SplineMesh->SetStartAndEnd(StartPos, StartTangent, EndPos, EndTangent, true);
	}
}

void AVRCharacter::UpdateSpline(const TArray<FVector> &Path)
{
	TeleportPath->ClearSplinePoints(false);

	for (int32 i = 0; i < Path.Num(); ++i) 
	{
		FVector LocalPosition = TeleportPath->GetComponentTransform().InverseTransformPosition(Path[i]);
		FSplinePoint Point(i, LocalPosition, ESplinePointType::Curve);
		TeleportPath->AddPoint(Point, false);
	}

	TeleportPath->UpdateSpline();
}

void AVRCharacter::SetupPlayerInputComponent(UInputComponent* PlayerInputComponent)
{
	Super::SetupPlayerInputComponent(PlayerInputComponent);

	PlayerInputComponent->BindAxis(TEXT("Forward"), this, &AVRCharacter::MoveForward);
	PlayerInputComponent->BindAxis(TEXT("Right"), this, &AVRCharacter::MoveRight);
	PlayerInputComponent->BindAction(TEXT("Teleport"), IE_Released, this, &AVRCharacter::BeginTeleport);
}

void AVRCharacter::MoveForward(float value)
{
	AddMovementInput(value * Camera->GetForwardVector());
}

void AVRCharacter::MoveRight(float value)
{
	AddMovementInput(value * Camera->GetRightVector());
}

void AVRCharacter::BeginTeleport()
{
	StartFade(0, 1);

	FTimerHandle Handle;

	GetWorldTimerManager().SetTimer(
		Handle, 
		this, 
		&AVRCharacter::FinishTeleport, 
		TeleportFadeTime
	);
}

void AVRCharacter::StartFade(float FromAlpha, float ToAlpha)
{
	PlayerController = PlayerController == nullptr ? Cast<APlayerController>(Controller) : PlayerController;

	if (PlayerController)
	{
		PlayerController->PlayerCameraManager->StartCameraFade(
			FromAlpha,
			ToAlpha,
			TeleportFadeTime,
			FLinearColor::Black
		);
	}
}

void AVRCharacter::FinishTeleport()
{
	FVector Destination = DestinationMarker->GetComponentLocation();
	Destination += GetCapsuleComponent()->GetScaledCapsuleHalfHeight() * GetActorUpVector();

	SetActorLocation(Destination);

	StartFade(1, 0);
}

