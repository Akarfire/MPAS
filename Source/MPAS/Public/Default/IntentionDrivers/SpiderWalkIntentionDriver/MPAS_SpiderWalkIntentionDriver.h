// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "../../../IntentionDriving/MPAS_IntentionStateMachine.h"
#include "../../../IntentionDriving/MPAS_IntentionStateBase.h"
#include "../../../MPAS_Handler.h"
#include "../../../Default/MPAS_Core.h"
#include "../../../MPAS_RigElement.h"

#include "Kismet/KismetMathLibrary.h"

#include "MPAS_SpiderWalkIntentionDriver.generated.h"


// ENUM
UENUM(BlueprintType)
enum class EMPAS_SpiderWalkID_State : uint8
{
	None UMETA(DisplayName = "None"),
	Idle UMETA(DisplayName = "Idle"),
	Turning UMETA(DisplayName = "Turning"),
	Walking UMETA(DisplayName = "Walking")
};


// STATES

/**
 * Idle state of the spider walk driver
 * Waits half a second before reducing step distance to allow for microcorrections of the pose
 */
UCLASS()
class MPAS_API UMPAS_SpiderWalk_Idle_State : public UMPAS_IntentionStateBase
{
	GENERATED_BODY()

	// Name of the timer, created by this state
	FName IdleTimerName = "SpiderWalkIdleStateRestingTimer";

public:

	// How long it takes the spider to enter the resting location
	float IdleTimerDuration = 0.5f;

	// Maximum core turn speed
	float MaxCoreTurnSpeed = 70.f;

	// Changes the distance, at which legs make a decision to make a step
	float StepTriggerDistanceMultiplier = 0.15f;

public:

	// Constructor
	UMPAS_SpiderWalk_Idle_State() {}

	// Called when the state is made active
	virtual void EnterState_Implementation() override
	{
		if (GetHandler()->TimerController->CreateTimer(IdleTimerName, IdleTimerDuration, false, false))
			GetHandler()->TimerController->SubscribeToTimer(IdleTimerName, this, "OnIdleTimerFinished");

		GetHandler()->TimerController->ResetTimer(IdleTimerName);
		GetHandler()->TimerController->StartTimer(IdleTimerName);
	}

	// Called once the active state is changed to a different one
	virtual void ExitState_Implementation() override
	{
		GetHandler()->TimerController->PauseTimer(IdleTimerName);
		GetHandler()->SetFloatParameter("INTENTION_LEGS_StepTriggerDistanceMultiplier", 1.f);
	}

	// Called every state machine update when the state is active
	virtual void UpdateState_Implementation(float DeltaTime) override
	{
		// Interpolate Core's rotation for micro corrections of the rotation

		GetHandler()->GetCore()->SetWorldRotation(
			UKismetMathLibrary::RInterpTo_Constant(
				GetHandler()->GetCore()->GetComponentRotation(), 
				GetHandler()->GetInputTargetRotation(), DeltaTime, MaxCoreTurnSpeed));
	}

	// Sets step distance to a small value, allowing for micro corrections of the pose
	UFUNCTION()
	void OnIdleTimerFinished(FName InTimerName)
	{
		if (InTimerName == IdleTimerName)
			GetHandler()->SetFloatParameter("INTENTION_LEGS_StepTriggerDistanceMultiplier", StepTriggerDistanceMultiplier);
	}
};


/**
 * Turning state of the spider walk driver
 * Locks movement to give spider time to rotate
 * Entered when a lot of rotation is required
 */
UCLASS()
class MPAS_API UMPAS_SpiderWalk_Turning_State : public UMPAS_IntentionStateBase
{
	GENERATED_BODY()

public:

	// Maximum core turn speed
	float MaxCoreTurnSpeed = 70.f;

	// Changes the distance, at which legs make a decision to make a step
	float StepTriggerDistanceMultiplier = 0.9f;

public:

	// Constructor
	UMPAS_SpiderWalk_Turning_State() {}

	// Called when the state is made active
	virtual void EnterState_Implementation() override
	{
		GetHandler()->SetFloatParameter("INTENTION_LEGS_StepLengthMultiplier", StepTriggerDistanceMultiplier);
	}

	// Called once the active state is changed to a different one
	virtual void ExitState_Implementation() override
	{
		GetHandler()->SetFloatParameter("INTENTION_LEGS_StepLengthMultiplier", 1.f);
	}

	// Called every state machine update when the state is active
	virtual void UpdateState_Implementation(float DeltaTime) override
	{
		// Interpolate Core's rotation
		GetHandler()->GetCore()->SetWorldRotation(
			UKismetMathLibrary::RInterpTo_Constant(
				GetHandler()->GetCore()->GetComponentRotation(),
				GetHandler()->GetInputTargetRotation(), DeltaTime, MaxCoreTurnSpeed));
	}
};

/**
 * Walking state of the spider walk driver
 * Accelerates the Spider the longer it is moving (with a fixed limit)
 * Moves the core onto a fixed distance in the direciton of the input
 * Attempts to find ground under the core to determine the elevation of the core
 */
