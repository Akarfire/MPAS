// Fill out your copyright notice in the Description page of Project Settings.


#include "MPAS_Limb.h"
#include "MPAS_Handler.h"
#include "Engine/SkeletalMesh.h"
#include "Kismet/KismetMathLibrary.h"
#include "Kismet/KismetStringLibrary.h"
#include "MPAS_PhysicsModelElement.h"
#include "PhysicsEngine/PhysicsConstraintComponent.h"

// Constructor
UMPAS_Limb::UMPAS_Limb() {}


// Initializes limb's segments and state
void UMPAS_Limb::InitLimb()
{
    // Resetting max extent
    MaxExtent = 0;

    // Custom chain initialization
    if (SetupType == EMPAS_LimbSetupType::CustomChain)
    {
        // Initializing state
        for (int32 i = 0; i < Segments.Num(); i++)
        {
            MaxExtent += Segments[i].Length;

            FMPAS_LimbSegmentState NewState;

            FVector PreviousLocation = GetComponentLocation();
            float PreviousLength = 0.f;
            if (i != 0) 
            {
                PreviousLocation = CurrentState[i - 1].Position;
                PreviousLength = Segments[i - 1].Length;
            }

            NewState.Position = PreviousLocation + FVector(0, 0, 1) * PreviousLength;
            
            if (i > 0)
                CurrentState[i - 1].Rotation = UKismetMathLibrary::FindLookAtRotation(PreviousLocation, NewState.Position);

            CurrentState.Add(NewState);
            WriteSegmentState(i, NewState);
        }

        // Add a fective state, that plays the role of the limb's tip (it is going to be mathched with the target)
        CurrentState.Add(FMPAS_LimbSegmentState());

        Initialized = true;
    }

    // Fetch from mesh initialization
    else if (SetupType == EMPAS_LimbSetupType::FetchFromMesh)
    {
        // Attempts to find Fetch mesh if it is not already set. Will grab the first skeletal mesh component with tag "MPAS_LimbFetch"
        if (!Fetch_MeshComponent)
        {
            TArray<UActorComponent*> Components = GetOwner()->GetComponentsByTag(USkeletalMeshComponent::StaticClass(), "MPAS_LimbFetch");
            if (Components.Num() > 0)
                Fetch_MeshComponent = Cast<USkeletalMeshComponent>(Components[0]);
        }

        if (Fetch_MeshComponent)
        {
            // Cleaning current data
            Segments.Empty();
            CurrentState.Empty();

            // Fetching the bone chain (in reverse order, because you can only get bone's parent and not children)
            TArray<FName> ReversedBoneChain;

            FName Bone = Fetch_TipBone;
            while (Fetch_MeshComponent->GetBoneIndex(Bone) != INDEX_NONE)
            {
                ReversedBoneChain.Add(Bone);

                if (Bone == Fetch_OriginBone)
                    break;

                Bone = Fetch_MeshComponent->GetParentBone(Bone);
            }

            // Processing bone chain to generate segments and initial state
            for (int32 i = ReversedBoneChain.Num() - 1; i > 0; i--)
            {
                FMPAS_LimbSegmentData NewSegment;
                FMPAS_LimbSegmentState NewState;

                NewState.Position = Fetch_MeshComponent->GetBoneLocation(ReversedBoneChain[i]);
                NewState.Rotation = (FRotator)Fetch_MeshComponent->GetBoneQuaternion(ReversedBoneChain[i]);

                NewSegment.BoneName = ReversedBoneChain[i];
                NewSegment.Length = (NewState.Position - Fetch_MeshComponent->GetBoneLocation(ReversedBoneChain[i - 1])).Size();
                
                MaxExtent += NewSegment.Length;
                
                Segments.Add(NewSegment);
                CurrentState.Add(NewState);
            }

            // Tip bone state
            FMPAS_LimbSegmentState TipBoneState;

            TipBoneState.Position = Fetch_MeshComponent->GetBoneLocation(ReversedBoneChain[0]);
            TipBoneState.Rotation = (FRotator)Fetch_MeshComponent->GetBoneQuaternion(ReversedBoneChain[0]);

            CurrentState.Add(TipBoneState);

            // Fetching origin position if needed
            if (Fetch_OriginPosition)
            {
                FVector ParentPosition = CalculatePositionLayerValue(GetPositionLayer(0, 0));
                FVector NewSelfPosition = CurrentState[0].Position - ParentPosition;

                SetPositionSourceValue(0, 1, this, NewSelfPosition);
            }

            Initialized = true;
        }  
    }

    // Fillin out target state with a default value of the initial state
    TargetState = CurrentState;
}

