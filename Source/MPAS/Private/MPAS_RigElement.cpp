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
	RegisterVectorLayer(0, "ParentLocation", EMPAS_LayerBlendingMode::Normal, EMPAS_LayerCombinationMode::Add, 1.f, 0, true); // This layer contains world space location of the parent element
	RegisterVectorLayer(0, "SelfLocation", EMPAS_LayerBlendingMode::Add, EMPAS_LayerCombinationMode::Add, 1.f, 0, true); // This layer contains locaiton of the element relative to it's parent

	// Registering default rotation stack and default layers 
	RegisterRotationStack("DefaultRotation");
	RegisterRotationLayer(0, "ParentRotation", EMPAS_LayerBlendingMode::Normal, 1.f, 0, true); // This layer contains world space rotation of the parent element
	RegisterRotationLayer(0, "SelfRotation", EMPAS_LayerBlendingMode::Add, 1.f, 0, true); // This layer contains rotation of the element relative to it's parent

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
	if (DefaultActive)
		SetRigElementActive(true);

	else
		SetRigElementActive(false);

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
		SetVectorSourceValue(0, 1, this, GetComponentLocation());
		SetRotationSourceValue(0, 1, this, GetComponentRotation());
	}
	
	// If parent is a normal element
	else
	{
		IsCoreElement = false;

		// Parent location and rotation initial cache
		ParentElement = RigData[ParentElementName].RigElement;
		SetVectorSourceValue(0, 0, ParentElement, ParentElement->GetComponentLocation());
		SetRotationSourceValue(0, 0, ParentElement, ParentElement->GetComponentRotation());

		// Self location and rotation fetching
		
		// Setting initial location / rotation
		InitialSelfTransform.SetLocation( UKismetMathLibrary::Quat_UnrotateVector(ParentElement->GetComponentRotation().Quaternion(), GetComponentLocation() - ParentElement->GetComponentLocation()) );
		InitialSelfTransform.SetRotation( UKismetMathLibrary::NormalizedDeltaRotator(GetComponentRotation(), ParentElement->GetComponentRotation()).Quaternion() );

		SetVectorSourceValue(0, 1, this, UKismetMathLibrary::Quat_RotateVector(ParentElement->GetComponentRotation().Quaternion(), InitialSelfTransform.GetLocation()) );
		SetRotationSourceValue(0, 1, this, FRotator(InitialSelfTransform.GetRotation()));
	}

	OnLinkRigElement(InHandler);
}

// CALLED BY THE HANDLER : Called after the linking phase has completed (no more side changes will be applied to the element)
void UMPAS_RigElement::PostLinkSetupRigElement(UMPAS_Handler* InHandler)
{
}


// CALLED BY THE HANDLER : Links element to it's physics model equivalent
void UMPAS_RigElement::InitPhysicsModel(const TArray<UMPAS_PhysicsModelElement*>& InPhysicsElements)
{
	PhysicsElements = InPhysicsElements;

	for (int32 i = 0; i < PhysicsElements.Num(); i++)
	{
		PhysicsElementsConfiguration[i].PositionStackID = RegisterVectorStack("PhysicsModelStack_" + UKismetStringLibrary::Conv_IntToString(i));
		PhysicsElementsConfiguration[i].RotationStackID = RegisterRotationStack("PhysicsModelStack_" + UKismetStringLibrary::Conv_IntToString(i));

		RegisterVectorLayer(PhysicsElementsConfiguration[i].PositionStackID, "PhysicsElementLocation", EMPAS_LayerBlendingMode::Normal, EMPAS_LayerCombinationMode::Add, 1.f, 0, true);
		RegisterRotationLayer(PhysicsElementsConfiguration[i].RotationStackID, "PhysicsElementRotation", EMPAS_LayerBlendingMode::Normal, 1.f, 0, true);
	}

	OnInitPhysicsModel();
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
		SetVectorSourceValue(0, 0, ParentElement, ParentElement->GetComponentLocation());
		SetRotationSourceValue(0, 0, ParentElement, ParentElement->GetComponentRotation());

		// Updating self location based on new parent rotation
		SetVectorSourceValue(0, 1, this, UKismetMathLibrary::Quat_RotateVector(ParentElement->GetComponentRotation().Quaternion(), InitialSelfTransform.GetLocation()) );
	}

	// Applying physics model if its enaled
	if (PhysicsModelEnabled && PhysicsElementsConfiguration.Num() > 0)
		ApplyPhysicsModelLocationAndRotation(DeltaTime);

	// If physics model is not enabled, then we apply normal positioning method
	else if (PositioningMode == EMPAS_ElementPositionMode::Normal)
	{
		ApplyDefaultLocationStack(DeltaTime);
		ApplyDefaultRotationStack(DeltaTime);
	}

	if (ReactivatePhysicsModelConstraintNextFrame)
	{
		GetHandler()->ReactivatePhysicsModelConstraint(RigElementName);
		ReactivatePhysicsModelConstraintNextFrame = false;
	}

	// Calculating velocity
	CachedVelocity = (GetComponentLocation() - PreviousFrameLocation) / DeltaTime;
	PreviousFrameLocation = GetComponentLocation();

	OnUpdateRigElement(DeltaTime);
}


