// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "Default/RigElements/MPAS_MotionRigElement.h"
#include "MPAS_Crawler.generated.h"

/**
 * 
 */
UCLASS()
class MPAS_API UMPAS_Crawler : public UMPAS_MotionRigElement
{
	GENERATED_BODY()

protected:

	// ...

public:
	UMPAS_Crawler() {}

	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = "Default|Movement")
	float MaxSpeed = 100.f;

	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = "Default|Movement")
	float Acceleration = 25.f;

	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = "Default|Movement")
	float GroundFriction = 10.f;

	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = "Default|Movement")
	FVector MovementAxes = FVector::OneVector;


	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = "Default|GroundCheck")
	float GroundCheckDistance = 100.f;

	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = "Default|GroundCheck")
	TEnumAsByte<ECollisionChannel> GroundCheckCollisionChannel = ECollisionChannel::ECC_Visibility;


protected:

	// ...

public:



	
	// CALLED BY THE HANDLER
	// Initializing Rig Element
	virtual void InitRigElement(class UMPAS_Handler* InHandler) override;

	// CALLED BY THE HANDLER : Contains the logic that links this element with other elements in the rig
	virtual void LinkRigElement(class UMPAS_Handler* InHandler) override;

	// Updating Rig Element every tick
	virtual void UpdateRigElement(float DeltaTime) override;
};
