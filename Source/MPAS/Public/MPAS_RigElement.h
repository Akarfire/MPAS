// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "Components/SceneComponent.h"
#include "MPAS_PhysicsModelElement.h"
#include "MPAS_RigElement.generated.h"


// LAYERS

// Blending mode for position/rotation/... layers in stacks
UENUM(BlueprintType)
enum class EMPAS_LayerBlendingMode : uint8
{
	Normal UMETA(DisplayName="Normal"),
	Add UMETA(DisplayName="Add"),
	Multiply UMETA(DisplayName="Multiply")
};

// The rule by which the elements in a single layer are combined
UENUM(BlueprintType)
enum class EMPAS_LayerCombinationMode : uint8
{
	Add UMETA(DisplayName="Add"),
	Multiply UMETA(DisplayName="Multiply"),
	Average UMETA(DisplayName="Average")
};


USTRUCT(BlueprintType)
struct FMPAS_VectorLayer
{
	GENERATED_USTRUCT_BODY()

	UPROPERTY(EditAnywhere, BlueprintReadWrite)
	EMPAS_LayerBlendingMode BlendingMode;

	UPROPERTY(EditAnywhere, BlueprintReadWrite)
	EMPAS_LayerCombinationMode CombinationMode;

	UPROPERTY(EditAnywhere, BlueprintReadWrite)
	TMap<UMPAS_RigElement*, FVector> LayerElements;

	UPROPERTY(EditAnywhere, BlueprintReadWrite)
	bool ForceAllElementsActive;

	FMPAS_VectorLayer(EMPAS_LayerBlendingMode InBlendingMode = EMPAS_LayerBlendingMode::Normal, EMPAS_LayerCombinationMode InLayerCombinationMode = EMPAS_LayerCombinationMode::Average, bool InForceAllElementsActive = false): BlendingMode(InBlendingMode), CombinationMode(InLayerCombinationMode), ForceAllElementsActive(InForceAllElementsActive) {}
};

USTRUCT(BlueprintType)
struct FMPAS_RotatorLayer
{
	GENERATED_USTRUCT_BODY()

	UPROPERTY(EditAnywhere, BlueprintReadWrite)
	EMPAS_LayerBlendingMode BlendingMode;

	UPROPERTY(EditAnywhere, BlueprintReadWrite)
	EMPAS_LayerCombinationMode CombinationMode;

	UPROPERTY(EditAnywhere, BlueprintReadWrite)
	TMap<UMPAS_RigElement*, FRotator> LayerElements;

	UPROPERTY(EditAnywhere, BlueprintReadWrite)
	bool ForceAllElementsActive;

	FMPAS_RotatorLayer(EMPAS_LayerBlendingMode InBlendingMode = EMPAS_LayerBlendingMode::Normal, EMPAS_LayerCombinationMode InLayerCombinationMode = EMPAS_LayerCombinationMode::Add, bool InForceAllElementsActive = false): BlendingMode(InBlendingMode), CombinationMode(InLayerCombinationMode), ForceAllElementsActive(InForceAllElementsActive) {}
};


// Type of physics element's attachement
UENUM(BlueprintType)
enum class EMPAS_PhysicsModelParentType : uint8
{
	// Physics element will be directly attached to the rig element
	RigElement UMETA(DisplayName="Rig Element"),

	// Physics element will be attached to another physics element, associated with this rig element, use ParentPhsicsElementID to specify parent physics element
	ChainedPhysicsElement UMETA(DisplayName="Chained Physics Element")
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


// ELEMENT TYPE
UENUM(BlueprintType)
enum class EMPAS_ElementPositionMode : uint8
{
	Normal UMETA(DisplayName="Normal"),
	Independent UMETA(DisplayName="Independent")
};


// STABILITY STATUS
UENUM(BlueprintType)
enum class EMPAS_StabilityStatus : uint8
{
	// Fully stable element, attached to another stable element
	Stable UMETA(DisplayName="Stable"),

