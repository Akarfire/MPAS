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

	// Registering default position stack and default layers
	RegisterPositionStack("Default");
	RegisterPositionLayer(0, "ParentPosition", EMPAS_LayerBlendingMode::Normal, EMPAS_LayerCombinationMode::Average, true); // This layer contains world space position of the parent element
	RegisterPositionLayer(0, "SelfPosition", EMPAS_LayerBlendingMode::Add, EMPAS_LayerCombinationMode::Average, true); // This layer contains position of the element relative to it's parent

	// Registering default rotation stack and default layers 
	RegisterRotationStack("Default");
	RegisterRotationLayer(0, "ParentRotation", EMPAS_LayerBlendingMode::Normal, true); // This layer contains world space rotation of the parent element
	RegisterRotationLayer(0, "SelfRotation", EMPAS_LayerBlendingMode::Add, true); // This layer contains rotation of the element relative to it's parent

	// Registering parameters

	// Modifies (Multiplies) the speed of the positional interpolation of each element in the rig
	if (!InHandler->IsFloatParameterValid("ORIENTATION_PositionInterpolationMultiplier"))
		GetHandler()->CreateFloatParameter("ORIENTATION_PositionInterpolationMultiplier", 1.0f);

	// Modifies (Multiplies) the speed of the rotational interpolation of each element in the rig
	if (!InHandler->IsFloatParameterValid("ORIENTATION_RotationInterpolationMultiplier"))
		GetHandler()->CreateFloatParameter("ORIENTATION_RotationInterpolationMultiplier", 1.0f);
	
	GetHandler()->SubscribeToParameter("ORIENTATION_PositionInterpolationMultiplier", this, "OnInterpolationMultiplierChanged");
	GetHandler()->SubscribeToParameter("ORIENTATION_RotationInterpolationMultiplier", this, "OnInterpolationMultiplierChanged");

	// Storing default value of previous frame position to prevent velocity spike on the first measurement
	PreviousFramePosition = GetComponentLocation();

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
		SetPositionSourceValue(0, 1, this, GetComponentLocation());
		SetRotationSourceValue(0, 1, this, GetComponentRotation());
	}
	
	// If parent is a normal element
	else
	{
		IsCoreElement = false;

		// Parent position and rotation initial cache
		ParentElement = RigData[ParentElementName].RigElement;
		SetPositionSourceValue(0, 0, ParentElement, ParentElement->GetComponentLocation());
		SetRotationSourceValue(0, 0, ParentElement, ParentElement->GetComponentRotation());

		// Self position and rotation fetching
		
		// Setting initial position / rotation
		InitialSelfTransform.SetLocation( UKismetMathLibrary::Quat_UnrotateVector(ParentElement->GetComponentRotation().Quaternion(), GetComponentLocation() - ParentElement->GetComponentLocation()) );
		InitialSelfTransform.SetRotation( UKismetMathLibrary::NormalizedDeltaRotator(GetComponentRotation(), ParentElement->GetComponentRotation()).Quaternion() );

		SetPositionSourceValue(0, 1, this, UKismetMathLibrary::Quat_RotateVector(ParentElement->GetComponentRotation().Quaternion(), InitialSelfTransform.GetLocation()) );
		SetRotationSourceValue(0, 1, this, FRotator(InitialSelfTransform.GetRotation()));
	}

	OnLinkRigElement(InHandler);
}


// CALLED BY THE HANDLER : Links element to it's physics model equivalent
void UMPAS_RigElement::InitPhysicsModel(const TArray<UMPAS_PhysicsModelElement*>& InPhysicsElements)
{
	PhysicsElements = InPhysicsElements;

	for (int32 i = 0; i < PhysicsElements.Num(); i++)
	{
		PhysicsElementsConfiguration[i].PositionStackID = RegisterPositionStack("PhysicsModelStack_" + UKismetStringLibrary::Conv_IntToString(i));
		PhysicsElementsConfiguration[i].RotationStackID = RegisterRotationStack("PhysicsModelStack_" + UKismetStringLibrary::Conv_IntToString(i));

		RegisterPositionLayer(PhysicsElementsConfiguration[i].PositionStackID, "PhysicsElementLocation", EMPAS_LayerBlendingMode::Normal, EMPAS_LayerCombinationMode::Add, true);
		RegisterRotationLayer(PhysicsElementsConfiguration[i].RotationStackID, "PhysicsElementRotation", EMPAS_LayerBlendingMode::Normal, true);
	}

	OnInitPhysicsModel();
}


