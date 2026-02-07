// Fill out your copyright notice in the Description page of Project Settings.

#include "Default/RigElements/MPAS_BodySegment.h"
#include "MPAS_Handler.h"
#include "Default/MPAS_Core.h"
#include "Kismet/KismetMathLibrary.h"


// Constructor
UMPAS_BodySegment::UMPAS_BodySegment()
{
    // Position Driver settings
    PositionDriverIntegration_LocationStackName = "DesiredLocation";
    PositionDriverIntegration_RotationStackName = "DesiredRotation";
}

// CALLED BY THE HANDLER : Initializing Rig Element
void UMPAS_BodySegment::InitRigElement(class UMPAS_Handler* InHandler)
{
    Super::InitRigElement(InHandler);

    // Registering desired transform stacks and layers + setiing core offset values

    DesiredLocationStackID = RegisterVectorStack("DesiredLocation");
    RegisterVectorLayer(DesiredLocationStackID, FMPAS_VectorLayer(EMPAS_LayerBlendingMode::Normal, 1.f, EMPAS_LayerCombinationMode::Add, 0, false, "Core"));
    RegisterVectorLayer(DesiredLocationStackID, FMPAS_VectorLayer(EMPAS_LayerBlendingMode::Add, 1.f, EMPAS_LayerCombinationMode::Add, 0, false, "OffsetFromCore"));

    GetVectorStack(DesiredLocationStackID)[1].SetSourceValue(this, GetComponentLocation() - InHandler->GetCore()->GetComponentLocation());

    DesiredRotationStackID = RegisterRotatorStack("DesiredRotation");
    RegisterRotatorLayer(DesiredRotationStackID, FMPAS_RotatorLayer(EMPAS_LayerBlendingMode::Normal, 1.f, EMPAS_LayerCombinationMode::Add, 0, false, "Core"));
    RegisterRotatorLayer(DesiredLocationStackID, FMPAS_RotatorLayer(EMPAS_LayerBlendingMode::Add, 1.f, EMPAS_LayerCombinationMode::Add, 0, false, "OffsetFromCore"));

    GetRotatorStack(DesiredRotationStackID)[1].SetSourceValue(this, GetComponentRotation() - InHandler->GetCore()->GetComponentRotation());

    // Bone Transform Sync
    BoneTransformSync_LocationLayerID = RegisterVectorLayer(0, FMPAS_VectorLayer(EMPAS_LayerBlendingMode::Add, 1.f, EMPAS_LayerCombinationMode::Add, BoneTransformSyncingLayerPriority, false, "BoneTransformSync"));
    BoneTransformSync_RotationLayerID = RegisterRotatorLayer(0, FMPAS_RotatorLayer(EMPAS_LayerBlendingMode::Add, 1.f, EMPAS_LayerCombinationMode::Add, BoneTransformSyncingLayerPriority, false, "BoneTransformSync"));
}

