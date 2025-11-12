// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "Components/ActorComponent.h"
#include "IntentionDriving/MPAS_IntentionStateMachine.h"
#include "STT_TimerController.h"
#include "MPAS_Handler.generated.h"


// DELEGATES

// Fires off when custom MPAS_Handler parameter value is changed
DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(FOnParameterValueChanged, FName, InParameterName);


// STRUCTURES

// Rig element data structure used in RigData

USTRUCT(BlueprintType)
struct FMPAS_RigElementData
{
	GENERATED_USTRUCT_BODY()

	// Name of the element in the RigData
	UPROPERTY(EditAnywhere, BlueprintReadWrite)
	FName Name;

	// Pointer to the corresponding component
	UPROPERTY(EditAnywhere, BlueprintReadWrite)
	class UMPAS_RigElement* RigElement;

	UPROPERTY(EditAnywhere, BlueprintReadWrite)
	FName ParentComponent;

	// List of Rig elemenets, attached to this one
	UPROPERTY(EditAnywhere, BlueprintReadWrite)
	TArray<FName> ChildElements;

	FMPAS_RigElementData() {}
	FMPAS_RigElementData(const FName& InName, class UMPAS_RigElement* InRigElementPtr, const FName& InParentComponent = "None"): Name(InName), RigElement(InRigElementPtr), ParentComponent(InParentComponent) {}

	// Adds a child element to the list
	void AddChildElement(const FName& InChildElementName) { ChildElements.Add(InChildElementName); }
};



/* Defines rules for a propogation call
 * Default Values:
 * 		Depth: 0
 * 		PropogateToChildren: true
 * 		PropogateToParent: true
 * 		EnableTagFilter: false
 * 		FilterBlackListMode: false
 * 		TagFilter: empty
 */
USTRUCT(BlueprintType)
struct FMPAS_PropogationSettings
{
	GENERATED_USTRUCT_BODY()

	// Propogation depth, if set to 0 the propogation will be unlimited (will cover all connected elements)
	UPROPERTY(EditAnywhere, BlueprintReadWrite)
	int32 Depth = 0;

	// Should an element propogate to it's children
	UPROPERTY(EditAnywhere, BlueprintReadWrite)
	bool PropogateToChildren = true;

	// Should an element propogate to it's parent
	UPROPERTY(EditAnywhere, BlueprintReadWrite)
	bool PropogateToParent = true;

	// Enables/Disables a filter, based on rig element's component tags
	UPROPERTY(EditAnywhere, BlueprintReadWrite)
	bool EnableTagFilter = false;

	// If true, the filter will switch from a White-List to a Black-List mode
	UPROPERTY(EditAnywhere, BlueprintReadWrite)
	bool FilterBlackListMode = false;

	// Rig Element component tags to filter
	UPROPERTY(EditAnywhere, BlueprintReadWrite)
	TArray<FName> TagFilter;
};


// --------------------------------------------------
// HANDLER

UCLASS(Blueprintable, ClassGroup=(Custom), meta=(BlueprintSpawnableComponent) )
class MPAS_API UMPAS_Handler : public UActorComponent
{
	GENERATED_BODY()

protected:
	// Handler's core
	UPROPERTY()
	class UMPAS_Core* Core;

	// Contains the structural information about the rig SHOULD BE PROTECTED
	TMap<FName, FMPAS_RigElementData> RigData;

	// List of all core elements in the rig
	TArray<FName> CoreElements;

	// Whether rig setup process has been completed
	bool SetupComplete = false;

public:

	/* A pointer to TimerController from STT_TimersAndTimelines
	 * It allows for handling custom timers and timelines with Delegate notifications
	 * Very useful for components that need to be animated and also for intention drivers
	 */
	UPROPERTY(BlueprintReadOnly, Category = "MPAS|Handler")
	USTT_TimerController* TimerController;

public:	
	// Sets default values for this component's properties
	UMPAS_Handler();


protected:
	// Called when the game starts
	virtual void BeginPlay() override;

	// Scans the rig and filss Core and Rig Data
	void ScanRig();

	// Recursively scans Rig Element
	void ScanElement(class UMPAS_RigElement* RigElement, const FName& ParentElementName);