/* Generates a config for physics elements that are going to be created during physics model generation phase
 * If automatic generation is disabled, it will still match defined physics elements with limb segments
 */
void UMPAS_Limb::GeneratePhysicsModelConfiguration()
{
    if (AutoPhysicsConfigGeneration)
    {
        /* 
        * Creating a mass distribution map for all limb segments 
        * each element is a percentage of how much mass needs to be allocated to each limb segment
        */
        TArray<float> LimbMassDistribution;
        for (auto& Seg: Segments)
            LimbMassDistribution.Add(Seg.Length / MaxExtent); 

        PhysicsElementsConfiguration.Empty();

        // Merging segment data with additional data
        if (AdditionalSegmentData.Num() > 0)
            for (int i = 0; i < Segments.Num(); i++)
            {
                Segments[i].AngularLimits = AdditionalSegmentData[UKismetMathLibrary::Clamp(i, 0, AdditionalSegmentData.Num() - 1)].AngularLimits;
                Segments[i].PhysicsMeshExtent = AdditionalSegmentData[UKismetMathLibrary::Clamp(i, 0, AdditionalSegmentData.Num() - 1)].PhysicsMeshExtent;
            }

        for (int i = 0; i < Segments.Num(); i++)
        {
            FMPAS_PhysicsElementConfiguration NewConfig;

            // General physics element config setup

            NewConfig.PhysicsElementClass = PhysicsElementClass;
            NewConfig.PhysicsMesh = LimbPhysicsMesh;

            NewConfig.PhysicsMeshRelativeTransform.SetScale3D(FVector( Segments[i].Length / 100, Segments[i].PhysicsMeshExtent.X, Segments[i].PhysicsMeshExtent.Y ));
            NewConfig.Mass = LimbMass * LimbMassDistribution[i];
            NewConfig.AirDrag = AirDrag;
            
            NewConfig.ParentPhysicalAttachmentType = SegmentParentPhysicalAttachmentType;

            NewConfig.PhysicsConstraintLimits = FVector(PositionDrift, PositionDrift, PositionDrift);
            if (Segments[i].AngularLimits != FRotator(0, 0, 0))
                NewConfig.PhysicsAngularLimits = Segments[i].AngularLimits;

            NewConfig.DisableCollisionWithParent = true;

            // Chain linking
            // First element case
            if (i == 0)
                NewConfig.ParentType = EMPAS_PhysicsModelParentType::RigElement;
        
            else
            {
                NewConfig.ParentType = EMPAS_PhysicsModelParentType::ChainedPhysicsElement;
                NewConfig.ParentPhysicsElementID = i - 1;
            }

            // Creating Position and Rotation stacks and applying them to the configuration
            NewConfig.PositionStackID = RegisterPositionStack("PhysicsElementPosition_" + UKismetStringLibrary::Conv_IntToString(i));
            NewConfig.RotationStackID = RegisterRotationStack("PhysicsElementRotation_" + UKismetStringLibrary::Conv_IntToString(i));

            RegisterPositionLayer(NewConfig.PositionStackID, "PhysicsElementPosition", EMPAS_LayerBlendingMode::Normal, EMPAS_LayerCombinationMode::Add, true);
            RegisterRotationLayer(NewConfig.RotationStackID, "PhysicsElementRotation", EMPAS_LayerBlendingMode::Normal, true);


            PhysicsElementsConfiguration.Add(NewConfig);
        }
    }
}


