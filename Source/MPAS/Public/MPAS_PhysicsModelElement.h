// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "Components/StaticMeshComponent.h"
#include "MPAS_PhysicsModelElement.generated.h"


/* 
 * Physics model attachement type:
 *		Hard - the element is directly attached to the other one, making them act as one big physics object;
 *		Limited - the element is attached via a physics constraint, limiting it's motion to a sphere around the parent;
 *		Free - the element is not attached to its parent, thus its able to freely move around.
*/
UENUM(BlueprintType)
enum class EMPAS_PhysicsModelAttachmentType : uint8
{
	Hard UMETA(DisplayName="Hard"),
	LimitedPosition UMETA(DisplayName="Limited Position"),
	LimitedRotation UMETA(DisplayName="Limited Rotation"),
	LimitedPositionRotation UMETA(DisplayName="Limited Position & Rotation"),
	Free UMETA(DisplayName="Free")
};


/**
 * This class is used to approximate physics behaviour of MPAS_RigElement when physics model is enabled (typically when rig is in unstable state)
 */
UCLASS(Blueprintable, ClassGroup=(Custom), meta=(BlueprintSpawnableComponent))
class MPAS_API UMPAS_PhysicsModelElement : public UStaticMeshComponent
{
	GENERATED_BODY()

protected:

	// IDs of Position and Rotation physics stacks in the corresponding rig element
	int32 TargetPositionStackID;
	int32 TargetRotationStackID;

	// Transform of the Physics Element relative to the rig element
	FTransform RelativeTransform;

	// The type of the element's physical attachment to its parent
	EMPAS_PhysicsModelAttachmentType ParentAttachmentType;

	// If this flag is set to 'true', then the next update the physics will be enabled for this element
	bool TriggerPhysicsNextUpdate;

	// Sets the postion and rotation of this physics element to the one desired by the associated rig element
	void FetchElementPositionFromRigElement();

public:
	UMPAS_PhysicsModelElement();

	// A pointer to the corresponding rig element
	UPROPERTY(BlueprintReadOnly, VisibleAnywhere)
	class UMPAS_RigElement* RigElement;

	// Index of the physics element in associated rig element's physics model configuration
	UPROPERTY(BlueprintReadOnly, VisibleAnywhere)
	int32 PhysicsElementIndex;
	

	// CALLED BY THE HANDLER : initializes physics model element based on data from the corresponding rig element
	void InitPhysicsElement(class UMPAS_RigElement* InRigElement, int32 InPhysicsElementIndex);

	// CALLED BY THE HANDLER : updates the physics element, sends data to the coressponding rig element. Called every handler update when the physics model is active
	void UpdatePhysicsElement(float DeltaTime);

	// CALLED BY THE RIG ELEMENT: called when physics model is enabled for the corresponding rig element
	void OnPhysicsModelEnabled();

	// CALLED BY THE RIG ELEMENT: called when physics model is disabled for the corresponding rig element
	void OnPhysicsModelDisabled();
	
	// CALLED BY THE RIG ELEMENT: called when this exact element does not have physics model enabled but has a child that does
	// Basically, it just enables collision, without enabling physics simulation
	void OnPhysicsModelSemiEnabled();

	// Returns a pointer to the corresponding rig element
	UFUNCTION(BlueprintCallable, BlueprintPure, Category="MPAS|PhysicsModel|PhysicsModelElement")
	class UMPAS_RigElement* GetRigElement() { return RigElement; }

	// Returns a pointer to the corresponding rig element
	UFUNCTION(BlueprintCallable, BlueprintPure, Category="MPAS|PhysicsModel|PhysicsModelElement")
	EMPAS_PhysicsModelAttachmentType GetParentAttachmentType() { return ParentAttachmentType; }
};
