// Fill out your copyright notice in the Description page of Project Settings.


#include "MPAS_RigElement.h"
#include "MPAS_Handler.h"
#include "Kismet/KismetMathLibrary.h"
#include "Kismet/KismetStringLibrary.h"

// Sets default values for this component's properties
UMPAS_RigElement::UMPAS_RigElement()
{
	// Set this component to be initialized when the game starts, and to be ticked every frame.  You can turn these features
	// off to improve performance if you don't need them.
	PrimaryComponentTick.bCanEverTick = false;
	
	SetComponentTickEnabled(false);

	// ...
}


// Called when the game starts
void UMPAS_RigElement::BeginPlay()
{
	Super::BeginPlay();
}


// Called every frame
void UMPAS_RigElement::TickComponent(float DeltaTime, ELevelTick TickType, FActorComponentTickFunction* ThisTickFunction)
{
	Super::TickComponent(DeltaTime, TickType, ThisTickFunction);

	// ...
}


void UMPAS_RigElement::InitRigElement(class UMPAS_Handler* InHandler)
{
	Handler = InHandler;

	// Registering default location stack and default layers
	RegisterVectorStack("DefaultLocation");
	FMPAS_VectorStack& DefaultLocationStack = GetVectorStack(0);

	// This layer contains world space location of the parent element
	DefaultLocationStack.AddLayer(FMPAS_VectorLayer(EMPAS_LayerBlendingMode::Normal, 1.f, EMPAS_LayerCombinationMode::Add, 0, true, "ParentLocation"));
	// This layer contains locaiton of the element relative to it's parent
	DefaultLocationStack.AddLayer(FMPAS_VectorLayer(EMPAS_LayerBlendingMode::Add, 1.f, EMPAS_LayerCombinationMode::Add, 0, true, "SelfLocation"));

	// Registering default rotation stack and default layers 
	RegisterRotatorStack("DefaultRotation");
	FMPAS_RotatorStack& DefaultRotationStack = GetRotatorStack(0);
	
	// This layer contains world space rotation of the parent element
	DefaultRotationStack.AddLayer(FMPAS_RotatorLayer(EMPAS_LayerBlendingMode::Normal, 1.f, EMPAS_LayerCombinationMode::Add, 0, true, "ParentRotation"));
	// This layer contains rotation of the element relative to it's parent
	DefaultRotationStack.AddLayer(FMPAS_RotatorLayer(EMPAS_LayerBlendingMode::Add, 1.f, EMPAS_LayerCombinationMode::Add, 0, true, "SelfRotation"));


	// Registering parameters

	// Modifies (Multiplies) the speed of the locational interpolation of each element in the rig
	if (!InHandler->IsFloatParameterValid("ORIENTATION_LocationInterpolationMultiplier"))
		GetHandler()->CreateFloatParameter("ORIENTATION_LocationInterpolationMultiplier", 1.0f);

	// Modifies (Multiplies) the speed of the rotational interpolation of each element in the rig
	if (!InHandler->IsFloatParameterValid("ORIENTATION_RotationInterpolationMultiplier"))
		GetHandler()->CreateFloatParameter("ORIENTATION_RotationInterpolationMultiplier", 1.0f);
	
	GetHandler()->SubscribeToParameter("ORIENTATION_LocationInterpolationMultiplier", this, "OnInterpolationMultiplierChanged");
	GetHandler()->SubscribeToParameter("ORIENTATION_RotationInterpolationMultiplier", this, "OnInterpolationMultiplierChanged");

	// Storing default value of previous frame location to prevent velocity spike on the first measurement
	PreviousFrameLocation = GetComponentLocation();

	// Default activation/deactivation
	if (DefaultEnabled)
		Enabled = true;

	else
		Enabled = false;

	OnInitRigElement(InHandler);
}