// Solves the limb by applying the specified algorithm to the segments
void UMPAS_Limb::SolveLimb()
{
    if (!CurrentlySolving)
    {
        CurrentlySolving = true;

        // Data that is going to be passed to the background thread
        EMPAS_LimbSolvingAlgorithm L_Algorithm = Algoritm;
        TArray<FMPAS_LimbSegmentData> L_Segments = Segments;
        TArray<FMPAS_LimbSegmentState> L_State = TargetState;

        const int32 L_MaxIterations = IK_MaxIterations;
        const float L_Tollerance = IK_ErrorTollerance;
        const FVector L_UpVector = GetUpVector();

        const bool L_EnableRollRecalculation = EnableRollRecalculation;
        const float L_LimbRoll = LimbRoll;

        FVector OriginLocation = GetComponentLocation();
        FVector TargetLocation = GetLimbTarget();

        // Pole target calclulation
        TArray<FVector> L_PoleTargets;

        // Base value just to make sure there always is at least some kind of a pole target
        FVector LastCalculatedPoleTarget = (OriginLocation + TargetLocation) / 2 + GetUpVector() * 100;
        
        for (int32 i = 0; i < Segments.Num(); i++)
        {
            const FMPAS_LimbSegmentData& Seg = Segments[i];
            if (PoleTargets.Contains(i))
            {
                const FMPAS_LimbPoleTarget& PoleTargetSettings = PoleTargets[i];

                // Auto calculation mode
                if (PoleTargetSettings.PositionMode == EMPAS_LimbPoleTargetPositionMode::AutoCalculation)
                {
                    FVector ForwardVector = (TargetLocation - OriginLocation).GetSafeNormal();
                    FVector UpVector = GetUpVector();
                    FVector RightVector = UKismetMathLibrary::Cross_VectorVector(ForwardVector, UpVector).GetSafeNormal();

                    LastCalculatedPoleTarget = OriginLocation + ( ForwardVector * PoleTargetSettings.AUTO_PoleTargetOffset.X + RightVector* PoleTargetSettings.AUTO_PoleTargetOffset.Y + UpVector* PoleTargetSettings.AUTO_PoleTargetOffset.Z );
                }

                // Position stack mode
                else if (PoleTargetSettings.PositionMode == EMPAS_LimbPoleTargetPositionMode::PoleTargetPositionStack)
                {
                    LastCalculatedPoleTarget = CalculatePositionStackValue(PoleTargetSettings.STACK_PositionStackID);
                }
            }
            
            L_PoleTargets.Add(LastCalculatedPoleTarget);
        }
        

        if (EnableAsyncCalculation)
        {
            // Calling the necessary algorithm on a background thread, so it doesn't waste the perfomance
            AsyncTask( ENamedThreads::AnyBackgroundThreadNormalTask, [L_Algorithm, L_Segments, L_State, OriginLocation, TargetLocation, L_PoleTargets, L_MaxIterations, L_Tollerance, L_UpVector, L_EnableRollRecalculation, L_LimbRoll, this] ()
            {
                // New state declaration
                TArray<FMPAS_LimbSegmentState> NewState;

                // Selecting an algorithm and calling solving 
                switch (L_Algorithm)
                {
                case EMPAS_LimbSolvingAlgorithm::FABRIK_IK: NewState = Solve_FABRIK_IK(OriginLocation, TargetLocation, L_Segments, L_State, L_MaxIterations, L_Tollerance); break;
                case EMPAS_LimbSolvingAlgorithm::CCD_IK: NewState = Solve_CCD_IK(OriginLocation, TargetLocation, L_Segments, L_State, L_MaxIterations, L_Tollerance); break;
                case EMPAS_LimbSolvingAlgorithm::PoleFABRIK_IK: NewState = Solve_PoleFABRIK_IK(OriginLocation, TargetLocation, L_Segments, L_State, L_PoleTargets, L_MaxIterations, L_Tollerance, L_UpVector); break;
                
                default: break;
                }

                // Recalculating segment roll
                if (L_EnableRollRecalculation)
                    RecalculateRoll(NewState, L_LimbRoll, OriginLocation, TargetLocation, L_Segments, L_PoleTargets, L_UpVector);

                // Calling back to the game thread, notifyinh the limb of the results
                AsyncTask( ENamedThreads::GameThread, [NewState, this] ()
                {
                    FinishSolving(NewState);
                });
            });
        }

        // Non async calculation to leave users with more options
        else
        {
            // New state declaration
            TArray<FMPAS_LimbSegmentState> NewState;

            // Selecting an algorithm and calling solving 
            switch (L_Algorithm)
            {
            case EMPAS_LimbSolvingAlgorithm::FABRIK_IK: NewState = Solve_FABRIK_IK(OriginLocation, TargetLocation, L_Segments, L_State, L_MaxIterations, L_Tollerance); break;
            case EMPAS_LimbSolvingAlgorithm::CCD_IK: NewState = Solve_CCD_IK(OriginLocation, TargetLocation, L_Segments, L_State, L_MaxIterations, L_Tollerance); break;
            case EMPAS_LimbSolvingAlgorithm::PoleFABRIK_IK: NewState = Solve_PoleFABRIK_IK(OriginLocation, TargetLocation, L_Segments, L_State, L_PoleTargets, L_MaxIterations, L_Tollerance, L_UpVector); break;
            
            default: break;
            }

            // Recalculating segment roll
            if (L_EnableRollRecalculation)
                RecalculateRoll(NewState, L_LimbRoll, OriginLocation, TargetLocation, L_Segments, L_PoleTargets, L_UpVector);

            FinishSolving(NewState);
        }
    }
}

