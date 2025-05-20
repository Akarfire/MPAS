// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "MPAS_PhysicsModel.generated.h"



// Constains data about a Physics Constraint, connecting elements in the physics model
USTRUCT(BlueprintType)
struct FMPAS_PhysicsModelConstraintData
{
	GENERATED_USTRUCT_BODY()

	// Pointer to the constraint componenet
	UPROPERTY(EditAnywhere, BlueprintReadWrite)
	class UPhysicsConstraintComponent* ConstraintComponent;

	// Pointer to the first connected physics element
	UPROPERTY(EditAnywhere, BlueprintReadWrite)
	class UPrimitiveComponent* Body_1;

	// Pointer to the second connected physics element
	UPROPERTY(EditAnywhere, BlueprintReadWrite)
	class UPrimitiveComponent* Body_2;

	FMPAS_PhysicsModelConstraintData(class UPhysicsConstraintComponent* InConstraintComponent = nullptr, class UPrimitiveComponent* InBody_1 = nullptr, class UPrimitiveComponent* InBody_2 = nullptr) : ConstraintComponent(InConstraintComponent), Body_1(InBody_1), Body_2(InBody_2) {}
};


// Constains an array of FMPAS_PhysicsModelConstraintData to be stored in a map
USTRUCT(BlueprintType)
struct FMPAS_ElementPhysicsConstraints
{
	GENERATED_USTRUCT_BODY()

	UPROPERTY(EditAnywhere, BlueprintReadWrite)
	TArray<FMPAS_PhysicsModelConstraintData> Constraints;
};


// Constains an array of UMPAS_PhysicsModelElement pointers to be stored in a map
USTRUCT(BlueprintType)
struct FMPAS_PhysicsElementsArray
{
	GENERATED_USTRUCT_BODY()

	UPROPERTY(EditAnywhere, BlueprintReadWrite)
	TArray<class UMPAS_PhysicsModelElement*> Elements;

	FMPAS_PhysicsElementsArray() {}
	FMPAS_PhysicsElementsArray(const TArray<class UMPAS_PhysicsModelElement*>& InElements) : Elements(InElements) {}
};


// Type of physics element's attachement
UENUM(BlueprintType)
enum class EMPAS_PhysicsModelParentType : uint8
{
	// Physics element will be directly attached to the rig element
	RigElement UMETA(DisplayName = "Rig Element"),

	// Physics element will be attached to another physics element, associated with this rig element, use ParentPhsicsElementID to specify parent physics element
	ChainedPhysicsElement UMETA(DisplayName = "Chained Physics Element")
};


/*
 * Physics model attachement type:
 *		Hard - the element is directly attached to the other one, making them act as one big physics object;
 *		Limited - the element is attached via a physics constraint, limiting it's motion to a sphere around the parent;
 *		Free - the element is not attached to its parent, thus its able to freely move around.
*/
UENUM(BlueprintType)
enum class EMPAS_PhysicsModelAttachmentType : uint8
{
	Hard UMETA(DisplayName = "Hard"),
	LimitedPosition UMETA(DisplayName = "Limited Position"),
	LimitedRotation UMETA(DisplayName = "Limited Rotation"),
	LimitedPositionRotation UMETA(DisplayName = "Limited Position & Rotation"),
	Free UMETA(DisplayName = "Free")
};



/**
 * This class is used to approximate physics behaviour of MPAS_RigElement when physics model is enabled (typically when rig is in unstable state)
 */
UCLASS(Blueprintable, ClassGroup = (Custom), meta = (BlueprintSpawnableComponent))
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
	UFUNCTION(BlueprintCallable, BlueprintPure, Category = "MPAS|PhysicsModel|PhysicsModelElement")
	class UMPAS_RigElement* GetRigElement() { return RigElement; }

	// Returns a pointer to the corresponding rig element
	UFUNCTION(BlueprintCallable, BlueprintPure, Category = "MPAS|PhysicsModel|PhysicsModelElement")
	EMPAS_PhysicsModelAttachmentType GetParentAttachmentType() { return ParentAttachmentType; }
};




USTRUCT(BlueprintType)
struct FMPAS_PhysicsElementConfiguration
{
	GENERATED_USTRUCT_BODY()

	// Class of the Physics Model Element generated for this element
	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = "PhysicsModel")
	TSubclassOf<UMPAS_PhysicsModelElement> PhysicsElementClass = UMPAS_PhysicsModelElement::StaticClass();

	// Type of physics element's attachement
	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = "PhysicsModel")
	EMPAS_PhysicsModelParentType ParentType;

	/*
	 * USED IF ParentType is set to ChainedPhysicsElement : specifies what physics element (associated with this rig element) is going to be the parent of this physics element
	 * NOTE: ParentPhysicsElementID must be SMALLER then the index of the this physics element configuration, WILL BE IGNORE OTHERWISE!
	 */
	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = "PhysicsModel")
	int32 ParentPhysicsElementID = -1;

	// The type of the element's physical attachment to its parent, determines the generation process and the behavior of the Physics Model
	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = "PhysicsModel")
	EMPAS_PhysicsModelAttachmentType ParentPhysicalAttachmentType = EMPAS_PhysicsModelAttachmentType::Hard;

	// Disables collision of the physics element with it's parent, can fix some weird behavior
	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = "PhysicsModel")
	bool DisableCollisionWithParent = false;

	// Mesh, used to approximate element's physics when physics model is used
	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = "PhysicsModel")
	class UStaticMesh* PhysicsMesh = nullptr;

	// Controls the transofrm of the physics element relative to the rig element
	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = "PhysicsModel")
	FTransform PhysicsMeshRelativeTransform;

	// Mass, used as a parameter for physics model
	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = "PhysicsModel")
	float Mass = 100.f;

	// Air drag parameter, used by the physics model
	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = "PhysicsModel")
	float AirDrag = 1.f;

	// Used for Limited Attachement Type, defines per axis limits for the constraint
	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = "PhysicsModel")
	FVector PhysicsConstraintLimits = FVector(100, 100, 100);

	// Used for Limited Attachement Type, defines per axis angular limits for the constraint
	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = "PhysicsModel")
	FRotator PhysicsAngularLimits = FRotator(90, 90, 90);

	// Smooths out position changes, if set to 0, no interpolation is applied
	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = "PhysicsModel")
	float PhysicsModelPositionInterpolationSpeed = 10.f;

	// Smooths out rotation changes, if set to 0, no interpolation is applied
	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = "PhysicsModel")
	float PhysicsModelRotationInterpolationSpeed = 10.f;

	// ID of the position stack, created for this physics element
	UPROPERTY(BlueprintReadOnly, Category = "PhysicsModel")
	int32 PositionStackID = -1;

	// ID of the rotation stack, created for this physics element
	UPROPERTY(BlueprintReadOnly, Category = "PhysicsModel")
	int32 RotationStackID = -1;


	FMPAS_PhysicsElementConfiguration() {}
};



/**
 * 
 */
class MPAS_API MPAS_PhysicsModel
{
public:
	MPAS_PhysicsModel();
	~MPAS_PhysicsModel();

};