	// A stable element, attached to an unstable/semi-stable element 
	SemiStable UMETA(DisplayName="Semi-Stable"),

	// An unstable element
	Unstable UMETA(DisplayName="Unstable")
};


// RIG ELEMENT
UCLASS(Blueprintable, ClassGroup=(Custom), meta=(BlueprintSpawnableComponent) )
class MPAS_API UMPAS_RigElement : public USceneComponent
{
	GENERATED_BODY()

protected:
	// Element's rig's handler
	class UMPAS_Handler* Handler;

	// Parent element of this element, nullptr if Core Element
	UMPAS_RigElement* ParentElement;

	// Initial position and rotation of the element relative to it's parent
	FTransform InitialSelfTransform;

	// If this flag is set, then physics model constaint will be reactivated next frame
	bool ReactivatePhysicsModelConstraintNextFrame;

	// Cached parameter values
	float PositionInterpolationMultiplier = 1.f;
	float RotationInterpolationMultiplier = 1.f;

	// Velocity Calculation
	FVector PreviousFramePosition;
	FVector CachedVelocity;

public:	
	// Sets default values for this component's properties
	UMPAS_RigElement();

	// Whether the element is currently active
	UPROPERTY(BlueprintReadOnly, VisibleAnywhere, Category = "Default")
	bool Active = true;

	// Whether the element should be active on initialization
	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = "Default")
	bool DefaultActive = true;


	// Name of the element in the rig
	UPROPERTY(BlueprintReadOnly, VisibleAnywhere, Category = "Default")
	FName RigElementName;

	// Whether the element's parent is the Core
	UPROPERTY(BlueprintReadOnly, VisibleAnywhere, Category = "Default")
	bool IsCoreElement;

	// Element's positioning mode: Noraml - keeps an offset from it's parent, default position stack is applied; Independent - stack values are not applied, the component is free to move
	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = "Default")
	EMPAS_ElementPositionMode PositioningMode;

	// Smooths out position changes, if set to 0, no interpolation is applied
	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = "Default|Orientation")
	float LocationInterpolationSpeed = 0.f;

	// Smooths out rotation changes, if set to 0, no interpolation is applied
	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = "Default|Orientation")
	float RotationInterpolationSpeed = 0.f;


protected:
	// Called when the game starts
	virtual void BeginPlay() override;

	// NOTIFICATION Called when ORIENTATION_PositionInterpolationMultiplier or ORIENTATION_RotationInterpolationMultiplier parameter value is changed
	void OnInterpolationMultiplierChanged(FName InParameterName);

public:	
	// Called every frame
	virtual void TickComponent(float DeltaTime, ELevelTick TickType, FActorComponentTickFunction* ThisTickFunction) override;

	// Returns Element's rig's Handler ptr
	class UMPAS_Handler* GetHandler() { return Handler; }

	// Makes the element Active / InActive
	UFUNCTION(BlueprintCallable, Category="MPAS|RigElement")
	void SetRigElementActive(bool NewActive) { Active = NewActive; }

	// Whether the element is active or not
	UFUNCTION(BlueprintPure, BlueprintCallable, Category="MPAS|RigElement")
	bool GetRigElementActive() { return Active; }

	// Returns the velocity of the rig element
	UFUNCTION(BlueprintPure, BlueprintCallable, Category="MPAS|RigElement")
	FVector GetVelocity() { return CachedVelocity; }



// POSITION LAYERS
protected:

	// Stacks by layers of which the position of the element is determined
	TArray<TArray<FMPAS_VectorLayer>> LocationStacks;

	// Name maps used to access Stacks and Layers by name (slower than ID)
	TMap<FString, int32> LocationStackNames;
	TArray<TMap<FString, int32>> LocationLayerNames;