	// Initializes all elements in RigData
	void InitRig();

	// Links all elements in RigData
	void LinkRig();

	// Finalizes rig elements' setup
	void PostLinkSetupRig();

	// Calls OnRigSetupFinished on all Intention Drivers
	void OnRigSetupComplete();

	// Updates all elements in RigData
	void UpdateRig(float DeltaTime);


	// Locates or creates a new timer controller
	void InitTimerController();


public:	
	// Called every frame
	virtual void TickComponent(float DeltaTime, ELevelTick TickType, FActorComponentTickFunction* ThisTickFunction) override;
		
	// Returns true if there is a valid scanned rig
	UFUNCTION(BlueprintCallable, BlueprintPure, Category="MPAS|Handler")
	bool HasValidRig() { return Core != nullptr; }

	// Returns Core Pointer
	UFUNCTION(BlueprintCallable, BlueprintPure, Category="MPAS|Handler")
	class UMPAS_Core* GetCore() { return Core; }

	// Returns RigData, read only
	UFUNCTION(BlueprintCallable, BlueprintPure, Category="MPAS|Handler")
	const TMap<FName, FMPAS_RigElementData>& GetRigData() { return RigData; };

	// Returns the list of all core elements (elements that are attached to the Core) in the rig
	UFUNCTION(BlueprintCallable, BlueprintPure, Category="MPAS|Handler")
	const TArray<FName>& GetCoreElements() { return CoreElements; }



// BONE BUFFER

protected:

	// Bone transform buffer
	TMap<FName, FTransform> BoneTransforms;

public:

	// Sets transform of a single bone
	UFUNCTION(BlueprintCallable, Category="MPAS|Handler|BoneBuffer")
	void SetBoneTransform(FName InBone, FTransform InTransform);

	// Sets location of a single bone
	UFUNCTION(BlueprintCallable, Category="MPAS|Handler|BoneBuffer")
	void SetBoneLocation(FName InBone, FVector InLocation);

	// Sets rotation of a single bone
	UFUNCTION(BlueprintCallable, Category="MPAS|Handler|BoneBuffer")
	void SetBoneRotation(FName InBone, FRotator InRotation);

	// Sets scale of a single bone
	UFUNCTION(BlueprintCallable, Category="MPAS|Handler|BoneBuffer")
	void SetBoneScale(FName InBone, FVector InScale);

	// Returns data about all bone transforms
	UFUNCTION(BlueprintCallable, BlueprintPure, Category="MPAS|Handler|BoneBuffer")
	const TMap<FName, FTransform>& GetBoneTransforms() { return BoneTransforms; }

	// Returns data about single bone transform
	UFUNCTION(BlueprintCallable, BlueprintPure, Category="MPAS|Handler|BoneBuffer")
	FTransform GetSingleBoneTransform(FName InBone);



// BONE TRANFORM FETCHING AND SYNCHRONIZATION

protected:

	// Buffer, containing bone transforms that were fetched from the BoneTransformFetchMesh 
	// (or any other mesh, specified during manual fetch)
	TMap<FName, FTransform> FetchedBoneTransforms;

	// Difference between bone transform applied on the last update and the newly fetched bone transforms
	TMap<FName, FTransform> FetchedBoneTransformDeltas;

	// Skeletal mesh component, from which autonomous bone transform fetching is performed
	USkeletalMeshComponent* AutoBoneTransformFetchMesh;

	// List of bones, whose transform shall be fetched in the autonomous bone fetch process
	TSet<FName> AutoBoneTransformFetchSelection;

	// Whether next update's synchronization should be a forced one (it will called on all elements)
	bool ForceSyncBoneTransforms = false;

	// Automatically fetches bone transforms from the AutoBoneTransformFetchMesh and stores them into FetchedBoneTransforms
	void AutoFetchBoneTransforms();

	// Synchronizes rig elements to the most recently fetched bone transforms
	void SyncBoneTransforms(float DeltaTime);

public:

	// Whether the bone transforms should be fetched automatically
	// Fetched bone transforms are then used to update (sync) rig elements positions (Hybrid Animation Methods)
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Default|BoneTransformFetching")
	bool UseAutoBoneTransformFetching = true;

