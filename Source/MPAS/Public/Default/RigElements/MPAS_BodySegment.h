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

public:
	UMPAS_BodySegment();

	// The bone of the body, controlled by this segment
	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = "Default")
	FName BoneName;

	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = "Default|Orientation")
	bool UseCoreRotation;

	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = "Default|Orientation")
	float LiniarRotationInterpolationSpeed = 3.f;

	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = "Default|Orientation")
	FRotator AdditionalBoneRotation = FRotator::ZeroRotator;

public:

	// Return Additional Rotation value
	UFUNCTION(BlueprintCallable, BlueprintPure, Category = "MPAS|BodySegment")
	const FRotator& GetAdditionalBoneRotation() { return AdditionalBoneRotation; }

	// Modifies Additional Rotation value
	UFUNCTION(BlueprintCallable, Category = "MPAS|BodySegment")
	void SetAdditionalBoneRotation(const FRotator& InRotation) { AdditionalBoneRotation = InRotation; };



// POSITION DRIVING
protected:

	// Desired transform stack IDs
	int32 DesiredLocationStackID = -1;
	int32 DesiredRotationStackID = -1;

	// Cached desired transform values
	FVector CachedDesiredLocation;
	FRotator CachedDesiredRotation;

public:

	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = "Default|PositionDriving")
	float DesiredLocationEnforcement = 1.f;

	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = "Default|PositionDriving")
	FVector LocationEnforcementDirectionalScaling = FVector(1, 1, 1);

	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = "Default|PositionDriving")
	float DesiredRotationEnforcement = 1.f;

public:
	// Returns the location, where the body needs to be placed
	UFUNCTION(BlueprintCallable, BlueprintPure, Category = "MPAS|BodySegment")
	const FVector& GetDesiredLocation() { return CachedDesiredLocation; }

	// Returns the rotation, by which the body needs to be rotated
	UFUNCTION(BlueprintCallable, BlueprintPure, Category = "MPAS|BodySegment")
	const FRotator& GetDesiredRotation() { return CachedDesiredRotation; }



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



// CALLED BY THE HANDLER

public:
	// CALLED BY THE HANDLER : Initializing Rig Element
	virtual void InitRigElement(class UMPAS_Handler* InHandler) override;

	// CALLED BY THE HANDLER :  Updating Rig Element every tick
	virtual void UpdateRigElement(float DeltaTime) override;

	// CALLED BY THE HANDLER : Synchronizes Rig Element to the most recently fetched bone transforms
	virtual void SyncToFetchedBoneTransforms(float DeltaTime) override;

};
