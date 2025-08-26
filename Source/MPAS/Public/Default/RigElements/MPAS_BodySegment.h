// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "MPAS_RigElement.h"
#include "MPAS_BodySegment.generated.h"

/**
 * 
 */
UCLASS( ClassGroup=(Custom), meta=(BlueprintSpawnableComponent) )
class MPAS_API UMPAS_BodySegment : public UMPAS_RigElement
{
	GENERATED_BODY()

protected:

	// Desired transform stack IDs

	int32 DesiredLocationStackID = -1;
	int32 DesiredRotationStackID = -1;


	// Cached desired transform values

	FVector CachedDesiredLocation;
	FRotator CachedDesiredRotation;

public:
	UMPAS_BodySegment();

	// The bone of the body, controlled by this segment
	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = "Default")
	FName BoneName;

	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = "Default|Orientation")
	bool UseCoreRotation;

	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = "Default|Orientation")
	float LiniarRotationInterpolationSpeed = 3.f;


public:
	// CALLED BY THE HANDLER : Initializing Rig Element
	virtual void InitRigElement(class UMPAS_Handler* InHandler) override;

	// CALLED BY THE HANDLER :  Updating Rig Element every tick
	virtual void UpdateRigElement(float DeltaTime) override;


	// Returns the location, where the body needs to be placed
	UFUNCTION(BlueprintCallable, BlueprintPure, Category = "MPAS|BodySegment")
	FVector GetDesiredLocation() { return CachedDesiredLocation; }

	// Returns the rotation, by which the body needs to be rotated
	UFUNCTION(BlueprintCallable, BlueprintPure, Category = "MPAS|BodySegment")
	FRotator GetDesiredRotation() { return CachedDesiredRotation; }

};
