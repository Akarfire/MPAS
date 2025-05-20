// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "SSM_StateBase.h"
#include "MPAS_IntentionStateBase.generated.h"

/**
 * 
 */
UCLASS()
class MPAS_API UMPAS_IntentionStateBase : public USSM_StateBase
{
	GENERATED_BODY()
	
	// Pointer to the Handler that is associated with the State Machine
	class UMPAS_Handler* Handler;

public:

	UMPAS_IntentionStateBase() {}

	// Returns the pointer to the Handler that is associated with the State Machine
	UFUNCTION(BlueprintCallable, BlueprintPure, Category= "MPAS|Handler|IntentionDriver|IntentionStateMachine|State")
	class UMPAS_Handler* GetHandler() { return Handler; }

	// Must NEVER be called manually. Fires off after the main STATEMACHINE_OnSetStateMachine function has be called. Is used for implemention custom State base classes, derived from this one
	virtual void STATEMACHINE_OnSetStateMachine_Implementation() override;
};
