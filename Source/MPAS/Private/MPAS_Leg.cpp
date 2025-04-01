// Fill out your copyright notice in the Description page of Project Settings.


#include "MPAS_Leg.h"
#include "MPAS_Handler.h"
#include "MPAS_Core.h"
#include "Curves/RichCurve.h"

// Constructor
UMPAS_Leg::UMPAS_Leg()
{
	//PositioningMode = EMPAS_ElementPositionMode::Independent;
	PhysicsElementsConfiguration.Add(FMPAS_PhysicsElementConfiguration());
}


// Initializing Rig Element
void UMPAS_Leg::InitRigElement(UMPAS_Handler* InHandler)
{
	Super::InitRigElement(InHandler);
	
	LegTargetOffset = GetComponentLocation() - InHandler->GetCore()->GetComponentLocation();

	StepTimelineName = FName("LegStepTimeline_" + RigElementName.ToString());
	InHandler->CreateTimeline(StepTimelineName, StepAnimationDuration, false, false);
	InHandler->SubscribeToTimeline(StepTimelineName, this, "OnStepAnimationTimelineUpdated", "OnStepAnimationTimelineFinished");

	// Currently scheduled to move group of legs
	InHandler->CreateIntParameter("CurrentLegGroup", 0);

	// The number of legs, that are currently moving
	InHandler->CreateIntParameter("CurrentlyMovingLegsCount", 0);

	// The maximum ID of a leg group in the rig, after this group has finished moving, leg group resets to 0
	if (!InHandler->IsIntParameterValid("MaxLegGroupID"))
		InHandler->CreateIntParameter("MaxLegGroupID", LegGroup);

	else if (LegGroup > InHandler->GetIntParameter("MaxLegGroupID"))
		InHandler->SetIntParameter("MaxLegGroupID", LegGroup);

	// The number of legs assigned to an individual leg group
	if (!InHandler->IsIntParameterValid(FName("LegGroupSize_" + FString::FromInt(LegGroup))))
		InHandler->CreateIntParameter(FName("LegGroupSize_" + FString::FromInt(LegGroup)), 1);

	else
		InHandler->SetIntParameter(FName("LegGroupSize_" + FString::FromInt(LegGroup)), InHandler->GetIntParameter(FName("LegGroupSize_" + FString::FromInt(LegGroup))) + 1);

	InHandler->SubscribeToParameter("CurrentLegGroup", this, "OnParameterChanged");

	// Absolute position layer - overrides default stack layers before it, detaching the leg from it's parent and placing it in world space
	SelfAbsolutePositionLayerID = RegisterPositionLayer(0, "AbsoluteLegPosition", EMPAS_LayerBlendingMode::Normal, EMPAS_LayerCombinationMode::Add, true);
	SetPositionSourceValue(0, SelfAbsolutePositionLayerID, this, GetComponentLocation());
	

	// Intention Driven Parameters
	if (!InHandler->IsFloatParameterValid("INTENTION_LEGS_StepLengthMultiplier"))
		GetHandler()->CreateFloatParameter("INTENTION_LEGS_StepLengthMultiplier", 1.f);

	if (!InHandler->IsFloatParameterValid("INTENTION_LEGS_StepTriggerDistanceMultiplier"))
		GetHandler()->CreateFloatParameter("INTENTION_LEGS_StepTriggerDistanceMultiplier", 1.f);

	if (!InHandler->IsFloatParameterValid("INTENTION_LEGS_ParentElevationMultiplier"))
		GetHandler()->CreateFloatParameter("INTENTION_LEGS_ParentElevationMultiplier", 1.f);

	if (!InHandler->IsFloatParameterValid("INTENTION_LEGS_AnimationSpeedMultiplier"))
		GetHandler()->CreateFloatParameter("INTENTION_LEGS_AnimationSpeedMultiplier", 1.f);

	if (!InHandler->IsFloatParameterValid("INTENTION_LEGS_SpeedMultiplier"))
		GetHandler()->CreateFloatParameter("INTENTION_LEGS_SpeedMultiplier", 1.f);

	GetHandler()->SubscribeToParameter("INTENTION_LEGS_StepLengthMultiplier", this, "OnParameterChanged");
	GetHandler()->SubscribeToParameter("INTENTION_LEGS_StepTriggerDistanceMultiplier", this, "OnParameterChanged");
	GetHandler()->SubscribeToParameter("INTENTION_LEGS_ParentElevationMultiplier", this, "OnParameterChanged");
	GetHandler()->SubscribeToParameter("INTENTION_LEGS_AnimationSpeedMultiplier", this, "OnParameterChanged");
	GetHandler()->SubscribeToParameter("INTENTION_LEGS_SpeedMultiplier", this, "OnParameterChanged");
}