// A call back from the background thread, that indicates that the limb has finished solving it's state
void UMPAS_Limb::FinishSolving(TArray<FMPAS_LimbSegmentState> ResultingState)
{
    CurrentlySolving = false;

    TargetState = ResultingState;
}

// Updates the current state of the specified segment
void UMPAS_Limb::WriteSegmentState(int32 InSegment, const FMPAS_LimbSegmentState& InState)
{
    // Writing the state
    CurrentState[InSegment] = InState;

    // Updating bone positions in the handler, if the bone name is the
    if (Segments[InSegment].BoneName != FName())
    {
        GetHandler()->SetBoneLocation(Segments[InSegment].BoneName, InState.Position);
        GetHandler()->SetBoneRotation(Segments[InSegment].BoneName, InState.Rotation);
    }
}


// Handles interpolation of the current state to the target state
void UMPAS_Limb::InterpolateLimb(float DeltaTime)
{
    // Ignore interpolation if LimbInterpolationSpeed is set to 0 or less
    if (LimbInterpolationSpeed > 0)
    {
        FVector NextSegmentPosition = GetComponentLocation();

        for (int i = 0 ; i < CurrentState.Num() - 1; i++)
        {
            CurrentState[i].Rotation = UKismetMathLibrary::RInterpTo_Constant(CurrentState[i].Rotation, TargetState[i].Rotation, DeltaTime, LimbInterpolationSpeed);

            CurrentState[i].Position = NextSegmentPosition;

            NextSegmentPosition = CurrentState[i].Position + Segments[i].Length * UKismetMathLibrary::GetForwardVector(CurrentState[i].Rotation);
        }

        // Fective segment's position update
        CurrentState[CurrentState.Num() - 1].Position = NextSegmentPosition;
    }

    else
        CurrentState = TargetState;


    // Writing states to send data to the handler
    for (int32 i = 0; i < Segments.Num(); i++)
        WriteSegmentState(i, CurrentState[i]);
}


