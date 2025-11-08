// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "Components/SceneComponent.h"
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

	// Since normal layers override everything that lies beneath them, it is logical to start compuatation from the top-most normal layer with blending factor of 1.0f
	// But that layer may not be active for any number of reasons, so we cache all of the normal layer with blending factor of 1.0f in this array, so we can check if they are active in runtime
	// Layer are identified by their stack order id and placed top to bottom (0 - the highest such layer)
	UPROPERTY(EditAnywhere, BlueprintReadWrite)
	TArray<int32> StartingLayersCache;

	// Adds a new layer into the stack and updates the stack order
	int32 AddVectorLayer(FMPAS_VectorLayer InLayer);

	// Recalucates StartingLayersCache array, should be called whenether a new layer is added/removed or some normal layer's blending factor is changed
	void RecalculateStartingLayerCache();


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

	// Since normal layers override everything that lies beneath them, it is logical to start compuatation from the top-most normal layer with blending factor of 1.0f
	// But that layer may not be active for any number of reasons, so we cache all of the normal layer with blending factor of 1.0f in this array, so we can check if they are active in runtime
	// Layer are identified by their stack order id and placed top to bottom (0 - the highest such layer)
	UPROPERTY(EditAnywhere, BlueprintReadWrite)
	TArray<int32> StartingLayersCache;

	// Adds a new layer into the stack and updates the stack order
	int32 AddRotatorLayer(FMPAS_RotatorLayer InLayer);

	// Recalucates StartingLayersCache array, should be called whenether a new layer is added/removed or some normal layer's blending factor is changed
	void RecalculateStartingLayerCache();


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

	// Whether the element is currently enabled
	UPROPERTY(BlueprintReadOnly, VisibleAnywhere, Category = "Default")
	bool Enabled = true;

	// Whether the element should be enabled on initialization
	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = "Default")
	bool DefaultEnabled = true;


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

	// Whether this element should always be synchronized with fetched bone transforms
	// If set to false, bone transform sync will only happen during Force Synchronization
	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = "Default|BoneTransformSync")
	bool AlwaysSyncBoneTransform = true;


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
	void SetRigElementEnabled(bool NewEnabled) { Enabled = NewEnabled; }

	// Whether the element is active or not
	UFUNCTION(BlueprintPure, BlueprintCallable, Category="MPAS|RigElement")
	bool GetRigElementEnabled() { return Enabled; }


	// Whether this element is currently active (Implementation can depend on the specific element, override this method if you need custom funcitonality)
	// Default behavior: return GetRigElementEnabled();
	UFUNCTION(BlueprintPure, BlueprintCallable, BlueprintNativeEvent, Category = "MPAS|RigElement")
	bool GetRigElementActive();
	virtual bool GetRigElementActive_Implementation() { return Enabled; }


	// Returns the velocity of the rig element
	UFUNCTION(BlueprintPure, BlueprintCallable, Category="MPAS|RigElement")
	FVector GetVelocity() { return CachedVelocity; }



// POSITION DRIVER INTEGRATION

public:
	// Name of the Vector Stack that will receive location data from a Position Driver (if one is present)
	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = "Advanced|PositionDriving")
	FString PositionDriverIntegration_LocationStackName = "DefaultLocation";

	// Name of the Rotation Stack that will receive rotation data from a Position Driver (if one is present)
	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = "Advanced|PositionDriving")
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



// CALLED BY THE HANDLER
public:

	// CALLED BY THE HANDLER : Initializing Rig Element
	virtual void InitRigElement(class UMPAS_Handler* InHandler);

	// CALLED BY THE HANDLER : Contains the logic that links this element with other elements in the rig
	virtual void LinkRigElement(class UMPAS_Handler* InHandler);

	// CALLED BY THE HANDLER : Called after the linking phase has completed (no more side changes will be applied to the element)
	virtual void PostLinkSetupRigElement(class UMPAS_Handler* InHandler);

	// CALLED BY THE HANDLER : Updating Rig Element every tick
	virtual void UpdateRigElement(float DeltaTime);

	// CALLED BY THE HANDLER : Synchronizes Rig Element to the most recently fetched bone transforms
	virtual void SyncToFetchedBoneTransforms();


	// CALLED BY THE HANDLER : Called when the rig is initialized by the handler - to be overriden in Blueprints
	UFUNCTION(BlueprintNativeEvent, Category="MPAS|RigElement|Overrides|Basic")
	void OnInitRigElement(class UMPAS_Handler* InHandler);
	virtual void OnInitRigElement_Implementation(class UMPAS_Handler* InHandler) {};

	// CALLED BY THE HANDLER : Contains the logic that links this element with other elements in the rig - to be overriden in Blueprints
	UFUNCTION(BlueprintNativeEvent, Category="MPAS|RigElement|Overrides|Basic")
	void OnLinkRigElement(class UMPAS_Handler* InHandler);
	virtual void OnLinkRigElement_Implementation(class UMPAS_Handler* InHandler) {};

	// CALLED BY THE HANDLER : Called every tick the element gets updated by the handler - to be overriden in Blueprints
	UFUNCTION(BlueprintNativeEvent, Category="MPAS|RigElement|Overrides|Basic")
	void OnUpdateRigElement(float DeltaTime);
	virtual void OnUpdateRigElement_Implementation(float DeltaTime) {};

	// CALLED BY THE HANDLER : CSynchronizes Rig Element to the most recently fetched bone transforms - to be overriden in Blueprints
	UFUNCTION(BlueprintNativeEvent, Category = "MPAS|RigElement|Overrides|Basic")
	void OnSyncToFetchedBoneTransforms();
	virtual void OnSyncToFetchedBoneTransforms_Implementation() {};




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
