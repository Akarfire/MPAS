// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "MPAS_MotionRigElement.h"
#include "MPAS_Leg.generated.h"

/**
 * 
 */
UCLASS( ClassGroup=(Custom), meta=(BlueprintSpawnableComponent) )
class MPAS_API UMPAS_Leg : public UMPAS_MotionRigElement
{
	GENERATED_BODY()

protected:

	// The id of the location layer, registered in the parent element. The layer sets the location of the parent element to be at the average positon of all active legs
	int32 LegEffectorLayerID;

	// The id of the location layer, that sets the absolute location of the leg 
	int32 SelfAbsoluteLocationLayerID;

	// ID of a vector stack, where the target location is calculated
	int32 LegTargetLocationStackID;

	// ID of a vector stack, where effector shift is calculated
	int32 EffectorShiftStackID;

	// Whether the leg is placed properly (used in determining whether the element is active or not)
	bool ValidPlacement;

	// Whether the leg is ready to make a step
	bool ReadyToStep;

	// When the leg is ready to make a step, but it has to wait for it's turn
	bool WaitingOnLegGroup;

	// Wether the leg has moved in current window (the time frame between CurrentLegGroup value changes)
	bool HasMovedInCurrentWindow;

	// Leg's target location relative to the the parent element
	FVector LegTargetOffset;

	// Leg's default location relative to the parent element
	FVector LegRestingPoseOffset;

	// Effector Shift uses interpolation, THIS is the current value of Effector Shift
	FVector RealEffectorShift;

	// Step animation
	bool IsMoving;
	FVector StepAnimationStartLocation;
	FVector StepAnimationTargetLocation;
	float StepDistance;

	// Parent body pointer
	class UMPAS_BodySegment* ParentBody;


	// Intention Driven Parameter cached values
	
	float StepLengthMultiplier = 1.f;
	float StepTriggerDistanceMultiplier = 1.f;
	float AnimationSpeedMultiplier = 1.f;
	float SpeedMultiplier = 1.f;


public:
	UMPAS_Leg();

	// Foot placement tracing
	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category="Default|FootPlacement")
	float MaxFootElevation = 100.f;

	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category="Default|FootPlacement")
	float MaxFootVerticalExtent = 200.f;

	// Leg's offset in inactive mode, relative to parent
	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category="Default|FootPlacement")
	FVector InactiveOffset = FVector::Zero();

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

	// Offset in time from the end of the step animation, when the leg group assumes the step to be finished
	// Useful for running, where the next group needs to start BEFORE the current one actually finishes
	// NOTE: only change this in editor, in runtime use a function to change this value!
	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = "Default|Step")
	float StepFinishTimeOffset = 0.0f;

	// Determines vertical movement of the leg during step animation, must start with 0, peack to 1 and end with 0
	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category="Default|Step")
	class UCurveFloat* StepAnimationHeightCurve = nullptr;

	// Determines horizontal movement of the leg during step animation, must start with 0 and end with 1
	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category="Default|Step")
	class UCurveFloat* StepAnimationExtentCurve = nullptr;

	// Determines how step height scales with the lenght of the step, recommended to keep it between 0.1 and 1
	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category="Default|Step")
	class UCurveFloat* StepHeightScalingCurve = nullptr;


	// Effector location shift minimal limitation
	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = "Default|ParentPlacement")
	FVector EffectorShift_Min = FVector(-100, -100, -50);

	// Effector location shift maximal limitation
	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = "Default|ParentPlacement")
	FVector EffectorShift_Max = FVector(100, 100, 200);

	// How fast the leg responds to changes in Effector Shift
	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = "Default|ParentPlacement")
	float EffectorShiftInterpolationSpeed = 10.f;


	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = "Default|Advanced")
	int32 EffectorLayerPriority = 1;

	//UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = "Default|Advanced")
	//float BoneTransformSyncingDistanceSquaredThreshold = 100.f;


protected:
	
	// Returns the vertical direction, relative to the host
	FVector GetUpVector() { return FVector(0.f, 0.f, 1.f); };

	// Returns leg's target location
	FVector GetTargetLocation();

	// Checks whether the leg should make a step
	bool ShouldStep();

	// Starts the step animation timeline
	void StartStepAnimation();


