// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "Components/SceneComponent.h"
#include "MPAS_PhysicsModel.h"
#include "MPAS_RigElement.generated.h"


// LAYERS

// Blending mode for location/rotation/... layers in stacks
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
	bool Enabled = true;

	UPROPERTY(EditAnywhere, BlueprintReadWrite)
	EMPAS_LayerBlendingMode BlendingMode;

	UPROPERTY(EditAnywhere, BlueprintReadWrite)
	float BlendingFactor = 1.f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite)
	EMPAS_LayerCombinationMode CombinationMode;

	UPROPERTY(EditAnywhere, BlueprintReadWrite)
	TMap<UMPAS_RigElement*, FVector> LayerElements;

	// Higher -> higher
	UPROPERTY(EditAnywhere, BlueprintReadWrite)
	int Priority = 0;

	UPROPERTY(EditAnywhere, BlueprintReadWrite)
	bool ForceAllElementsActive;

	FMPAS_VectorLayer(	EMPAS_LayerBlendingMode InBlendingMode = EMPAS_LayerBlendingMode::Normal,
						float InBlendingFactor = 1.f,
						EMPAS_LayerCombinationMode InLayerCombinationMode = EMPAS_LayerCombinationMode::Average, 
						int InPriority = 0,
						bool InForceAllElementsActive = false): 

		BlendingMode(InBlendingMode), BlendingFactor(InBlendingFactor), CombinationMode(InLayerCombinationMode), Priority(InPriority), ForceAllElementsActive(InForceAllElementsActive) {}
};

USTRUCT(BlueprintType)
struct FMPAS_VectorStack
{
	GENERATED_USTRUCT_BODY()

	// [LayerID] -> <Vector Layer>
	UPROPERTY(EditAnywhere, BlueprintReadWrite)
	TArray<FMPAS_VectorLayer> Layers;

	// [Execution Step] -> <LayerID>
	UPROPERTY(EditAnywhere, BlueprintReadWrite)
	TArray<int32> StackOrder;

	// Adds a new layer into the stack and updates the stack order
	int32 AddVectorLayer(FMPAS_VectorLayer InLayer);

	int32 Num() { return Layers.Num(); }

	FMPAS_VectorLayer& operator[] (int32 InID) { return Layers[InID]; }
};


USTRUCT(BlueprintType)
struct FMPAS_RotatorLayer
{
	GENERATED_USTRUCT_BODY()

	UPROPERTY(EditAnywhere, BlueprintReadWrite)
	bool Enabled = true;

	UPROPERTY(EditAnywhere, BlueprintReadWrite)
	EMPAS_LayerBlendingMode BlendingMode;

	UPROPERTY(EditAnywhere, BlueprintReadWrite)
	float BlendingFactor = 1.f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite)
	EMPAS_LayerCombinationMode CombinationMode;

	UPROPERTY(EditAnywhere, BlueprintReadWrite)
	TMap<UMPAS_RigElement*, FRotator> LayerElements;

	// Higher -> higher
	UPROPERTY(EditAnywhere, BlueprintReadWrite)
	int Priority = 0;

	UPROPERTY(EditAnywhere, BlueprintReadWrite)
	bool ForceAllElementsActive;

	FMPAS_RotatorLayer(	EMPAS_LayerBlendingMode InBlendingMode = EMPAS_LayerBlendingMode::Normal,
						float InBlendingFactor = 1.f,
						EMPAS_LayerCombinationMode InLayerCombinationMode = EMPAS_LayerCombinationMode::Average,
						int InPriority = 0,
						bool InForceAllElementsActive = false) :

		BlendingMode(InBlendingMode), BlendingFactor(InBlendingFactor), CombinationMode(InLayerCombinationMode), Priority(InPriority), ForceAllElementsActive(InForceAllElementsActive) {}
};

USTRUCT(BlueprintType)
struct FMPAS_RotatorStack
{
	GENERATED_USTRUCT_BODY()

	// [LayerID] -> <Rotator Layer>
	UPROPERTY(EditAnywhere, BlueprintReadWrite)
	TArray<FMPAS_RotatorLayer> Layers;

	// [Execution Step] -> <LayerID>
	UPROPERTY(EditAnywhere, BlueprintReadWrite)
	TArray<int32> StackOrder;

	// Adds a new layer into the stack and updates the stack order
	int32 AddRotatorLayer(FMPAS_RotatorLayer InLayer);

	int32 Num() { return Layers.Num(); }

	FMPAS_RotatorLayer& operator[] (int32 InID) { return Layers[InID]; }
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

	// Initial location and rotation of the element relative to it's parent
	FTransform InitialSelfTransform;

	// If this flag is set, then physics model constaint will be reactivated next frame
	bool ReactivatePhysicsModelConstraintNextFrame;

	// Cached parameter values
	float LocationInterpolationMultiplier = 1.f;
	float RotationInterpolationMultiplier = 1.f;

	// Velocity Calculation
	FVector PreviousFrameLocation;
	FVector CachedVelocity;

	// Cached default location stack value from the latest call of ApplyDefaultLocationStack
	FVector CachedDefaultLocationStackValue;

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