// CALLED BY THE HANDLER :  Updating Rig Element every tick
void UMPAS_BodySegment::UpdateRigElement(float DeltaTime)
{
    Super::UpdateRigElement(DeltaTime);

    // Processing UseCoreRotation option
    if (UseCoreRotation)
    {
        FRotator CurrentSelfRotation = GetRotatorStack(0)[1].GetSourceValue(this);
        FRotator NewRotation = UKismetMathLibrary::RInterpTo_Constant(CurrentSelfRotation, GetHandler()->GetCore()->GetComponentRotation(), DeltaTime, LiniarRotationInterpolationSpeed);

        GetRotatorStack(0)[1].SetSourceValue(this, NewRotation);
    }

    // Updating desired transform stacks

    // Core transform update

    GetVectorStack(DesiredLocationStackID)[0].SetSourceValue(this, GetHandler()->GetCore()->GetComponentLocation());
    GetRotatorStack(DesiredRotationStackID)[0].SetSourceValue(this, GetHandler()->GetCore()->GetComponentRotation());

    // Caching desired transform
    CachedDesiredLocation = UStacksAndLayers::CalculateStack_Vector(GetVectorStack(DesiredLocationStackID));
    CachedDesiredRotation = UStacksAndLayers::CalculateStack_Rotator(GetRotatorStack(DesiredRotationStackID));

    // Updating enforcement
    //FVector EnforcementVector = UKismetMathLibrary::VInterpTo(GetComponentLocation(), CachedDesiredLocation, DeltaTime, DesiredPositionEnforcement) - GetComponentLocation();
    
    if (!IsCoreElement) // Core element's desired location is not enforced (maybe I will make it optional in the future)
    {
        // Location enforcement
        //SetWorldLocation(UKismetMathLibrary::VLerp(GetComponentLocation(), CachedDesiredLocation, DesiredLocationEnforcement));

        FVector DesiredDelta = CachedDesiredLocation - GetComponentLocation();
        FVector LocalizedDelta = UKismetMathLibrary::Quat_UnrotateVector(GetComponentQuat(), DesiredDelta);

        FVector EnforcementLocalDelta = LocalizedDelta * DesiredLocationEnforcement * LocationEnforcementDirectionalScaling;

        FVector EnforcementDelta = GetComponentQuat().RotateVector(EnforcementLocalDelta);
        
        SetWorldLocation(GetComponentLocation() + EnforcementDelta);

        // Rotation enforcement
        SetWorldRotation(UKismetMathLibrary::RLerp(GetComponentRotation(), CachedDesiredRotation, DesiredRotationEnforcement, true));
    }

    // Calculating Applied Bone Transform
    if (BoneName != NAME_None)
    {
        FTransform BoneTransform = GetComponentTransform();

        BoneTransform.SetRotation(BoneTransform.GetRotation() * AdditionalBoneRotation.Quaternion());

        Handler->SetBoneTransform(BoneName, BoneTransform);
    }
}

// CALLED BY THE HANDLER : Synchronizes Rig Element to the most recently fetched bone transforms
void UMPAS_BodySegment::SyncToFetchedBoneTransforms(float DeltaTime)
{
    const FTransform* FetchedBoneTransform = GetHandler()->GetCachedFetchedBoneTransforms().Find(BoneName);
	if (FetchedBoneTransform)
	{
		FVector DeltaLocation = (*FetchedBoneTransform).GetLocation() - GetComponentLocation();
		FQuat DeltaRotator = UKismetMathLibrary::NormalizedDeltaRotator(((*FetchedBoneTransform).GetRotation() * AdditionalBoneRotation.Quaternion().Inverse()).Rotator(), GetComponentRotation()).Quaternion();

		// Checking if deltas are large enough to consider transform modified
        if (    DeltaLocation.Size() > BoneTransformSync_LocationDeltaSensitivityThreshold
            &&  acos(DeltaRotator.Vector().Dot(FVector::UnitX())) > BoneTransformSync_AngularDeltaSensitivityThreshold)
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

            FVector NewSyncOffset = UKismetMathLibrary::VInterpTo(  CurrentSyncOffset, 
                                                                    BoneTransformSync_AppliedBoneLocationOffset,
                                                                    DeltaTime, BoneTransformSync_OffsetLocationRealocationSpeed);

            FRotator AppliedAngularOffsetRot = BoneTransformSync_AppliedBoneAngularOffset.Rotator();
            FRotator NewSyncAngle = UKismetMathLibrary::RInterpTo(  CurrentSyncAngle,
                                                                    AppliedAngularOffsetRot,
                                                                    DeltaTime, BoneTransformSync_OffsetAngularRealocationSpeed);

            GetVectorStack(0)[BoneTransformSync_LocationLayerID].SetSourceValue(this, NewSyncOffset);
            GetRotatorStack(0)[BoneTransformSync_RotationLayerID].SetSourceValue(this, NewSyncAngle);

            BoneTransformSync_AppliedBoneLocationOffset -= NewSyncOffset - CurrentSyncOffset;
            BoneTransformSync_AppliedBoneAngularOffset = (AppliedAngularOffsetRot - (NewSyncAngle - CurrentSyncAngle)).Quaternion();
        }
	}
}
