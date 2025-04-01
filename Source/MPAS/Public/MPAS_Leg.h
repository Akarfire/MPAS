// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "MPAS_RigElement.h"
#include "MPAS_Leg.generated.h"

/**
 * 
 */
UCLASS( ClassGroup=(Custom), meta=(BlueprintSpawnableComponent) )
class MPAS_API UMPAS_Leg : public UMPAS_RigElement
{
	GENERATED_BODY()

protected:

	// The id of the position layer, registered in the parent element. The layer sets the position of the parent element to be at the average positon of all active legs
	int32 LegEffectorLayerID;

	// The id of the position layer, that sets the absolute position of the leg 
	int32 SelfAbsolutePositionLayerID;

	// Whether the leg is ready to make a step
	bool ReadyToStep;

	// When the leg is ready to make a step, but it has to wait for it's turn
	bool WaitingOnLegGroup;

	// Wether the leg has moved in current window (the time frame between CurrentLegGroup value changes)
	bool HasMovedInCurrentWindow;

	// Leg's target position relative to the core
	FVector LegTargetOffset;

	// Leg's default position relative to the parent element
	FVector LegRestingPoseOffset;

	// Step animation
	bool IsMoving;
	FVector StepAnimationStartLocation;
	FVector StepAnimationTargetLocation;
	float StepDistance;


	// Intention Driven Parameter cached values
	
	float StepLengthMultiplier = 1.f;
	float StepTriggerDistanceMultiplier = 1.f;
	float ParentElevationMultiplier = 1.f;
	float AnimationSpeedMultiplier = 1.f;
	float SpeedMultiplier = 1.f;

public:
	UMPAS_Leg();
	
	// Foot bone name
	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category="Default")
	FName FootBone;

	// Foot placement tracing
	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category="Default|FootPlacement")
	float MaxFootElevation = 100.f;

	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category="Default|FootPlacement")
	float MaxFootVerticalExtent = 200.f;

	// Step
	UPROPERTY(BlueprintReadOnly, Category=Background)
	FName StepTimelineName;

	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category="Default|Step")
	int32 LegGroup = 0;

	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category="Default|Step")
	float StepLength = 200.f;

	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category="Default|Step")
	float StepTriggerDistance = 100.f;

	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category="Default|Step")
	float StepHeight = 100.f;

	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category="Default|Step")
	float StepAnimationDuration = 1.f;

	// Determines vertical movement of the leg during step animation, must start with 0, peack to 1 and end with 0
	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category="Default|Step")
	class UCurveFloat* StepAnimationHeightCurve = nullptr;

	// Determines horizontal movement of the leg during step animation, must start with 0 and end with 1
	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category="Default|Step")
	class UCurveFloat* StepAnimationExtentCurve = nullptr;

	// Determines how step height scales with the lenght of the step, recommended to keep it between 0.1 and 1
	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category="Default|Step")
	class UCurveFloat* StepHeightScalingCurve = nullptr;

	// Effector positioning
	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category="Default|ParentPlacement")
	float ParentVerticalOffset;
	
	// Leg's offset in inactive mode, relative to parent
	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category="Default|Stability")
	FVector InactiveOffset = FVector();

protected:
	
	// Returns the vertical direction, relative to the host
	FVector GetUpVector() { return FVector(0.f, 0.f, 1.f); };

	// Returns leg's target position
	FVector GetTargetPosition();

	// Casts a trace that finds a suitable foot position, (0, 0, 0) if failed
	FVector FootTrace(const FVector& StepTargetPosition);

	// Checks whether the leg should make a step
	bool ShouldStep();

	// Starts the step animation timeline
	void StartStepAnimation();


public:

	// Whether the leg is ready to make a step
	UFUNCTION(BlueprintCallable, BlueprintPure)
	bool IsReadyToStep() { return ReadyToStep; }

	// Whether the leg is moving at this moment
	UFUNCTION(BlueprintCallable, BlueprintPure)
	bool IsLegMoving() { return IsMoving; }

	// CALLED BY THE HANDLER
	// Initializing Rig Element
	virtual void InitRigElement(class UMPAS_Handler* InHandler) override;

	// CALLED BY THE HANDLER : Contains the logic that links this element with other elements in the rig
	virtual void LinkRigElement(class UMPAS_Handler* InHandler) override;

	// Updating Rig Element every tick
	virtual void UpdateRigElement(float DeltaTime) override;


	// CALLED BY THE HANDLER : NOTIFICATION Called when a subscribed-to parameter is changed
	UFUNCTION()
	void OnParameterChanged(FName InParameterName);

	// CALLED BY THE HANDLER : NOTIFICATION Called when a subscribed-to timeline is updated
	UFUNCTION()
	void OnStepAnimationTimelineUpdated(FName InTimelineName, float CurrentTime);

	// CALLED BY THE HANDLER : NOTIFICATION Called when a subscribed-to timeline is finished
	UFUNCTION()
	void OnStepAnimationTimelineFinished(FName InTimelineName);

	// Called when physics model is disabled for this element
	virtual void OnPhysicsModelDisabled_Implementation() override;

	// DEBUG

	// Calls FootTrace for debugging purposes
	UFUNCTION(BlueprintCallable)
	FVector DEBUG_FootTrace() { return FootTrace(GetTargetPosition()); }
};