	// Element's positioning mode: Normal - keeps an offset from it's parent, default location and rotation stacks are applied; Independent - stack values are not applied, the component is free to move
	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = "Default")
	EMPAS_ElementPositionMode PositioningMode;

	// Smooths out location changes, if set to 0, no interpolation is applied
	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = "Default|Orientation")
	float LocationInterpolationSpeed = 0.f;

	// Smooths out rotation changes, if set to 0, no interpolation is applied
	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = "Default|Orientation")
	float RotationInterpolationSpeed = 0.f;


protected:
	// Called when the game starts
	virtual void BeginPlay() override;

	// NOTIFICATION Called when ORIENTATION_LocationInterpolationMultiplier or ORIENTATION_RotationInterpolationMultiplier parameter value is changed
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


// POSITION DRIVER INTEGRATION

	// Name of the Vector Stack that will receive location data from a Position Driver (if one is present)
	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = "Default|PositionDriving")
	FString PositionDriverIntegration_LocationStackName = "DefaultLocation";

	// Name of the Rotation Stack that will receive rotation data from a Position Driver (if one is present)
	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = "Default|PositionDriving")
	FString PositionDriverIntegration_RotationStackName = "DefaultRotation";



// VECTOR LAYERS
protected:

	// Stacks of multi-purpose vector layers
	TArray<FMPAS_VectorStack> VectorStacks;

	// Name maps used to access Stacks and Layers by name (slower than ID)
	TMap<FString, int32> VectorStackNames;
	TArray<TMap<FString, int32>> VectorLayerNames;


protected:

	// Applies the default location stack to the element's world location
	void ApplyDefaultLocationStack(float DeltaTime);


	// Calculates the final vector of the given vector stack
	FVector CalculateVectorStackValue(int32 InLocationStackID);

	// Calculates the final vector of the given vector layer
	FVector CalculateVectorLayerValue(const FMPAS_VectorLayer InLayer, bool& OutHasActiveElements);


public:
	// Registers a new vector stack and returns it's ID, returns an existing ID if the stack is already registered
	UFUNCTION(BlueprintCallable, Category="MPAS|RigElement|VectorStacks")
	int32 RegisterVectorStack(const FString& InStackName);

	// Registers a new vector layer in the given stack and returns it's ID, returns an existing ID if the layer is already registered, returns -1 if Stack does not exist
	UFUNCTION(BlueprintCallable, Category="MPAS|RigElement|VectorStacks")
	int32 RegisterVectorLayer(int32 InVectorStackID, const FString& InLayerName, EMPAS_LayerBlendingMode InBlendingMode, EMPAS_LayerCombinationMode InCombinationMode, float InBlendingFactor = 1.f, int32 InPriority = 0, bool InForceAllElementsActive = false);


	// Returns the ID of the given stack, -1 if stack not found
	UFUNCTION(BlueprintPure, BlueprintCallable, Category="MPAS|RigElement|VectorStacks")
	int32 GetVectorStackID(const FString& InStackName);

	// Returns the ID of the given layer in the given stack, -1 if stack or layer not found
	UFUNCTION(BlueprintPure, BlueprintCallable, Category="MPAS|RigElement|VectorStacks")
	int32 GetVectorLayerID(int32 InVectorStackID, const FString& InLayerName);


	// Sets the value of a vector source in the given Stack and Layer, if succeded: returns true, false - overwise
	UFUNCTION(BlueprintCallable, Category="MPAS|RigElement|VectorStacks")
	bool SetVectorSourceValue(int32 InVectorStackID, int32 InVectorLayerID, UMPAS_RigElement* InSourceElement, FVector InSourceValue);

	// Removes the value of a vector source in the given Stack and Layer, if succeded: returns true, false - overwise
	UFUNCTION(BlueprintCallable, Category="MPAS|RigElement|VectorStacks")
	bool RemoveVectorSourceValue(int32 InVectorStackID, int32 InVectorLayerID, UMPAS_RigElement* InSourceElement);

	// Returns the value of a location source in the given Stack and Layer
	UFUNCTION(BlueprintCallable, BlueprintPure, Category = "MPAS|RigElement|VectorStacks")
	const FVector& GetVectorSourceValue(int32 InVectorStackID, int32 InVectorLayerID, UMPAS_RigElement* InSourceElement)
	{
		return VectorStacks[InVectorStackID][InVectorLayerID].LayerElements[InSourceElement];
	}


	// Enables/Disables the specified vector layer, if succeded: returns true, false - overwise
	UFUNCTION(BlueprintCallable, Category = "MPAS|RigElement|VectorStacks")
	bool SetVectorLayerEnabled(int32 InVectorStackID, int32 InVectorLayerID, bool InNewEnabled);

	// Whenther the specified layer is enabled or not ("false" if the layer doesnt exist)
	UFUNCTION(BlueprintCallable, BlueprintPure, Category = "MPAS|RigElement|VectorStacks")
	bool GetVectorLayerEnabled(int32 InVectorStackID, int32 InVectorLayerID);


	// Modifies blending factor of the specified vector layer, if succeded: returns true, false - overwise
	UFUNCTION(BlueprintCallable, Category = "MPAS|RigElement|VectorStacks")
	bool SetVectorLayerBlendingFactor(int32 InVectorStackID, int32 InVectorLayerID, float InNewBlendingFactor);

	// Returns the blending factor of the specified layer is enabled or not ("0" if the layer doesnt exist)
	UFUNCTION(BlueprintCallable, BlueprintPure, Category = "MPAS|RigElement|VectorStacks")
	float GetVectorLayerBlendingFactor(int32 InVectorStackID, int32 InVectorLayerID);