void UMPAS_RigElement::LinkRigElement(class UMPAS_Handler* InHandler)
{
	auto& RigData = Handler->GetRigData();

	FName ParentElementName = RigData[RigElementName].ParentComponent;

	// If parent is the core
	if (ParentElementName == "Core")
	{
		IsCoreElement = true;
		GetVectorStack(0)[1].LayerData.SetSourceValue(this, GetComponentLocation());
		GetRotatorStack(0)[1].LayerData.SetSourceValue(this, GetComponentRotation());
	}
	
	// If parent is a normal element
	else
	{
		IsCoreElement = false;

		// Parent location and rotation initial cache
		ParentElement = RigData[ParentElementName].RigElement;
		GetVectorStack(0)[0].LayerData.SetSourceValue(ParentElement, ParentElement->GetComponentLocation());
		GetRotatorStack(0)[0].LayerData.SetSourceValue(ParentElement, ParentElement->GetComponentRotation());

		// Self location and rotation fetching
		
		// Setting initial location / rotation
		InitialSelfTransform.SetLocation( UKismetMathLibrary::Quat_UnrotateVector(ParentElement->GetComponentRotation().Quaternion(), GetComponentLocation() - ParentElement->GetComponentLocation()) );
		InitialSelfTransform.SetRotation( UKismetMathLibrary::NormalizedDeltaRotator(GetComponentRotation(), ParentElement->GetComponentRotation()).Quaternion() );

		GetVectorStack(0)[1].LayerData.SetSourceValue(this, UKismetMathLibrary::Quat_RotateVector(ParentElement->GetComponentRotation().Quaternion(), InitialSelfTransform.GetLocation()));
		GetRotatorStack(0)[1].LayerData.SetSourceValue(this, FRotator(InitialSelfTransform.GetRotation()));
	}

	OnLinkRigElement(InHandler);
}

// CALLED BY THE HANDLER : Called after the linking phase has completed (no more side changes will be applied to the element)
void UMPAS_RigElement::PostLinkSetupRigElement(UMPAS_Handler* InHandler)
{
}


// NOTIFICATION Called when ORIENTATION_LocationInterpolationMultiplier or ORIENTATION_RotationInterpolationMultiplier parameter value is changed
void UMPAS_RigElement::OnInterpolationMultiplierChanged(FName InParameterName)
{
	if (InParameterName == "ORIENTATION_LocationInterpolationMultiplier")
		LocationInterpolationMultiplier = GetHandler()->GetFloatParameter(InParameterName);

	else if (InParameterName == "ORIENTATION_RotationInterpolationMultiplier")
		RotationInterpolationMultiplier = GetHandler()->GetFloatParameter(InParameterName);
		
}


void UMPAS_RigElement::UpdateRigElement(float DeltaTime)
{
	// Non-Core elements have their parent location sampled from the parent element every frame
	if (!IsCoreElement)
	{
		// Sampling parent transform
		GetVectorStack(0)[0].LayerData.SetSourceValue(ParentElement, ParentElement->GetComponentLocation());
		GetRotatorStack(0)[0].LayerData.SetSourceValue(ParentElement, ParentElement->GetComponentRotation());

		// Updating self location based on new parent rotation
		GetVectorStack(0)[1].LayerData.SetSourceValue(this, UKismetMathLibrary::Quat_RotateVector(ParentElement->GetComponentRotation().Quaternion(), InitialSelfTransform.GetLocation()));
	}

	// Applying default position stacks
	ApplyDefaultLocationStack(DeltaTime);
	ApplyDefaultRotationStack(DeltaTime);

	// Calculating velocity
	CachedVelocity = (GetComponentLocation() - PreviousFrameLocation) / DeltaTime;
	PreviousFrameLocation = GetComponentLocation();

	OnUpdateRigElement(DeltaTime);
}

// CALLED BY THE HANDLER : Synchronizes Rig Element to the most recently fetched bone transforms
void UMPAS_RigElement::SyncToFetchedBoneTransforms(float DeltaTime)
{
	OnSyncToFetchedBoneTransforms(DeltaTime);
}


