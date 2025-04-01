// Fill out your copyright notice in the Description page of Project Settings.


#include "MPAS_Handler.h"
#include "MPAS_Core.h"
#include "MPAS_RigElement.h"
#include "MPAS_VisualRigElement.h"
#include "Kismet/GameplayStatics.h"
#include "PhysicsEngine/PhysicsConstraintComponent.h"


// Sets default values for this component's properties
UMPAS_Handler::UMPAS_Handler()
{
	// Set this component to be initialized when the game starts, and to be ticked every frame.  You can turn these features
	// off to improve performance if you don't need them.
	PrimaryComponentTick.bCanEverTick = true;

	// ...
}


// Called when the game starts
void UMPAS_Handler::BeginPlay()
{
	Super::BeginPlay();

	// Scans rig on begin play
	ScanRig();

	// Initialize rig after scanning
	InitRig();

	// Generates rig's physics model
	GeneratePhysicsModel();

	// Lings rig after initializing
	LinkRig();
	
}


// Called every frame
void UMPAS_Handler::TickComponent(float DeltaTime, ELevelTick TickType, FActorComponentTickFunction* ThisTickFunction)
{
	Super::TickComponent(DeltaTime, TickType, ThisTickFunction);

	// Updates rig every tick
	UpdateRig(DeltaTime);

	// Updates all of the timers
	UpdateTimers(DeltaTime);

	// Updates all timelines
	UpdateTimelines(DeltaTime);

	// Updates intention driver
	UpdateIntentionDriver(DeltaTime);

	// Updates physics model if its enabled
	UpdatePhysicsModel(DeltaTime);
}


// Scans the rig and filss Core and Rig Data
void UMPAS_Handler::ScanRig()
{
	// Finding Core
	Core = Cast<UMPAS_Core>(GetOwner()->FindComponentByClass(UMPAS_Core::StaticClass()));

	if (Core)
	{
		// Getting Core Children
		TArray<USceneComponent*> CoreChildComponents;
		Core->GetChildrenComponents(false, CoreChildComponents);

		// Looping over all of them, checking if they are Rig Elements, and if true: scanning them with ScanElement
		for (int32 i = 0; i < CoreChildComponents.Num(); i++)
		{
			UMPAS_RigElement* RigElement = Cast<UMPAS_RigElement>(CoreChildComponents[i]);
			if (RigElement)
				ScanElement(RigElement, "Core");
		}		
	}
}


// Recursively scans Rig Element
void UMPAS_Handler::ScanElement(UMPAS_RigElement* RigElement, const FName& ParentElementName)
{
	// Getting Element's name and Initializing Rig Element Data struct
	FName Name = FName(UKismetSystemLibrary::GetObjectName(RigElement));
	FMPAS_RigElementData ElementData(Name, RigElement, ParentElementName);

	// Setting element data
	RigElement->RigElementName = Name;

	// Storing Core elements
	if (ParentElementName == "Core")
		CoreElements.Add(Name);

	// Whether the element is visual
	bool IsVisual = Cast<UMPAS_VisualRigElement>(RigElement) != nullptr;
	
	// Getting all children of this element
	TArray<USceneComponent*> CoreChildComponents;
	RigElement->GetChildrenComponents(false, CoreChildComponents);

	// Looping over all of them
	for (int32 i = 0; i < CoreChildComponents.Num(); i++)
	{
		// Checking if they are also rig elements
		UMPAS_RigElement* ChildElement = Cast<UMPAS_RigElement>(CoreChildComponents[i]);
		
		if (ChildElement)
		{
			// Registering them as children of the current element
			FName ChildName = FName(UKismetSystemLibrary::GetObjectName(ChildElement));
			ElementData.AddChildElement(ChildName);

			// Scanning child elements
			ScanElement(ChildElement, (!IsVisual) ? Name : ParentElementName);
		}
	}

	// Storing current Element's data in RigData
	RigData.Add(Name, ElementData);
}


// Initializes all elements in RigData
void UMPAS_Handler::InitRig()
{
	for (auto& RigElement : RigData)
	{
		RigElement.Value.RigElement->InitRigElement(this);

		FDetachmentTransformRules DetachRules(EDetachmentRule::KeepWorld, true);
		RigElement.Value.RigElement->DetachFromComponent(DetachRules);
	}
}


