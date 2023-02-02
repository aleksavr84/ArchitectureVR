// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "MotionControllerComponent.h"
#include "HandController.generated.h"

UCLASS()
class ARCHITECTUREVR_API AHandController : public AActor
{
	GENERATED_BODY()
	
public:	
	AHandController();
	
protected:
	virtual void BeginPlay() override;

public:	
	virtual void Tick(float DeltaTime) override;

private:
	// Default sub object
	UPROPERTY(VisibleAnywhere)
	UMotionControllerComponent* MotionController;


public:
	void SetHand(EControllerHand Hand) { MotionController->SetTrackingSource(Hand); }
	
};