// Algorithms
// 'static' because they are going to run in a background thread

// Rotate To Target
void UMPAS_Limb::Solve_RotateToTarget()
{

}

// FABRIK IK
TArray<FMPAS_LimbSegmentState> UMPAS_Limb::Solve_FABRIK_IK(const FVector& InOriginPosition, const FVector& InTargetPosition, const TArray<FMPAS_LimbSegmentData>& InSegments, const TArray<FMPAS_LimbSegmentState>& InCurrentState, int32 InMaxIterations, float InTollerance)
{
    TArray<FMPAS_LimbSegmentState> State = InCurrentState;

    size_t Iteration = 0;

    while ((Iteration < InMaxIterations) && (((State[State.Num() - 1].Position - InTargetPosition).Size() > InTollerance) || ((State[0].Position - InOriginPosition).Size() > InTollerance * 0.1f)))
    {
        // Backward pass
        State[State.Num() - 1].Position = InTargetPosition;
        for (size_t i = State.Num() - 1; i > 0; i--)
        {
            State[i - 1].Position = State[i].Position + (State[i - 1].Position - State[i].Position).GetSafeNormal() * InSegments[i - 1].Length;
            State[i - 1].Rotation = UKismetMathLibrary::FindLookAtRotation(State[i - 1].Position, State[i].Position);
        }

        // Forward pass
        State[0].Position = InOriginPosition;
        for (size_t i = 0; i < State.Num() - 1; i++)
        {
            State[i + 1].Position = State[i].Position + (State[i + 1].Position - State[i].Position).GetSafeNormal() * InSegments[i].Length;
            State[i].Rotation = UKismetMathLibrary::FindLookAtRotation(State[i].Position, State[i + 1].Position);
        }

        Iteration++;
    }

    return State;
}


// CCD IK
TArray<FMPAS_LimbSegmentState> UMPAS_Limb::Solve_CCD_IK(const FVector& InOriginPosition, const FVector& InTargetPosition, const TArray<FMPAS_LimbSegmentData>& InSegments, const TArray<FMPAS_LimbSegmentState>& InCurrentState, int32 InMaxIterations, float InTollerance)
{
    TArray<FMPAS_LimbSegmentState> State = InCurrentState;

    State[0].Position = InOriginPosition;

    // Initial position recalculation, handling cases, where target position didn't change, but the origin position did
    for (int32 j = 1; j < State.Num(); j++)
        State[j].Position = State[j - 1].Position + UKismetMathLibrary::GetForwardVector(State[j - 1].Rotation) * InSegments[j - 1].Length;
   
    size_t Iteration = 0;
    while ((Iteration < InMaxIterations) && ( (State[State.Num() - 1].Position - InTargetPosition).Size() > InTollerance) )
    {
        // Main CCD pass
        for (int32 i = State.Num() - 2; i >= 0; i--)
        {   
            State[i].Rotation = UKismetMathLibrary::ComposeRotators(UKismetMathLibrary::NormalizedDeltaRotator(UKismetMathLibrary::FindLookAtRotation(State[i].Position, State[State.Num() - 1].Position), UKismetMathLibrary::FindLookAtRotation(State[i].Position, InTargetPosition)), State[i].Rotation);

            // Forward pass to recalculate segment positions
            for (int32 j = 1; j < State.Num(); j++)
                State[j].Position = State[j - 1].Position + UKismetMathLibrary::GetForwardVector(State[j - 1].Rotation) * InSegments[j - 1].Length;
        }

        Iteration++;
    }

    if ((State[State.Num() - 1].Position - InTargetPosition).Size() <= InTollerance)
        return State;

    return InCurrentState;
}


