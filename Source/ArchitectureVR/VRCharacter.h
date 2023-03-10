// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Character.h"
#include "HandController.h"
#include "VRCharacter.generated.h"

UCLASS()
class ARCHITECTUREVR_API AVRCharacter : public ACharacter
{
	GENERATED_BODY()

public:
	AVRCharacter();

protected:
	virtual void BeginPlay() override;

public:	
	virtual void Tick(float DeltaTime) override;

	virtual void SetupPlayerInputComponent(class UInputComponent* PlayerInputComponent) override;

private:
	bool FindTeleportDestination(TArray<FVector> &OutPath, FVector &OutLocation);
	void UpdateDestinationMarker();
	void UpdateBlinkers();
	void DrawTeleportPath(const TArray<FVector> &Path);
	void UpdateSpline(const TArray<FVector> &Path);
	FVector2D GetBlinkerCenter();

	void MoveForward(float Value);
	void MoveRight(float Value);

	void GripLeft() { LeftController->Grip(); }
	void ReleaseLeft() { LeftController->Release(); }

	void GripRight() { RightController->Grip(); }
	void ReleaseRight() { RightController->Release(); }

	void BeginTeleport();
	void StartFade(float FromAlpha, float ToAlpha);
	void FinishTeleport();

private:
	UPROPERTY(VisibleAnywhere)
	class UCameraComponent* Camera;

	UPROPERTY(VisibleAnywhere)
	AHandController* LeftController;

	UPROPERTY(VisibleAnywhere)
	AHandController* RightController;

	UPROPERTY(VisibleAnywhere)
	class USceneComponent* VRRoot;

	UPROPERTY(VisibleAnywhere)
	class USplineComponent* TeleportPath;

	UPROPERTY(VisibleAnywhere)
	class UStaticMeshComponent* DestinationMarker;

	UPROPERTY()
	class UPostProcessComponent* PostProcessComponent;

	UPROPERTY()
	class UMaterialInstanceDynamic* BlinkerMaterialInstance;

	UPROPERTY()
	TArray<class USplineMeshComponent*> TeleportPathMeshPool;

	class APlayerController* PlayerController;

private:
	UPROPERTY(EditAnywhere)
	float TeleportProjectileRadius = 10.f;

	UPROPERTY(EditAnywhere)
	float TeleportProjectileSpeed = 1000.f;

	UPROPERTY(EditAnywhere)
	float TeleportSimulationTime = 2.f;

	UPROPERTY(EditAnywhere)
	float TeleportFadeTime = 1.f;

	UPROPERTY(EditAnywhere)
	FVector TeleportProjectionExtent = FVector(100, 100, 100);

	UPROPERTY(EditAnywhere)
	class UMaterialInterface* BlinkerMaterialBase;

	UPROPERTY(EditAnywhere)
	class UCurveFloat* RadiusVsVelocity;

	UPROPERTY(EditAnywhere)
	float ControllerRotation = 0.f;

	UPROPERTY(EditDefaultsOnly)
	class UStaticMesh* TeleportArchMesh;

	UPROPERTY(EditDefaultsOnly)
	class UMaterialInterface* TeleportArchMaterial;

	UPROPERTY(EditDefaultsOnly)
	TSubclassOf<AHandController> HandControllerClass;

};
