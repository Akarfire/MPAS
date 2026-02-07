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

	// Used to refresh configuration of the state machine and it's states (to be overriden)
	UFUNCTION(BlueprintNativeEvent, Category = "MPAS|Handler|IntentionDriver|IntentionStateMachine|State")
	void OnUpdateConfiguration();
	virtual void OnUpdateConfiguration_Implementation() {}

	// CALLED BY THE HANDLER: Called once the rig has finished it's Scanning, Initing and Linking processes
	UFUNCTION(BlueprintNativeEvent, Category = "MPAS|Handler|IntentionDriver|IntentionStateMachine|State")
	void OnRigSetupFinished();
	virtual void OnRigSetupFinished_Implementation() {}


	// Returns the pointer to the Handler that is associated with the State Machine
	UFUNCTION(BlueprintCallable, BlueprintPure, Category= "MPAS|Handler|IntentionDriver|IntentionStateMachine|State")
	class UMPAS_Handler* GetHandler() { return Handler; }

	// Used to refresh configuration of the state machine and it's states
	UFUNCTION(BlueprintCallable, Category = "MPAS|Handler|IntentionDriver|IntentionStateMachine|State")
	void UpdateConfiguration();

	// Must NEVER be called manually. Fires off after the main STATEMACHINE_OnSetStateMachine function has been called. Is used for implementing custom State base classes, derived from this one
	virtual void STATEMACHINE_OnSetStateMachine_Implementation() override;
};
