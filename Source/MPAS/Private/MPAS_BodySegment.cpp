// Fill out your copyright notice in the Description page of Project Settings.

#include "MPAS_BodySegment.h"
#include "MPAS_Handler.h"
#include "MPAS_Core.h"
#include "Kismet/KismetMathLibrary.h"


// Constructor
UMPAS_BodySegment::UMPAS_BodySegment()
{
    PhysicsElementsConfiguration.Add(FMPAS_PhysicsElementConfiguration());
}

// CALLED BY THE HANDLER : Initializing Rig Element
void UMPAS_BodySegment::InitRigElement(class UMPAS_Handler* InHandler)
{
    Super::InitRigElement(InHandler);
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
}


// Returns the location, where the body needs to be placed
FVector UMPAS_BodySegment::GetDesiredLocation()
{
    return GetHandler()->GetCore()->GetComponentLocation();
}

// Returns the rotation, by which the body needs to be rotated
FRotator UMPAS_BodySegment::GetDesiredRotation()
{
    return GetHandler()->GetCore()->GetComponentRotation();
}