// VECTOR LAYERS

// Applies the default vector stack to the element's world location
void UMPAS_RigElement::ApplyDefaultLocationStack(float DeltaTime)
{
	CachedDefaultLocationStackValue = CalculateVectorStackValue(0);

	FVector NewLocation = CachedDefaultLocationStackValue;
	if (LocationInterpolationSpeed != 0)
		NewLocation = UKismetMathLibrary::VInterpTo(GetComponentLocation(), CachedDefaultLocationStackValue, DeltaTime, LocationInterpolationSpeed * LocationInterpolationMultiplier);

	SetWorldLocation(NewLocation);
}

// Calculates the final vector of the given vector stack
FVector UMPAS_RigElement::CalculateVectorStackValue(int32 InVectorStackID)
{
	FVector FinalVector;

	for (int32 LayerID : VectorStacks[InVectorStackID].StackOrder)
	{
		FMPAS_VectorLayer& Layer = VectorStacks[InVectorStackID].Layers[LayerID];

		if (!Layer.Enabled) continue;

		bool HasActiveElements = false;
		FVector LayerValue = CalculateVectorLayerValue(Layer, HasActiveElements);

		if (HasActiveElements)
		{
			switch (Layer.BlendingMode)
			{
			case EMPAS_LayerBlendingMode::Normal:
				FinalVector = UKismetMathLibrary::VLerp(FinalVector, LayerValue, Layer.BlendingFactor);
				break;
			
			case EMPAS_LayerBlendingMode::Add:
				FinalVector += LayerValue * Layer.BlendingFactor;
				break;

			case EMPAS_LayerBlendingMode::Multiply:
				FinalVector = UKismetMathLibrary::VLerp(FinalVector, FinalVector * LayerValue, Layer.BlendingFactor);
				break;

			default: break;
			}
		}
	}

	return FinalVector;
}

// Calculates the final vector of the given vector layer
FVector UMPAS_RigElement::CalculateVectorLayerValue(const FMPAS_VectorLayer InLayer, bool& OutHasActiveElements)
{
	FVector OutVector = FVector(0, 0, 0);
	int32 ActiveElementCount = 0;


	FVector VectorSum = FVector(0, 0, 0);
	FVector VectorM = FVector(1, 1, 1);

	switch (InLayer.CombinationMode)
	{

	case EMPAS_LayerCombinationMode::Average:

		for (auto& Source : InLayer.LayerElements)
		{
			bool CurrentActive = InLayer.ForceAllElementsActive || Source.Key->GetRigElementActive();

			VectorSum += Source.Value * CurrentActive;
			ActiveElementCount += CurrentActive;
		}

		if (ActiveElementCount > 0)
			OutVector = VectorSum / ActiveElementCount;
			
		break;

	case EMPAS_LayerCombinationMode::Add:

		for (auto& Source : InLayer.LayerElements)
		{
			bool CurrentActive = InLayer.ForceAllElementsActive || Source.Key->GetRigElementActive();

			VectorSum += Source.Value * CurrentActive;
			ActiveElementCount += CurrentActive;
		}

		OutVector = VectorSum;
		break;

	case EMPAS_LayerCombinationMode::Multiply:

		for (auto& Source : InLayer.LayerElements)
		{
			bool CurrentActive = InLayer.ForceAllElementsActive || Source.Key->GetRigElementActive();

			ActiveElementCount += CurrentActive;
			if (CurrentActive)
				VectorM *= Source.Value;
		}

		OutVector = VectorM;
		break;
	
	default:
		OutVector = FVector();
		break;
	}

	OutHasActiveElements = ActiveElementCount > 0;
	return OutVector;
}