public:

	// Casts a trace that finds a suitable foot location, (0, 0, 0) if failed
	UFUNCTION(BlueprintCallable, Category = "MPAS|Elements|Leg")
	FVector FootTrace(const FVector& StepTargetLocation);

	// Returns initial leg's location relative to it's parent element
	UFUNCTION(BlueprintCallable, BlueprintPure, Category = "MPAS|Elements|Leg")
	FVector GetLegTargetOffset() { return LegTargetOffset; }

	// Whether the leg is ready to make a step
	UFUNCTION(BlueprintCallable, BlueprintPure, Category = "MPAS|Elements|Leg")
	bool IsReadyToStep() { return ReadyToStep; }

	// Whether the leg is moving at this moment
	UFUNCTION(BlueprintCallable, BlueprintPure, Category = "MPAS|Elements|Leg")
	bool IsLegMoving() { return IsMoving; }

	// Offset in time from the end of the step animation, when the leg group assumes the step to be finished
	// Useful for running, where the next group needs to start BEFORE the current one actually finishes
	UFUNCTION(BlueprintCallable, Category = "MPAS|Elements|Leg")
	void SetStepFinishTimeOffset(float newOffset);

	// Whether this element is currently active=
	virtual bool GetRigElementActive_Implementation() { return Enabled && ValidPlacement; }

	

// BONE TRANSFORM SYNCING
protected:

	// Bone Transform Syncing
	int32 BoneTransformSync_LocationLayerID;
	int32 BoneTransformSync_RotationLayerID;

	// Counts time after the latest change in fetched bone transform deltas before offset realocation shall start
	float BoneTransformSync_Timer;

	FVector BoneTransformSync_AppliedBoneLocationOffset = FVector::ZeroVector;
	FQuat BoneTransformSync_AppliedBoneAngularOffset = FQuat::Identity;

public:

	// Foot bone name
	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = "Default|BoneTransformSync")
	FName FootBone;

	// Priority of "BoneTransformSync" layers in default location and default rotation stacks
	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = "Default|BoneTransformSync")
	int32 BoneTransformSyncingLayerPriority = 1;


	// Mimiimal fetched transform location delta size that is considered "modifed"
	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = "Default|BoneTransformSync")
	float BoneTransformSync_LocationDeltaSensitivityThreshold = 2.f;

	// Mimiimal fetched transform rotation delta size that is considered "modifed"
	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = "Default|BoneTransformSync")
	float BoneTransformSync_AngularDeltaSensitivityThreshold = 1.f;

	// The ammount of time that needs to pass after the latest change in fetched bone transform deltas before offset realocation will start
	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = "Default|BoneTransformSync")
	float BoneTransformSync_Timeout = 1.f;

	// How fast applied bone transform offsets will be transfered into bone trasnform sync layer during offset realocation 
	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = "Default|BoneTransformSync")
	float BoneTransformSync_OffsetLocationRealocationSpeed = 10.f;

	// How fast applied bone transform offsets will be transfered into bone trasnform sync layer during offset realocation 
	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = "Default|BoneTransformSync")
	float BoneTransformSync_OffsetAngularRealocationSpeed = 10.f;



// INTENTION DRIVEN
public:

	// Returns ID of a vector stack, where leg target location is calculated
	// Leg target location is a vector, that determines the location, where the leg is intended to be placed
	UFUNCTION(BlueprintCallable, BlueprintPure, Category = "MPAS|Elements|Leg")
	int32 GetTargetLocationStackID() { return LegTargetLocationStackID; }

	// Returns ID of a vector stack, where effector shift is calculated
	// Effector shift is the offset, that is added to the leg's location before finding the average of all legs
	UFUNCTION(BlueprintCallable, BlueprintPure, Category = "MPAS|Elements|Leg")
	int32 GetEffectorShiftStackID() { return EffectorShiftStackID; }


	// CALLED BY THE HANDLER
	// Initializing Rig Element
	virtual void InitRigElement(class UMPAS_Handler* InHandler) override;

	// CALLED BY THE HANDLER : Contains the logic that links this element with other elements in the rig
	virtual void LinkRigElement(class UMPAS_Handler* InHandler) override;

	// Updating Rig Element every tick
	virtual void UpdateRigElement(float DeltaTime) override;

	// CALLED BY THE HANDLER : Synchronizes Rig Element to the most recently fetched bone transforms
	virtual void SyncToFetchedBoneTransforms(float DeltaTime) override;


	// CALLED BY THE HANDLER : NOTIFICATION Called when a subscribed-to parameter is changed
	UFUNCTION()
	void OnParameterChanged(FName InParameterName);

	// CALLED BY THE HANDLER : NOTIFICATION Called when a subscribed-to timeline is updated
	UFUNCTION()
	void OnStepAnimationTimelineUpdated(FName InTimelineName, float CurrentTime);

	// CALLED BY THE HANDLER : NOTIFICATION Called when a subscribed-to timeline is finished
	UFUNCTION()
	void OnStepAnimationTimelineFinished(FName InTimelineName);

	// CALLED BY THE HANDLER : NOTIFICATION Called when a subscribed-to timeline is finished
	UFUNCTION()
	void OnStepAnimationTimelineNotify(FName InTimelineName, FName InNotifyName);

	// DEBUG

	// Calls FootTrace for debugging purposes
	UFUNCTION(BlueprintCallable)
	FVector DEBUG_FootTrace() { return FootTrace(GetTargetLocation()); }
};