// VECTOR LAYERS

// Applies the default vector stack to the element's world location
void UMPAS_RigElement::ApplyDefaultLocationStack(float DeltaTime)
{
	CachedDefaultLocationStackValue = VectorStacks[0].StackData.CalculateStackValue();

	FVector NewLocation = CachedDefaultLocationStackValue;
	if (LocationInterpolationSpeed != 0)
		NewLocation = UKismetMathLibrary::VInterpTo(GetComponentLocation(), CachedDefaultLocationStackValue, DeltaTime, LocationInterpolationSpeed * LocationInterpolationMultiplier);

	SetWorldLocation(NewLocation);
}


// Registers a new vector stack and returns it's ID, returns an existing ID if the stack is already registered
int32 UMPAS_RigElement::RegisterVectorStack(const FName& InStackName)
{
	for (int32 i = 0; i < VectorStacks.Num(); i++)
		if (VectorStacks[i].Name() == InStackName)
			return i;

	FMPAS_VectorStack NewStack(InStackName);

	int32 StackID = VectorStacks.Add(NewStack);

	TMap<FString, int32> NewStackLayerNames;

	return StackID;
}

int32 UMPAS_RigElement::RegisterVectorLayer(int32 InStackID, const FMPAS_VectorLayer& InLayer)
{
	check(VectorStacks.IsValidIndex(InStackID));
	return VectorStacks[InStackID].AddLayer(InLayer);
}

FMPAS_VectorStack& UMPAS_RigElement::GetVectorStack(int32 InStackID)
{
	check(InStackID >= 0 && InStackID < VectorStacks.Num());
	return VectorStacks[InStackID];
}

// Returns the ID of the given stack, -1 if stack not found
int32 UMPAS_RigElement::GetVectorStackID(const FName& InStackName)
{
	for (int32 i = 0; i < VectorStacks.Num(); i++)
		if (VectorStacks[i].Name() == InStackName)
			return i;

	return -1;
}


// ROTATION LAYERS

// Applies the default rotation stack to the element's world location
void UMPAS_RigElement::ApplyDefaultRotationStack(float DeltaTime)
{
	FRotator StackValue = RotatorStacks[0].StackData.CalculateStackValue();

	FRotator NewRotation = StackValue;
	if (RotationInterpolationSpeed != 0)
		NewRotation = UKismetMathLibrary::RInterpTo(GetComponentRotation(), StackValue, DeltaTime, RotationInterpolationSpeed * RotationInterpolationMultiplier);

	SetWorldRotation(NewRotation);
}

// Registers a new rotation stack and returns it's ID, returns an existing ID if the stack is already registered
int32 UMPAS_RigElement::RegisterRotatorStack(const FName& InStackName)
{
	for (int32 i = 0; i < RotatorStacks.Num(); i++)
		if (RotatorStacks[i].Name() == InStackName)
			return i;

	FMPAS_RotatorStack NewStack(InStackName);

	int32 StackID = RotatorStacks.Add(NewStack);

	TMap<FString, int32> NewStackLayerNames;

	return StackID;
}

int32 UMPAS_RigElement::RegisterRotatorLayer(int32 InStackID, const FMPAS_RotatorLayer& InLayer)
{
	check(RotatorStacks.IsValidIndex(InStackID));
	return RotatorStacks[InStackID].AddLayer(InLayer);
}

FMPAS_RotatorStack& UMPAS_RigElement::GetRotatorStack(int32 InStackID)
{
	check(InStackID >= 0 && InStackID < RotatorStacks.Num());
	return RotatorStacks[InStackID];
}


// Returns the ID of the given stack, -1 if stack not found
int32 UMPAS_RigElement::GetRotatorStackID(const FName& InStackName)
{
	for (int32 i = 0; i < RotatorStacks.Num(); i++)
		if (RotatorStacks[i].Name() == InStackName)
			return i;

	return -1;
}