// Registers a new vector stack and returns it's ID, returns an existing ID if the stack is already registered
int32 UMPAS_RigElement::RegisterVectorStack(const FString& InStackName)
{
	if (VectorStackNames.Contains(InStackName))
		return VectorStackNames[InStackName];

	FMPAS_VectorStack NewStack;

	int32 StackID = VectorStacks.Add(NewStack);
	VectorStackNames.Add(InStackName, StackID);

	TMap<FString, int32> NewStackLayerNames;
	VectorLayerNames.Add(NewStackLayerNames);

	return StackID;
}

// Registers a new vector layer in the given stack and returns it's ID, returns an existing ID if the layer is already registered, returns -1 if Stack does not exist
int32 UMPAS_RigElement::RegisterVectorLayer(int32 InVectorStackID, const FString& InLayerName, EMPAS_LayerBlendingMode InBlendingMode, EMPAS_LayerCombinationMode InCombinationMode, float InBlendingFactor, int32 InPriority, bool InForceAllElementsActive)
{
	if (InVectorStackID < 0 || InVectorStackID >= VectorStacks.Num())
		return -1;

	if (VectorLayerNames[InVectorStackID].Contains(InLayerName))
		return VectorLayerNames[InVectorStackID][InLayerName];

	FMPAS_VectorLayer NewLayer(InBlendingMode, InBlendingFactor, InCombinationMode, InPriority, InForceAllElementsActive);

	int32 LayerID = VectorStacks[InVectorStackID].AddVectorLayer(NewLayer);

	VectorLayerNames[InVectorStackID].Add(InLayerName, LayerID);

	return LayerID;
}


// Returns the ID of the given stack, -1 if stack not found
int32 UMPAS_RigElement::GetVectorStackID(const FString& InStackName)
{
	if (!VectorStackNames.Contains(InStackName))
		return -1;

	return VectorStackNames[InStackName];
}

// Returns the ID of the given layer in the given stack, -1 if stack or layer not found
int32 UMPAS_RigElement::GetVectorLayerID(int32 InVectorStackID, const FString& InLayerName)
{
	if (InVectorStackID < 0 || InVectorStackID >= VectorStacks.Num())
		return -1;

	if (!VectorLayerNames[InVectorStackID].Contains(InLayerName))
		return -1;

	return VectorLayerNames[InVectorStackID][InLayerName];
}



// Sets the value of a vector source in the given Stack and Layer, if succeded: returns true, false - overwise
bool UMPAS_RigElement::SetVectorSourceValue(int32 InVectorStackID, int32 InVectorLayerID, UMPAS_RigElement* InSourceElement, FVector InSourceValue)
{
	if (InVectorStackID < 0 || InVectorStackID >= VectorStacks.Num())
		return false;

	if (InVectorLayerID < 0 || InVectorLayerID >= VectorStacks[InVectorStackID].Num())
		return false;

	VectorStacks[InVectorStackID][InVectorLayerID].LayerElements.Add(InSourceElement, InSourceValue);
	return true;
}


// Removes the value of a vector source in the given Stack and Layer, if succeded: returns true, false - overwise
bool UMPAS_RigElement::RemoveVectorSourceValue(int32 InVectorStackID, int32 InVectorLayerID, UMPAS_RigElement* InSourceElement)
{
	if (InVectorStackID < 0 || InVectorStackID >= VectorStacks.Num())
		return false;

	if (InVectorLayerID < 0 || InVectorLayerID >= VectorStacks[InVectorStackID].Num())
		return false;

	return VectorStacks[InVectorStackID][InVectorLayerID].LayerElements.Remove(InSourceElement) > 0;
}


// Enables/Disables the specified vector layer, if succeded: returns true, false - overwise
bool UMPAS_RigElement::SetVectorLayerEnabled(int32 InVectorStackID, int32 InVectorLayerID, bool InNewEnabled)
{
	if (InVectorStackID < 0 || InVectorStackID >= VectorStacks.Num())
		return false;

	if (InVectorLayerID < 0 || InVectorLayerID >= VectorStacks[InVectorStackID].Num())
		return false;

	VectorStacks[InVectorStackID][InVectorLayerID].Enabled = InNewEnabled;
	return true;
}

