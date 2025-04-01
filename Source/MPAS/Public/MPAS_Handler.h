// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "Components/ActorComponent.h"
#include "MPAS_IntentionStateMachine.h"
#include "MPAS_Handler.generated.h"


// DELEGATES

// Fires off when custom MPAS_Handler parameter value is changed
DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(FOnParameterValueChanged, FName, InParameterName);

// Fires off when MPAS_Handler timer has finished
DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(FOnTimerFinished, FName, InTimerName);

// Fires off every tick when MPAS_Handler timeline is playing, reports the current time of the timeline
DECLARE_DYNAMIC_MULTICAST_DELEGATE_TwoParams(FOnTimelineUpdated, FName, InTimelineName, float, InCurrentTime);

// Fires off when MPAS_Handler timeline has finished playing
DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(FOnTimelineFinished, FName, InTimelineName);



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

	FMPAS_PhysicsModelConstraintData(class UPhysicsConstraintComponent* InConstraintComponent = nullptr, class UPrimitiveComponent* InBody_1 = nullptr, class UPrimitiveComponent* InBody_2 = nullptr): ConstraintComponent(InConstraintComponent), Body_1(InBody_1), Body_2(InBody_2) {}
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
	FMPAS_PhysicsElementsArray(const TArray<class UMPAS_PhysicsModelElement*>& InElements): Elements(InElements) {}
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

	// Updates all elements in RigData
	void UpdateRig(float DeltaTime);


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

	// Activates / Deactivates selected Intention State Machine
	UFUNCTION(BlueprintCallable, Category = "MPAS|Handler|IntentionDriver")
	void SetIntentionStateMachineActive(FName InStateMachineName, bool NewActive);

	// Whether the selected Intention State Machine is active or not
	UFUNCTION(BlueprintCallable, BlueprintPure, Category = "MPAS|Handler|IntentionDriver")
	bool IsIntentionStateMachineActive(FName InStateMachineName);



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



// PHYSICS MODEL

protected:

	// A map of all physics elements in the physics model : < Rig Element Name - Pointer to physics element component>
	UPROPERTY() 
	TMap<FName, FMPAS_PhysicsElementsArray> PhysicsModelElements;

	/* 
	 * A map of all physics constraints in the physics model : < Attached Element Name - Constraint Data >
	 * Used for reactivating constraints when the physics model is enabled
	 * (Honestly, this is a work around of an Unreal thing, that quitly kills the constraint 
	 * when the collision on one of the constrainted objects is disabled)
	 */
	UPROPERTY() 
	TMap<FName, FMPAS_ElementPhysicsConstraints> PhysicsModelConstraints;

	// Whether the physics model is processed for this rig or not
	bool PhysicsModelEnabled;

public:

	// Time (in seconds) between restabilization attempts (when physics model is enabled)
	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category="Default|PhysicsModel")
	float RestabilizationCycleTime = 3.f;

protected:

	// Generates a physics model based on all rig elements
	void GeneratePhysicsModel();

	// An iteration of generating of a physics model
	void GeneratePhysicsElement(FName InRigElementName);

	// Updates all physics elements in the physics model, called when the physics model is enabled
	void UpdatePhysicsModel(float DeltaTime);

public:

	// Tells the handler whether the physics model should be processed for this rig or not
	UFUNCTION(BlueprintCallable, Category =  "MPAS|Handler|PhysicsModel")
	void SetPhysicsModelEnabled(bool InNewEnabled);

	// Whether the physics model is processed for this rig or not
	UFUNCTION(BlueprintCallable, Category =  "MPAS|Handler|PhysicsModel")
	bool GetPhysicsModelEnabled() { return PhysicsModelEnabled; }

	// Enables physics model for all rig elements
	UFUNCTION(BlueprintCallable, Category =  "MPAS|Handler|PhysicsModel")
	void EnablePhysicsModelFullRig();

	// Returns data about all of the registered physics model constraints
	UFUNCTION(BlueprintCallable, BlueprintPure, Category =  "MPAS|Handler|PhysicsModel")
	const TMap<FName, FMPAS_ElementPhysicsConstraints>& GetPhysicsModelConstraints() { return PhysicsModelConstraints; }

	// Called by a restabilization timer, attempts restabilization of the rig, if it's physics model is enabled
	UFUNCTION()
	void OnRestabilizationCycleTicked();

public:
	// CALLED BY THE RIG ELEMENT: Reactivates element's parent  physics constraint in the physics model (see PhysicsModelConstraints description)
	void ReactivatePhysicsModelConstraint(FName InRigElementName);

	// CALLED BY THE RIG ELEMENT: Deactivates element's parent  physics constraint in the physics model (see PhysicsModelConstraints description)
	void DeactivatePhysicsModelConstraint(FName InRigElementName);


// TIMERS

protected:

	// Timer data struct
	struct FTimer
	{
		FName TimerName;
		bool Paused = true;
		float Time = 0.f;
		float InitialTime = 0.f;
		bool Loop = false;

		FOnTimerFinished OnTimerFinished;
	};

	// Map of all active Timers
	TMap<FName, FTimer> Timers;

	// Updates all active timers
	void UpdateTimers(float DeltaTime);