// CALLED BY THE HANDLER : Contains the logic that links this element with other elements in the rig
void UMPAS_Leg::LinkRigElement(class UMPAS_Handler* InHandler)
{
	Super::LinkRigElement(InHandler);

	// Registers the effector layer
	LegEffectorLayerID = ParentElement->RegisterPositionLayer(0, "LegsPositionEffector", EMPAS_LayerBlendingMode::Normal, EMPAS_LayerCombinationMode::Average);

	// Get resting pose offset
	LegRestingPoseOffset = GetComponentLocation() - ParentElement->GetComponentLocation();

	// Stability effector registration
	ParentElement->SetStabilityEffectorValue(this, 1.f);

	// Initial Leg Placement
	FVector TraceResult = FootTrace(ParentElement->GetComponentLocation() + ParentElement->GetComponentRotation().RotateVector(LegRestingPoseOffset));
	if (TraceResult != FVector(0, 0, 0))
	{
		SetRigElementActive(true);
		SetPositionSourceValue(0, SelfAbsolutePositionLayerID, this, TraceResult);
	}

	else
		ParentElement->SetStabilityEffectorValue(this, 0.f);
}


// Updating Rig Element every tick
void UMPAS_Leg::UpdateRigElement(float DeltaTime)
{
	Super::UpdateRigElement(DeltaTime);

	if (FootBone != FName())
		Handler->SetBoneTransform(FootBone, GetComponentTransform());

	if (!GetPhysicsModelEnabled())
	{
		if (!IsMoving && !IsReadyToStep())
			ReadyToStep = ShouldStep();

		if (!IsMoving && !WaitingOnLegGroup && IsReadyToStep())
		{
			if (Handler->GetIntParameter("CurrentLegGroup") == LegGroup && !HasMovedInCurrentWindow)
				StartStepAnimation();

			else
				WaitingOnLegGroup = true;
		}
	}

	// If element is active 
	if (GetRigElementActive())
		ParentElement->SetPositionSourceValue(0, LegEffectorLayerID, this, GetComponentLocation() + GetUpVector() * ParentVerticalOffset * ParentElevationMultiplier);

	else if (GetStabilityStatus() == EMPAS_StabilityStatus::Stable)
	{
		SetPositionSourceValue(0, SelfAbsolutePositionLayerID, this, ParentElement->GetComponentLocation() + ParentElement->GetComponentRotation().RotateVector(LegRestingPoseOffset) + InactiveOffset);

		FVector TraceResult = FootTrace(ParentElement->GetComponentLocation() +  ParentElement->GetComponentRotation().RotateVector(LegRestingPoseOffset));
		if (TraceResult != FVector(0, 0, 0))
		{
			SetRigElementActive(true);
			SetPositionSourceValue(0, SelfAbsolutePositionLayerID, this, TraceResult);
			ParentElement->SetStabilityEffectorValue(this, 1.f);
		}
	}

	else
	{
		FVector TraceResult = FootTrace(GetComponentLocation() + GetUpVector() * MaxFootElevation);
		if (TraceResult != FVector(0, 0, 0))
		{
			SetRigElementActive(true);
			SetPositionSourceValue(0, SelfAbsolutePositionLayerID, this, TraceResult);
			ParentElement->SetStabilityEffectorValue(this, 1.f);
		}
	}
}