// Whenther the specified layer is enabled or not ("false" if the layer doesnt exist)
bool UMPAS_RigElement::GetVectorLayerEnabled(int32 InVectorStackID, int32 InVectorLayerID)
{
	if (InVectorStackID < 0 || InVectorStackID >= VectorStacks.Num())
		return false;

	if (InVectorLayerID < 0 || InVectorLayerID >= VectorStacks[InVectorStackID].Num())
		return false;

	return VectorStacks[InVectorStackID][InVectorLayerID].Enabled;
}


// Modifies blending factor of the specified vector layer, if succeded: returns true, false - overwise
bool UMPAS_RigElement::SetVectorLayerBlendingFactor(int32 InVectorStackID, int32 InVectorLayerID, float InNewBlendingFactor)
{
	if (InVectorStackID < 0 || InVectorStackID >= VectorStacks.Num())
		return false;

	if (InVectorLayerID < 0 || InVectorLayerID >= VectorStacks[InVectorStackID].Num())
		return false;

	VectorStacks[InVectorStackID][InVectorLayerID].BlendingFactor = InNewBlendingFactor;
	return true;
}

// Returns the blending factor of the specified layer is enabled or not ("0" if the layer doesnt exist)
float UMPAS_RigElement::GetVectorLayerBlendingFactor(int32 InVectorStackID, int32 InVectorLayerID)
{
	if (InVectorStackID < 0 || InVectorStackID >= VectorStacks.Num())
		return 0.0;

	if (InVectorLayerID < 0 || InVectorLayerID >= VectorStacks[InVectorStackID].Num())
		return 0.0;

	return VectorStacks[InVectorStackID][InVectorLayerID].BlendingFactor;
}



// ROTATION LAYERS

// Applies the default rotation stack to the element's world location
void UMPAS_RigElement::ApplyDefaultRotationStack(float DeltaTime)
{
	FRotator StackValue = CalculateRotationStackValue(0);

	FRotator NewRotation = StackValue;
	if (RotationInterpolationSpeed != 0)
		NewRotation = UKismetMathLibrary::RInterpTo(GetComponentRotation(), StackValue, DeltaTime, RotationInterpolationSpeed * RotationInterpolationMultiplier);

	SetWorldRotation(NewRotation);
}

// Calculates the final rotation of the given rotation stack
FRotator UMPAS_RigElement::CalculateRotationStackValue(int32 InRotationStackID)
{
	FRotator FinalRotation = FRotator::ZeroRotator;

	for (int32 LayerID : RotationStacks[InRotationStackID].StackOrder)
	{
		FMPAS_RotatorLayer& Layer = RotationStacks[InRotationStackID].Layers[LayerID];

		if (!Layer.Enabled) continue;

		bool HasActiveElements;
		FRotator LayerValue = CalculateRotationLayerValue(Layer, HasActiveElements);

		if (HasActiveElements)
		{
			switch (Layer.BlendingMode)
			{

			case EMPAS_LayerBlendingMode::Normal:
				FinalRotation = UKismetMathLibrary::RLerp(FinalRotation, LayerValue, Layer.BlendingFactor, true);
				break;
			
			case EMPAS_LayerBlendingMode::Add:
				FinalRotation += UKismetMathLibrary::RLerp(FRotator::ZeroRotator, LayerValue, Layer.BlendingFactor, true);;
				break;

			case EMPAS_LayerBlendingMode::Multiply:

				FinalRotation.Pitch *= UKismetMathLibrary::Lerp(1.f, LayerValue.Pitch, Layer.BlendingFactor);
				FinalRotation.Yaw *= UKismetMathLibrary::Lerp(1.f, LayerValue.Yaw, Layer.BlendingFactor);
				FinalRotation.Roll *= UKismetMathLibrary::Lerp(1.f, LayerValue.Roll, Layer.BlendingFactor);
				break;

			default: break;
			}
		}
	}

	return FinalRotation;
}

