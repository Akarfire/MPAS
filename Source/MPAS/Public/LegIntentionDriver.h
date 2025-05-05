// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "MPAS_IntentionStateMachine.h"
#include "MPAS_IntentionStateBase.h"
#include "LegIntentionDriver.generated.h"


/**
 * The only state of this intention dirver
 */
UCLASS()
class SCARLETSTATEMACHINES_API UMPAS_LegIntentionDriverState : public UMPAS_IntentionStateBase
{
	GENERATED_BODY()
	
	// Body segments and legs corresponding to them
	TMap<UMPAS_BodySegment*, TArray<UMPAS_Leg*>> LegsData;


public:

	// Constructor
	USSM_CodeExampleStateThree() {}

	// Called when the state is made active
	virtual void EnterState_Implementation() override;

	// Called once the active state is changed to a different one
	virtual void ExitState_Implementation() override;

	// Called every state machine update when the state is active
	virtual void UpdateState_Implementation(float DeltaTime) override;
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

		// Set Default State
		ForceCallStateTransition(1);
	}

	// Called every time the state machine is updated (after the main update)
	virtual void OnUpdateStateMachine_Implementation(float DeltaTime) override {}
};
