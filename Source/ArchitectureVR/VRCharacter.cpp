
#include "VRCharacter.h"
#include "Camera/CameraComponent.h"
#include "Components/StaticMeshComponent.h"
#include "TimerManager.h"
#include "GameFramework/Controller.h"
#include "Components/CapsuleComponent.h"
#include "NavigationSystem.h"
#include "Components/PostProcessComponent.h"
#include "Materials/MaterialInstanceDynamic.h"
#include "MotionControllerComponent.h"

AVRCharacter::AVRCharacter()
{
	PrimaryActorTick.bCanEverTick = true;

	VRRoot = CreateDefaultSubobject<USceneComponent>(FName("VRRoot"));
	VRRoot->SetupAttachment(GetRootComponent());

	Camera = CreateDefaultSubobject<UCameraComponent>(FName("Camera"));
	Camera->SetupAttachment(VRRoot);

	LeftController = CreateDefaultSubobject<UMotionControllerComponent>(FName("LeftController"));
	LeftController->SetupAttachment(VRRoot);
	LeftController->SetTrackingSource(EControllerHand::Left);
	LeftController->bDisplayDeviceModel = true;

	RightController = CreateDefaultSubobject<UMotionControllerComponent>(FName("RightController"));
	RightController->SetupAttachment(VRRoot);
	RightController->SetTrackingSource(EControllerHand::Right);
	RightController->bDisplayDeviceModel = true;

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

		//BlinkerMaterialInstance->SetScalarParameterValue(FName("Radius"), 0.2f);
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

bool AVRCharacter::FindTeleportDestination(FVector& OutLocation)
{

	FVector Start = RightController->GetComponentLocation();
	FVector Look = RightController->GetForwardVector();
	Look = Look.RotateAngleAxis(ControllerRotation, RightController->GetRightVector());

	FVector End = Start + Look * MaxTeleportDistance;
	FHitResult HitResult;

	bool bHit = GetWorld()->LineTraceSingleByChannel(
		HitResult,
		Start,
		End,
		ECC_Visibility
	);

	if (!bHit) return false;

	FNavLocation NavLocation;

	bool bOnNavMesh = UNavigationSystemV1::GetCurrent(GetWorld())->ProjectPointToNavigation(
		HitResult.Location,
		NavLocation,
		TeleportProjectionExtent
	);

	if (!bOnNavMesh) return false;

	OutLocation = NavLocation.Location;

	return bHit && bOnNavMesh;
}

void AVRCharacter::UpdateDestinationMarker()
{
	FVector Location;
	bool bHasDestination = FindTeleportDestination(Location);

	if (bHasDestination)
	{
		DestinationMarker->SetVisibility(true);
		DestinationMarker->SetWorldLocation(Location);
	} 
	else
	{
		DestinationMarker->SetVisibility(false);
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
	SetActorLocation(DestinationMarker->GetComponentLocation() + GetCapsuleComponent()->GetScaledCapsuleHalfHeight());

	StartFade(1, 0);
}