// ROTATION LAYERS
protected:

	// Stacks by layers of which the rotation of the element is determined
	TArray<FMPAS_RotatorStack> RotationStacks;

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
	int32 RegisterRotationLayer(int32 InRotationStackID, const FString& InLayerName, EMPAS_LayerBlendingMode InBlendingMode, float InBlendingFactor = 1.f, int32 InPriority = 0, bool InForceAllElementsActive = false);


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


	// Enables/Disables the specified rotation layer, if succeded: returns true, false - overwise
	UFUNCTION(BlueprintCallable, Category = "MPAS|RigElement|RotationStacks")
	bool SetRotationLayerEnabled(int32 InRotationStackID, int32 InRotationLayerID, bool InNewEnabled);

	// Whenther the specified layer is enabled or not ("false" if the layer doesnt exist)
	UFUNCTION(BlueprintCallable, BlueprintPure, Category = "MPAS|RigElement|RotationStacks")
	bool GetRotationLayerEnabled(int32 InRotationStackID, int32 InRotationLayerID);


	// Modifies blending factor of the specified rotation layer, if succeded: returns true, false - overwise
	UFUNCTION(BlueprintCallable, Category = "MPAS|RigElement|RotationStacks")
	bool SetRotationLayerBlendingFactor(int32 InRotationStackID, int32 InRotationLayerID, float InNewBlendingFactor);

	// Returns the blending factor of the specified layer is enabled or not ("0" if the layer doesnt exist)
	UFUNCTION(BlueprintCallable, BlueprintPure, Category = "MPAS|RigElement|RotationStacks")
	float GetRotationLayerBlendingFactor(int32 InRotationStackID, int32 InRotationLayerID);



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

	// Called when physics model is enabled, applies location and rotation of physics elements to the rig element
	UFUNCTION(BlueprintNativeEvent, Category="MPAS|RigElement|PhysicsModel")
	void ApplyPhysicsModelLocationAndRotation(float DeltaTime);
	virtual void ApplyPhysicsModelLocationAndRotation_Implementation(float DeltaTime);

	// Returns the location, where physics element needs to be once the physics model is enabled
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

	// CALLED BY THE HANDLER : Called after the linking phase has completed (no more side changes will be applied to the element)
	virtual void PostLinkSetupRigElement(class UMPAS_Handler* InHandler);

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

	UFUNCTION(BlueprintCallable, BlueprintPure, Category="MPAS|RigElement|Debug|VectorStacks")
	const TMap<FString, int32>& DEBUG_GetVectorStackNames() { return VectorStackNames; }

	UFUNCTION(BlueprintCallable, BlueprintPure, Category="MPAS|RigElement|Debug|VectorStacks")
	const TMap<FString, int32>& DEBUG_GetVectorLayerNames(int32 InVectorStackID) { return VectorLayerNames[InVectorStackID]; }

	UFUNCTION(BlueprintCallable, BlueprintPure, Category = "MPAS|RigElement|Debug|VectorStacks")
	const TArray<int32>& DEBUG_GetVectorStackExecutionOrder(int32 InVectorStackID) { return VectorStacks[InVectorStackID].StackOrder; }

	UFUNCTION(BlueprintCallable, BlueprintPure, Category="MPAS|RigElement|Debug|VectorStacks")
	const FMPAS_VectorLayer& DEBUG_GetVectorLayer(int32 InVectorStackID, int32 InVectorLayerID) { return VectorStacks[InVectorStackID][InVectorLayerID]; }


	UFUNCTION(BlueprintCallable, BlueprintPure, Category="MPAS|RigElement|Debug|RotationStacks")
	const TMap<FString, int32>& DEBUG_GetRotationStackNames() { return RotationStackNames; }

	UFUNCTION(BlueprintCallable, BlueprintPure, Category="MPAS|RigElement|Debug|RotationStacks")
	const TMap<FString, int32>& DEBUG_GetRotationLayerNames(int32 InRotationStackID) { return RotationLayerNames[InRotationStackID]; }

	UFUNCTION(BlueprintCallable, BlueprintPure, Category = "MPAS|RigElement|Debug|RotationStacks")
	const TArray<int32>& DEBUG_GetRotationStackExecutionOrder(int32 InRotationStackID) { return RotationStacks[InRotationStackID].StackOrder; }

	UFUNCTION(BlueprintCallable, BlueprintPure, Category="MPAS|RigElement|Debug|RotationStacks")
	const FMPAS_RotatorLayer& DEBUG_GetRotationLayer(int32 InRotationStackID, int32 InRotationLayerID) { return RotationStacks[InRotationStackID][InRotationLayerID]; }
};