// NOTIFICATION Called when ORIENTATION_PositionInterpolationMultiplier or ORIENTATION_RotationInterpolationMultiplier parameter value is changed
void UMPAS_RigElement::OnInterpolationMultiplierChanged(FName InParameterName)
{
	if (InParameterName == "ORIENTATION_PositionInterpolationMultiplier")
		PositionInterpolationMultiplier = GetHandler()->GetFloatParameter(InParameterName);

	else if (InParameterName == "ORIENTATION_RotationInterpolationMultiplier")
		RotationInterpolationMultiplier = GetHandler()->GetFloatParameter(InParameterName);
		
}


void UMPAS_RigElement::UpdateRigElement(float DeltaTime)
{
	// Non-Core elements have their parent position sampled from the parent element every frame
	if (!IsCoreElement)
	{
		// Sampling parent transform
		SetPositionSourceValue(0, 0, ParentElement, ParentElement->GetComponentLocation());
		SetRotationSourceValue(0, 0, ParentElement, ParentElement->GetComponentRotation());

		// Updating self position based on new parent rotation
		SetPositionSourceValue(0, 1, this, UKismetMathLibrary::Quat_RotateVector(ParentElement->GetComponentRotation().Quaternion(), InitialSelfTransform.GetLocation()) );
	}

	// Applying physics model if its enaled
	if (PhysicsModelEnabled && PhysicsElementsConfiguration.Num() > 0)
		ApplyPhysicsModelPositionAndRotation(DeltaTime);

	// If physics model is not enabled, then we apply normal positioning method
	else if (PositioningMode == EMPAS_ElementPositionMode::Normal)
	{
		ApplyDefaultPositionStack(DeltaTime);
		ApplyDefaultRotationStack(DeltaTime);
	}

	if (ReactivatePhysicsModelConstraintNextFrame)
	{
		GetHandler()->ReactivatePhysicsModelConstraint(RigElementName);
		ReactivatePhysicsModelConstraintNextFrame = false;
	}

	// Calculating velocity
	CachedVelocity = (GetComponentLocation() - PreviousFramePosition) / DeltaTime;
	PreviousFramePosition = GetComponentLocation();

	OnUpdateRigElement(DeltaTime);
}


// POSITION LAYERS

// Applies the position stacks to the element's world location
void UMPAS_RigElement::ApplyDefaultPositionStack(float DeltaTime)
{
	FVector StackValue = CalculatePositionStackValue(0);

	FVector NewLocation = StackValue;
	if (PositionInterpolationSpeed != 0)
		NewLocation = UKismetMathLibrary::VInterpTo(GetComponentLocation(), StackValue, DeltaTime, PositionInterpolationSpeed * PositionInterpolationMultiplier);

	SetWorldLocation(NewLocation);
}

// Calculates the final position of the given position stack
FVector UMPAS_RigElement::CalculatePositionStackValue(int32 InPositionStackID)
{
	FVector FinalPosition;

	for (auto& Layer : PositionStacks[InPositionStackID])
	{
		FVector LayerValue = CalculatePositionLayerValue(Layer);

		if (LayerValue != FVector(0, 0, 0))
		{
			switch (Layer.BlendingMode)
			{
			case EMPAS_LayerBlendingMode::Normal:
				FinalPosition = LayerValue;
				break;
			
			case EMPAS_LayerBlendingMode::Add:
				FinalPosition += LayerValue;
				break;

			case EMPAS_LayerBlendingMode::Multiply:
				FinalPosition *= LayerValue;
				break;

			default: break;
			}
		}
	}

	return FinalPosition;
}

