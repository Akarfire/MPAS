// Fill out your copyright notice in the Description page of Project Settings.


#include "MPAS_PhysicsModel.h"
#include "MPAS_RigElement.h"

MPAS_PhysicsModel::MPAS_PhysicsModel()
{
}

MPAS_PhysicsModel::~MPAS_PhysicsModel()
{
}


UMPAS_PhysicsModelElement::UMPAS_PhysicsModelElement()
{
    SetCollisionProfileName(FName("PhysicsActor"));
    SetHiddenInGame(true);
}

// Sets the postion and rotation of this physics element to the one desired by the associated rig element
void UMPAS_PhysicsModelElement::FetchElementPositionFromRigElement()
{
    FTransform DesiredTransform = RigElement->GetDesiredPhysicsElementTransform(PhysicsElementIndex);

    SetWorldLocation(DesiredTransform.GetLocation() + RelativeTransform.GetLocation());
    SetWorldRotation(DesiredTransform.GetRotation());
    SetWorldScale3D(DesiredTransform.GetScale3D() * RelativeTransform.GetScale3D());
}

// CALLED BY THE HANDLER : initializes physics model element based on data from the corresponding rig element
void UMPAS_PhysicsModelElement::InitPhysicsElement(UMPAS_RigElement* InRigElement, int32 InPhysicsElementIndex)
{
    RigElement = InRigElement;
    PhysicsElementIndex = InPhysicsElementIndex;

    if (RigElement)
    {
        SetStaticMesh(RigElement->PhysicsElementsConfiguration[PhysicsElementIndex].PhysicsMesh);
        SetMassOverrideInKg(NAME_None, RigElement->PhysicsElementsConfiguration[PhysicsElementIndex].Mass);

        SetLinearDamping(RigElement->PhysicsElementsConfiguration[PhysicsElementIndex].AirDrag * 0.01f);
        SetAngularDamping(RigElement->PhysicsElementsConfiguration[PhysicsElementIndex].AirDrag * 0.01f);

        TargetPositionStackID = RigElement->PhysicsElementsConfiguration[PhysicsElementIndex].PositionStackID;
        TargetRotationStackID = RigElement->PhysicsElementsConfiguration[PhysicsElementIndex].RotationStackID;

        RelativeTransform = RigElement->PhysicsElementsConfiguration[PhysicsElementIndex].PhysicsMeshRelativeTransform;

        ParentAttachmentType = RigElement->PhysicsElementsConfiguration[PhysicsElementIndex].ParentPhysicalAttachmentType;
    }

    SetCollisionEnabled(ECollisionEnabled::NoCollision);
    
    FetchElementPositionFromRigElement();
}

// CALLED BY THE RIG ELEMENT: called when physics model is enabled for the corresponding rig element
void UMPAS_PhysicsModelElement::OnPhysicsModelEnabled()
{
    SetCollisionEnabled(ECollisionEnabled::QueryAndPhysics);

    if (ParentAttachmentType != EMPAS_PhysicsModelAttachmentType::Hard)
        TriggerPhysicsNextUpdate = true;

    FetchElementPositionFromRigElement();
    SetPhysicsLinearVelocity(RigElement->GetDesiredPhysicsElementVelocity(PhysicsElementIndex));

    RigElement->SetVectorSourceValue(TargetPositionStackID, 0, RigElement, GetComponentLocation() - RelativeTransform.GetLocation());
    RigElement->SetRotationSourceValue(TargetRotationStackID, 0, RigElement, GetComponentRotation());
}

// CALLED BY THE RIG ELEMENT: called when physics model is disabled for the corresponding rig element
void UMPAS_PhysicsModelElement::OnPhysicsModelDisabled()
{
    if (ParentAttachmentType != EMPAS_PhysicsModelAttachmentType::Hard)
        SetSimulatePhysics(false);

    SetCollisionEnabled(ECollisionEnabled::NoCollision);

    RigElement->SetWorldLocation(GetComponentLocation() - RelativeTransform.GetLocation());
    RigElement->SetWorldRotation(GetComponentRotation());
}

// CALLED BY THE RIG ELEMENT: called when this exact element does not have physics model enabled but has a child that does
// Basically, it just enables collision, without enabling physics simulation
void UMPAS_PhysicsModelElement::OnPhysicsModelSemiEnabled()
{
    SetCollisionEnabled(ECollisionEnabled::QueryAndPhysics);
}

// CALLED BY THE HANDLER : updates the physics element, sends data to the coresseponding rig element. Called every handler update when the physics model is active
void UMPAS_PhysicsModelElement::UpdatePhysicsElement(float DeltaTime)
{
    if (RigElement && RigElement->GetPhysicsModelEnabled())
    {
        RigElement->SetVectorSourceValue(TargetPositionStackID, 0, RigElement, GetComponentLocation() - RelativeTransform.GetLocation());
        RigElement->SetRotationSourceValue(TargetRotationStackID, 0, RigElement, GetComponentRotation());
    }

    if (TriggerPhysicsNextUpdate)
    {
        SetSimulatePhysics(true);
        TriggerPhysicsNextUpdate = false;
    }
}