// Calculates the final rotation of the given rotation layer
FRotator UMPAS_RigElement::CalculateRotationLayerValue(const FMPAS_RotatorLayer InLayer, bool& OutHasActiveElements)
{
	FRotator RotatorSum = FRotator(0, 0, 0);
	int32 ActiveLayerCount = 0;

	for (auto& Source : InLayer.LayerElements)
	{
		bool CurrentActive = InLayer.ForceAllElementsActive || Source.Key->GetRigElementActive();

		if (CurrentActive)
			RotatorSum += Source.Value;

		// ElementRotationSum.X += Source.Value.Pitch * CurrentActive;
		// ElementRotationSum.Y += Source.Value.Yaw * CurrentActive;
		// ElementRotationSum.Z += Source.Value.Roll * CurrentActive;

		ActiveLayerCount += CurrentActive;
	}

	//FRotator RotationAverage = FRotator(ElementRotationSum.X / ActiveLayerCount, ElementRotationSum.Y / ActiveLayerCount, ElementRotationSum.Z / ActiveLayerCount);
	//return RotationAverage;

	OutHasActiveElements = ActiveLayerCount > 0;
	return RotatorSum;
}


// Registers a new rotation stack and returns it's ID, returns an existing ID if the stack is already registered
int32 UMPAS_RigElement::RegisterRotationStack(const FString& InStackName)
{
	if (RotationStackNames.Contains(InStackName))
		return RotationStackNames[InStackName];

	FMPAS_RotatorStack NewStack;

	int32 StackID = RotationStacks.Add(NewStack);
	RotationStackNames.Add(InStackName, StackID);

	TMap<FString, int32> NewStackLayerNames;
	RotationLayerNames.Add(NewStackLayerNames);

	return StackID;
}

// Registers a new rotation layer in the given stack and returns it's ID, returns an existing ID if the layer is already registered, returns -1 if Stack does not exist
int32 UMPAS_RigElement::RegisterRotationLayer(int32 InRotationStackID, const FString& InLayerName, EMPAS_LayerBlendingMode InBlendingMode, float InBlendingFactor, int32 InPriority, bool InForceAllElementsActive)
{
	if (InRotationStackID < 0 || InRotationStackID >= RotationStacks.Num())
		return -1;

	if (RotationLayerNames[InRotationStackID].Contains(InLayerName))
		return RotationLayerNames[InRotationStackID][InLayerName];

	FMPAS_RotatorLayer NewLayer(InBlendingMode, InBlendingFactor, EMPAS_LayerCombinationMode::Add, InPriority, InForceAllElementsActive);

	int32 LayerID = RotationStacks[InRotationStackID].AddRotatorLayer(NewLayer);

	RotationLayerNames[InRotationStackID].Add(InLayerName, LayerID);
	return LayerID;
}


// Returns the ID of the given stack, -1 if stack not found
int32 UMPAS_RigElement::GetRotationStackID(const FString& InStackName)
{
	if (!RotationStackNames.Contains(InStackName))
		return -1;

	return RotationStackNames[InStackName];
}

// Returns the ID of the given layer in the given stack, -1 if stack or layer not found
int32 UMPAS_RigElement::GetRotationLayerID(int32 InRotationStackID, const FString& InLayerName)
{
	if (InRotationStackID < 0 || InRotationStackID >= RotationStacks.Num())
		return -1;

	if (!RotationLayerNames[InRotationStackID].Contains(InLayerName))
		return -1;

	return RotationLayerNames[InRotationStackID][InLayerName];
}


// Sets the value of a rotation source in the given Stack and Layer, if succeded: returns true, false - overwise
bool UMPAS_RigElement::SetRotationSourceValue(int32 InRotationStackID, int32 InRotationLayerID, UMPAS_RigElement* InSourceElement, FRotator InSourceValue)
{
	if (InRotationStackID < 0 || InRotationStackID >= RotationStacks.Num())
		return false;

	if (InRotationLayerID < 0 || InRotationLayerID >= RotationStacks[InRotationStackID].Num())
		return false;

	RotationStacks[InRotationStackID][InRotationLayerID].LayerElements.Add(InSourceElement, InSourceValue);
	return true;
}


// Removes the value of a rotation source in the given Stack and Layer, if succeded: returns true, false - overwise
bool UMPAS_RigElement::RemoveRotationSourceValue(int32 InRotationStackID, int32 InRotationLayerID, UMPAS_RigElement* InSourceElement)
{
	if (InRotationStackID < 0 || InRotationStackID >= RotationStacks.Num())
		return false;

	if (InRotationLayerID < 0 || InRotationLayerID >= RotationStacks[InRotationStackID].Num())
		return false;

	return RotationStacks[InRotationStackID][InRotationLayerID].LayerElements.Remove(InSourceElement) > 0;
}