// Links all elements in RigData
void UMPAS_Handler::LinkRig()
{
	for (auto& RigElement: RigData)
		RigElement.Value.RigElement->LinkRigElement(this);
}


// Updates all elements in RigData
void UMPAS_Handler::UpdateRig(float DeltaTime)
{
	for (auto& RigElement : RigData)
		RigElement.Value.RigElement->UpdateRigElement(DeltaTime);
}



// BONE BUFFER

// Sets transform of a single bone
void UMPAS_Handler::SetBoneTransform(FName InBone, FTransform InTransform)
{
	BoneTransforms.Add(InBone, InTransform);
}

// Sets location of a single bone
void UMPAS_Handler::SetBoneLocation(FName InBone, FVector InLocation)
{
	if (!BoneTransforms.Contains(InBone))
		BoneTransforms.Add(InBone, FTransform());

	BoneTransforms[InBone].SetLocation(InLocation);
}

// Sets rotation of a single bone
void UMPAS_Handler::SetBoneRotation(FName InBone, FRotator InRotation)
{
	if (!BoneTransforms.Contains(InBone))
		BoneTransforms.Add(InBone, FTransform());

	BoneTransforms[InBone].SetRotation(FQuat(InRotation));
}

// Sets scale of a single bone
void UMPAS_Handler::SetBoneScale(FName InBone, FVector InScale)
{
	if (!BoneTransforms.Contains(InBone))
		BoneTransforms.Add(InBone, FTransform());

	BoneTransforms[InBone].SetScale3D(InScale);
}

// Returns data about single bone transform
FTransform UMPAS_Handler::GetSingleBoneTransform(FName InBone)
{
	if (!BoneTransforms.Contains(InBone))
		return FTransform();

	return BoneTransforms[InBone];
}



// INTENTION DRIVER

// Updates all Intention State Machines
void UMPAS_Handler::UpdateIntentionDriver(float DeltaTime)
{
	for (auto& IntentionSM: IntentionStateMachines)
		if (IntentionSM.Value->IsActive())
			IntentionSM.Value->UpdateStateMachine(DeltaTime);
}


// Initializes default custom parameters, that can be controlled by the Intention Driver
void UMPAS_Handler::InitDefaultControlParameters()
{
	
	// To be added later

}


// Adds a Intention State Machine to the Intention Driver
bool UMPAS_Handler::AddIntentionStateMachine(TSubclassOf<UMPAS_IntentionStateMachine> InStateMachineClass, FName InStateMachineName)
{
	if (IntentionStateMachines.Contains(InStateMachineName))
		return false;

	UMPAS_IntentionStateMachine* NewStateMachine = NewObject<UMPAS_IntentionStateMachine>(this, InStateMachineClass);

	if (!NewStateMachine)
		return false;

	NewStateMachine->LinkToHandler(this, InStateMachineName);
	NewStateMachine->InitStateMachine();
	
	IntentionStateMachines.Add(InStateMachineName, NewStateMachine);

	return true;
}

// Activates / Deactivates selected Intention State Machine
void UMPAS_Handler::SetIntentionStateMachineActive(FName InStateMachineName, bool NewActive)
{
	if (IntentionStateMachines.Contains(InStateMachineName))
		return;

	IntentionStateMachines[InStateMachineName]->SetActive(NewActive);
}

// Whether the selected Intention State Machine is active or not
bool UMPAS_Handler::IsIntentionStateMachineActive(FName InStateMachineName)
{
	if (IntentionStateMachines.Contains(InStateMachineName))
		return false;

	return IntentionStateMachines[InStateMachineName]->IsActive();
}



// PHYSICS MODEL

// Generates a physics model based on all rig elements
void UMPAS_Handler::GeneratePhysicsModel()
{
	for (auto& RigElementData: RigData)
	{	
		if (RigElementData.Value.ParentComponent == FName("Core"))
			GeneratePhysicsElement(RigElementData.Key);
	}

	CreateTimer("RestabilizationTimer", RestabilizationCycleTime, true, false);
	SubscribeToTimer("RestabilizationTimer", this, "OnRestabilizationCycleTicked");
}