protected:

	// Applies the default position stack to the element's world location
	void ApplyDefaultPositionStack(float DeltaTime);

	// Calculates the final position of the given position stack
	FVector CalculatePositionStackValue(int32 InPositionStackID);

	// Calculates the final position of the given position layer
	FVector CalculatePositionLayerValue(const FMPAS_VectorLayer InLayer);


public:
	// Registers a new position stack and returns it's ID, returns an existing ID if the stack is already registered
	UFUNCTION(BlueprintCallable, Category="MPAS|RigElement|PositionStacks")
	int32 RegisterPositionStack(const FString& InStackName);

	// Registers a new position layer in the given stack and returns it's ID, returns an existing ID if the layer is already registered, returns -1 if Stack does not exist
	UFUNCTION(BlueprintCallable, Category="MPAS|RigElement|PositionStacks")
	int32 RegisterPositionLayer(int32 InPositionStackID, const FString& InLayerName, EMPAS_LayerBlendingMode InBlendingMode, EMPAS_LayerCombinationMode InCombinationMode, bool InForceAllElementsActive = false);


	// Returns the ID of the given stack, -1 if stack not found
	UFUNCTION(BlueprintPure, BlueprintCallable, Category="MPAS|RigElement|PositionStacks")
	int32 GetPositionStackID(const FString& InStackName);

	// Returns the ID of the given layer in the given stack, -1 if stack or layer not found
	UFUNCTION(BlueprintPure, BlueprintCallable, Category="MPAS|RigElement|PositionStacks")
	int32 GetPositionLayerID(int32 InPositionStackID, const FString& InLayerName);


	// Sets the value of a position source in the given Stack and Layer, if succeded: returns true, false - overwise
	UFUNCTION(BlueprintCallable, Category="MPAS|RigElement|PositionStacks")
	bool SetPositionSourceValue(int32 InPositionStackID, int32 InPositionLayerID, UMPAS_RigElement* InSourceElement, FVector InSourceValue);

	// Removes the value of a position source in the given Stack and Layer, if succeded: returns true, false - overwise
	UFUNCTION(BlueprintCallable, Category="MPAS|RigElement|PositionStacks")
	bool RemovePositionSourceValue(int32 InPositionStackID, int32 InPositionLayerID, UMPAS_RigElement* InSourceElement);

	// Returns the value of a position source in the given Stack and Layer
	UFUNCTION(BlueprintCallable, BlueprintPure, Category="MPAS|RigElement|PositionStacks")
	FVector GetPositionSourceValue(int32 InPositionStackID, int32 InPositionLayerID, UMPAS_RigElement* InSourceElement)
	{
		return LocationStacks[InPositionStackID][InPositionLayerID].LayerElements[InSourceElement];
	}



// ROTATION LAYERS
protected:

	// Stacks by layers of which the position of the element is determined
	TArray<TArray<FMPAS_RotatorLayer>> RotationStacks;

	// Name maps used to access Stacks and Layers by name (slower than ID)
	TMap<FString, int32> RotationStackNames;
	TArray<TMap<FString, int32>> RotationLayerNames;


protected:

	// Applies the default rotation stack to the element's world location
	void ApplyDefaultRotationStack(float DeltaTime);

	// Calculates the final rotation of the given rotation stack
	FRotator CalculateRotationStackValue(int32 InRotationStackID);

	// Calculates the final rotation of the given rotation layer
	FRotator CalculateRotationLayerValue(const FMPAS_RotatorLayer InLayer, bool& OutHasActiveElements);


