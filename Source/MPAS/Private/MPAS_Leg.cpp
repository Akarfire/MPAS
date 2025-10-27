// Fill out your copyright notice in the Description page of Project Settings.


#include "Default/RigElements/MPAS_Leg.h"
#include "MPAS_Handler.h"
#include "Default/MPAS_Core.h"
#include "Kismet/KismetMathLibrary.h"
#include "Curves/RichCurve.h"
#include "Default/RigElements/MPAS_BodySegment.h"

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

	StepTimelineName = FName("LegStepTimeline_" + RigElementName.ToString());
	InHandler->TimerController->CreateTimeline(StepTimelineName, StepAnimationDuration, false, false);
	InHandler->TimerController->RegisterTimelineNotify(StepTimelineName, "StepFinishedNotify", StepAnimationDuration - StepFinishTimeOffset);
	InHandler->TimerController->SubscribeToTimeline(StepTimelineName, this, "OnStepAnimationTimelineUpdated", "OnStepAnimationTimelineFinished", "OnStepAnimationTimelineNotify");

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


	// Absolute location layer - overrides default stack layers before it, detaching the leg from it's parent and placing it in world space
	SelfAbsoluteLocationLayerID = RegisterVectorLayer(0, "AbsoluteLegLocation", EMPAS_LayerBlendingMode::Normal, EMPAS_LayerCombinationMode::Add, 1.f, 0, true);
	SetVectorSourceValue(0, SelfAbsoluteLocationLayerID, this, GetComponentLocation());
	
	// Leg target location stack
	LegTargetLocationStackID = RegisterVectorStack("LegTargetLocation");
	RegisterVectorLayer(LegTargetLocationStackID, "BasicTargetLocation", EMPAS_LayerBlendingMode::Normal, EMPAS_LayerCombinationMode::Add, 1.f, 0, true);

	// Effector shift stack
	EffectorShiftStackID = RegisterVectorStack("EffectorShift");

	// Bone Transform Sync
	BoneTransformSync_LocationLayerID = RegisterVectorLayer(0, "BoneTransformSync", EMPAS_LayerBlendingMode::Add, EMPAS_LayerCombinationMode::Add, 1.f, BoneTransformSyncingLayerPriority);
	BoneTransformSync_RotationLayerID = RegisterRotationLayer(0, "BoneTransformSync", EMPAS_LayerBlendingMode::Add, 1.f, BoneTransformSyncingLayerPriority);


	// Intention Driven Parameters
	if (!InHandler->IsFloatParameterValid("INTENTION_LEGS_StepLengthMultiplier"))
		GetHandler()->CreateFloatParameter("INTENTION_LEGS_StepLengthMultiplier", 1.f);

	if (!InHandler->IsFloatParameterValid("INTENTION_LEGS_StepTriggerDistanceMultiplier"))
		GetHandler()->CreateFloatParameter("INTENTION_LEGS_StepTriggerDistanceMultiplier", 1.f);


	if (!InHandler->IsFloatParameterValid("INTENTION_LEGS_AnimationSpeedMultiplier"))
		GetHandler()->CreateFloatParameter("INTENTION_LEGS_AnimationSpeedMultiplier", 1.f);

	if (!InHandler->IsFloatParameterValid("INTENTION_LEGS_SpeedMultiplier"))
		GetHandler()->CreateFloatParameter("INTENTION_LEGS_SpeedMultiplier", 1.f);

	GetHandler()->SubscribeToParameter("INTENTION_LEGS_StepLengthMultiplier", this, "OnParameterChanged");
	GetHandler()->SubscribeToParameter("INTENTION_LEGS_StepTriggerDistanceMultiplier", this, "OnParameterChanged");
	GetHandler()->SubscribeToParameter("INTENTION_LEGS_AnimationSpeedMultiplier", this, "OnParameterChanged");
	GetHandler()->SubscribeToParameter("INTENTION_LEGS_SpeedMultiplier", this, "OnParameterChanged");
}


// CALLED BY THE HANDLER : Contains the logic that links this element with other elements in the rig
void UMPAS_Leg::LinkRigElement(class UMPAS_Handler* InHandler)
{
	Super::LinkRigElement(InHandler);

	if (!ParentElement) return;

	// Fetching leg target offset
	LegTargetOffset = GetComponentLocation() - ParentElement->GetComponentLocation();

	// Registers the effector layer
	LegEffectorLayerID = ParentElement->RegisterVectorLayer(0, "LegsLocationEffector", EMPAS_LayerBlendingMode::Normal, EMPAS_LayerCombinationMode::Average, 1.f, EffectorLayerPriority);

	// Get resting pose offset
	LegRestingPoseOffset = GetComponentLocation() - ParentElement->GetComponentLocation();

	// Stability effector registration
	ParentElement->SetStabilityEffectorValue(this, 1.f);

	ParentBody = Cast<UMPAS_BodySegment>(ParentElement);

	// Initial Leg Placement
	FVector TraceResult = FootTrace(ParentElement->GetComponentLocation() + ParentElement->GetComponentRotation().RotateVector(LegRestingPoseOffset));
	if (TraceResult != FVector(0, 0, 0))
	{
		SetRigElementActive(true);
		SetVectorSourceValue(0, SelfAbsoluteLocationLayerID, this, TraceResult);
	}

	else
		ParentElement->SetStabilityEffectorValue(this, 0.f);
}