	// Prevents mesh's default pose from being fetched on the first handler update
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Default|BoneTransformFetching")
	bool SkipAutoBoneTransformFetchForTheFirstUpdate = true;


	// Specifies the skeletal mesh, from which bone transforms shall be fetched during autonomous bone fetch process
	UFUNCTION(BlueprintCallable, Category = "MPAS|Handler|BoneTransformFetching")
	void SetAutoBoneTransformFetchMesh(USkeletalMeshComponent* InMesh, bool AddAllBonesToFetchSelection = true);

	// Modifies the selection of bones, whose transform needs to be fetched during autonomous bone fetch process
	UFUNCTION(BlueprintCallable, Category = "MPAS|Handler|BoneTransformFetching")
	void AddAutoFetchBone(const FName& InBoneName) { AutoBoneTransformFetchSelection.Add(InBoneName); }

	// Modifies the selection of bones, whose transform needs to be fetched during autonomous bone fetch process
	UFUNCTION(BlueprintCallable, Category = "MPAS|Handler|BoneTransformFetching")
	void RemoveAutoFetchBone(const FName& InBoneName) { AutoBoneTransformFetchSelection.Remove(InBoneName); }


	// Manually fetches bone transforms from the specified mesh
	// NOTE: Autonomous Fetch OVERRIDES cached bone transforms, so it must be disabled in order to use Manual Fetching.
	UFUNCTION(BlueprintCallable, Category = "MPAS|Handler|BoneTransformFetching")
	void ManualFetchBoneTransforms(USkeletalMeshComponent* InFetchMesh, const TSet<FName>& Selection);


	// Performs a Forced Synchronization on the next update (synchronization will be applied to all elements)
	UFUNCTION(BlueprintCallable, Category = "MPAS|Handler|BoneTransformFetching")
	void ForceSynchronizeBoneTransforms() { ForceSyncBoneTransforms = true; }


	// Buffer, containing bone transforms that were fetched from the BoneTransformFetchMesh 
	// (or any other mesh, specified during manual fetch)
	UFUNCTION(BlueprintCallable, BlueprintPure, Category = "MPAS|Handler|BoneTransformFetching")
	const TMap<FName, FTransform>& GetCachedFetchedBoneTransforms() { return FetchedBoneTransforms; }

	// Difference between bone transform applied on the last update and the newly fetched bone transforms
	UFUNCTION(BlueprintCallable, BlueprintPure, Category = "MPAS|Handler|BoneTransformFetching")
	const TMap<FName, FTransform>& GetCachedFetchedBoneTransformDeltas() { return FetchedBoneTransformDeltas; }

	// Skeletal mesh component, from which autonomous bone transform fetching is performed
	UFUNCTION(BlueprintCallable, BlueprintPure, Category = "MPAS|Handler|BoneTransformFetching")
	USkeletalMeshComponent* GetAutoBoneTransformFetchMesh() { return AutoBoneTransformFetchMesh; }

	// List of bones, whose transform shall be fetched in the autonomous bone fetch process
	UFUNCTION(BlueprintCallable, BlueprintPure, Category = "MPAS|Handler|BoneTransformFetching")
	const TSet<FName>& GetAutoBoneTransformFetchSelection() { return AutoBoneTransformFetchSelection; }



// INTENTION DRIVER

protected:

	/* 
	 * Dictionary of state machines, all running at the same time
	 * These state machines convert input from the AI and the Movement control node
	 * into actions performed by the rig.
	*/
	UPROPERTY()
	TMap<FName, UMPAS_IntentionStateMachine*> IntentionStateMachines;

	// Updates all Intention State Machines
	void UpdateIntentionDriver(float DeltaTime);

	// Initializes default custom parameters, that can be controlled by the Intention Driver
	void InitDefaultControlParameters();
	
public:

	// Adds a Intention State Machine to the Intention Driver
	UFUNCTION(BlueprintCallable, Category = "MPAS|Handler|IntentionDriver")
	bool AddIntentionStateMachine(TSubclassOf<UMPAS_IntentionStateMachine> InStateMachineClass, FName InStateMachineName);