// PoleFABRIK IK - custom version of FABRIK IK, sligtly slower, but implements support for pole targets, making it the most usable algorithm ot of the ones presented here
TArray<FMPAS_LimbSegmentState> UMPAS_Limb::Solve_PoleFABRIK_IK(const FVector& InOriginPosition, const FVector& InTargetPosition, const TArray<FMPAS_LimbSegmentData>& InSegments, const TArray<FMPAS_LimbSegmentState>& InCurrentState, const TArray<FVector>& InPoleTargets, int32 InMaxIterations, float InTollerance, const FVector& InUpVector)
{
    TArray<FMPAS_LimbSegmentState> State = InCurrentState;

    // Reinitiating state to match pole targets

    State[0].Position = InOriginPosition;
    for (int32 i = 0; i < InSegments.Num(); i++)
        State[i + 1].Position = State[i].Position + (InPoleTargets[i] - State[i].Position).GetSafeNormal() * InSegments[i].Length;

    // FABRIK iterating
    // Solved positions are projected onto a solution plane (see ProjectPositionOnToSolutionPlane description) to keep the movement naturall

    size_t Iteration = 0;
    while ((Iteration < InMaxIterations) && (((State[State.Num() - 1].Position - InTargetPosition).Size() > InTollerance) || ((State[0].Position - InOriginPosition).Size() > InTollerance * 0.1f)))
    {
        // Backward pass
        State[State.Num() - 1].Position = InTargetPosition;
        for (size_t i = State.Num() - 1; i > 0; i--)
        {
            State[i - 1].Position = ProjectPositionOnToSolutionPlane(State[i].Position + (State[i - 1].Position - State[i].Position).GetSafeNormal() * InSegments[i - 1].Length, InOriginPosition, InTargetPosition, InPoleTargets[i - 1]);
            State[i - 1].Rotation = UKismetMathLibrary::FindLookAtRotation(State[i - 1].Position, State[i].Position);
        }

        // Forward pass
        State[0].Position = InOriginPosition;
        for (size_t i = 0; i < State.Num() - 2; i++)
        {
            State[i + 1].Position = ProjectPositionOnToSolutionPlane(State[i].Position + (State[i + 1].Position - State[i].Position).GetSafeNormal() * InSegments[i].Length, InOriginPosition, InTargetPosition, InPoleTargets[i + 1]);
            State[i].Rotation = UKismetMathLibrary::FindLookAtRotation(State[i].Position, State[i + 1].Position);
        }

        Iteration++;
    }

    return State;
}


// Recalculating segment roll rotation
void UMPAS_Limb::RecalculateRoll(TArray<FMPAS_LimbSegmentState>& InOutState, float InLimbRoll, const FVector& InOriginPosition, const FVector& InTargetPosition, const TArray<FMPAS_LimbSegmentData>& InSegments, const TArray<FVector>& InPoleTargets, const FVector& InUpVector)
{
    // Segment roll recalculation
    for (int32 i = 0; i < InSegments.Num(); i++)
    {
        InOutState[i].Rotation.Roll = 0;

        FVector SolutionPlaneNormal = FVector(0, 0, 0);
        FVector TargetDirection = FVector(0, 0, 0);

        // General case
        if (i > 0)
        {
            FVector OriginToStart = InOutState[i].Position - InOriginPosition;
            FVector OriginToEnd = InOutState[i].Position + InOutState[i].Rotation.Vector() * InSegments[i].Length - InOriginPosition;

            SolutionPlaneNormal = UKismetMathLibrary::Cross_VectorVector(OriginToStart, OriginToEnd).GetSafeNormal();

            TargetDirection = (InOutState[i].Position + InOutState[i].Position + InOutState[i].Rotation.Vector() * InSegments[i].Length) / 2.f - InOriginPosition;
        }

        // First segment case
        else
        {
            TargetDirection = InUpVector;

            SolutionPlaneNormal = UKismetMathLibrary::Cross_VectorVector(InUpVector, InOutState[i].Rotation.Vector()).GetSafeNormal();
        }


        // Sollution
        FVector TargetNormal = UKismetMathLibrary::Cross_VectorVector(SolutionPlaneNormal, InOutState[i].Rotation.Vector()).GetSafeNormal();

        if (UKismetMathLibrary::Dot_VectorVector(TargetNormal, TargetDirection) < UKismetMathLibrary::Dot_VectorVector(TargetNormal * -1.f, TargetDirection))
            TargetNormal *= -1.f;

        float Angle = UKismetMathLibrary::DegAcos(UKismetMathLibrary::Dot_VectorVector(TargetNormal, UKismetMathLibrary::GetUpVector(InOutState[i].Rotation)));

        InOutState[i].Rotation.Roll = Angle + InLimbRoll;
    }
}



