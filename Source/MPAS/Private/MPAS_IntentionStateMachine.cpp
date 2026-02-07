// Fill out your copyright notice in the Description page of Project Settings.


#include "IntentionDriving/MPAS_IntentionStateMachine.h"
#include "IntentionDriving/MPAS_IntentionStateBase.h"


void UMPAS_IntentionStateMachine::RigSetupFinished()
{
	OnRigSetupFinished();

	for (auto& State : States)
	{
		UMPAS_IntentionStateBase* IntentionState = Cast<UMPAS_IntentionStateBase>(State.Value);
		if (IntentionState)
			IntentionState->OnRigSetupFinished();
	}
}

// Used to refresh configuration of the state machine and it's states
void UMPAS_IntentionStateMachine::UpdateConfiguration()
{
	OnUpdateConfiguration();

	for (auto& State : States)
	{
		UMPAS_IntentionStateBase* IntentionState = Cast<UMPAS_IntentionStateBase>(State.Value);
		if (IntentionState)
			IntentionState->UpdateConfiguration();
	}
}