// Enables/Disables the specified rotation layer, if succeded: returns true, false - overwise
bool UMPAS_RigElement::SetRotationLayerEnabled(int32 InRotationStackID, int32 InRotationLayerID, bool InNewEnabled)
{
	if (InRotationStackID < 0 || InRotationStackID >= RotationStacks.Num())
		return false;

	if (InRotationLayerID < 0 || InRotationLayerID >= RotationStacks[InRotationStackID].Num())
		return false;

	RotationStacks[InRotationStackID][InRotationLayerID].Enabled = InNewEnabled;

	return true;
}

// Whenther the specified layer is enabled or not ("false" if the layer doesnt exist)
bool UMPAS_RigElement::GetRotationLayerEnabled(int32 InRotationStackID, int32 InRotationLayerID)
{
	if (InRotationStackID < 0 || InRotationStackID >= RotationStacks.Num())
		return false;

	if (InRotationLayerID < 0 || InRotationLayerID >= RotationStacks[InRotationStackID].Num())
		return false;

	return RotationStacks[InRotationStackID][InRotationLayerID].Enabled;
}


// Modifies blending factor of the specified rotation layer, if succeded: returns true, false - overwise
bool UMPAS_RigElement::SetRotationLayerBlendingFactor(int32 InRotationStackID, int32 InRotationLayerID, float InNewBlendingFactor)
{
	if (InRotationStackID < 0 || InRotationStackID >= RotationStacks.Num())
		return false;

	if (InRotationLayerID < 0 || InRotationLayerID >= RotationStacks[InRotationStackID].Num())
		return false;

	RotationStacks[InRotationStackID][InRotationLayerID].BlendingFactor = InNewBlendingFactor;

	return true;
}

// Returns the blending factor of the specified layer is enabled or not ("0" if the layer doesnt exist)
float UMPAS_RigElement::GetRotationLayerBlendingFactor(int32 InRotationStackID, int32 InRotationLayerID)
{
	if (InRotationStackID < 0 || InRotationStackID >= RotationStacks.Num())
		return 0.0f;

	if (InRotationLayerID < 0 || InRotationLayerID >= RotationStacks[InRotationStackID].Num())
		return 0.0f;

	return RotationStacks[InRotationStackID][InRotationLayerID].BlendingFactor;
}



// PHYSICS MODEL

// Enables/Disables physics model usage for this element
void UMPAS_RigElement::SetPhysicsModelEnabled(bool InEnabled)
{
	if (InEnabled != PhysicsModelEnabled)
	{
		PhysicsModelEnabled = InEnabled;

		// Notify eacg physics element
		// This will enable physics simulation and collision for all of tghe physics elements
		if(InEnabled)
		{
			for (auto& PhysicsElement: PhysicsElements)
				if (PhysicsElement)
					PhysicsElement->OnPhysicsModelEnabled();

			// Semi enable physics model on the parent if its not enabled already
			if (ParentElement)
			if (!ParentElement->GetPhysicsModelEnabled())
				for(auto& ParentPhysicsElement: ParentElement->GetPhysicsElements())
					ParentPhysicsElement->OnPhysicsModelSemiEnabled();

			// Reactivate constraint if it exists
			ReactivatePhysicsModelConstraintNextFrame = true;
			//GetHandler()->ReactivatePhysicsModelConstraint(RigElementName);

			OnPhysicsModelEnabled();
		}
		
		else
		{
			GetHandler()->DeactivatePhysicsModelConstraint(RigElementName);

			for (auto& PhysicsElement: PhysicsElements)
				if (PhysicsElement)
					PhysicsElement->OnPhysicsModelDisabled();

			OnPhysicsModelDisabled();
		}
	}
}

// Called when physics model is enabled, applies position and rotation of physics elements to the rig element
void UMPAS_RigElement::ApplyPhysicsModelLocationAndRotation_Implementation(float DeltaTime) 
{
	if(PhysicsElements.IsValidIndex(0))
		if (PhysicsElements[0])
		{
			//SetWorldLocation( UKismetMathLibrary::VInterpTo(GetComponentLocation(), CalculatePositionStackValue(PhysicsElementsConfiguration[0].PositionStackID), DeltaTime, PhysicsElementsConfiguration[0].PhysicsModelPositionInterpolationSpeed ));
			//SetWorldRotation( UKismetMathLibrary::RInterpTo(GetComponentRotation(), CalculateRotationStackValue(PhysicsElementsConfiguration[0].RotationStackID), DeltaTime, PhysicsElementsConfiguration[0].PhysicsModelRotationInterpolationSpeed ));
			SetWorldLocation(CalculateVectorStackValue(PhysicsElementsConfiguration[0].PositionStackID));
			SetWorldRotation(CalculateRotationStackValue(PhysicsElementsConfiguration[0].RotationStackID));
		}
}



