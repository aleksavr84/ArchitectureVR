#include "HandController.h"
#include "GameFramework/Controller.h"
#include "Haptics/HapticFeedbackEffect_Base.h"
#include "VRCharacter.h"
#include "GameFramework/CharacterMovementComponent.h"

AHandController::AHandController()
{
	PrimaryActorTick.bCanEverTick = true;

	MotionController = CreateDefaultSubobject<UMotionControllerComponent>(FName("MotionController"));
	SetRootComponent(MotionController);
	MotionController->bDisplayDeviceModel = true;

}

void AHandController::BeginPlay()
{
	Super::BeginPlay();
	
	OnActorBeginOverlap.AddDynamic(this, &AHandController::ActorBeginOverlap);
	OnActorEndOverlap.AddDynamic(this, &AHandController::ActorEndOverlap);
}

void AHandController::Tick(float DeltaTime)
{
	Super::Tick(DeltaTime);

	if (bIsClimbing)
	{
		FVector HandControllerDelta = GetActorLocation() - ClimbingStartLocation;

		GetAttachParentActor()->AddActorWorldOffset(-HandControllerDelta);
	}
}

void AHandController::PairController(AHandController* Controller)
{
	OtherController = Controller;
	OtherController->OtherController = this;
}

void AHandController::ActorBeginOverlap(AActor* OverlappedActor, AActor* OtherActor)
{
	bool bNewCanClimb = CanClimb();

	if (!bCanClimb && bNewCanClimb)
	{
		//UE_LOG(LogTemp, Warning, TEXT("Can Climb!"));
		PlayerController = PlayerController == nullptr ? Cast<APlayerController>(Cast<AVRCharacter>(GetOwner())->Controller) : PlayerController;

		if (PlayerController && HapticEffect)
		{
			PlayerController->PlayHapticEffect(
				HapticEffect,
				MotionController->GetTrackingSource(),
				1,
				false
			);
		}
	}

	bCanClimb = bNewCanClimb;
}

void AHandController::ActorEndOverlap(AActor* OverlappedActor, AActor* OtherActor)
{
	bCanClimb = CanClimb();
}

bool AHandController::CanClimb() const
{
	TArray<AActor*> OverlappingActors;
	GetOverlappingActors(OverlappingActors);
	
	for (AActor* OverlappingActor : OverlappingActors)
	{
		if (OverlappingActor->ActorHasTag(TEXT("Climbable")))
		{
			return true;
		}
	}

	return false;
}

void AHandController::SetMovementMode(EMovementMode Mode)
{
	PlayerCharacter = PlayerCharacter == nullptr ? Cast<AVRCharacter>(GetOwner()) : PlayerCharacter;

	if (PlayerCharacter)
	{
		PlayerCharacter->GetCharacterMovement()->SetMovementMode(EMovementMode::MOVE_Falling);
	}
}

void AHandController::Grip()
{
	if (bCanClimb) 
	{
		if (!bIsClimbing)
		{
			bIsClimbing = true;
			ClimbingStartLocation = GetActorLocation();

			OtherController->bIsClimbing = false;

			SetMovementMode(EMovementMode::MOVE_Flying);
		}
	}
}

void AHandController::Release()
{
	if (bIsClimbing)
	{
		bIsClimbing = false;

		SetMovementMode(EMovementMode::MOVE_Falling);
	}
}