// Returns leg's target position
FVector UMPAS_Leg::GetTargetPosition()
{
	return (ParentElement->GetComponentRotation().RotateVector(LegTargetOffset) + GetHandler()->GetCore()->GetComponentLocation());
}


// Casts a trace that finds a suitable foot position, (0, 0, 0) if failed
FVector UMPAS_Leg::FootTrace(const FVector& StepTargetPosition)
{
	FVector StartPosition = StepTargetPosition + GetUpVector() * MaxFootElevation;
	FVector EndPosition = StartPosition - GetUpVector() * MaxFootVerticalExtent;

	FHitResult Hit;
	if (GetWorld()->LineTraceSingleByChannel(Hit, StartPosition, EndPosition, ECC_Visibility))
		return Hit.Location;

	// If no location was found FVector(0.f, 0.f, 0.f) is returned
	else
		return FVector(0, 0, 0);
}


// Checks whether the leg should make a step
bool UMPAS_Leg::ShouldStep()
{
	FVector CurrentFootPosition = GetComponentLocation() * ( FVector(1) - GetUpVector() );
	FVector TargetFootPosition = GetTargetPosition() * ( FVector(1) - GetUpVector() );

	//float MaxStepLength = abs(StepLength - FVector::Distance(GetComponentLocation() * (FVector(1) - GetUpVector()), (ParentElement->GetComponentLocation() + ParentElement->GetComponentRotation().RotateVector(LegRestingPoseOffset)) * (FVector(1) - GetUpVector())));
	float MaxStepLength = StepTriggerDistance * StepLengthMultiplier * StepTriggerDistanceMultiplier * SpeedMultiplier;
	bool StepCondition = FVector::Distance(CurrentFootPosition, TargetFootPosition) > FMath::Min(StepTriggerDistance * StepTriggerDistanceMultiplier, MaxStepLength);

	return StepCondition;
}


// CALLED BY THE HANDLER :  Called when a subscribed-to parameter is changed
void UMPAS_Leg::OnParameterChanged(FName InParameterName)
{
	if (InParameterName == "CurrentLegGroup")
	{
		HasMovedInCurrentWindow = false;
		if (WaitingOnLegGroup)
			if (Handler->GetIntParameter(InParameterName) == LegGroup)
				StartStepAnimation();
	}

	else if (InParameterName == "INTENTION_LEGS_StepLengthMultiplier")
		StepLengthMultiplier = GetHandler()->GetFloatParameter(InParameterName);

	else if (InParameterName == "INTENTION_LEGS_StepTriggerDistanceMultiplier")
		StepTriggerDistanceMultiplier = GetHandler()->GetFloatParameter(InParameterName);

	else if (InParameterName == "INTENTION_LEGS_ParentElevationMultiplier")
		ParentElevationMultiplier = GetHandler()->GetFloatParameter(InParameterName);

	else if (InParameterName == "INTENTION_LEGS_AnimationSpeedMultiplier")
		AnimationSpeedMultiplier = GetHandler()->GetFloatParameter(InParameterName);

	else if (InParameterName == "INTENTION_LEGS_SpeedMultiplier")
		SpeedMultiplier = GetHandler()->GetFloatParameter(InParameterName);
}