UCLASS()
class MPAS_API UMPAS_SpiderWalk_Walking_State : public UMPAS_IntentionStateBase
{
	GENERATED_BODY()


public:

	// Maximum core turn speed
	float MaxCoreTurnSpeed = 70.f;

	// Maximum distance on which the core is brought out
	float MaxCoreOffset = 200.f;

	// How hight above the ground shold the core be located
	float CoreElevation = 250.f;

	// How long should be the trace, that looks for the ground
	float TraceLength = 1000.f;


	// How fast the speed bonus builds up
	float SpeedBuildUpRate = 0.1f;

	// Upper limit of the speed bonus
	float MaxSpeedBuildUp = 2.f;

	// How fast should the first core component be moving, to trigger speed bonus build up
	float SpeedBuildUpTrigger = 100.f;


protected:

	// How much speed has been accumulated
	float SpeedBuildUp = 1.f;

	// Attempts to find ground under the core to determine the elevation of the core
	FVector CoreGroundTrace(FVector InputCoreLocation)
	{
		FHitResult Hit;
		if (GetHandler()->GetWorld()->LineTraceSingleByChannel(Hit, InputCoreLocation + FVector::UnitZ() * TraceLength / 2, InputCoreLocation - FVector::UnitZ() * TraceLength / 2, ECC_Visibility))
			return Hit.Location + FVector::UnitZ() * CoreElevation;

		return InputCoreLocation;
	}


public:

	// Constructor
	UMPAS_SpiderWalk_Walking_State() {}

	// Called when the state is made active
	virtual void EnterState_Implementation() override {}

	// Called once the active state is changed to a different one
	virtual void ExitState_Implementation() override
	{
		GetHandler()->SetFloatParameter("INTENTION_LEGS_SpeedMultiplier", 1.0);
		SpeedBuildUp = 1.f;
	}

	// Called every state machine update when the state is active
	virtual void UpdateState_Implementation(float DeltaTime) override
	{
		UMPAS_RigElement* CoreBodySegment = GetHandler()->GetRigData()[GetHandler()->GetCoreElements()[0]].RigElement;

		FVector NewCoreLocation = UKismetMathLibrary::VInterpTo(
			GetHandler()->GetCore()->GetComponentLocation(),
			CoreGroundTrace(GetHandler()->GetMovementInputDirection() * MaxCoreOffset * SpeedBuildUp + CoreBodySegment->GetComponentLocation()),
			DeltaTime, 5.f);

		FRotator NewCoreRotation = UKismetMathLibrary::RInterpTo_Constant(
					GetHandler()->GetCore()->GetComponentRotation(),
					GetHandler()->GetInputTargetRotation(), DeltaTime, MaxCoreTurnSpeed);

		GetHandler()->GetCore()->SetWorldLocationAndRotation(NewCoreLocation, NewCoreRotation);

		// Speed build up logic
		if (CoreBodySegment->GetVelocity().Size() > SpeedBuildUpTrigger)
			SpeedBuildUp = UKismetMathLibrary::FClamp(SpeedBuildUp + DeltaTime * SpeedBuildUpRate, 1, MaxSpeedBuildUp);

		else
			SpeedBuildUp = UKismetMathLibrary::FClamp(SpeedBuildUp - DeltaTime * SpeedBuildUpRate, 1, MaxSpeedBuildUp);

		GetHandler()->SetFloatParameter("INTENTION_LEGS_SpeedMultiplier", SpeedBuildUp);
	}
};



// STATE MACHINE

/**
 * 
 */
UCLASS(Blueprintable)
class MPAS_API UMPAS_SpiderWalkIntentionDriver : public UMPAS_IntentionStateMachine
{
	GENERATED_BODY()

public:

	// How long it takes the spider to enter the resting position
	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category="Default|Idle")
	float IdleTimerDuration = 0.5f;

	// Maximum core turn speed
	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = "Default|Idle")
	float Idle_MaxCoreTurnSpeed = 70.f;

	// Changes the distance, at which legs make a decision to make a step
	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = "Default|Idle")
	float Idle_StepTriggerDistanceMultiplier = 0.15f;


	// Angular difference required
	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = "Default|Turning")
	float Turning_TriggerAngle = 1.f;

	// Maximum core turn speed
	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = "Default|Turning")
	float Turning_MaxCoreTurnSpeed = 70.f;

	// Changes the distance, at which legs make a decision to make a step
	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = "Default|Turning")
	float Turning_StepTriggerDistanceMultiplier = 0.9f;



	// Maximum core turn speed
	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = "Default|Walking")
	float Walking_MaxCoreTurnSpeed = 70.f;

	// Maximum distance on which the core is brought out
	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = "Default|Walking")
	float MaxCoreOffset = 200.f;

	// How hight above the ground shold the core be located
	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = "Default|Walking")
	float CoreElevation = 250.f;

	// How long should be the trace, that looks for the ground
	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = "Default|Walking")
	float TraceLength = 1000.f;


	// How fast the speed bonus builds up
	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = "Default|Walking|SpeedBuildUp")
	float SpeedBuildUpRate = 0.1f;

	// Upper limit of the speed bonus
	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = "Default|Walking|SpeedBuildUp")
	float MaxSpeedBuildUp = 2.f;

	// How fast should the first core component be moving, to trigger speed bonus build up
	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = "Default|Walking|SpeedBuildUp")
	float SpeedBuildUpTrigger = 100.f;


