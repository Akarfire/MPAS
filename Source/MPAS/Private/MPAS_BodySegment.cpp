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

    // Depricated
    PhysicsElementsConfiguration.Add(FMPAS_PhysicsElementConfiguration());
}

// CALLED BY THE HANDLER : Initializing Rig Element
void UMPAS_BodySegment::InitRigElement(class UMPAS_Handler* InHandler)
{
    Super::InitRigElement(InHandler);

    // Registering desired transform stacks and layers + setiing core offset values

    DesiredLocationStackID = RegisterVectorStack("DesiredLocation");
    RegisterVectorLayer(DesiredLocationStackID, "Core", EMPAS_LayerBlendingMode::Normal, EMPAS_LayerCombinationMode::Add);
    RegisterVectorLayer(DesiredLocationStackID, "OffsetFromCore", EMPAS_LayerBlendingMode::Add, EMPAS_LayerCombinationMode::Add);
    
    SetVectorSourceValue(DesiredLocationStackID, 1, this, InHandler->GetCore()->GetComponentLocation() - GetComponentLocation());


    DesiredRotationStackID = RegisterRotationStack("DesiredRotation");
    RegisterRotationLayer(DesiredRotationStackID, "Core", EMPAS_LayerBlendingMode::Normal);
    RegisterRotationLayer(DesiredRotationStackID, "OffsetFromCore", EMPAS_LayerBlendingMode::Add);

    SetRotationSourceValue(DesiredRotationStackID, 1, this, InHandler->GetCore()->GetComponentRotation() - GetComponentRotation());

}

// CALLED BY THE HANDLER :  Updating Rig Element every tick
void UMPAS_BodySegment::UpdateRigElement(float DeltaTime)
{
    Super::UpdateRigElement(DeltaTime);

    if (BoneName != FName())
        Handler->SetBoneTransform(BoneName, GetComponentTransform());

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
}