public:
	// Creates a new timer
	UFUNCTION(BlueprintCallable, Category="MPAS|Handler|Timers")
	bool CreateTimer(FName TimerName, float Time, bool Loop = false, bool AutoStart = true);

	// Sets timer back to it's initial value
	UFUNCTION(BlueprintCallable, Category="MPAS|Handler|Timers")
	bool ResetTimer(FName TimerName);

	// Starts a timer
	UFUNCTION(BlueprintCallable, Category="MPAS|Handler|Timers")
	bool StartTimer(FName TimerName);
	
	// Pauses a timer
	UFUNCTION(BlueprintCallable, Category="MPAS|Handler|Timers")
	bool PauseTimer(FName TimerName);

	// Deletes a timer
	UFUNCTION(BlueprintCallable, Category="MPAS|Handler|Timers")
	bool DeleteTimer(FName TimerName);

	// Subscribes a rig element to a timer
	UFUNCTION(BlueprintCallable, Category="MPAS|Handler|Timers")
	bool SubscribeToTimer(FName TimerName, UObject* Subscriber, FName NotificationFunctionName);

	// Unsubscribes a rig element from a timer
	UFUNCTION(BlueprintCallable, Category="MPAS|Handler|Timers")
	bool UnSubscribeFromTimer(FName TimerName, UObject* Subscriber, FName NotificationFunctionName);

	// Returns the value of the timer
	UFUNCTION(BlueprintCallable, BlueprintPure, Category="MPAS|Handler|Timers")
	float GetTimerValue(FName TimerName);



// TIMELINES

protected:

	// Timeline data struct
	struct FTimeline
	{
		FName TimelineName;
		FTimer Timer;

		float PlaybackSpeed = 1.f;
		bool Reversed = false;

		FOnTimelineUpdated OnTimelineUpdated;
		FOnTimelineFinished OnTimelineFinished;
	};

	// Map of all active timelines
	TMap<FName, FTimeline> Timelines;

	// Updates all active timelines
	void UpdateTimelines(float DeltaTime);

public:
	// Creates a new timeline
	UFUNCTION(BlueprintCallable, Category="MPAS|Handler|Timelines")
	bool CreateTimeline(FName TimelineName, float Length, bool Loop = false, bool AutoStart = false);

	// Starts timeline playback
	UFUNCTION(BlueprintCallable, Category="MPAS|Handler|Timelines")
	bool StartTimeline(FName TimelineName);
	
	// Pauses timeline playback
	UFUNCTION(BlueprintCallable, Category="MPAS|Handler|Timelines")
	bool PauseTimeline(FName TimelineName);
	
	// Plays timeline starting at the current time
	UFUNCTION(BlueprintCallable, Category="MPAS|Handler|Timelines")
	bool PlayTimeline(FName TimelineName);

	// Reverses timeline playback
	UFUNCTION(BlueprintCallable, Category="MPAS|Handler|Timelines")
	bool ReverseTimeline(FName TimelineName);

	// Resets time line time to zero
	UFUNCTION(BlueprintCallable, Category="MPAS|Handler|Timelines")
	bool ResetTimeline(FName TimelineName);

	// Sets timepline current time to a new value
	UFUNCTION(BlueprintCallable, Category="MPAS|Handler|Timelines")
	bool SetTimelineTime(FName TimelineName, float NewTime);

	// Sets timepline playback speed to a new value
	UFUNCTION(BlueprintCallable, Category="MPAS|Handler|Timelines")
	bool SetTimelinePlaybackSpeed(FName TimelineName, float NewSpeed);

	// Deletes a timeline
	UFUNCTION(BlueprintCallable, Category="MPAS|Handler|Timelines")
	bool DeleteTimeline(FName TimelineName);

	// Subscribes a rig element to a timepline, pass in "" as function name, if you don't want to subscriber to that event
	UFUNCTION(BlueprintCallable, Category="MPAS|Handler|Timelines")
	bool SubscribeToTimeline(FName TimelineName, UObject* Subscriber, FName OnUpdateFunctionName, FName OnFinishedFunctionName);

	// Unsubscribes a rig element from a timeline
	UFUNCTION(BlueprintCallable, Category="MPAS|Handler|Timelines")
	bool UnSubscribeFromTimeline(FName TimelineName, UObject* Subscriber, FName OnUpdateFunctionName, FName OnFinishedFunctionName);

	// Returns the value of the current time of the timeline
	UFUNCTION(BlueprintCallable, BlueprintPure, Category="MPAS|Handler|Timelines")
	float GetTimelineTime(FName TimelineName);

	// Returns the length of the timeline
	UFUNCTION(BlueprintCallable, BlueprintPure, Category="MPAS|Handler|Timelines")
	float GetTimelineLength(FName TimelineName);

	// Returns the playback speed of the timeline
	UFUNCTION(BlueprintCallable, BlueprintPure, Category="MPAS|Handler|Timelines")
	float GetTimelinePlaybackspeed(FName TimelineName);

	// Returns timeline is reversed value
	UFUNCTION(BlueprintCallable, BlueprintPure, Category="MPAS|Handler|Timelines")
	bool GetTimelineReversed(FName TimelineName);



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