// The point is space (World Space), which the limb is trying to reach
FVector UMPAS_Limb::GetLimbTarget()
{
    FVector Target = FVector(0, 0, 0);

    switch (TargetType)
    {

    case EMPAS_LimbTargetType::FirstChildComponent:
        if (TargetComponent)
            Target = TargetComponent->GetComponentLocation();
        break;

    case EMPAS_LimbTargetType::TargetPositionStack:
        Target = CalculatePositionStackValue(TargetStackID);
        break;
        
    default: break;
    }

    // Limit target to limb's max extent range
    if ((Target - GetComponentLocation()).Size() > MaxExtent)
        Target = GetComponentLocation() + (Target - GetComponentLocation()).GetSafeNormal() * MaxExtent;

    return Target;
}


// Reinitializes limb's segments and state
void UMPAS_Limb::ReinitLimb()
{
    Initialized = false;
    Segments.Empty();
    CurrentState.Empty();

    InitLimb();
}




// CALLED BY THE HANDLER
// Initializing Rig Element
void UMPAS_Limb::InitRigElement(class UMPAS_Handler* InHandler)
{
    Super::InitRigElement(InHandler);

    if (TargetType == EMPAS_LimbTargetType::TargetPositionStack)
        TargetStackID = RegisterPositionStack("LimbTarget");

    for (auto& Pole: PoleTargets)
        if (Pole.Value.PositionMode == EMPAS_LimbPoleTargetPositionMode::PoleTargetPositionStack)
            Pole.Value.STACK_PositionStackID = RegisterPositionStack("PoleTarget_" + UKismetStringLibrary::Conv_IntToString(Pole.Key));

    InitLimb();

    GeneratePhysicsModelConfiguration();
}

// CALLED BY THE HANDLER : Contains the logic that links this element with other elements in the rig
void UMPAS_Limb::LinkRigElement(class UMPAS_Handler* InHandler)
{
    Super::LinkRigElement(InHandler);

    if (TargetType == EMPAS_LimbTargetType::FirstChildComponent)
    {
        if (GetHandler()->GetRigData()[RigElementName].ChildElements.Num() > 0)
            TargetComponent = GetHandler()->GetRigData()[GetHandler()->GetRigData()[RigElementName].ChildElements[0]].RigElement;
    }
}

// CALLED BY THE HANDLER : Links element to it's physics model equivalent
void UMPAS_Limb::InitPhysicsModel(const TArray<UMPAS_PhysicsModelElement*>& InPhysicsElements)
{
    Super::InitPhysicsModel(InPhysicsElements);

    // Setting physics contraints' positions to the correct ones
    const auto& Constraints = GetHandler()->GetPhysicsModelConstraints();

    for (int32 i = 1; i < Constraints[RigElementName].Constraints.Num(); i++)
    {
        auto& Constr = Constraints[RigElementName].Constraints[i];
        Constr.ConstraintComponent->SetWorldLocation( Constr.Body_1->GetComponentLocation() + Constr.Body_1->GetForwardVector() * Constr.Body_1->GetComponentScale().X * 100 );
    }
}

// Updating Rig Element every tick
void UMPAS_Limb::UpdateRigElement(float DeltaTime)
{
    Super::UpdateRigElement(DeltaTime);

    if (Initialized && GetRigElementActive() && !GetPhysicsModelEnabled())
    {
        InterpolateLimb(DeltaTime);
        SolveLimb();
    }
}


