// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "Default/RigElements/MPAS_MotionRigElement.h"
#include "MPAS_Crawler.generated.h"

/**
 * 
 */
UCLASS(ClassGroup = (Custom), meta = (BlueprintSpawnableComponent))
class MPAS_API UMPAS_Crawler : public UMPAS_MotionRigElement
{
	GENERATED_BODY()

protected:

	// ID of a layer inside of the default location stack, that is responsible for the crawler's world location
	int32 SelfAbsoluteLocationLayer;

	// ID of a layer inside of the parent element's default location stack, that is responsible for applying crawler's location to the parent element
	int32 ParentLocationEffectorLayer;

	int32 TargetLocationStackID;

	// Default offset of the parent element, relative to the crawler
	FVector ParentOffset;
	
	// Current velocity of the crawler (the element is moved according to this velocity)
	FVector MovementVelocity;

	// Whether the crawler has found ground underneath itself
	bool IsGrounded = false;

	// Pointer to a parent body segment (or nullptr if parent is not UMPAS_BodySegment)
	class UMPAS_BodySegment* ParentBody;


public:
	UMPAS_Crawler() {}

	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = "Default|Movement")
	float MaxSpeed = 100.f;

	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = "Default|Movement")
	float Acceleration = 25.f;

	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = "Default|Movement")
	FVector MovementAxes = FVector::OneVector;

	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = "Default|Movement")
	float MovementTriggerDistance = 10.f;

	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = "Default|Movement")
	float GroundFriction = 10.f;

	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = "Default|Movement")
	float BreakingFriction = 10.f;


	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = "Default|GroundCheck")
	float GroundCheckDistance = 100.f;

	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = "Default|GroundCheck")
	TEnumAsByte<ECollisionChannel> GroundCheckCollisionChannel = ECollisionChannel::ECC_Visibility;


protected:

	// Checks if there is ground under the crawler
	bool GroundCheck(FHitResult& Hit);

	// Returns the location, that the crawler is supposed to assume
	FVector GetTargetLocation();

	// Per-frame movement logic of the crawler
	void UpdateMovement(float DeltaTime);


public:

	// Whether the crawler has found ground underneath itself
	UFUNCTION(BlueprintCallable, BlueprintPure, Category = "MPAS|Elements|Crawler")
	bool GetIsGrounded() { return IsGrounded; }

	// Returns id of a vector stack, where the target location is calculated
	UFUNCTION(BlueprintCallable, BlueprintPure, Category = "MPAS|Elements|Crawler")
	int32 GetTargetLocationStackID() { return TargetLocationStackID; }

	
	// CALLED BY THE HANDLER
	// Initializing Rig Element
	virtual void InitRigElement(class UMPAS_Handler* InHandler) override;

	// CALLED BY THE HANDLER : Contains the logic that links this element with other elements in the rig
	virtual void LinkRigElement(class UMPAS_Handler* InHandler) override;

	// Updating Rig Element every tick
	virtual void UpdateRigElement(float DeltaTime) override;
};
