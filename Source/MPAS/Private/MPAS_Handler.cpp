// Fill out your copyright notice in the Description page of Project Settings.


#include "MPAS_Handler.h"
#include "Default/MPAS_Core.h"
#include "MPAS_RigElement.h"
#include "Default/RigElements/MPAS_VoidRigElement.h"
#include "Kismet/GameplayStatics.h"
#include "Kismet/KismetMathLibrary.h"
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
	SyncBoneTransforms(DeltaTime);

	// Updates rig every tick
	UpdateRig(DeltaTime);

	// Updates intention driver
	UpdateIntentionDriver(DeltaTime);
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
	if (SkipAutoBoneTransformFetchForTheFirstUpdate)
	{
		SkipAutoBoneTransformFetchForTheFirstUpdate = false;
		return;
	}

	if (!AutoBoneTransformFetchMesh || AutoBoneTransformFetchSelection.Num() == 0) return;
	
	for (auto& BoneName : AutoBoneTransformFetchSelection)
	{
		int32 BoneIndex = AutoBoneTransformFetchMesh->GetBoneIndex(BoneName);

		if (BoneIndex != INDEX_NONE)
			FetchedBoneTransforms.Add(BoneName, AutoBoneTransformFetchMesh->GetBoneTransform(BoneIndex));
	}
}

// Synchronizes rig elements to the most recently fetched bone transforms
void UMPAS_Handler::SyncBoneTransforms(float DeltaTime)
{
	// Calculating fetched bone transform deltas
	for (auto& BoneToNewTransform : FetchedBoneTransforms)
	{
		FTransform* BoneTransformData = BoneTransforms.Find(BoneToNewTransform.Key);
		if (BoneTransformData)
		{
			FTransform DeltaTransform;
			DeltaTransform.SetLocation(BoneToNewTransform.Value.GetLocation() - BoneTransformData->GetLocation());
			DeltaTransform.SetRotation(UKismetMathLibrary::NormalizedDeltaRotator(	BoneToNewTransform.Value.GetRotation().Rotator(), 
																					BoneTransformData->GetRotation().Rotator()).Quaternion());

			FetchedBoneTransformDeltas.Add({ BoneToNewTransform.Key, DeltaTransform });
		}
	}

	// Calling SyncToFetchedBoneTransforms on rig elements
	for (auto& RigElementData : RigData)
		if (RigElementData.Value.RigElement->AlwaysSyncBoneTransform || ForceSyncBoneTransforms)
			RigElementData.Value.RigElement->SyncToFetchedBoneTransforms(DeltaTime);

	ForceSyncBoneTransforms = false;
}

// Specifies the skeletal mesh, from which bone transforms shall be fetched during autonomous bone fetch process
void UMPAS_Handler::SetAutoBoneTransformFetchMesh(USkeletalMeshComponent* InMesh, bool AddAllBonesToFetchSelection)
{
	AutoBoneTransformFetchMesh = InMesh;

	if (AddAllBonesToFetchSelection)
	{
		TArray<FName> BoneNames;
		AutoBoneTransformFetchMesh->GetBoneNames(BoneNames);

		for (auto& Bone : BoneNames)
			AddAutoFetchBone(Bone);
	}
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