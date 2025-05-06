// Fill out your copyright notice in the Description page of Project Settings.


#include "LegIntentionDriver.h"
#include "MPAS_Handler.h"
#include "MPAS_RigElement.h"
#include "MPAS_Leg.h"
#include "MPAS_BodySegment.h"

// Called when the state is made active
void UMPAS_LegIntentionDriverState::EnterState_Implementation()
{
	// Scanning the rig to find legs and their parent body segments
	// This intention driver will only control leg's that are attached to body segments (ignoring visual elements like limbs)
	for (auto& RigElementDataPair : GetHandler()->GetRigData())
	{
		FMPAS_RigElementData RigElementData = RigElementDataPair.Value;

		// Ignore elements that are directly attached to the core
		if (RigElementData.ParentComponent == "Core") continue;

		auto Leg = Cast<UMPAS_Leg>(RigElementData.RigElement);
		auto ParentBody = Cast<UMPAS_BodySegment>(GetHandler()->GetRigData()[RigElementData.ParentComponent].RigElement);

		if (Leg && ParentBody)
		{
			if (LegsData.Contains(ParentBody)) LegsData[ParentBody].Add(Leg);
			else LegsData.Add(ParentBody, TArray<UMPAS_Leg*> { Leg });
		}
	}
}


// Called once the active state is changed to a different one
void UMPAS_LegIntentionDriverState::ExitState_Implementation() {}


// Called every state machine update when the state is active
void UMPAS_LegIntentionDriverState::UpdateState_Implementation(float DeltaTime)
{
	// Calculating target locations and offsets for all of the legs, based on the desired transform of the corresponding body segment

	for (auto& BodyLegs : LegsData)
	{
		UMPAS_BodySegment* Body = BodyLegs.Key;

		// Fetching desired transform from the body segment
		FVector DesiredLocation = Body->GetDesiredLocation();
		FRotator DesiredRotation = Body->GetDesiredRotation();

		// Initial Leg Placement + Average location calculation
		int32 NumberOfLegs = 0;
		FVector LegAverageLocation = FVector(0);
		TArray<UMPAS_Leg*> ActiveLegs;

		// Horizontal Shift limitations for each active leg <Min, Max>
		TArray<TPair<FVector, FVector>> ShiftLimitations;

		for (auto& Leg : BodyLegs.Value)
		{
			FVector TraceLocation = DesiredRotation.RotateVector(Leg->GetLegTargetOffset()) + DesiredLocation;
			FVector PlacementLocation = PlaceLegTargetLocation(Leg->FootTrace(TraceLocation));

			// If we failed to place the leg, we deactivate it and continue to the next leg
			if (PlacementLocation == FVector(0)) { Leg->SetActive(false); continue; }

			// If leg's placement was succesful we fetch it's data and mark it as an active leg

			// Setting leg's target location
			Leg->LegTargetLocation = PlacementLocation;

			// Marking leg as active
			ActiveLegs.Add(Leg);

			// Average calculation steps
			NumberOfLegs++;
			LegAverageLocation += PlacementLocation;

			// Fetching limitations
			ShiftLimitations.Add(TPair<FVector, FVector>(Leg->EffectorShift_Min, Leg->EffectorShift_Max));
		}
		
		// Completing average calculation
		LegAverageLocation /= NumberOfLegs;

		// Calculating each leg's effector shift
	}
}


/*
* Finds a sutable location to place the leg, approaching DesiredPlacementLocation as closely as possible
* Returns FVector(0) if no suitable location was found
*/
FVector UMPAS_LegIntentionDriverState::PlaceLegTargetLocation(const FVector& DesiredPlacementLocation)
{
	return DesiredPlacementLocation;
}