// An iteration of generating of a physics model
void UMPAS_Handler::GeneratePhysicsElement(FName InRigElementName)
{
	UMPAS_RigElement* RigElement = RigData[InRigElementName].RigElement;

	TArray<UMPAS_PhysicsModelElement*> AllPhysicsElements;

	PhysicsModelConstraints.Add(InRigElementName, FMPAS_ElementPhysicsConstraints());

	for (int32 i = 0; i < RigElement->PhysicsElementsConfiguration.Num(); i++)
	{
		if(RigElement->PhysicsElementsConfiguration[i].PhysicsElementClass)
		{
			const FMPAS_PhysicsElementConfiguration& PhysicsElementConfig = RigElement->PhysicsElementsConfiguration[i];
			
			// Creating physics element
			UActorComponent* PhysicsElementActorComponent = GetOwner()->AddComponentByClass(PhysicsElementConfig.PhysicsElementClass, true, FTransform(), false);
			UMPAS_PhysicsModelElement* PhysicsElement = Cast<UMPAS_PhysicsModelElement>(PhysicsElementActorComponent);
			PhysicsElement->SetWorldTransform(RigElement->GetComponentTransform());
			GetOwner()->AddInstanceComponent(PhysicsElement);

			PhysicsElement->SetMobility(EComponentMobility::Movable);

			// Attachements

			if (RigData[InRigElementName].ParentComponent != "Core")
			{
				
				// Hard
				if (PhysicsElementConfig.ParentPhysicalAttachmentType == EMPAS_PhysicsModelAttachmentType::Hard)
				{
					FAttachmentTransformRules HardAttachmentRules = FAttachmentTransformRules(EAttachmentRule::KeepWorld, true);

					if (PhysicsElementConfig.ParentType == EMPAS_PhysicsModelParentType::ChainedPhysicsElement && AllPhysicsElements.IsValidIndex(PhysicsElementConfig.ParentPhysicsElementID))
						PhysicsElement->AttachToComponent(AllPhysicsElements[PhysicsElementConfig.ParentPhysicsElementID], HardAttachmentRules);

					else if(PhysicsModelElements[RigData[InRigElementName].ParentComponent].Elements.IsValidIndex(0))
						PhysicsElement->AttachToComponent(PhysicsModelElements[RigData[InRigElementName].ParentComponent].Elements[0], HardAttachmentRules);
				}

				// Limited
				else if (PhysicsElementConfig.ParentPhysicalAttachmentType == EMPAS_PhysicsModelAttachmentType::LimitedPosition || 
						 PhysicsElementConfig.ParentPhysicalAttachmentType == EMPAS_PhysicsModelAttachmentType::LimitedRotation ||
						 PhysicsElementConfig.ParentPhysicalAttachmentType == EMPAS_PhysicsModelAttachmentType::LimitedPositionRotation)
				{
					// Create and setup a physics constraint
					// Creating the component
					UActorComponent* PhysicsConstraintActorComponent = GetOwner()->AddComponentByClass(UPhysicsConstraintComponent::StaticClass(), false, RigElement->GetDesiredPhysicsElementTransform(i), false);
					UPhysicsConstraintComponent* PhysicsConstraintComponent = Cast<UPhysicsConstraintComponent>(PhysicsConstraintActorComponent);
					GetOwner()->AddInstanceComponent(PhysicsConstraintComponent);

					PhysicsConstraintComponent->SetDisableCollision(PhysicsElementConfig.DisableCollisionWithParent);

					// Constraint Component attachement

					UPrimitiveComponent* ParentElement = nullptr;

					if (PhysicsElementConfig.ParentType == EMPAS_PhysicsModelParentType::ChainedPhysicsElement && AllPhysicsElements.IsValidIndex(PhysicsElementConfig.ParentPhysicsElementID))
						ParentElement = AllPhysicsElements[PhysicsElementConfig.ParentPhysicsElementID];

					else if(PhysicsModelElements[RigData[InRigElementName].ParentComponent].Elements.IsValidIndex(0) && PhysicsModelElements[RigData[InRigElementName].ParentComponent].Elements[0] != PhysicsElement)
						ParentElement = PhysicsModelElements[RigData[InRigElementName].ParentComponent].Elements[0];

					else
					{
						UActorComponent* PhysicsHookComponent = GetOwner()->AddComponentByClass(UPrimitiveComponent::StaticClass(), false, RigElement->GetDesiredPhysicsElementTransform(i), false);
						ParentElement = Cast<UPrimitiveComponent>(PhysicsHookComponent);
						GetOwner()->AddInstanceComponent(PhysicsHookComponent);

						ParentElement->AttachToComponent(RigElement, FAttachmentTransformRules(EAttachmentRule::KeepWorld, true));
					}

					PhysicsConstraintComponent->SetWorldLocation(PhysicsElement->GetComponentLocation());
					FAttachmentTransformRules LimitedConstraintAttachmentRules = FAttachmentTransformRules(EAttachmentRule::KeepWorld, EAttachmentRule::SnapToTarget, EAttachmentRule::KeepWorld, true);
					//FAttachmentTransformRules LimitedConstraintAttachmentRules = FAttachmentTransformRules(EAttachmentRule::SnapToTarget, true);
					PhysicsConstraintComponent->AttachToComponent(ParentElement, LimitedConstraintAttachmentRules);

					// Setting constrainted elements
					//PhysicsConstraintComponent->SetConstrainedComponents(PhysicsModelElements[RigData[InRigElementName].ParentComponent], FName(), PhysicsElement, FName());

					// Constraint settings

					// Positional limits
					if (PhysicsElementConfig.ParentPhysicalAttachmentType == EMPAS_PhysicsModelAttachmentType::LimitedPosition ||
						PhysicsElementConfig.ParentPhysicalAttachmentType == EMPAS_PhysicsModelAttachmentType::LimitedPositionRotation)
					{
						PhysicsConstraintComponent->SetLinearXLimit(ELinearConstraintMotion::LCM_Limited, PhysicsElementConfig.PhysicsConstraintLimits.X);
						PhysicsConstraintComponent->SetLinearYLimit(ELinearConstraintMotion::LCM_Limited, PhysicsElementConfig.PhysicsConstraintLimits.Y);
						PhysicsConstraintComponent->SetLinearZLimit(ELinearConstraintMotion::LCM_Limited, PhysicsElementConfig.PhysicsConstraintLimits.Z);
					}

					// Angular Limitis
					if (PhysicsElementConfig.ParentPhysicalAttachmentType == EMPAS_PhysicsModelAttachmentType::LimitedRotation ||
						PhysicsElementConfig.ParentPhysicalAttachmentType == EMPAS_PhysicsModelAttachmentType::LimitedPositionRotation)
					{
						PhysicsConstraintComponent->SetAngularSwing1Limit(EAngularConstraintMotion::ACM_Limited, PhysicsElementConfig.PhysicsAngularLimits.Yaw);
						PhysicsConstraintComponent->SetAngularSwing2Limit(EAngularConstraintMotion::ACM_Limited, PhysicsElementConfig.PhysicsAngularLimits.Pitch);
						PhysicsConstraintComponent->SetAngularTwistLimit(EAngularConstraintMotion::ACM_Limited, PhysicsElementConfig.PhysicsAngularLimits.Roll);
					}
					
					//PhysicsConstraintComponent->UpdateConstraintFrames();
					//PhysicsConstraintComponent->ConstraintInstance.InitConstraint(PhysicsModelElements[RigData[InRigElementName].ParentComponent]->GetBodyInstance(), PhysicsElement->GetBodyInstance(), 1.f, this);

					// Storing constraint data
					FMPAS_PhysicsModelConstraintData ConstraintData(PhysicsConstraintComponent, ParentElement, PhysicsElement);
					PhysicsModelConstraints[InRigElementName].Constraints.Add(ConstraintData);

					PhysicsConstraintComponent->ComponentTags.Add(FName("PhysicsConstraint: " + ParentElement->GetName() + " -> " + PhysicsElement->GetName()));
				}

				// Free
				else if (PhysicsElementConfig.ParentPhysicalAttachmentType == EMPAS_PhysicsModelAttachmentType::Free)
				{
					// Do nothing, I guess
				}
			}

			else
			{
				// Core Physics Elements Attachment
				//FAttachmentTransformRules CoreElementsAttachmentRules = FAttachmentTransformRules(EAttachmentRule::KeepWorld, false);
				//PhysicsElement->AttachToComponent(GetCore(), CoreElementsAttachmentRules);
			}

			AllPhysicsElements.Add(PhysicsElement);
		}
	}

	// Storing physics elements data in handler
	PhysicsModelElements.Add(InRigElementName, FMPAS_PhysicsElementsArray(AllPhysicsElements));

	// Initializing model elements
	RigElement->InitPhysicsModel(AllPhysicsElements);

	for (int32 i = 0; i < AllPhysicsElements.Num(); i++)
		AllPhysicsElements[i]->InitPhysicsElement(RigElement, i);
		
	// Propogate physics model generation
	for (FName& ChildElement: RigData[InRigElementName].ChildElements)
		GeneratePhysicsElement(ChildElement);
}