	// Returns a pointer to the requested Intention Driver, returns nullptr if failed to find
	UFUNCTION(BlueprintCallable, BlueprintPure, Category = "MPAS|Handler|IntentionDriver")
	UMPAS_IntentionStateMachine* GetIntentionDriver(FName InStateMachineName);

	// Activates / Deactivates selected Intention State Machine
	UFUNCTION(BlueprintCallable, Category = "MPAS|Handler|IntentionDriver")
	void SetIntentionStateMachineActive(FName InStateMachineName, bool NewActive);

	// Whether the selected Intention State Machine is active or not
	UFUNCTION(BlueprintCallable, BlueprintPure, Category = "MPAS|Handler|IntentionDriver")
	bool IsIntentionStateMachineActive(FName InStateMachineName);



// POSITION DRIVERS

protected:

	// Map of all position drivers, located in the rig, <Driver Name, Pointer>
	TMap<FName, class UMPAS_PositionDriver*> PositionDrivers;


public:

	// Returns a map of all position drivers, located in the rig, <Driver Name, Pointer>
	UFUNCTION(BlueprintCallable, BlueprintPure, Category= "MPAS|Handler|PositionDriver")
	const TMap<FName, class UMPAS_PositionDriver*>& GetPositionDrivers() { return PositionDrivers; }



// INPUT

protected:
	
	// Movement Input Direction, comes from the controller, is processed by the Intention Driver
	FVector MovementInputDirection;

	// Determines the desired rotation of the core, comes fromt the controller, is processed by the Intention Driver
	FRotator InputTargetRotation;

public:

	// Sets Movement Input Direction - the Rig will attempt moving in this direction
	UFUNCTION(BlueprintCallable, Category = "MPAS|Handler|Input")
	void SetMovementInputDirection(FVector InMovementInputDirection) { MovementInputDirection = InMovementInputDirection; }

	// Returns Movement Input Direction
	UFUNCTION(BlueprintCallable, BlueprintPure, Category = "MPAS|Handler|Input")
	FVector GetMovementInputDirection() { return MovementInputDirection; }

	// Sets Input Target Rotation - the Rig will attempt orienting to it
	UFUNCTION(BlueprintCallable, Category = "MPAS|Handler|Input")
	void SetInputTargetRotation(FRotator InInputTargetRotation) { InputTargetRotation = InInputTargetRotation; }

	// Returns Input Target Rotation
	UFUNCTION(BlueprintCallable, BlueprintPure, Category = "MPAS|Handler|Input")
	FRotator GetInputTargetRotation() { return InputTargetRotation; }



// CUSTOM PARAMETERS

protected:

	// Parameter subscribers map
	UPROPERTY()
	TMap<FName, FOnParameterValueChanged> ParameterSubscriptions;


	// Integer Parameters Storage
	UPROPERTY()
	TMap<FName, int32> IntParameters = { {"Default", 0} };

	// Float Parameters Storage
	UPROPERTY()
	TMap<FName,float> FloatParameters = { {"Default", 0} };;

	// Vector Parameters Storage
	UPROPERTY()
	TMap<FName, FVector> VectorParameters = { {"Default", FVector()} };;



protected:

	// Notifies subscribers of the parameter change
	void OnParameterUpdated(FName ParameterName);

	// Sets <T> Parameter in given Map:
	template< typename T >
	void SetParameterValue(TMap<FName, T>& ParameterStorage, const FName& ParameterName, const T& Value)
	{
		if (ParameterStorage.Contains(ParameterName))
		{
			ParameterStorage[ParameterName] = Value;
			OnParameterUpdated(ParameterName); 
		}
	}

	// Gets <T> Parameter from given Map
	template< typename T >
	const T& GetParameterValue(TMap<FName, T>& ParameterStorage, const FName& ParameterName)
	{
		if (ParameterStorage.Contains(ParameterName))
			return ParameterStorage[ParameterName];

		return ParameterStorage["Default"];
	}

public:
 
	// Subscriptions

