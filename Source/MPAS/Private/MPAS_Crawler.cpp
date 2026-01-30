// Fill out your copyright notice in the Description page of Project Settings.


#include "Default/RigElements/MPAS_Crawler.h"
#include "Default/RigElements/MPAS_BodySegment.h"
#include "Kismet/KismetMathLibrary.h"
#include "MPAS_Handler.h"


// CALLED BY THE HANDLER : Initializing Rig Element
void UMPAS_Crawler::InitRigElement(UMPAS_Handler* InHandler)
{
	Super::InitRigElement(InHandler);

	// Registering a location override layer 
	
	SelfAbsoluteLocationLayer = RegisterVectorLayer(0, FMPAS_VectorLayer(EMPAS_LayerBlendingMode::Normal, 1.f, EMPAS_LayerCombinationMode::Add, 0, true, "SelfAbsoluteLocation"));
	GetVectorStack(0)[SelfAbsoluteLocationLayer].SetSourceValue(this, GetComponentLocation());

	// Target location stack
	TargetLocationStackID = RegisterVectorStack("TargetLocationStack");
	RegisterVectorLayer(TargetLocationStackID, FMPAS_VectorLayer(EMPAS_LayerBlendingMode::Normal, 1.f, EMPAS_LayerCombinationMode::Add, 0, true, "BasicTargetLocation"));
	
	// Effector shift stack
	EffectorShiftStackID = RegisterVectorStack("EffectorShiftStack");

	// Bone Transform Sync
	BoneTransformSync_LocationLayerID = RegisterVectorLayer(0, FMPAS_VectorLayer(EMPAS_LayerBlendingMode::Add, 1.f, EMPAS_LayerCombinationMode::Add, BoneTransformSyncingLayerPriority, false, "BoneTransformSync"));
	BoneTransformSync_RotationLayerID = RegisterRotatorLayer(0, FMPAS_RotatorLayer(EMPAS_LayerBlendingMode::Add, 1.f, EMPAS_LayerCombinationMode::Add, BoneTransformSyncingLayerPriority, false, "BoneTransformSync"));
}

// CALLED BY THE HANDLER : Contains the logic that links this element with other elements in the rig
void UMPAS_Crawler::LinkRigElement(UMPAS_Handler* InHandler)
{
	Super::LinkRigElement(InHandler);

	if (!ParentElement) return;

	ParentBody = Cast<UMPAS_BodySegment>(ParentElement);

	// Fetching parent offset
	ParentOffset = UKismetMathLibrary::Quat_UnrotateVector(ParentElement->GetComponentQuat(), ParentElement->GetComponentLocation() - GetComponentLocation());

	// Regisering crawler effector layer
	ParentLocationEffectorLayer = ParentElement->RegisterVectorLayer(0, FMPAS_VectorLayer(EMPAS_LayerBlendingMode::Normal, 1.f, EMPAS_LayerCombinationMode::Average, 0, true, "CrawlersLocationEffector"));
	ParentElement->GetVectorStack(0)[ParentLocationEffectorLayer].SetSourceValue(this, GetComponentLocation() + ParentElement->GetComponentRotation().RotateVector(ParentOffset));
}

// CALLED BY THE HANDLER : Updating Rig Element every tick
void UMPAS_Crawler::UpdateRigElement(float DeltaTime)
{
	Super::UpdateRigElement(DeltaTime);

	if (IsCoreElement || !ParentElement) return;

	UpdateMovement(DeltaTime);

	// Updating parent location effector value
	UpdateParentEffector(DeltaTime);
}


// Checks if there is ground under the crawler
bool UMPAS_Crawler::GroundCheck(FHitResult& Hit)
{
	return GetWorld()->LineTraceSingleByChannel(Hit, GetComponentLocation(), GetComponentLocation() - GetUpVector() * GroundCheckDistance, GroundCheckCollisionChannel);
}


// Returns the location, that the crawler is supposed to assume
FVector UMPAS_Crawler::GetTargetLocation()
{
	// Basic target location calculation
	ParentElement->GetVectorStack(TargetLocationStackID)[0].SetSourceValue(this, ParentBody->GetDesiredLocation() + ParentBody->GetDesiredRotation().RotateVector(-1 * ParentOffset));
	
	return UStacksAndLayers::CalculateStack_Vector(GetVectorStack(TargetLocationStackID));
}

