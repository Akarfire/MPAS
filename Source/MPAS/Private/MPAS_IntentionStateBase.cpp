// Fill out your copyright notice in the Description page of Project Settings.


#include "IntentionDriving/MPAS_IntentionStateBase.h"
#include "MPAS_Handler.h"
#include "IntentionDriving/MPAS_IntentionStateMachine.h"

void UMPAS_IntentionStateBase::STATEMACHINE_OnSetStateMachine_Implementation()
{
    // Attempting to find the associated MPAS_Handler

    UMPAS_IntentionStateMachine* IntentionSM = Cast<UMPAS_IntentionStateMachine>(GetStateMachine());
    if (IntentionSM)
    {
        Handler = IntentionSM->GetHandler();
    }
}