// Calculates the final position of the given position layer
FVector UMPAS_RigElement::CalculatePositionLayerValue(const FMPAS_VectorLayer InLayer)
{
	FVector OutPosition = FVector(0, 0, 0);
	int32 ActiveElementCount = 0;


	FVector PositionSum = FVector(0, 0, 0);
	FVector PositionM = FVector(1, 1, 1);

	switch (InLayer.CombinationMode)
	{

	case EMPAS_LayerCombinationMode::Average:

		for (auto& Source : InLayer.LayerElements)
		{
			bool CurrentActive = InLayer.ForceAllElementsActive || Source.Key->GetRigElementActive();

			PositionSum += Source.Value * CurrentActive;
			ActiveElementCount += CurrentActive;
		}

		if (ActiveElementCount > 0)
			OutPosition = PositionSum / ActiveElementCount;
			
		break;

	case EMPAS_LayerCombinationMode::Add:

		for (auto& Source : InLayer.LayerElements)
		{
			bool CurrentActive = InLayer.ForceAllElementsActive || Source.Key->GetRigElementActive();

			PositionSum += Source.Value * CurrentActive;
			ActiveElementCount += CurrentActive;
		}

		OutPosition = PositionSum;
		break;

	case EMPAS_LayerCombinationMode::Multiply:

		for (auto& Source : InLayer.LayerElements)
		{
			bool CurrentActive = InLayer.ForceAllElementsActive || Source.Key->GetRigElementActive();

			ActiveElementCount += CurrentActive;
			if (CurrentActive)
				PositionM *= Source.Value;
		}

		OutPosition = PositionM;
		break;
	
	default:
		OutPosition = FVector();
		break;
	}

	return OutPosition;
}


// Registers a new position stack and returns it's ID, returns an existing ID if the stack is already registered
int32 UMPAS_RigElement::RegisterPositionStack(const FString& InStackName)
{
	if (PositionStackNames.Contains(InStackName))
		return PositionStackNames[InStackName];

	TArray<FMPAS_VectorLayer> NewStack;

	int32 StackID = PositionStacks.Add(NewStack);
	PositionStackNames.Add(InStackName, StackID);

	TMap<FString, int32> NewStackLayerNames;
	PositionLayerNames.Add(NewStackLayerNames);

	return StackID;
}

// Registers a new position layer in the given stack and returns it's ID, returns an existing ID if the layer is already registered, returns -1 if Stack does not exist
int32 UMPAS_RigElement::RegisterPositionLayer(int32 InPositionStackID, const FString& InLayerName, EMPAS_LayerBlendingMode InBlendingMode, EMPAS_LayerCombinationMode InCombinationMode, bool InForceAllElementsActive)
{
	if (InPositionStackID < 0 || InPositionStackID >= PositionStacks.Num())
		return -1;

	if (PositionLayerNames[InPositionStackID].Contains(InLayerName))
		return PositionLayerNames[InPositionStackID][InLayerName];

	FMPAS_VectorLayer NewLayer(InBlendingMode, InCombinationMode, InForceAllElementsActive);

	int32 LayerID = PositionStacks[InPositionStackID].Add(NewLayer);
	PositionLayerNames[InPositionStackID].Add(InLayerName, LayerID);

	return LayerID;
}


// Returns the ID of the given stack, -1 if stack not found
int32 UMPAS_RigElement::GetPositionStackID(const FString& InStackName)
{
	if (!PositionStackNames.Contains(InStackName))
		return -1;

	return PositionStackNames[InStackName];
}

// Returns the ID of the given layer in the given stack, -1 if stack or layer not found
int32 UMPAS_RigElement::GetPositionLayerID(int32 InPositionStackID, const FString& InLayerName)
{
	if (InPositionStackID < 0 || InPositionStackID >= PositionStacks.Num())
		return -1;

	if (!PositionLayerNames[InPositionStackID].Contains(InLayerName))
		return -1;

	return PositionLayerNames[InPositionStackID][InLayerName];
}