// Per-frame movement logic of the crawler
void UMPAS_Crawler::UpdateMovement(float DeltaTime)
{
	// Check for ground underneath the crawler
	FHitResult GroundHit;
	IsGrounded = GroundCheck(GroundHit);

	if (IsGrounded)
	{
		// Ground movement logic

		FVector TargetLocation = GetTargetLocation();

		// Current speed and movement direction
		float Speed = MovementVelocity.Size() + 0.0001f;
		FVector VelocityDirection = MovementVelocity / Speed;

		// Movement vector calculations
		FVector DeltaVector = (TargetLocation - GetComponentLocation());

		float MovementDistance = DeltaVector.Size() + 0.0001f;
		FVector MovementDirection = DeltaVector / MovementDistance;

		// Friction force for breaking implementation
		float ResultingFriction = GroundFriction;

		// Actual movement
		// Only move if delta location is larger than some trigger value
		if (MovementDistance > MovementTriggerDistance)
		{
			float CurrentTimeToTarget = 0;

			// Solving Current time to target equation

			float D = Speed * Speed - 4 * GroundFriction * MovementDistance;

			if (D < 0) CurrentTimeToTarget = 1e20;
			else CurrentTimeToTarget = UKismetMathLibrary::FMin(Speed - sqrt(D), Speed + sqrt(D)) / (2 * GroundFriction);

			// Finding breaking time
			// Time until velocity reaches zero just from the ground friction effect
			float CurrentTimeToFrictionBreak = Speed / GroundFriction;

			// Making decisions based on the calculated times

			// Accelerate only if current time to target exeeds breaking time
			if (CurrentTimeToTarget > CurrentTimeToFrictionBreak)
			{
				float RequiredAcceleration = Acceleration;

				MovementVelocity += MovementDirection * RequiredAcceleration * DeltaTime;
			}
		}

		// Breaking
		else if (Speed > 0) { ResultingFriction += BreakingFriction; }

		// Friction
		VelocityDirection = MovementVelocity.GetSafeNormal();

		MovementVelocity += -1 * VelocityDirection * ResultingFriction * DeltaTime;
		if (FVector::DotProduct(VelocityDirection, MovementVelocity.GetSafeNormal()) < 0) MovementVelocity = FVector::ZeroVector;

		// Velocity clamping
		Speed = MovementVelocity.Size() + 0.0001f;
		VelocityDirection = MovementVelocity / Speed;

		if (Speed > MaxSpeed)
			MovementVelocity = VelocityDirection * MaxSpeed;
	}

	// Updating element's location layer value
	FVector NewLocation = GetVectorStack(0)[SelfAbsoluteLocationLayer].GetSourceValue(this) + MovementVelocity;
	GetVectorStack(0)[SelfAbsoluteLocationLayer].SetSourceValue(this, NewLocation);
}

// Updates effector on the parent element's location
void UMPAS_Crawler::UpdateParentEffector(float DeltaTime)
{
	if (!ParentElement) return;

	RealEffectorShift = UKismetMathLibrary::VInterpTo(RealEffectorShift, UStacksAndLayers::CalculateStack_Vector(GetVectorStack(EffectorShiftStackID)), DeltaTime, EffectorShiftInterpolationSpeed);

	FVector LocalizedRealShift = UKismetMathLibrary::Quat_UnrotateVector(ParentElement->GetComponentQuat(), RealEffectorShift);
	FVector LocalizedLimitedRealShift = ClampVector(LocalizedRealShift, EffectorShift_Min, EffectorShift_Max);
	FVector LimitedRealShift = ParentElement->GetComponentQuat().RotateVector(LocalizedLimitedRealShift);

	ParentElement->GetVectorStack(0)[ParentLocationEffectorLayer].SetSourceValue(this, GetComponentLocation() + ParentElement->GetComponentRotation().RotateVector(ParentOffset) + LimitedRealShift);
}


// CALLED BY THE HANDLER : Synchronizes Rig Element to the most recently fetched bone transforms
void UMPAS_Crawler::SyncToFetchedBoneTransforms(float DeltaTime)
{
	const FTransform* FetchedBoneTransform = GetHandler()->GetCachedFetchedBoneTransforms().Find(BoneName);
	if (FetchedBoneTransform)
	{
		FVector DeltaLocation = (*FetchedBoneTransform).GetLocation() - GetComponentLocation();
		FQuat DeltaRotator = UKismetMathLibrary::NormalizedDeltaRotator((*FetchedBoneTransform).GetRotation().Rotator(), GetComponentRotation()).Quaternion();

		// Checking if deltas are large enough to consider transform modified
		if (DeltaLocation.Size() > BoneTransformSync_LocationDeltaSensitivityThreshold
			&& acos(DeltaRotator.Vector().Dot(FVector::UnitX())) > BoneTransformSync_AngularDeltaSensitivityThreshold)
		{
			// Resetting timeout timer
			BoneTransformSync_Timer = BoneTransformSync_Timeout;

			// Updating applied bone transform offsets
			BoneTransformSync_AppliedBoneLocationOffset = DeltaLocation;
			BoneTransformSync_AppliedBoneAngularOffset = DeltaRotator;
		}

		// Counting down the timer if bone transform was not modifed
		else if (BoneTransformSync_Timer > 0) BoneTransformSync_Timer -= DeltaTime;

		// Offset realocation
		if (BoneTransformSync_Timer <= 0)
		{
			FVector CurrentSyncOffset = GetVectorStack(0)[BoneTransformSync_LocationLayerID].GetSourceValue(this);
			FRotator CurrentSyncAngle = GetRotatorStack(0)[BoneTransformSync_RotationLayerID].GetSourceValue(this);

			FVector NewSyncOffset = UKismetMathLibrary::VInterpTo(CurrentSyncOffset,
				BoneTransformSync_AppliedBoneLocationOffset,
				DeltaTime, BoneTransformSync_OffsetLocationRealocationSpeed);

			FRotator AppliedAngularOffsetRot = BoneTransformSync_AppliedBoneAngularOffset.Rotator();
			FRotator NewSyncAngle = UKismetMathLibrary::RInterpTo(CurrentSyncAngle,
				AppliedAngularOffsetRot,
				DeltaTime, BoneTransformSync_OffsetAngularRealocationSpeed);

			GetVectorStack(0)[BoneTransformSync_LocationLayerID].SetSourceValue(this, NewSyncOffset);
			GetRotatorStack(0)[BoneTransformSync_RotationLayerID].SetSourceValue(this, NewSyncAngle);

			BoneTransformSync_AppliedBoneLocationOffset -= NewSyncOffset - CurrentSyncOffset;
			BoneTransformSync_AppliedBoneAngularOffset = (AppliedAngularOffsetRot - (NewSyncAngle - CurrentSyncAngle)).Quaternion();
		}
	}
}