// Starts the step animation timeline
void UMPAS_Leg::StartStepAnimation()
{
	WaitingOnLegGroup = false;
	ReadyToStep = false;

	StepAnimationStartLocation = GetComponentLocation();

	FVector StepVector = GetTargetPosition() - GetComponentLocation();
	FVector StepDirection = StepVector;
	StepDirection.Normalize();

	//float MaxStepLength = abs(StepLength - FVector::Distance(GetComponentLocation() * (FVector(1) - GetUpVector()), (ParentElement->GetComponentLocation() + ParentElement->GetComponentRotation().RotateVector(LegRestingPoseOffset)) * (FVector(1) - GetUpVector())));
	float MaxStepLength = StepLength * StepLengthMultiplier * SpeedMultiplier;

	FVector AdjustedFootTraceLocation = StepDirection * FMath::Min(StepVector.Size(), MaxStepLength) + GetComponentLocation();
	StepAnimationTargetLocation = FootTrace(AdjustedFootTraceLocation + MaxFootElevation * GetUpVector());

	StepDistance = FVector::Distance(StepAnimationStartLocation, StepAnimationTargetLocation);

	if (StepAnimationTargetLocation == FVector(0, 0, 0))
	{
		SetRigElementActive(false);
		ParentElement->RemovePositionSourceValue(0, LegEffectorLayerID, this);
		ParentElement->SetStabilityEffectorValue(this, 0.f);
	}

	else
	{
		SetRigElementActive(true);
		GetHandler()->SetIntParameter("CurrentlyMovingLegsCount", GetHandler()->GetIntParameter("CurrentlyMovingLegsCount") + 1);
		GetHandler()->SetTimelinePlaybackSpeed(StepTimelineName, AnimationSpeedMultiplier * SpeedMultiplier);
		GetHandler()->StartTimeline(StepTimelineName);
		IsMoving = true;
		HasMovedInCurrentWindow = true;
		ParentElement->SetStabilityEffectorValue(this, 1.f);
	}
}


// CALLED BY THE HANDLER :  Called when a subscribed-to timeline is updated
void UMPAS_Leg::OnStepAnimationTimelineUpdated(FName InTimelineName, float CurrentTime)
{
	if (InTimelineName == StepTimelineName)
	{
		float ExtentFactor = CurrentTime / StepAnimationDuration;
		if (StepAnimationExtentCurve)
			ExtentFactor = StepAnimationExtentCurve->GetFloatValue(ExtentFactor);

		float HeightFactor = (1 - abs(( (CurrentTime / StepAnimationDuration) - 0.5) * 2));
		if (StepAnimationHeightCurve)
			HeightFactor = StepAnimationHeightCurve->GetFloatValue(CurrentTime / StepAnimationDuration);

		float ResultingStepHeight = StepHeight * (0.5 + (StepDistance / (StepLength * StepLengthMultiplier)) / 2);
		if (StepHeightScalingCurve)
			ResultingStepHeight = StepHeight * StepHeightScalingCurve->GetFloatValue(StepDistance / (StepLength * StepLengthMultiplier));

		FVector NewLocation = FMath::Lerp(StepAnimationStartLocation, StepAnimationTargetLocation, ExtentFactor) + GetUpVector() * ResultingStepHeight * HeightFactor;
		SetPositionSourceValue(0, SelfAbsolutePositionLayerID, this, NewLocation);
	}
}

// CALLED BY THE HANDLER :  Called when a subscribed-to timeline is finished
void UMPAS_Leg::OnStepAnimationTimelineFinished(FName InTimelineName)
{
	if (InTimelineName == StepTimelineName)
	{
		IsMoving = false;

		int32 CurrentlyMovingLegsCount = GetHandler()->GetIntParameter("CurrentlyMovingLegsCount");
		GetHandler()->SetIntParameter("CurrentlyMovingLegsCount", CurrentlyMovingLegsCount - 1);

		if (CurrentlyMovingLegsCount - 1 == 0)
		{
			int32 CurrentLegGroup = GetHandler()->GetIntParameter("CurrentLegGroup");

			if (CurrentLegGroup == GetHandler()->GetIntParameter("MaxLegGroupID"))
				CurrentLegGroup = 0;

			else
				CurrentLegGroup++;
			
			// Skiping empty leg groups
			while ( !GetHandler()->IsIntParameterValid( FName( "LegGroupSize_" + FString::FromInt(CurrentLegGroup) ) ) )
				CurrentLegGroup++;

			GetHandler()->SetIntParameter("CurrentLegGroup", CurrentLegGroup);
		}
	}
}


void UMPAS_Leg::OnPhysicsModelDisabled_Implementation()
{
	if (PhysicsElements.IsValidIndex(0))
		if(PhysicsElements[0])
			SetPositionSourceValue(0, SelfAbsolutePositionLayerID, this, PhysicsElements[0]->GetComponentLocation());
}	