// Tells the handler whether the physics model should be processed for this rig or not
void UMPAS_Handler::SetPhysicsModelEnabled(bool InNewEnabled) 
{ 
	PhysicsModelEnabled = InNewEnabled;

	if (PhysicsModelEnabled)
	{
		// Restarting a restabilization Timer
		ResetTimer("RestabilizationTimer");
		StartTimer("RestabilizationTimer");
	}

	else
		PauseTimer("RestabilizationTimer");
}


// Updates all physics elements in the physics model, called when the physics model is enabled
void UMPAS_Handler::UpdatePhysicsModel(float DeltaTime)
{
	if (PhysicsModelEnabled)
	{
		for (auto& PhysicsElementArray: PhysicsModelElements)
			for (auto& PhysicsElement: PhysicsElementArray.Value.Elements)
				PhysicsElement->UpdatePhysicsElement(DeltaTime);
	}
}


// Enables physics model for all rig elements
void UMPAS_Handler::EnablePhysicsModelFullRig()
{
	SetPhysicsModelEnabled(true);
	for (auto& RigElement: RigData)
		RigElement.Value.RigElement->SetPhysicsModelEnabled(true);
}


// CALLED BY THE RIG ELEMENT: Reactivates element's parent  physics constraint in the physics model (see PhysicsModelConstraints description)
void UMPAS_Handler::ReactivatePhysicsModelConstraint(FName InRigElementName)
{
	if (!PhysicsModelConstraints.Contains(InRigElementName))
		return;

	for (auto& ConstraintData: PhysicsModelConstraints[InRigElementName].Constraints)
	{
		ConstraintData.ConstraintComponent->Activate();
		
		// Maybe in the future I will add bone names to this, but not sure how useful this is in the case of an auto generated physics model
		ConstraintData.ConstraintComponent->SetConstrainedComponents(ConstraintData.Body_1, FName(), ConstraintData.Body_2, FName());
	}
}