// PHYSICS MODEL

// Called when physics model is enabled, applies position and rotation of physics elements to the rig element
void UMPAS_Limb::ApplyPhysicsModelPositionAndRotation_Implementation(float DeltaTime)
{
    for(int32 i = 0; i < PhysicsElementsConfiguration.Num(); i++)
    {
        FMPAS_LimbSegmentState NewState;
        NewState.Position = CalculatePositionStackValue(PhysicsElementsConfiguration[i].PositionStackID);
        NewState.Rotation = CalculateRotationStackValue(PhysicsElementsConfiguration[i].RotationStackID);

        WriteSegmentState(i, NewState);
    }
}

// Returns the position, where physics element needs to be once the physics model is enabled
FTransform UMPAS_Limb::GetDesiredPhysicsElementTransform_Implementation(int32 PhysicsElementID)
{
    FTransform LimbSegmentTransform;
    
    if (CurrentState.IsValidIndex(PhysicsElementID))
    {
        LimbSegmentTransform.SetLocation(CurrentState[PhysicsElementID].Position);
        LimbSegmentTransform.SetRotation(FQuat(CurrentState[PhysicsElementID].Rotation));
        LimbSegmentTransform.SetScale3D(FVector(1, 1, 1));
    }

    return LimbSegmentTransform;
}


// Math/Utilities

// Finds rotation between two vectors
FRotator UMPAS_Limb::GetRotationBetweenVectors(const FVector& InVector_1, const FVector& InVector_2, bool InVectorsNormalized)
{
    
    float XY_PlaneAngle = 0, XZ_PlaneAngle = 0, YZ_PlaneAngle = 0;

    if (InVectorsNormalized)
    {
        XY_PlaneAngle = acos( FVector::DotProduct( InVector_1 * FVector(1, 1, 0), InVector_2 * FVector(1, 1, 0) ) );
        XZ_PlaneAngle = acos( FVector::DotProduct( InVector_1 * FVector(1, 0, 1), InVector_2 * FVector(1, 0, 1) ) );
        YZ_PlaneAngle = acos( FVector::DotProduct( InVector_1 * FVector(0, 1, 1), InVector_2 * FVector(0, 1, 1) ) );
    }

    else
    {
        FVector Vector_1_Normalized = InVector_1.GetSafeNormal();
        FVector Vector_2_Normalized = InVector_2.GetSafeNormal();

        XY_PlaneAngle = acos( FVector::DotProduct( Vector_1_Normalized * FVector(1, 1, 0), Vector_2_Normalized * FVector(1, 1, 0) ) );
        XZ_PlaneAngle = acos( FVector::DotProduct( Vector_1_Normalized * FVector(1, 0, 1), Vector_2_Normalized * FVector(1, 0, 1) ) );
        YZ_PlaneAngle = acos( FVector::DotProduct( Vector_1_Normalized * FVector(0, 1, 1), Vector_2_Normalized * FVector(0, 1, 1) ) );
    }

    return FRotator(XZ_PlaneAngle, XY_PlaneAngle, YZ_PlaneAngle);

}

// Projects position (location) vector on to a "solution plane" (plane constructed from 3 points: Limb Origin, Limb Target and Pole Target)
FVector UMPAS_Limb::ProjectPositionOnToSolutionPlane(const FVector& InPosition, const FVector& InLimbOrigin, const FVector& InLimbTarget, const FVector& InPoleTarget)
{
    FVector PlaneNormal = UKismetMathLibrary::Cross_VectorVector(InLimbTarget - InLimbOrigin, InPoleTarget - InLimbOrigin).GetSafeNormal();
    FVector Projection = UKismetMathLibrary::ProjectVectorOnToPlane(InPosition - InLimbOrigin, PlaneNormal);

    return Projection + InLimbOrigin;
}