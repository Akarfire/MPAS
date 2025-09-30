// Fill out your copyright notice in the Description page of Project Settings.


#include "Default/IntentionDrivers/LegIntentionDriver/LegIntentionDriver.h"
#include "MPAS_Handler.h"
#include "MPAS_RigElement.h"
#include "Default/RigElements/MPAS_Leg.h"
#include "Default/RigElements/MPAS_BodySegment.h"
#include "Kismet/KismetMathLibrary.h"

// Called when the state is made active
void UMPAS_LegIntentionDriverState::EnterState_Implementation()
{
	LegsData.Empty();
	TargetLayers.Empty();

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

			TargetLayers.Add(Leg, Leg->RegisterVectorLayer(Leg->GetTargetLocationStackID(), "IntentionDrivenLegTarget", EMPAS_LayerBlendingMode::Normal, EMPAS_LayerCombinationMode::Add, 1.f, 1, true));
			EffectorShiftLayers.Add(Leg, Leg->RegisterVectorLayer(Leg->GetEffectorShiftStackID(), "IntentionDrivenEffectorShift", EMPAS_LayerBlendingMode::Normal, EMPAS_LayerCombinationMode::Add, 1.f, 1, true));
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
			Leg->SetVectorSourceValue(Leg->GetTargetLocationStackID(), TargetLayers[Leg], Leg, PlacementLocation);

			// Marking leg as active
			ActiveLegs.Add(Leg);

			// Average calculation steps
			NumberOfLegs++;
			LegAverageLocation += Leg->GetComponentLocation();

			// Fetching limitations
			ShiftLimitations.Add(TPair<FVector, FVector>(Leg->EffectorShift_Min, Leg->EffectorShift_Max));
		}

		// Completing average calculation
		LegAverageLocation /= NumberOfLegs;

		// Calculating each leg's effector shift

		// Initializing shift array
		TArray<FVector> Shift;
		Shift.Init(FVector(0), NumberOfLegs);

		// Calculating target value
		FVector Target = DesiredLocation - LegAverageLocation;

		CalculateEffectorShift(Shift, Target, ShiftLimitations, Body->GetComponentQuat());

		// Setting leg shift with interpolation
		for (int i = 0; i < ActiveLegs.Num(); i++)
			ActiveLegs[i]->SetVectorSourceValue(ActiveLegs[i]->GetEffectorShiftStackID(), EffectorShiftLayers[ActiveLegs[i]], ActiveLegs[i], Shift[i]);
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


/*
* Calculates effector shift for each leg, described by Limitations (Size of Limitations = number of legs that will be processed)
* Puts the result into OutShift (it MUST already have the required size)
*/
void UMPAS_LegIntentionDriverState::CalculateEffectorShift(TArray<FVector>& OutShift, const FVector& InTarget, const TArray<TPair<FVector, FVector>>& InLimitations, const FQuat& LimitSpaceRotation)
{
	// Assigning target shift value to each leg
	for (int i = 0; i < OutShift.Num(); i++) OutShift[i] = InTarget;

	// Initializing clamp cache (stores whether each leg was clamped with most recent clamp call)
	TArray<bool> ClampCache;
	ClampCache.Init(false, OutShift.Num());

	// Initial clamp
	ClampShift(OutShift, ClampCache, InLimitations, LimitSpaceRotation);

	// Redistribution iterations
	for (int i = 1; i < OutShift.Num(); i++)
	{
		ShiftRedistributionIteration(OutShift, InTarget, ClampCache);
		ClampShift(OutShift, ClampCache, InLimitations, LimitSpaceRotation);
	}
}

/*
* Clamps effector shift to it's limitations
* OutClamped contains data, whether each individual offset was clamped
*/
void UMPAS_LegIntentionDriverState::ClampShift(TArray<FVector>& InOutShift, TArray<bool>& OutClamped, const TArray<TPair<FVector, FVector>>& InLimitations, const FQuat& LimitSpaceRotation)
{
	for (int i = 0; i < InOutShift.Num(); i++)
	{
		FVector PreviousShift = InOutShift[i];

		FVector LocalizedShift = UKismetMathLibrary::Quat_UnrotateVector(LimitSpaceRotation, InOutShift[i]);
		FVector LimitedLocalizedShift = ClampVector(LocalizedShift, InLimitations[i].Key, InLimitations[i].Value);

		InOutShift[i] = LimitSpaceRotation.RotateVector(LimitedLocalizedShift);

		OutClamped[i] = !UKismetMathLibrary::EqualEqual_VectorVector(InOutShift[i], PreviousShift);
	}
}

/*
* Redistributes shift from leg's that hit there limitation to the ones that are still capable of moving
* Updates the values in the InOutShift
*/
void UMPAS_LegIntentionDriverState::ShiftRedistributionIteration(TArray<FVector>& InOutShift, const FVector& InTarget, const TArray<bool>& InClamped)
{
	for (int i = 0; i < InOutShift.Num(); i++)
	{
		// Do not change clamped values
		if (InClamped[i]) continue;

		int NumberOfNonClamped = 0;
		FVector Sum = FVector(0);

		for (int j = 0; j < InOutShift.Num(); j++)
		{
			NumberOfNonClamped += (int)(!InClamped[j]);
			Sum += InTarget - InOutShift[j];
		}

		InOutShift[i] += (Sum / NumberOfNonClamped);
	}
}
