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

        auto Leg = Cast<UMPAS_Leg>(RigElementData.RigElement);
        auto ParentBody = Cast<UMPAS_BodySegment>(GetHandler()->GetRigData()[RigElementData.ParentComponent].RigElement);

        if (Leg && ParentBody)
        {
            if (LegsData.Contains(ParentBody)) LegsData[ParentBody].Add(Leg);
            else LegsData.Add(ParentBody, TArray<UMPAS_Leg*> { Leg, } );
        }
    }
}

// Called once the active state is changed to a different one
void UMPAS_LegIntentionDriverState::ExitState_Implementation()
{

}

// Called every state machine update when the state is active
void UMPAS_LegIntentionDriverState::UpdateState_Implementation(float DeltaTime)
{

}