public:
	// Registers a new rotation stack and returns it's ID, returns an existing ID if the stack is already registered
	UFUNCTION(BlueprintCallable, Category="MPAS|RigElement|RotationStacks")
	int32 RegisterRotationStack(const FString& InStackName);

	// Registers a new rotation layer in the given stack and returns it's ID, returns an existing ID if the layer is already registered, returns -1 if Stack does not exist
	UFUNCTION(BlueprintCallable, Category="MPAS|RigElement|RotationStacks")
	int32 RegisterRotationLayer(int32 InRotationStackID, const FString& InLayerName, EMPAS_LayerBlendingMode InBlendingMode, bool InForceAllElementsActive = false);


	// Returns the ID of the given stack, -1 if stack not found
	UFUNCTION(BlueprintPure, BlueprintCallable, Category="MPAS|RigElement|RotationStacks")
	int32 GetRotationStackID(const FString& InStackName);

	// Returns the ID of the given layer in the given stack, -1 if stack or layer not found
	UFUNCTION(BlueprintPure, BlueprintCallable, Category="MPAS|RigElement|RotationStacks")
	int32 GetRotationLayerID(int32 InRotationStackID, const FString& InLayerName);


	// Sets the value of a rotation source in the given Stack and Layer, if succeded: returns true, false - overwise
	UFUNCTION(BlueprintCallable, Category="MPAS|RigElement|RotationStacks")
	bool SetRotationSourceValue(int32 InRotationStackID, int32 InRotationLayerID, UMPAS_RigElement* InSourceElement, FRotator InSourceValue);

	// Removes the value of a rotation source in the given Stack and Layer, if succeded: returns true, false - overwise
	UFUNCTION(BlueprintCallable, Category="MPAS|RigElement|RotationStacks")
	bool RemoveRotationSourceValue(int32 InRotationStackID, int32 InRotationLayerID, UMPAS_RigElement* InSourceElement);

	// Returns the value of a rotation source in the given Stack and Layer
	UFUNCTION(BlueprintCallable, BlueprintPure, Category="MPAS|RigElement|RotationStacks")
	FRotator GetRotationSourceValue(int32 InRotationStackID, int32 InRotationLayerID, UMPAS_RigElement* InSourceElement)
	{
		return RotationStacks[InRotationStackID][InRotationLayerID].LayerElements[InSourceElement];
	}



// PHYSICS MODEL
protected:

	// Whether the physics model is enabled for this element
	bool PhysicsModelEnabled;

	// An array of pointers to all associated physics model elements
	UPROPERTY()
	TArray<UMPAS_PhysicsModelElement*> PhysicsElements;

public:

	// An array of all configurations of physics model elements, that will be created for this rig element
	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = "PhysicsModel")
	TArray<FMPAS_PhysicsElementConfiguration> PhysicsElementsConfiguration;

public:
	
	// Enables/Disables physics model usage for this element
	UFUNCTION(BlueprintCallable, Category = "MPAS|RigElement|PhysicsModel")
	void SetPhysicsModelEnabled(bool InEnabled);

	// Wheter the physics model is enabled for this element
	UFUNCTION(BlueprintCallable, BlueprintPure, Category = "MPAS|RigElement|PhysicsModel")
	bool GetPhysicsModelEnabled() { return PhysicsModelEnabled; }

	// Returns an array of pointers to the all associated physics model elements
	UFUNCTION(BlueprintCallable, BlueprintPure, Category = "MPAS|RigElement|PhysicsModel")
	const TArray<UMPAS_PhysicsModelElement*>& GetPhysicsElements() { return PhysicsElements; }

	// Returns a pointer to the specified associated physics model element
	UFUNCTION(BlueprintCallable, BlueprintPure, Category = "MPAS|RigElement|PhysicsModel")
	UMPAS_PhysicsModelElement* GetPhysicsElement(int32 Index) { return PhysicsElements[Index]; }

	// Called when physics model is enabled for this element
	UFUNCTION(BlueprintNativeEvent, Category="MPAS|RigElement|PhysicsModel")
	void OnPhysicsModelEnabled();
	virtual void OnPhysicsModelEnabled_Implementation() {}

	// Called when physics model is disabled for this element
	UFUNCTION(BlueprintNativeEvent, Category="MPAS|RigElement|PhysicsModel")
	void OnPhysicsModelDisabled();
	virtual void OnPhysicsModelDisabled_Implementation() {}

	// Called when physics model is enabled, applies position and rotation of physics elements to the rig element
	UFUNCTION(BlueprintNativeEvent, Category="MPAS|RigElement|PhysicsModel")
	void ApplyPhysicsModelPositionAndRotation(float DeltaTime);
	virtual void ApplyPhysicsModelPositionAndRotation_Implementation(float DeltaTime);

	// Returns the position, where physics element needs to be once the physics model is enabled
	UFUNCTION(BlueprintNativeEvent, Category="MPAS|RigElement|PhysicsModel")
	FTransform GetDesiredPhysicsElementTransform(int32 PhysicsElementID);
	virtual FTransform GetDesiredPhysicsElementTransform_Implementation(int32 PhysicsElementID) { return GetComponentTransform(); }

	// Returns the velocity that needs to be applied to the physics element once the physics model is enabled
	UFUNCTION(BlueprintNativeEvent, Category="MPAS|RigElement|PhysicsModel")
	FVector GetDesiredPhysicsElementVelocity(int32 PhysicsElementID);
	virtual FVector GetDesiredPhysicsElementVelocity_Implementation(int32 PhysicsElementID) { return GetVelocity(); }