// CALLED BY THE RIG ELEMENT: Deactivates element's parent  physics constraint in the physics model (see PhysicsModelConstraints description)
void UMPAS_Handler::DeactivatePhysicsModelConstraint(FName InRigElementName)
{
	if (!PhysicsModelConstraints.Contains(InRigElementName))
		return;

	for (auto& ConstraintData: PhysicsModelConstraints[InRigElementName].Constraints)
		ConstraintData.ConstraintComponent->Deactivate();
}

// Called by a restabilization timer, attempts restabilization of the rig, if it's physics model is enabled
void UMPAS_Handler::OnRestabilizationCycleTicked()
{
	for (auto& CoreElement: GetCoreElements())
	{
		if (GetRigData()[CoreElement].RigElement->GetVelocity().Size() < 5.f)
		{
			FMPAS_PropogationSettings PropogationSettings;
			PropogationSettings.PropogateToParent = false;

			TArray<FName> Propogation;
			PropogateFromElement(Propogation, CoreElement, PropogationSettings);

			for (auto& Element: Propogation)
			{
				UMPAS_RigElement* RigElement = GetRigData()[Element].RigElement;
				if (RigElement->GetStabilityStatus() == EMPAS_StabilityStatus::Stable)
				{
					RigElement->SetPhysicsModelEnabled(false);
				}
			}
		}
	}
}



// TIMERS

// Updates all active timers
void UMPAS_Handler::UpdateTimers(float DeltaTime)
{
	TArray<FName> FinishedTimers;

	for (auto& TimerData : Timers)
		if (!TimerData.Value.Paused)
		{
			TimerData.Value.Time -= DeltaTime;

			if (TimerData.Value.Time <= 0)
				FinishedTimers.Add(TimerData.Value.TimerName);
		}

	for (auto& Timer : FinishedTimers)
	{
		Timers[Timer].OnTimerFinished.Broadcast(Timer);

		if (Timers[Timer].Loop)
			Timers[Timer].Time = Timers[Timer].InitialTime;

		else
			DeleteTimer(Timer);
	}
}


