// Fill out your copyright notice in the Description page of Project Settings.


#include "Default/RigElements/PositionDrivers/MPAS_JointPositionDriver.h"


// VIRTUAL, Called right after the position driver has gathered data about it's driven elements
void UMPAS_JointPositionDriver::OnPositionDriverInitialized_Implementation()
{
	// Initializing joint data for all of the driven elements
	for (auto Element : DrivenElements)
	{
		FMPAS_JointPositionDriverElementData Joint;

		Joint.Distance = FVector::Distance(GetComponentLocation(), Element.Key->GetComponentLocation());
		Joint.Rotation = (Element.Key->GetComponentLocation() - GetComponentLocation()).Rotation() - GetComponentRotation();
		Joint.DefaultElementRotation = Element.Key->GetComponentRotation();

		JointsData.Add(Element.Key, Joint);
	}
}

// VIRTUAL, Calculates the required transform (location and rotation) for the specified element, 
// If the specified element is not one of the affected by this Position Driver, (0, 0, 0)-s are returned
void UMPAS_JointPositionDriver::CalculateElementTransform_Implementation(FVector& OutLocation, FRotator& OutRotation, UMPAS_RigElement* InRigElement)
{
	OutLocation = JointsData[InRigElement].Rotation.RotateVector(GetForwardVector())* JointsData[InRigElement].Distance;
	OutRotation = JointsData[InRigElement].DefaultElementRotation + JointsData[InRigElement].Rotation;
}



// INTENTION DRIVING INTEGRATION

// Outputs Distance and Rotation of the joint for the specified attached element
void UMPAS_JointPositionDriver::GetJointState(float& OutDistance, FRotator& OutRotation, int32 InElementIndex)
{
	FMPAS_JointPositionDriverElementData State = JointsData[DrivenElementsList[InElementIndex]];

	OutDistance = State.Distance;
	OutRotation = State.Rotation;
}

// Updates Distance and Rotation of the joint for the specified attached element
void UMPAS_JointPositionDriver::SetJointState(int32 InElementIndex, float InDistance, const FRotator& InRotation)
{
	JointsData[DrivenElementsList[InElementIndex]].Distance = InDistance;
	JointsData[DrivenElementsList[InElementIndex]].Rotation = InRotation;
}