// STABILITY

/*
 * Stability is a float value, that determines how stable the component is. If the value goes below a certain threshold, then the element and all of it's children become unstable
 * The value ranges from 0 to 1 for each element
 * The stability of the element can depend on other elements, including it's children and parents
 * Note, that if the parent goes unstable, the child will also go unstable, though it does not gurantee that child's stability value is below it's threshold
 */

protected:
	
	/*
	 * A map of rig elements, that have an effect on this element's stability
	 * The resulting element's stability value is the average of this map
	 */
	TMap<UMPAS_RigElement*, float> StabilityEffectors;

	// The latest stability value - cached average value of the StabilityEffectors map
	float CachedStability = 1.f;

	// Current stability status
	EMPAS_StabilityStatus StabilityStatus;

public:

	// A threshold for element's Stability value, going under which, the element becomes unstable
	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = "Default|Stability")
	float StabilityThreshold = 0.5f;

protected:

	// Recalculates stability and calls stability events
	void UpdateStability();

public:

	/*
	* Returns the element's Stability value
	* Stability is a float value, that determines how stable the component is. If the value goes below a certain threshold, then the element and all of it's children become unstable
	* The value ranges from 0 to 1 for each element
	* The stability of the element can depend on other elements, including it's children and parents
	* Note, that if the parent goes unstable, the child will also go unstable, though it does not gurantee that child's stability value is below it's threshold
	*/
	UFUNCTION(BlueprintCallable, BlueprintPure, Category = "MPAS|RigElement|Stability")
	float GetStability() { return CachedStability; }

	// Whether the element is stable or not
	UFUNCTION(BlueprintCallable, BlueprintPure, Category = "MPAS|RigElement|Stability")
	bool GetIsStable();

	// Returns current stability status
	UFUNCTION(BlueprintCallable, BlueprintPure, Category = "MPAS|RigElement|Stability")
	EMPAS_StabilityStatus GetStabilityStatus() { return StabilityStatus; }

	// Sets a stability effector value, the average of all effector values determines the stability of the element
	UFUNCTION(BlueprintCallable, Category = "MPAS|RigElement|Stability")
	void SetStabilityEffectorValue(UMPAS_RigElement* InEffector, float InStability);

	// Removes an effector from stability effectors (if it is a valid effector)
	UFUNCTION(BlueprintCallable, Category = "MPAS|RigElement|Stability")
	void RemoveStabilityEffector(UMPAS_RigElement* InEffector);


	// Called when the element's stability status is changed (do not forget to call SUPER version)
	UFUNCTION(BlueprintNativeEvent, Category="MPAS|RigElement|Stability")
	void OnStabilityStatusChanged(EMPAS_StabilityStatus NewStabilityStatus);
	virtual void OnStabilityStatusChanged_Implementation(EMPAS_StabilityStatus NewStabilityStatus);