// Creates a new timer
bool UMPAS_Handler::CreateTimer(FName TimerName, float Time, bool Loop, bool AutoStart)
{
	if (Timers.Contains(TimerName))
		return false;

	FTimer Timer;
	Timer.TimerName = TimerName;
	Timer.Time = Time;
	Timer.InitialTime = Time;
	Timer.Loop = Loop;
	
	Timers.Add(TimerName, Timer);

	if (AutoStart)
		StartTimer(TimerName);

	return true;
}

// Sets timer back to it's initial value
bool UMPAS_Handler::ResetTimer(FName TimerName)
{
	if (!Timers.Contains(TimerName))
		return false;

	FTimer& Timer = Timers[TimerName];
	Timer.Time = Timer.InitialTime;

	return true;
}

// Starts a timer
bool UMPAS_Handler::StartTimer(FName TimerName)
{
	if (!Timers.Contains(TimerName))
		return false;

	FTimer& Timer = Timers[TimerName];
	Timer.Paused = false;

	return true;
}

// Pauses a timer
bool UMPAS_Handler::PauseTimer(FName TimerName)
{
	if (!Timers.Contains(TimerName))
		return false;

	FTimer& Timer = Timers[TimerName];
	Timer.Paused = true;

	return true;
}

// Deletes a timer
bool UMPAS_Handler::DeleteTimer(FName TimerName)
{
	if (!Timers.Contains(TimerName))
		return false;

	Timers.Remove(TimerName);

	return true;
}


// Subscribes a rig element to a timer
bool UMPAS_Handler::SubscribeToTimer(FName TimerName, UObject* Subscriber, FName NotificationFunctionName)
{
	if (!Timers.Contains(TimerName) || !Subscriber)
		return false;

	TScriptDelegate Delegate;
	Delegate.BindUFunction(Subscriber, NotificationFunctionName);
	Timers[TimerName].OnTimerFinished.Add(Delegate);

	return true;
}

// Unsubscribes a rig element from a timer
bool UMPAS_Handler::UnSubscribeFromTimer(FName TimerName, UObject* Subscriber, FName NotificationFunctionName)
{
	if (!Timers.Contains(TimerName) || !Subscriber)
		return false;

	TScriptDelegate Delegate;
	Delegate.BindUFunction(Subscriber, NotificationFunctionName);
	Timers[TimerName].OnTimerFinished.Remove(Delegate);

	return true;
}

// Returns the value of the timer
float UMPAS_Handler::GetTimerValue(FName TimerName)
{
	if (!Timers.Contains(TimerName))
		return 0;

	return Timers[TimerName].Time;
}


// TIMELINES

// Updates all active timelines
void UMPAS_Handler::UpdateTimelines(float DeltaTime)
{
	TArray<FName> FinishedTimelines;

	for (auto& Timeline : Timelines)
	{
		if (!Timeline.Value.Timer.Paused)
		{
			float Decr = DeltaTime * Timeline.Value.PlaybackSpeed;
			if (Timeline.Value.Reversed)
				Decr *= -1;

			Timeline.Value.Timer.Time -= Decr;

			if (Timeline.Value.Timer.Time <= 0 || Timeline.Value.Timer.Time > Timeline.Value.Timer.InitialTime)
				FinishedTimelines.Add(Timeline.Key);

			Timeline.Value.OnTimelineUpdated.Broadcast(Timeline.Key, Timeline.Value.Timer.InitialTime - Timeline.Value.Timer.Time);
		}
	}

	for (auto& Timeline : FinishedTimelines)
	{
		Timelines[Timeline].OnTimelineFinished.Broadcast(Timeline);

		if (Timelines[Timeline].Timer.Loop)
		{
			if (Timelines[Timeline].Reversed)
				Timelines[Timeline].Timer.Time = 0;

			else
				Timelines[Timeline].Timer.Time = Timelines[Timeline].Timer.InitialTime;
		}

		else
			Timelines[Timeline].Timer.Paused = true;
	}
}