// STABILITY

// Recalculates stability and calls stability events
void UMPAS_RigElement::UpdateStability()
{
	// Recalculating cached stability
	float Sum = 0.f;
	int32 Count = 0;

	for (auto& Effector: StabilityEffectors)
	{
		Sum += Effector.Value;
		Count++;
	}

	if (Count > 0)
		CachedStability = Sum / Count;
	
	else
		CachedStability = 1.f;

	EMPAS_StabilityStatus CurrentStabilityStatus = GetStabilityStatus();

	// Recalculating stability status
	if (GetIsStable())
		StabilityStatus = EMPAS_StabilityStatus::Stable;

	else if (CachedStability >= StabilityThreshold)
		StabilityStatus = EMPAS_StabilityStatus::SemiStable;
	
	else
		StabilityStatus = EMPAS_StabilityStatus::Unstable;


	// Calling Events
	if (GetStabilityStatus() != CurrentStabilityStatus)
		OnStabilityStatusChanged(GetStabilityStatus());
}

// Whether the element is stable or not
bool UMPAS_RigElement::GetIsStable()
{
	if (ParentElement)
		return (GetStability() >= StabilityThreshold) && ParentElement->GetIsStable();

	else
		return GetStability() >= StabilityThreshold;
}

// Sets a stability effector value, the average of all effector values determines the stability of the element
void UMPAS_RigElement::SetStabilityEffectorValue(UMPAS_RigElement* InEffector, float InStability)
{
	StabilityEffectors.Add(InEffector, InStability);

	UpdateStability();
}

// Removes an effector from stability effectors (if it is a valid effector)
void UMPAS_RigElement::RemoveStabilityEffector(UMPAS_RigElement* InEffector)
{
	StabilityEffectors.Remove(InEffector);

	UpdateStability();
}

// Called when the element becomes unstable (do not forget to call SUPER version)
void UMPAS_RigElement::OnStabilityStatusChanged_Implementation(EMPAS_StabilityStatus NewStabilityStatus)
{

	// Enable physics model if unstable
	if (NewStabilityStatus != EMPAS_StabilityStatus::Stable)
	{
		GetHandler()->SetPhysicsModelEnabled(true);
		SetPhysicsModelEnabled(true);
	}

	// Propogating stability updates to children

	FMPAS_PropogationSettings ChildPropogationSettings;
	ChildPropogationSettings.PropogateToParent = false;

	TArray<FName> ChildPropogation;
	GetHandler()->PropogateFromElement(ChildPropogation, RigElementName, ChildPropogationSettings);

	for (auto& Element: ChildPropogation)
		if (RigElementName != Element)
			GetHandler()->GetRigData()[Element].RigElement->UpdateStability();	
}


// STACKS

// Adds a new layer into the stack and updates the stack order
int32 FMPAS_VectorStack::AddVectorLayer(FMPAS_VectorLayer InLayer)
{
	// Storing the layer
	int32 ID = Layers.Add(InLayer);

	// Updating stack order
	int32 LastIndex = StackOrder.Add(ID);

	// Insertion sort for just the last element (works in O(N), N - number of elements in the stack)
	int32 i = LastIndex - 1;
	while (i >= 0 && Layers[StackOrder[i]].Priority > InLayer.Priority)
	{
		StackOrder[i + 1] = StackOrder[i];
		i--;
	}

	StackOrder[i + 1] = ID;

	return ID;
}

// Adds a new layer into the stack and updates the stack order
int32 FMPAS_RotatorStack::AddRotatorLayer(FMPAS_RotatorLayer InLayer)
{
	// Storing the layer
	int32 ID = Layers.Add(InLayer);

	// Updating stack order
	int32 LastIndex = StackOrder.Add(ID);

	// Insertion sort for just the last element (works in O(N), N - number of elements in the stack)
	int32 i = LastIndex - 1;
	while (i >= 0 && Layers[StackOrder[i]].Priority > InLayer.Priority)
	{
		StackOrder[i + 1] = StackOrder[i];
		i--;
	}

	StackOrder[i + 1] = ID;

	return ID;
}
