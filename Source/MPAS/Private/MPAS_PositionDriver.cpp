// Fill out your copyright notice in the Description page of Project Settings.


#include "Default/RigElements/PositionDrivers/MPAS_PositionDriver.h"
#include "MPAS_Handler.h"
#include "Default/MPAS_Core.h"
#include "Kismet/KismetMathLibrary.h"


// CALLED BY THE HANDLER

// CALLED BY THE HANDLER :  Called when the rig is initialized by the handler - to be overriden in Blueprints
void UMPAS_PositionDriver::InitRigElement(UMPAS_Handler* InHandler)
{
	Super::InitRigElement(InHandler);
}

// CALLED BY THE HANDLER :  Contains the logic that links this element with other elements in the rig - to be overriden in Blueprints
void UMPAS_PositionDriver::LinkRigElement(UMPAS_Handler* InHandler)
{
	Super::LinkRigElement(InHandler);

	// Gathering Driven Elements

	auto& ChildElements = GetHandler()->GetRigData()[RigElementName].ChildElements;

	for (FName ChildName : ChildElements)
	{
		UMPAS_RigElement* Child = GetHandler()->GetRigData()[ChildName].RigElement;

		// Accessing vector stack
		int32 LocationStackID = Child->GetVectorStackID(FName(Child->PositionDriverIntegration_LocationStackName));

		// Creating it if failed to find
		if (LocationStackID == -1)
		{
			LocationStackID = Child->RegisterVectorStack(FName(PositionDriverIntegration_LocationStackName));
		}

		// Creating new layer in the specified stack
		int32 LocationLayerID = Child->RegisterVectorLayer(LocationStackID, FMPAS_VectorLayer(EMPAS_LayerBlendingMode::Normal, 1.f, EMPAS_LayerCombinationMode::Add, 0, false, "PositionDriverIntegration"));
		
		// Accessing rotation stack
		int32 RotationStackID = Child->GetRotatorStackID(FName(Child->PositionDriverIntegration_RotationStackName));

		// Creating it if failed to find
		if (RotationStackID == -1)
		{
			RotationStackID = Child->RegisterRotatorStack(FName(PositionDriverIntegration_RotationStackName));
		}

		// Creating new layer in the specified stack
		int32 RotationLayerID = Child->RegisterRotatorLayer(LocationStackID, FMPAS_RotatorLayer(EMPAS_LayerBlendingMode::Normal, 1.f, EMPAS_LayerCombinationMode::Add, 0, false, "PositionDriverIntegration"));


		// Writing Driven Element Data
		DrivenElements.Add(Child, FMPAS_PositionDrivenElementData(Child, LocationStackID, LocationLayerID, RotationStackID, RotationLayerID));
	
		// Caching element into the list
		DrivenElementsList.Add(Child);
	}

	// If this position driver is a core element, then it hard attaches to the core as if it was it's parent
	
	if (IsCoreElement)
	{
		// Core location and rotation initial cache
		GetVectorStack(0)[0].SetSourceValue(this, GetHandler()->GetCore()->GetComponentLocation());
		GetRotatorStack(0)[0].SetSourceValue(this, GetHandler()->GetCore()->GetComponentRotation());

		// Self location and rotation fetching

		// Setting initial location / rotation
		InitialSelfTransform.SetLocation(UKismetMathLibrary::Quat_UnrotateVector(GetHandler()->GetCore()->GetComponentRotation().Quaternion(), GetComponentLocation() - GetHandler()->GetCore()->GetComponentLocation()));
		InitialSelfTransform.SetRotation(UKismetMathLibrary::NormalizedDeltaRotator(GetComponentRotation(), GetHandler()->GetCore()->GetComponentRotation()).Quaternion());

		GetVectorStack(0)[1].SetSourceValue(this, UKismetMathLibrary::Quat_RotateVector(GetHandler()->GetCore()->GetComponentRotation().Quaternion(), InitialSelfTransform.GetLocation()));
		GetRotatorStack(0)[1].SetSourceValue(this, FRotator(InitialSelfTransform.GetRotation()));
	}

	OnPositionDriverInitialized();
}

// CALLED BY THE HANDLER :  Called every tick the element gets updated by the handler - to be overriden in Blueprints
void UMPAS_PositionDriver::UpdateRigElement(float DeltaTime)
{

	// Updating core transform in case this is a core element
	if (IsCoreElement)
	{
		// Sampling core transform
		GetVectorStack(0)[0].SetSourceValue(this, GetHandler()->GetCore()->GetComponentLocation());
		GetRotatorStack(0)[0].SetSourceValue(this, GetHandler()->GetCore()->GetComponentRotation());

		// Updating self location based on new core rotation
		GetVectorStack(0)[1].SetSourceValue(this, UKismetMathLibrary::Quat_RotateVector(GetHandler()->GetCore()->GetComponentRotation().Quaternion(), InitialSelfTransform.GetLocation()));
	}

	Super::UpdateRigElement(DeltaTime);

	// Calculating transforms for each Driven Element and putting them into specified stacks & layers
	for (auto& DrivenElementEntry : DrivenElements)
	{
		FVector RequiredLocation;
		FRotator RequiredRotation;

		CalculateElementTransform(RequiredLocation, RequiredRotation, DrivenElementEntry.Key);

		DrivenElementEntry.Key->GetVectorStack(DrivenElementEntry.Value.LocationStackID)[DrivenElementEntry.Value.LocationLayerID].SetSourceValue(this, RequiredLocation);
		DrivenElementEntry.Key->GetRotatorStack(DrivenElementEntry.Value.RotationStackID)[DrivenElementEntry.Value.RotationLayerID].SetSourceValue(this, RequiredRotation);
	}
}