// Creates a new timeline
bool UMPAS_Handler::CreateTimeline(FName TimelineName, float Length, bool Loop, bool AutoStart)
{
	if (Timelines.Contains(TimelineName))
		return false;

	FTimeline NewTimeline;
	NewTimeline.TimelineName = TimelineName;

	FTimer TimelineTimer;
	TimelineTimer.TimerName = FName("TIMELINE_TIMER_" + TimelineName.ToString());
	TimelineTimer.InitialTime = Length;
	TimelineTimer.Time = Length;
	TimelineTimer.Loop = Loop;
	TimelineTimer.Paused = true;
	
	NewTimeline.Timer = TimelineTimer;
	
	Timelines.Add(TimelineName, NewTimeline);

	if (AutoStart)
		StartTimeline(TimelineName);

	return true;
}

// Starts timeline playback
bool UMPAS_Handler::StartTimeline(FName TimelineName)
{
	if (!Timelines.Contains(TimelineName))
		return false;

	ResetTimeline(TimelineName);
	Timelines[TimelineName].Timer.Paused = false;

	return true;
}

// Pauses timeline playback
bool UMPAS_Handler::PauseTimeline(FName TimelineName)
{
	if (!Timelines.Contains(TimelineName))
		return false;

	Timelines[TimelineName].Timer.Paused = true;

	return true;
}

// Plays timeline starting at the current time
bool UMPAS_Handler::PlayTimeline(FName TimelineName)
{
	if (!Timelines.Contains(TimelineName))
		return false;

	Timelines[TimelineName].Timer.Paused = false;

	return true;
}

// Reverses timeline playback
bool UMPAS_Handler::ReverseTimeline(FName TimelineName)
{
	if (!Timelines.Contains(TimelineName))
		return false;

	Timelines[TimelineName].Reversed = !Timelines[TimelineName].Reversed;

	return true;
}

// Resets time line time to zero
bool UMPAS_Handler::ResetTimeline(FName TimelineName)
{
	if (!Timelines.Contains(TimelineName))
		return false;

	Timelines[TimelineName].Timer.Time = Timelines[TimelineName].Timer.InitialTime;

	return true;
}

// Sets timepline current time to a new value
bool UMPAS_Handler::SetTimelineTime(FName TimelineName, float NewTime)
{
	if (!Timelines.Contains(TimelineName))
		return false;

	Timelines[TimelineName].Timer.Time = NewTime;

	return true;
}

// Sets timepline playback speed to a new value
bool UMPAS_Handler::SetTimelinePlaybackSpeed(FName TimelineName, float NewSpeed)
{
	if (!Timelines.Contains(TimelineName))
		return false;

	Timelines[TimelineName].PlaybackSpeed = NewSpeed;

	return true;
}

// Deletes a timeline
bool UMPAS_Handler::DeleteTimeline(FName TimelineName)
{
	if (!Timelines.Contains(TimelineName))
		return false;

	Timelines.Remove(TimelineName);

	return true;
}

// Subscribes a rig element to a timepline
bool UMPAS_Handler::SubscribeToTimeline(FName TimelineName, UObject* Subscriber, FName OnUpdateFunctionName, FName OnFinishedFunctionName)
{
	if (!Timelines.Contains(TimelineName) || !Subscriber)
		return false;

	// On Updated event
	if (OnUpdateFunctionName != FName(""))
	{
		FScriptDelegate Delegate;
		Delegate.BindUFunction(Subscriber, OnUpdateFunctionName);
		Timelines[TimelineName].OnTimelineUpdated.Add(Delegate);
	}
	
	// On Finished event
	if (OnFinishedFunctionName != FName(""))
	{
		FScriptDelegate Delegate;
		Delegate.BindUFunction(Subscriber, OnFinishedFunctionName);
		Timelines[TimelineName].OnTimelineFinished.Add(Delegate);
	}

	return true;
}

// Unsubscribes a rig element from a timeline
bool UMPAS_Handler::UnSubscribeFromTimeline(FName TimelineName, UObject* Subscriber, FName OnUpdateFunctionName, FName OnFinishedFunctionName)
{
	if (!Timelines.Contains(TimelineName) || !Subscriber)
		return false;

	// On Updated event
	if (OnUpdateFunctionName != FName(""))
	{
		FScriptDelegate Delegate;
		Delegate.BindUFunction(Subscriber, OnUpdateFunctionName);
		Timelines[TimelineName].OnTimelineUpdated.Remove(Delegate);
	}
	
	// On Finished event
	if (OnFinishedFunctionName != FName(""))
	{
		FScriptDelegate Delegate;
		Delegate.BindUFunction(Subscriber, OnFinishedFunctionName);
		Timelines[TimelineName].OnTimelineFinished.Remove(Delegate);
	}

	return true;
}

