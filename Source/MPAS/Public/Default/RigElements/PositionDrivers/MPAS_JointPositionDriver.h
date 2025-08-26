// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "Default/RigElements/PositionDrivers/MPAS_PositionDriver.h"
#include "MPAS_JointPositionDriver.generated.h"

USTRUCT()
struct FMPAS_JointPositionDriverElementData
{
	// Distance between the joint origin and the attached element (it`s center)
	float Distance;

	// Relative rotation of the joint
	FRotator Rotation;

	// Rotation of the element
	FRotator DefaultElementRotation;
};

/**
 * 
 */
UCLASS(Blueprintable, ClassGroup = (Custom), meta = (BlueprintSpawnableComponent))
class MPAS_API UMPAS_JointPositionDriver : public UMPAS_PositionDriver
{
	GENERATED_BODY()
	
protected:

	// Joint data for each attached element
	TMap<UMPAS_RigElement*, FMPAS_JointPositionDriverElementData> JointsData;

public:
	UMPAS_JointPositionDriver() {}


public:

	// VIRTUAL, Called right after the position driver has gathered data about it's driven elements
	virtual void OnPositionDriverInitialized_Implementation() override;

	// VIRTUAL, Calculates the required transform (location and rotation) for the specified element, 
	// If the specified element is not one of the affected by this Position Driver, (0, 0, 0)-s are returned
	virtual void CalculateElementTransform_Implementation(FVector& OutLocation, FRotator& OutRotation, UMPAS_RigElement* InRigElement) override;



	// INTENTION DRIVING INTEGRATION

	// Outputs Distance and Rotation of the joint for the specified attached element
	UFUNCTION(BlueprintCallable, BlueprintPure, Category="MPAS|Elements|PositionDriver|Joint")
	void GetJointState(float& OutDistance, FRotator& OutRotation, int32 InElementIndex);

	// Updates Distance and Rotation of the joint for the specified attached element
	UFUNCTION(BlueprintCallable, Category = "MPAS|Elements|PositionDriver|Joint")
	void SetJointState(int32 InElementIndex, float InDistance, const FRotator& InRotation);