// Sets the value of a position source in the given Stack and Layer, if succeded: returns true, false - overwise
bool UMPAS_RigElement::SetPositionSourceValue(int32 InPositionStackID, int32 InPositionLayerID, UMPAS_RigElement* InSourceElement, FVector InSourceValue)
{
	if (InPositionStackID < 0 || InPositionStackID >= PositionStacks.Num())
		return false;

	if (InPositionLayerID < 0 || InPositionLayerID >= PositionStacks[InPositionStackID].Num())
		return false;

	PositionStacks[InPositionStackID][InPositionLayerID].LayerElements.Add(InSourceElement, InSourceValue);
	return true;
}


// Removes the value of a position source in the given Stack and Layer, if succeded: returns true, false - overwise
bool UMPAS_RigElement::RemovePositionSourceValue(int32 InPositionStackID, int32 InPositionLayerID, UMPAS_RigElement* InSourceElement)
{
	if (InPositionStackID < 0 || InPositionStackID >= PositionStacks.Num())
		return false;

	if (InPositionLayerID < 0 || InPositionLayerID >= PositionStacks[InPositionStackID].Num())
		return false;

	return PositionStacks[InPositionStackID][InPositionLayerID].LayerElements.Remove(InSourceElement) > 0;
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
	FRotator FinalRotation = FRotator(0, 0, 0);

	for (auto& Layer : RotationStacks[InRotationStackID])
	{
		bool HasActiveElements;
		FRotator LayerValue = CalculateRotationLayerValue(Layer, HasActiveElements);

		if (HasActiveElements)
		{
			switch (Layer.BlendingMode)
			{

			case EMPAS_LayerBlendingMode::Normal:
				FinalRotation = LayerValue;
				break;
			
			case EMPAS_LayerBlendingMode::Add:
				FinalRotation += LayerValue;
				break;

			case EMPAS_LayerBlendingMode::Multiply:

				FinalRotation.Pitch *= LayerValue.Pitch;
				FinalRotation.Yaw *= LayerValue.Yaw;
				FinalRotation.Roll *= LayerValue.Roll;
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

	TArray<FMPAS_RotatorLayer> NewStack;

	int32 StackID = RotationStacks.Add(NewStack);
	RotationStackNames.Add(InStackName, StackID);

	TMap<FString, int32> NewStackLayerNames;
	RotationLayerNames.Add(NewStackLayerNames);

	return StackID;
}

// Registers a new rotation layer in the given stack and returns it's ID, returns an existing ID if the layer is already registered, returns -1 if Stack does not exist
int32 UMPAS_RigElement::RegisterRotationLayer(int32 InRotationStackID, const FString& InLayerName, EMPAS_LayerBlendingMode InBlendingMode, bool InForceAllElementsActive)
{
	if (InRotationStackID < 0 || InRotationStackID >= RotationStacks.Num())
		return -1;

	if (RotationLayerNames[InRotationStackID].Contains(InLayerName))
		return RotationLayerNames[InRotationStackID][InLayerName];

	FMPAS_RotatorLayer NewLayer(InBlendingMode, EMPAS_LayerCombinationMode::Add, InForceAllElementsActive);

	int32 LayerID = RotationStacks[InRotationStackID].Add(NewLayer);
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
void UMPAS_RigElement::ApplyPhysicsModelPositionAndRotation_Implementation(float DeltaTime) 
{
	if(PhysicsElements.IsValidIndex(0))
		if (PhysicsElements[0])
		{
			//SetWorldLocation( UKismetMathLibrary::VInterpTo(GetComponentLocation(), CalculatePositionStackValue(PhysicsElementsConfiguration[0].PositionStackID), DeltaTime, PhysicsElementsConfiguration[0].PhysicsModelPositionInterpolationSpeed ));
			//SetWorldRotation( UKismetMathLibrary::RInterpTo(GetComponentRotation(), CalculateRotationStackValue(PhysicsElementsConfiguration[0].RotationStackID), DeltaTime, PhysicsElementsConfiguration[0].PhysicsModelRotationInterpolationSpeed ));
			SetWorldLocation(CalculatePositionStackValue(PhysicsElementsConfiguration[0].PositionStackID));
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