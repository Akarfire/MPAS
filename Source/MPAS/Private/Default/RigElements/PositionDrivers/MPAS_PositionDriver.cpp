// Fill out your copyright notice in the Description page of Project Settings.


#include "Default/RigElements/PositionDrivers/MPAS_PositionDriver.h"
#include "MPAS_Handler.h"


// DATA GETTERS

// Returns an array, containing pointers to all of the rig elements that are affected by this position driver
TArray<UMPAS_RigElement*> UMPAS_PositionDriver::GetDrivenElementsList()
{
	TArray<UMPAS_RigElement*> OutElements;
	DrivenElements.GetKeys(OutElements);

	return OutElements;
}


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
		int32 LocationStackID = Child->GetVectorStackID(Child->PositionDriverIntegration_LocationStackName);

		// Creating it if failed to find
		if (LocationStackID == -1)
		{
			LocationStackID = Child->RegisterVectorStack(PositionDriverIntegration_LocationStackName);
		}

		// Creating new layer in the specified stack
		int32 LocationLayerID = Child->RegisterVectorLayer(LocationStackID, "PositionDriverIntegration", EMPAS_LayerBlendingMode::Normal, EMPAS_LayerCombinationMode::Add, false);


		// Accessing rotation stack
		int32 RotationStackID = Child->GetRotationStackID(Child->PositionDriverIntegration_RotationStackName);

		// Creating it if failed to find
		if (RotationStackID == -1)
		{
			RotationStackID = Child->RegisterRotationStack(PositionDriverIntegration_RotationStackName);
		}

		// Creating new layer in the specified stack
		int32 RotationLayerID = Child->RegisterRotationLayer(LocationStackID, "PositionDriverIntegration", EMPAS_LayerBlendingMode::Normal, false);


		// Writing Driven Element Data
		DrivenElements.Add(Child, FMPAS_PositionDrivenElementData(Child, LocationStackID, LocationLayerID, RotationStackID, RotationLayerID));
	}

	OnPositionDriverInitialized();
}

// CALLED BY THE HANDLER :  Called every tick the element gets updated by the handler - to be overriden in Blueprints
void UMPAS_PositionDriver::UpdateRigElement(float DeltaTime)
{
	Super::UpdateRigElement(DeltaTime);

	// Calculating transforms for each Driven Element and putting them into specified stacks & layers
	for (auto& DrivenElementEntry : DrivenElements)
	{
		FVector RequiredLocation;
		FRotator RequiredRotation;

		CalculateElementTransform(RequiredLocation, RequiredRotation, DrivenElementEntry.Key);

		DrivenElementEntry.Key->SetVectorSourceValue(DrivenElementEntry.Value.LocationStackID, DrivenElementEntry.Value.LocationLayerID, this, RequiredLocation);
		DrivenElementEntry.Key->SetRotationSourceValue(DrivenElementEntry.Value.RotationStackID, DrivenElementEntry.Value.RotationLayerID, this, RequiredRotation);
	}
}
