// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "Components/SceneComponent.h"
#include "StacksAndLayers.h"
#include "MPAS_RigElement.generated.h"


// ELEMENT TYPE
UENUM(BlueprintType)
enum class EMPAS_ElementPositionMode : uint8
{
	Normal UMETA(DisplayName="Normal"),
	Independent UMETA(DisplayName="Independent")
};


// RIG ELEMENT
UCLASS(Blueprintable, ClassGroup=(Custom), meta=(BlueprintSpawnableComponent) )
class MPAS_API UMPAS_RigElement : public USceneComponent, public ISourceInterface
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
	bool GetRigElementActive() const;
	virtual bool GetRigElementActive_Implementation() const { return Enabled; }


	// Returns the velocity of the rig element
	UFUNCTION(BlueprintPure, BlueprintCallable, Category="MPAS|RigElement")
	FVector GetVelocity() { return CachedVelocity; }



// BONE TRANSFORM SYNC
public:

	// Whether this element should always be synchronized with fetched bone transforms
	// If set to false, bone transform sync will only happen during Force Synchronization
	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = "Default|BoneTransformSync")
	bool AlwaysSyncBoneTransform = true;



// POSITION DRIVER INTEGRATION

public:
	// Name of the Vector Stack that will receive location data from a Position Driver (if one is present)
	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = "Advanced|PositionDriving")
	FString PositionDriverIntegration_LocationStackName = "DefaultLocation";

	// Name of the Rotation Stack that will receive rotation data from a Position Driver (if one is present)
	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = "Advanced|PositionDriving")
	FString PositionDriverIntegration_RotationStackName = "DefaultRotation";


// VECTOR LAYERS
public:

	// Stacks of multi-purpose vector layers
	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = "MPAS|RigElement|VectorStacks")
	TArray<FMPAS_VectorStack> VectorStacks;


protected:

	// Applies the default location stack to the element's world location
	void ApplyDefaultLocationStack(float DeltaTime);


public:
	// Registers a new vector stack and returns it's ID, returns an existing ID if the stack is already registered
	UFUNCTION(BlueprintCallable, Category="MPAS|RigElement|VectorStacks")
	int32 RegisterVectorStack(const FName& InStackName);

	// Registers a new vector layer and returns it's ID
	UFUNCTION(BlueprintCallable, Category = "MPAS|RigElement|VectorStacks")
	int32 RegisterVectorLayer(int32 InStackID, const FMPAS_VectorLayer& InLayer);

	// Returns a reference to the given stack
	UFUNCTION(BlueprintPure, BlueprintCallable, Category = "MPAS|RigElement|VectorStacks")
	FMPAS_VectorStack& GetVectorStack(int32 InStackID);

	// Returns the ID of the given stack, -1 if stack not found
	UFUNCTION(BlueprintPure, BlueprintCallable, Category="MPAS|RigElement|VectorStacks")
	int32 GetVectorStackID(const FName& InStackName);


// ROTATION LAYERS
public:

	// Stacks by layers of which the rotation of the element is determined
	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = "MPAS|RigElement|RotatorStacks")
	TArray<FMPAS_RotatorStack> RotatorStacks;


protected:

	// Applies the default rotation stack to the element's world location
	void ApplyDefaultRotationStack(float DeltaTime);


public:
	// Registers a new rotation stack and returns it's ID, returns an existing ID if the stack is already registered
	UFUNCTION(BlueprintCallable, Category="MPAS|RigElement|RotatorStacks")
	int32 RegisterRotatorStack(const FName& InStackName);

	// Registers a new vector layer and returns it's ID
	UFUNCTION(BlueprintCallable, Category = "MPAS|RigElement|RotatorStacks")
	int32 RegisterRotatorLayer(int32 InStackID, const FMPAS_RotatorLayer& InLayer);

	// Returns a reference to the given stack
	UFUNCTION(BlueprintPure, BlueprintCallable, Category = "MPAS|RigElement|RotatorStacks")
	FMPAS_RotatorStack& GetRotatorStack(int32 InStackID);

	// Returns the ID of the given stack, -1 if stack not found
	UFUNCTION(BlueprintPure, BlueprintCallable, Category="MPAS|RigElement|RotatorStacks")
	int32 GetRotatorStackID(const FName& InStackName);


public:
	bool IsSourceActive_Implementation() const { return GetRigElementActive(); }


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
	virtual void SyncToFetchedBoneTransforms(float DeltaTime);


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
	void OnSyncToFetchedBoneTransforms(float DeltaTime);
	virtual void OnSyncToFetchedBoneTransforms_Implementation(float DeltaTime) {};




// DEBUGGING
public:

	UFUNCTION(BlueprintCallable, BlueprintPure, Category = "MPAS|RigElement|Debug|VectorStacks")
	const TArray<int32>& DEBUG_GetVectorStackExecutionOrder(int32 InVectorStackID) { return VectorStacks[InVectorStackID].StackData.StackOrder; }


	UFUNCTION(BlueprintCallable, BlueprintPure, Category = "MPAS|RigElement|Debug|RotatorStacks")
	const TArray<int32>& DEBUG_GetRotationStackExecutionOrder(int32 InRotationStackID) { return RotatorStacks[InRotationStackID].StackData.StackOrder; }
};
