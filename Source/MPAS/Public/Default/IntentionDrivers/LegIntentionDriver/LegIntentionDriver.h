// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "../../../IntentionDriving/MPAS_IntentionStateMachine.h"
#include "../../../IntentionDriving/MPAS_IntentionStateBase.h"
#include "LegIntentionDriver.generated.h"


/**
 * The only state of this intention dirver
 */
UCLASS()
class MPAS_API UMPAS_LegIntentionDriverState : public UMPAS_IntentionStateBase
{
	GENERATED_BODY()
	
	// Body segments and legs corresponding to them
	TMap<class UMPAS_BodySegment*, TArray <class UMPAS_Leg* >> LegsData;

	// Map of leg target location vector layers for each leg
	TMap<class UMPAS_Leg*, int32> TargetLayers;

	// Map of effector shift vector layers for each leg
	TMap<class UMPAS_Leg*, int32> EffectorShiftLayers;

public:

	// Constructor
	UMPAS_LegIntentionDriverState() {}

	// Called when the state is made active
	virtual void EnterState_Implementation() override;
	
	// Called once the active state is changed to a different one
	virtual void ExitState_Implementation() override;

	// Called every state machine update when the state is active
	virtual void UpdateState_Implementation(float DeltaTime) override;


protected:

	/*
	 * Finds a sutable location to place the leg, approaching DesiredPlacementLocation as closely as possible
	 * Returns FVector(0) if no suitable location was found
	 */
	FVector PlaceLegTargetLocation(const FVector& DesiredPlacementLocation);

	/*
	 * Calculates effector shift for each leg, described by Limitations (Size of Limitations = number of legs that will be processed)
	 * Puts the result into OutShift (it MUST already have the required size)
	 */
	void CalculateEffectorShift(TArray<FVector>& OutShift, 
								const FVector& InTarget, 
								const TArray<TPair<FVector, FVector>>& InLimitations,
								const FQuat& LimitSpaceRotation);

	/* 
	 * Clamps effector shift to it's limitations
	 * OutClamped contains data, whether each individual offset was clamped
	 */
	void ClampShift(TArray<FVector>& InOutShift, TArray<bool>& OutClamped, const TArray<TPair<FVector, FVector>>& InLimitations, const FQuat& LimitSpaceRotation);

	/*
	 * Redistributes shift from leg's that hit there limitation to the ones that are still capable of moving 
	 * Updates the values in the InOutShift
	 */
	void ShiftRedistributionIteration(TArray<FVector>& InOutShift, const FVector& InTarget, const TArray<bool>& InClamped);
};


/**
 * 
 */
UCLASS()
class MPAS_API UMPAS_LegIntentionDriver : public UMPAS_IntentionStateMachine
{
	GENERATED_BODY()
	
public:

	UMPAS_LegIntentionDriver() {}


	// Called when the state machine is initialized (after the main init)
	virtual void OnInitStateMachine_Implementation() override
	{
		// Registering State
		AddNewState( 1, UMPAS_LegIntentionDriverState::StaticClass() );
	}

	// CALLED BY THE HANDLER: Called once the rig has finished it's Scanning, Initing and Linking processes
	virtual void OnRigSetupFinished_Implementation() override
	{
		// Setting State
		// Doing it here, because the rig needs to already be prossed by the time the State is entered 
		ForceCallStateTransition(1);
	}

	// Called every time the state machine is updated (after the main update)
	virtual void OnUpdateStateMachine_Implementation(float DeltaTime) override  {}
};