// CALLED BY THE HANDLER
public:

	// CALLED BY THE HANDLER : Initializing Rig Element
	virtual void InitRigElement(class UMPAS_Handler* InHandler);

	// CALLED BY THE HANDLER : Contains the logic that links this element with other elements in the rig
	virtual void LinkRigElement(class UMPAS_Handler* InHandler);

	// CALLED BY THE HANDLER : Links element to it's physics model equivalent
	virtual void InitPhysicsModel(const TArray<UMPAS_PhysicsModelElement*>& InPhysicsElements);

	// CALLED BY THE HANDLER :  Updating Rig Element every tick
	virtual void UpdateRigElement(float DeltaTime);

	// CALLED BY THE HANDLER :  Called when the rig is initialized by the handler - to be overriden in Blueprints
	UFUNCTION(BlueprintNativeEvent, Category="MPAS|RigElement|Overrides|Basic")
	void OnInitRigElement(class UMPAS_Handler* InHandler);
	virtual void OnInitRigElement_Implementation(class UMPAS_Handler* InHandler) {};

	// CALLED BY THE HANDLER :  Contains the logic that links this element with other elements in the rig - to be overriden in Blueprints
	UFUNCTION(BlueprintNativeEvent, Category="MPAS|RigElement|Overrides|Basic")
	void OnLinkRigElement(class UMPAS_Handler* InHandler);
	virtual void OnLinkRigElement_Implementation(class UMPAS_Handler* InHandler) {};

	// CALLED BY THE HANDLER :  Links element to it's physics model equivalent - to be overriden in Blueprints
	UFUNCTION(BlueprintNativeEvent, Category="MPAS|RigElement|Overrides|Basic")
	void OnInitPhysicsModel();
	virtual void OnInitPhysicsModel_Implementation() {};

	// CALLED BY THE HANDLER :  Called every tick the element gets updated by the handler - to be overriden in Blueprints
	UFUNCTION(BlueprintNativeEvent, Category="MPAS|RigElement|Overrides|Basic")
	void OnUpdateRigElement(float DeltaTime);
	virtual void OnUpdateRigElement_Implementation(float DeltaTime) {};




// DEBUGGING
public:

	UFUNCTION(BlueprintCallable, BlueprintPure, Category="MPAS|RigElement|Debug|PositionStacks")
	const TMap<FString, int32>& GetPositionStackNames() { return LocationStackNames; }

	UFUNCTION(BlueprintCallable, BlueprintPure, Category="MPAS|RigElement|Debug|PositionStacks")
	const TMap<FString, int32>& GetPositionLayerNames(int32 InPositionStackID) { return LocationLayerNames[InPositionStackID]; }

	UFUNCTION(BlueprintCallable, BlueprintPure, Category="MPAS|RigElement|Debug|PositionStacks")
	const FMPAS_VectorLayer& GetPositionLayer(int32 InPositionStackID, int32 InPositionLayerID) { return LocationStacks[InPositionStackID][InPositionLayerID]; }

	UFUNCTION(BlueprintCallable, BlueprintPure, Category="MPAS|RigElement|Debug|RotationStacks")
	const TMap<FString, int32>& GetRotationStackNames() { return RotationStackNames; }

	UFUNCTION(BlueprintCallable, BlueprintPure, Category="MPAS|RigElement|Debug|RotationStacks")
	const TMap<FString, int32>& GetRotationLayerNames(int32 InRotationStackID) { return RotationLayerNames[InRotationStackID]; }

	UFUNCTION(BlueprintCallable, BlueprintPure, Category="MPAS|RigElement|Debug|RotationStacks")
	const FMPAS_RotatorLayer& GetRotationLayer(int32 InRotationStackID, int32 InRotationLayerID) { return RotationStacks[InRotationStackID][InRotationLayerID]; }
};
