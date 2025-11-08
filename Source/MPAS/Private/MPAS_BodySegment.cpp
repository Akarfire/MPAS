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
    RegisterVectorLayer(DesiredLocationStackID, "Core", EMPAS_LayerBlendingMode::Normal, EMPAS_LayerCombinationMode::Add);
    RegisterVectorLayer(DesiredLocationStackID, "OffsetFromCore", EMPAS_LayerBlendingMode::Add, EMPAS_LayerCombinationMode::Add);
    
    SetVectorSourceValue(DesiredLocationStackID, 1, this, GetComponentLocation() - InHandler->GetCore()->GetComponentLocation());


    DesiredRotationStackID = RegisterRotationStack("DesiredRotation");
    RegisterRotationLayer(DesiredRotationStackID, "Core", EMPAS_LayerBlendingMode::Normal);
    RegisterRotationLayer(DesiredRotationStackID, "OffsetFromCore", EMPAS_LayerBlendingMode::Add);

    SetRotationSourceValue(DesiredRotationStackID, 1, this, GetComponentRotation() - InHandler->GetCore()->GetComponentRotation());

    // Bone Transform Sync
    BoneTransformSync_LocationLayerID = RegisterVectorLayer(0, "BoneTransformSync", EMPAS_LayerBlendingMode::Add, EMPAS_LayerCombinationMode::Add, 1.f, BoneTransformSyncingLayerPriority);
    BoneTransformSync_RotationLayerID = RegisterRotationLayer(0, "BoneTransformSync", EMPAS_LayerBlendingMode::Add, 1.f, BoneTransformSyncingLayerPriority);
}

// CALLED BY THE HANDLER :  Updating Rig Element every tick
void UMPAS_BodySegment::UpdateRigElement(float DeltaTime)
{
    Super::UpdateRigElement(DeltaTime);

    if (BoneName != FName())
    {
        FTransform BoneTransform = GetComponentTransform();

        BoneTransform.SetRotation(BoneTransform.GetRotation() * AdditionalBoneRotation.Quaternion());

        Handler->SetBoneTransform(BoneName, BoneTransform);
    }

    if (UseCoreRotation)
    {
        FRotator CurrentSelfRotation = GetRotationSourceValue(0, 1, this);
        FRotator NewRotation = UKismetMathLibrary::RInterpTo_Constant(CurrentSelfRotation, GetHandler()->GetCore()->GetComponentRotation(), DeltaTime, LiniarRotationInterpolationSpeed);

        SetRotationSourceValue(0, 1, this, NewRotation);
    }

    // Updating desired transform stacks

    // Core transform update
    SetVectorSourceValue(DesiredLocationStackID, 0, this, GetHandler()->GetCore()->GetComponentLocation());
    SetRotationSourceValue(DesiredRotationStackID, 0, this, GetHandler()->GetCore()->GetComponentRotation());

    // Caching desired transform
    CachedDesiredLocation = CalculateVectorStackValue(DesiredLocationStackID);
    CachedDesiredRotation = CalculateRotationStackValue(DesiredRotationStackID);

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
}

// CALLED BY THE HANDLER : Synchronizes Rig Element to the most recently fetched bone transforms
void UMPAS_BodySegment::SyncToFetchedBoneTransforms()
{
    const FTransform* FetchedBoneTransform = GetHandler()->GetCachedFetchedBoneTransforms().Find(BoneName);
	if (FetchedBoneTransform)
	{
		FVector DeltaLocation = (*FetchedBoneTransform).GetLocation() - GetComponentLocation();
		FRotator DeltaRotator = UKismetMathLibrary::NormalizedDeltaRotator(((*FetchedBoneTransform).GetRotation() * AdditionalBoneRotation.Quaternion().Inverse()).Rotator(), GetComponentRotation());

		SetVectorSourceValue(0, BoneTransformSync_LocationLayerID, this, DeltaLocation);
		SetRotationSourceValue(0, BoneTransformSync_RotationLayerID, this, DeltaRotator);
	}
}
