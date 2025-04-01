// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "SSM_StateMachine.h"
#include "MPAS_IntentionStateMachine.generated.h"

/**
 * 
 */
UCLASS()
class MPAS_API UMPAS_IntentionStateMachine : public USSM_StateMachine
{
	GENERATED_BODY()

protected:

	// Pointer to the associated handler
	class UMPAS_Handler* Handler;

	// Name of the State Machine in the Intention Driver
	FName Name = TEXT("IntentionStateMachine");

	// Flag, marking whether the handler should update this state machine, no actual effect on the State Machine
	bool Active = true;

public:

	// Returns the pointer to the associated MPAS handler
	UFUNCTION(BlueprintCallable, BlueprintPure, Category = "MPAS|Handler|IntentionDriver|IntentionStateMachine")
	class UMPAS_Handler* GetHandler() { return Handler; }

	// Returns the name of the State Machine in the Intention Driver
	UFUNCTION(BlueprintCallable, BlueprintPure, Category = "MPAS|Handler|IntentionDriver|IntentionStateMachine")
	FName GetName() { return Name; }

	// Changes the Active value - a flag, marking whether the handler should update this state machine, no actual effect on the State Machine
	UFUNCTION(BlueprintCallable, Category = "MPAS|Handler|IntentionDriver|IntentionStateMachine")
	void SetActive(bool NewActive) { Active = NewActive; }

	// Returns the Active value - a flag, marking whether the handler should update this state machine, no actual effect on the State Machine
	UFUNCTION(BlueprintCallable, BlueprintPure, Category = "MPAS|Handler|IntentionDriver|IntentionStateMachine")
	bool IsActive() { return Active; }

	// CALLED BY THE HANDLER: Links the state machine to the Handler
	void LinkToHandler(class UMPAS_Handler* InHandler, FName InName) { Handler = InHandler; Name = InName; }

};