// Updating Rig Element every tick
void UMPAS_Leg::UpdateRigElement(float DeltaTime)
{
	Super::UpdateRigElement(DeltaTime);

	if (!ParentElement) return;

	/*if (FootBone != FName())
		Handler->SetBoneTransform(FootBone, GetComponentTransform());*/

	if (!GetPhysicsModelEnabled())
	{
		// Actual leg update

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
	{
		RealEffectorShift = UKismetMathLibrary::VInterpTo(RealEffectorShift, CalculateVectorStackValue(EffectorShiftStackID), DeltaTime, EffectorShiftInterpolationSpeed);

		FVector LocalLimitedRealEffectorShift = ClampVector(UKismetMathLibrary::Quat_UnrotateVector(GetComponentQuat(), RealEffectorShift), EffectorShift_Min, EffectorShift_Max);

		ParentElement->SetVectorSourceValue(0, LegEffectorLayerID, this, GetComponentLocation() + ParentElement->GetComponentQuat().RotateVector(LocalLimitedRealEffectorShift));
	}

	else if (GetStabilityStatus() == EMPAS_StabilityStatus::Stable)
	{
		SetVectorSourceValue(0, SelfAbsoluteLocationLayerID, this, ParentElement->GetComponentLocation() + ParentElement->GetComponentRotation().RotateVector(LegRestingPoseOffset) + InactiveOffset);

		FVector TraceResult = FootTrace(ParentElement->GetComponentLocation() +  ParentElement->GetComponentRotation().RotateVector(LegRestingPoseOffset));
		if (TraceResult != FVector(0, 0, 0))
		{
			SetRigElementActive(true);
			SetVectorSourceValue(0, SelfAbsoluteLocationLayerID, this, TraceResult);
			ParentElement->SetStabilityEffectorValue(this, 1.f);
		}
	}

	else
	{
		FVector TraceResult = FootTrace(GetComponentLocation() + GetUpVector() * MaxFootElevation);
		if (TraceResult != FVector(0, 0, 0))
		{
			SetRigElementActive(true);
			SetVectorSourceValue(0, SelfAbsoluteLocationLayerID, this, TraceResult);
			ParentElement->SetStabilityEffectorValue(this, 1.f);
		}
	}
}

// CALLED BY THE HANDLER : Synchronizes Rig Element to the most recently fetched bone transforms
//void UMPAS_Leg::SyncToFetchedBoneTransforms()
//{
//	const FTransform* FetchedBoneTransform = GetHandler()->GetCachedFetchedBoneTransforms().Find(FootBone);
//	if (FetchedBoneTransform)
//	{
//		//FVector DeltaLocation = (*FetchedBoneTransform).GetLocation() - GetComponentLocation();
//		//FRotator DeltaRotator = UKismetMathLibrary::NormalizedDeltaRotator((*FetchedBoneTransform).GetRotation().Rotator(), GetComponentRotation());
//
//		if (((*FetchedBoneTransform).GetLocation() - GetComponentLocation()).SizeSquared() > BoneTransformSyncingDistanceSquaredThreshold)
//			SetVectorSourceValue(0, SelfAbsoluteLocationLayerID, this, (*FetchedBoneTransform).GetLocation());
//	}
//}


// Returns leg's target location
FVector UMPAS_Leg::GetTargetLocation()
{
	// Basic target location calculation
	if (ParentBody)
		SetVectorSourceValue(LegTargetLocationStackID, 0, this, ParentBody->GetDesiredRotation().RotateVector(GetLegTargetOffset()) + ParentBody->GetDesiredLocation());

	//(ParentElement->GetComponentRotation().RotateVector(LegTargetOffset) + ParentElement->GetComponentLocation())
	return CalculateVectorStackValue(LegTargetLocationStackID);
}


// Casts a trace that finds a suitable foot location, (0, 0, 0) if failed
FVector UMPAS_Leg::FootTrace(const FVector& StepTargetLocation)
{
	FVector StartLocation = StepTargetLocation + GetUpVector() * MaxFootElevation;
	FVector EndLocation = StepTargetLocation - GetUpVector() * MaxFootVerticalExtent;

	FHitResult Hit;
	if (GetWorld()->LineTraceSingleByChannel(Hit, StartLocation, EndLocation, ECC_Visibility))
		return Hit.Location;

	// If no location was found FVector(0.f, 0.f, 0.f) is returned
	else
		return FVector(0, 0, 0);
}


// Checks whether the leg should make a step
bool UMPAS_Leg::ShouldStep()
{
	FVector CurrentFootLocation = GetComponentLocation() * ( FVector(1) - GetUpVector() );
	FVector TargetFootLocation = GetTargetLocation() * ( FVector(1) - GetUpVector() );

	//float MaxStepLength = abs(StepLength - FVector::Distance(GetComponentLocation() * (FVector(1) - GetUpVector()), (ParentElement->GetComponentLocation() + ParentElement->GetComponentRotation().RotateVector(LegRestingPoseOffset)) * (FVector(1) - GetUpVector())));
	float MaxStepLength = StepTriggerDistance * StepLengthMultiplier * StepTriggerDistanceMultiplier * SpeedMultiplier;
	bool StepCondition = FVector::Distance(CurrentFootLocation, TargetFootLocation) > FMath::Min(StepTriggerDistance * StepTriggerDistanceMultiplier, MaxStepLength);

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

	FVector StepVector = GetTargetLocation() - GetComponentLocation();
	FVector StepDirection = StepVector;
	StepDirection.Normalize();

	//float MaxStepLength = abs(StepLength - FVector::Distance(GetComponentLocation() * (FVector(1) - GetUpVector()), (ParentElement->GetComponentLocation() + ParentElement->GetComponentRotation().RotateVector(LegRestingPoseOffset)) * (FVector(1) - GetUpVector())));
	float MaxStepLength = StepLength * StepLengthMultiplier * SpeedMultiplier;

	FVector AdjustedFootTraceLocation = StepDirection * FMath::Min(StepVector.Size(), MaxStepLength) + GetComponentLocation();
	StepAnimationTargetLocation = FootTrace(AdjustedFootTraceLocation);

	StepDistance = FVector::Distance(StepAnimationStartLocation, StepAnimationTargetLocation);

	if (StepAnimationTargetLocation == FVector(0, 0, 0))
	{
		SetRigElementActive(false);
		ParentElement->RemoveVectorSourceValue(0, LegEffectorLayerID, this);
		ParentElement->SetStabilityEffectorValue(this, 0.f);
	}

	else
	{
		SetRigElementActive(true);
		GetHandler()->SetIntParameter("CurrentlyMovingLegsCount", GetHandler()->GetIntParameter("CurrentlyMovingLegsCount") + 1);
		GetHandler()->TimerController->SetTimelinePlaybackSpeed(StepTimelineName, AnimationSpeedMultiplier * SpeedMultiplier);
		GetHandler()->TimerController->StartTimeline(StepTimelineName);
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
		SetVectorSourceValue(0, SelfAbsoluteLocationLayerID, this, NewLocation);
	}
}

// CALLED BY THE HANDLER :  Called when a subscribed-to timeline is finished
void UMPAS_Leg::OnStepAnimationTimelineFinished(FName InTimelineName)
{
	if (InTimelineName == StepTimelineName)
	{
		IsMoving = false;
	}
}

// CALLED BY THE HANDLER : NOTIFICATION Called when a subscribed-to timeline is finished
void UMPAS_Leg::OnStepAnimationTimelineNotify(FName InTimelineName, FName InNotifyName)
{
	if (InTimelineName == StepTimelineName && InNotifyName == "StepFinishedNotify")
	{
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
			while (!GetHandler()->IsIntParameterValid(FName("LegGroupSize_" + FString::FromInt(CurrentLegGroup))))
				CurrentLegGroup++;

			GetHandler()->SetIntParameter("CurrentLegGroup", CurrentLegGroup);
		}
	}
}


void UMPAS_Leg::OnPhysicsModelDisabled_Implementation()
{
	if (PhysicsElements.IsValidIndex(0))
		if(PhysicsElements[0])
			SetVectorSourceValue(0, SelfAbsoluteLocationLayerID, this, PhysicsElements[0]->GetComponentLocation());
}	


// Offset in time from the end of the step animation, when the leg group assumes the step to be finished
// Useful for running, where the next group needs to start BEFORE the current one actually finishes
void UMPAS_Leg::SetStepFinishTimeOffset(float newOffset)
{
	StepFinishTimeOffset = newOffset;

	// Updating value in the timeline
	GetHandler()->TimerController->ModifyTimelineNotify(StepTimelineName, "StepFinishedNotify", StepAnimationDuration - StepFinishTimeOffset);
}