public:

	UMPAS_SpiderWalkIntentionDriver() {}

	// Called when the state machine is initialized (after the main init)
	virtual void OnInitStateMachine_Implementation() override
	{
		// Registering States
		AddNewState((uint8)EMPAS_SpiderWalkID_State::Idle, UMPAS_SpiderWalk_Idle_State::StaticClass());
		AddNewState((uint8)EMPAS_SpiderWalkID_State::Turning, UMPAS_SpiderWalk_Turning_State::StaticClass());
		AddNewState((uint8)EMPAS_SpiderWalkID_State::Walking, UMPAS_SpiderWalk_Walking_State::StaticClass());


		// Configuring states

		// Idle state
		auto IdleState = Cast<UMPAS_SpiderWalk_Idle_State>(GetState((uint8)EMPAS_SpiderWalkID_State::Idle));
		IdleState->IdleTimerDuration = IdleTimerDuration;
		IdleState->MaxCoreTurnSpeed = Idle_MaxCoreTurnSpeed;
		IdleState->StepTriggerDistanceMultiplier = Idle_StepTriggerDistanceMultiplier;

		// Turning state
		auto TurningState = Cast<UMPAS_SpiderWalk_Turning_State>(GetState((uint8)EMPAS_SpiderWalkID_State::Turning));
		TurningState->MaxCoreTurnSpeed = Turning_MaxCoreTurnSpeed;
		TurningState->StepTriggerDistanceMultiplier = Turning_StepTriggerDistanceMultiplier;

		// Walking state
		auto WalkingState = Cast<UMPAS_SpiderWalk_Walking_State>(GetState((uint8)EMPAS_SpiderWalkID_State::Walking));
		WalkingState->MaxCoreTurnSpeed = Walking_MaxCoreTurnSpeed;
		WalkingState->MaxCoreOffset = MaxCoreOffset;
		WalkingState->CoreElevation = CoreElevation;
		WalkingState->TraceLength = TraceLength;
		WalkingState->SpeedBuildUpRate = SpeedBuildUpRate;
		WalkingState->MaxSpeedBuildUp = MaxSpeedBuildUp;
		WalkingState->SpeedBuildUpTrigger = SpeedBuildUpTrigger;


		// Registering Transitions
		RegisterTransitionLocal((uint8)EMPAS_SpiderWalkID_State::Idle, (uint8)EMPAS_SpiderWalkID_State::Walking, "Condition_IdleToWalking");
		RegisterTransitionLocal((uint8)EMPAS_SpiderWalkID_State::Walking, (uint8)EMPAS_SpiderWalkID_State::Idle, "Condition_WalkingToIdle");

		RegisterTransitionLocal((uint8)EMPAS_SpiderWalkID_State::Idle, (uint8)EMPAS_SpiderWalkID_State::Turning, "Condition_Turning");
		RegisterTransitionLocal((uint8)EMPAS_SpiderWalkID_State::Walking, (uint8)EMPAS_SpiderWalkID_State::Turning, "Condition_Turning");

		RegisterTransitionLocal((uint8)EMPAS_SpiderWalkID_State::Turning, (uint8)EMPAS_SpiderWalkID_State::Idle, "Condition_TurningToIdle");
		RegisterTransitionLocal((uint8)EMPAS_SpiderWalkID_State::Turning, (uint8)EMPAS_SpiderWalkID_State::Walking, "Condition_TurningToWalking");

		// Initial State
		ForceCallStateTransition((uint8)EMPAS_SpiderWalkID_State::Idle);
	}

	// CALLED BY THE HANDLER: Called once the rig has finished it's Scanning, Initing and Linking processes
	virtual void OnRigSetupFinished_Implementation() override {}

	// Called every time the state machine is updated (after the main update)
	virtual void OnUpdateStateMachine_Implementation(float DeltaTime) override {}


	// Transition functions

	UFUNCTION()
	bool Condition_IdleToWalking() 
	{
		return GetHandler()->GetMovementInputDirection() != FVector(0);
	}

	UFUNCTION()
	bool Condition_WalkingToIdle()
	{
		return !Condition_IdleToWalking();
	}

	UFUNCTION()
	bool Condition_Turning()
	{
		FRotator DeltaRotator = GetHandler()->GetInputTargetRotation() - GetHandler()->GetCore()->GetComponentRotation();

		return DeltaRotator.Yaw > Turning_TriggerAngle;
	}

	UFUNCTION()
	bool Condition_TurningToIdle()
	{
		return (!Condition_Turning()) && Condition_WalkingToIdle();
	}

	UFUNCTION()
	bool Condition_TurningToWalking()
	{
		return (!Condition_Turning()) && Condition_IdleToWalking();
	}

};