	// Subscribes the given rig element to the given custom parameter's update notifications
	UFUNCTION(BlueprintCallable, Category="MPAS|Handler|CustomParameters")
	void SubscribeToParameter(FName ParameterName, UObject* Subscriber, FName NotificationFunctionName);


	// INTEGER Parameters

	UFUNCTION(BlueprintCallable, Category="MPAS|Handler|CustomParameters")
	void CreateIntParameter(FName ParameterName, int32 DefaultValue) { IntParameters.Add(ParameterName, DefaultValue); OnParameterUpdated(ParameterName); }

	UFUNCTION(BlueprintCallable, Category="MPAS|Handler|CustomParameters")
	void SetIntParameter(FName ParameterName, int32 Value) { SetParameterValue<int>(IntParameters, ParameterName, Value); }

	UFUNCTION(BlueprintCallable, BlueprintPure, Category="MPAS|Handler|CustomParameters")
	int32 GetIntParameter(FName ParameterName) { return GetParameterValue<int>(IntParameters, ParameterName); }

	UFUNCTION(BlueprintCallable, BlueprintPure, Category="MPAS|Handler|CustomParameters")
	bool IsIntParameterValid(FName ParameterName) { return IntParameters.Contains(ParameterName); }


	// FLOAT Parameters

	UFUNCTION(BlueprintCallable, Category="MPAS|Handler|CustomParameters")
	void CreateFloatParameter(FName ParameterName, float DefaultValue) { FloatParameters.Add(ParameterName, DefaultValue); OnParameterUpdated(ParameterName); }

	UFUNCTION(BlueprintCallable, Category="MPAS|Handler|CustomParameters")
	void SetFloatParameter(FName ParameterName, float Value) { SetParameterValue<float>(FloatParameters, ParameterName, Value); }

	UFUNCTION(BlueprintCallable, BlueprintPure, Category="MPAS|Handler|CustomParameters")
	float GetFloatParameter(FName ParameterName) { return GetParameterValue<float>(FloatParameters, ParameterName); }

	UFUNCTION(BlueprintCallable, BlueprintPure, Category="MPAS|Handler|CustomParameters")
	bool IsFloatParameterValid(FName ParameterName) { return FloatParameters.Contains(ParameterName); }


	// FVECTOR Parameters

	UFUNCTION(BlueprintCallable, Category="MPAS|Handler|CustomParameters")
	void CreateVectorParameter(FName ParameterName, FVector DefaultValue) { VectorParameters.Add(ParameterName, DefaultValue); OnParameterUpdated(ParameterName); }

	UFUNCTION(BlueprintCallable, Category="MPAS|Handler|CustomParameters")
	void SetVectorParameter(FName ParameterName, FVector Value) { SetParameterValue<FVector>(VectorParameters, ParameterName, Value); }

	UFUNCTION(BlueprintCallable, BlueprintPure, Category="MPAS|Handler|CustomParameters")
	FVector GetVectorParameter(FName ParameterName) { return GetParameterValue<FVector>(VectorParameters, ParameterName); }

	UFUNCTION(BlueprintCallable, BlueprintPure, Category="MPAS|Handler|CustomParameters")
	bool IsVectorParameterValid(FName ParameterName) { return VectorParameters.Contains(ParameterName); }




/* PROPOGATION
 * 
 * A set of functions that make rig element propogation easy
 * What is propogation?
 * 		Pick an element in the rig, then pick it's adjacent elements, then their adjacent elements and so on, until you rich the progopation depth
 */

protected:

	// Recursive function, processing a single rig element and calling itself on adjacent elements
	void Propogation_ProcessElement(TArray<FName>& OutPropogation, const FName& InElement, const FMPAS_PropogationSettings& InPropogationSettings, int32 InCurrentDepth);

public:

	/*
	 * Pick an element in the rig, then pick it's adjacent elements, then their adjacent elements and so on, until you rich the progopation depth
	 * Upon finishing, OutPropogation array will contain all processed elements (including the starting one) in order of processing
	 */
	UFUNCTION(BlueprintCallable, Category="MPAS|Handler|Propogation")
	void PropogateFromElement(TArray<FName>& OutPropogation, FName InStartingElement, FMPAS_PropogationSettings InPropogationSettings);
};