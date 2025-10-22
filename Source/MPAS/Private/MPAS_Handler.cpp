// Fill out your copyright notice in the Description page of Project Settings.


#include "MPAS_Handler.h"
#include "Default/MPAS_Core.h"
#include "MPAS_RigElement.h"
#include "Default/RigElements/MPAS_VoidRigElement.h"
#include "Kismet/GameplayStatics.h"
#include "PhysicsEngine/PhysicsConstraintComponent.h"
#include "PhysicsEngine/ConstraintInstanceBlueprintLibrary.h"
#include "Default/RigElements/PositionDrivers/MPAS_PositionDriver.h"


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

	// Locates or creates a new timer controller
	InitTimerController();

	// Scans rig on begin play
	ScanRig();

	// Initialize rig after scanning
	InitRig();

	// Generates rig's physics model
	GeneratePhysicsModel();

	// Lings rig after initializing
	LinkRig();

	// Finalizes rig elements' setup
	PostLinkSetupRig();
	
	SetupComplete = true;

	// Finish Intention driver setup
	OnRigSetupComplete();
}


// Called every frame
void UMPAS_Handler::TickComponent(float DeltaTime, ELevelTick TickType, FActorComponentTickFunction* ThisTickFunction)
{
	Super::TickComponent(DeltaTime, TickType, ThisTickFunction);

	// Autonomously fetching bone transforms, if needed
	if (UseAutoBoneTransformFetching)
		AutoFetchBoneTransforms();

	// Synchronizing rig elements with the fetched bone transforms
	SyncBoneTransforms();

	// Updates rig every tick
	UpdateRig(DeltaTime);

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

	// Whether the element is void
	bool IsVoid = Cast<UMPAS_VoidRigElement>(RigElement) != nullptr;

	// Registering position drivers
	UMPAS_PositionDriver* PositionDriver = Cast<UMPAS_PositionDriver>(RigElement);
	if (PositionDriver)
	{
		// Generating unique name for the driver (if the specified name is already occupied)
		FName UniqueName = PositionDriver->DriverName;
		int32 Index = 0;

		while (PositionDrivers.Contains(UniqueName))
			UniqueName = FName(PositionDriver->DriverName.ToString() + "_" + FString::FromInt(Index++));

		PositionDrivers.Add(UniqueName, PositionDriver);
	}
	
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
			ScanElement(ChildElement, (!IsVoid) ? Name : ParentElementName);
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

// Finalizes rig elements' setup
void UMPAS_Handler::PostLinkSetupRig()
{
	for (auto& RigElement : RigData)
		RigElement.Value.RigElement->PostLinkSetupRigElement(this);
}

// Calls OnRigSetupFinished on all Intention Drivers
void UMPAS_Handler::OnRigSetupComplete()
{
	for (auto& IntentionDriver : IntentionStateMachines)
		IntentionDriver.Value->OnRigSetupFinished();
}


// Updates all elements in RigData
void UMPAS_Handler::UpdateRig(float DeltaTime)
{
	for (auto& RigElement : RigData)
		RigElement.Value.RigElement->UpdateRigElement(DeltaTime);
}



// Locates or creates a new timer controller
void UMPAS_Handler::InitTimerController()
{
	// Trying to locate existing timer controller
	UActorComponent* TimerControllerComponent = GetOwner()->GetComponentByClass(USTT_TimerController::StaticClass());
	if (TimerControllerComponent)
	{
		TimerController = Cast<USTT_TimerController>(TimerControllerComponent);
		return;
	}

	// If failed to find, create a new one
	TimerControllerComponent = GetOwner()->AddComponentByClass(USTT_TimerController::StaticClass(), false, FTransform(), false);
	GetOwner()->AddInstanceComponent(TimerControllerComponent);

	TimerController = Cast<USTT_TimerController>(TimerControllerComponent);
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



// BONE TRANFORM FETCHING AND SYNCHRONIZATION

// Automatically fetches bone transforms from the AutoBoneTransformFetchMesh and stores them into FetchedBoneTransforms
void UMPAS_Handler::AutoFetchBoneTransforms()
{
	if (!AutoBoneTransformFetchMesh || AutoBoneTransformFetchSelection.Num() == 0) return;
	
	for (auto& BoneName : AutoBoneTransformFetchSelection)
	{
		int32 BoneIndex = AutoBoneTransformFetchMesh->GetBoneIndex(BoneName);

		if (BoneIndex != INDEX_NONE)
			FetchedBoneTransforms.Add(BoneName, AutoBoneTransformFetchMesh->GetBoneTransform(BoneIndex));
	}
}

// Synchronizes rig elements to the most recently fetched bone transforms
void UMPAS_Handler::SyncBoneTransforms()
{
	for (auto& RigElementData : RigData)
		RigElementData.Value.RigElement->SyncToFetchedBoneTransforms();
}


// Manually fetches bone transforms from the specified mesh
// NOTE: Autonomous Fetch OVERRIDES cached bone transforms, so it must be disabled in order to use Manual Fetching.
void UMPAS_Handler::ManualFetchBoneTransforms(USkeletalMeshComponent* InFetchMesh, const TSet<FName>& Selection)
{
	if (!InFetchMesh || Selection.Num() == 0) return;
	
	for (auto& BoneName : Selection)
	{
		int32 BoneIndex = InFetchMesh->GetBoneIndex(BoneName);

		if (BoneIndex != INDEX_NONE)
			FetchedBoneTransforms.Add(BoneName, InFetchMesh->GetBoneTransform(BoneIndex));
	}
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

	// Calling OnRigSetupFinished on new intention drivers if the setup has been completed before they were added
	if (SetupComplete) NewStateMachine->OnRigSetupFinished();

	return true;
}

// Returns a pointer to the requested Intention Driver, returns nullptr if failed to find
UMPAS_IntentionStateMachine* UMPAS_Handler::GetIntentionDriver(FName InStateMachineName)
{
	auto IntentionDriver = IntentionStateMachines.Find(InStateMachineName);

	if (!IntentionDriver) return nullptr;

	return *IntentionDriver;
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

	TimerController->CreateTimer("RestabilizationTimer", RestabilizationCycleTime, true, false);
	TimerController->SubscribeToTimer("RestabilizationTimer", this, "OnRestabilizationCycleTicked");
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
					//PhysicsConstraintComponent->SetAngularDriveParams(PhysicsElementConfig.ConstraintAngularPositionStrength, PhysicsElementConfig.ConstraintAngularVelocityStrength, PhysicsElementConfig.ConstraintAngularMaxForce);
					//PhysicsConstraintComponent->SetLinearDriveParams(PhysicsElementConfig.ConstraintLinearPositionStrength, PhysicsElementConfig.ConstraintLinearVelocityStrength, PhysicsElementConfig.ConstraintLinearMaxForce);

						/*
						// This was an experiment to enable parent dominates
						// It failed because physics elemetns are not in any heirarchichal relationship engine-wise (they are not attached to each other)

						FConstraintInstanceAccessor ConstraintAccessor = PhysicsConstraintComponent->GetConstraint();
						UConstraintInstanceBlueprintLibrary::SetParentDominates(ConstraintAccessor, true);
						ConstraintAccessor.Modify();
						*/

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
		TimerController->ResetTimer("RestabilizationTimer");
		TimerController->StartTimer("RestabilizationTimer");
	}

	else
		TimerController->PauseTimer("RestabilizationTimer");
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