// Returns the value of the current time of the timeline
float UMPAS_Handler::GetTimelineTime(FName TimelineName)
{
	if (!Timelines.Contains(TimelineName))
		return 0;

	return Timelines[TimelineName].Timer.Time;
}

// Returns the length of the timeline
float UMPAS_Handler::GetTimelineLength(FName TimelineName)
{
	if (!Timelines.Contains(TimelineName))
		return 0;

	return Timelines[TimelineName].Timer.InitialTime;
}

// Returns the playback speed of the timeline
float UMPAS_Handler::GetTimelinePlaybackspeed(FName TimelineName)
{
	if (!Timelines.Contains(TimelineName))
		return 0;

	return Timelines[TimelineName].PlaybackSpeed;
}

// Returns timeline is reversed value
bool UMPAS_Handler::GetTimelineReversed(FName TimelineName)
{
	if (!Timelines.Contains(TimelineName))
		return false;

	return Timelines[TimelineName].Reversed;
}



// Notifies subscribers of the parameter change
void UMPAS_Handler::OnParameterUpdated(FName ParameterName)
{
	if (ParameterSubscriptions.Contains(ParameterName))
		ParameterSubscriptions[ParameterName].Broadcast(ParameterName);
}


// Subscribes the given rig element to the given custom parameter's update notifications
void UMPAS_Handler::SubscribeToParameter(FName ParameterName, UObject* Subscriber, FName NotificationFunctionName)
{
	if (Subscriber)
	{
		TScriptDelegate Delegate;
		Delegate.BindUFunction(Subscriber, NotificationFunctionName);

		if (!ParameterSubscriptions.Contains(ParameterName))
			ParameterSubscriptions.Add(ParameterName, FOnParameterValueChanged());

		ParameterSubscriptions[ParameterName].Add(Delegate);			
	}
}



// PROPOGATION

// Recursive function, processing a single rig element and calling itself on adjacent elements
void UMPAS_Handler::Propogation_ProcessElement(TArray<FName>& OutPropogation, const FName& InElement, const FMPAS_PropogationSettings& InPropogationSettings, int32 InCurrentDepth)
{

	// Depth check
	if (InPropogationSettings.Depth != 0 && InCurrentDepth > InPropogationSettings.Depth)
		return;

	// Filter
	// If the element does not pass, then the function will return
	if (InPropogationSettings.EnableTagFilter)
	{
		// If filter is in black list mode
		if (InPropogationSettings.FilterBlackListMode)
		{
			bool BlackListed = false;
			for (auto& Tag: InPropogationSettings.TagFilter)
				if (RigData[InElement].RigElement->ComponentHasTag(Tag))
				{
					BlackListed = true;
					break;
				}

			if (BlackListed)
				return;
		}

		// If filter is in white list mode
		else
		{
			bool WhiteListed = false;
			for (auto& Tag: InPropogationSettings.TagFilter)
				if (RigData[InElement].RigElement->ComponentHasTag(Tag))
				{
					WhiteListed = true;
					break;
				}

			if (!WhiteListed)
				return;
		}
	}

	// If the element wasn't filtered, then process it

	OutPropogation.Add(InElement);

	if (InPropogationSettings.PropogateToChildren)
		for (auto& Child: RigData[InElement].ChildElements)
			Propogation_ProcessElement(OutPropogation, Child, InPropogationSettings, InCurrentDepth + 1);

	if (InPropogationSettings.PropogateToParent && RigData[InElement].ParentComponent != "Core")
		Propogation_ProcessElement(OutPropogation, RigData[InElement].ParentComponent, InPropogationSettings, InCurrentDepth + 1);
}


/*
* Pick an element in the rig, then pick it's adjacent elements, then their adjacent elements and so on, until you rich the progopation depth
* Upon finishing, OutPropogation array will contain all processed elements (including the starting one) in order of processing
*/
void UMPAS_Handler::PropogateFromElement(TArray<FName>& OutPropogation, FName InStartingElement, FMPAS_PropogationSettings InPropogationSettings)
{
	OutPropogation.Empty();
	
	if (RigData.Contains(InStartingElement))
		Propogation_ProcessElement(OutPropogation, InStartingElement, InPropogationSettings, 0);
}