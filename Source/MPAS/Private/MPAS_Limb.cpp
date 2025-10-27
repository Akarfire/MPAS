// Fill out your copyright notice in the Description page of Project Settings.


#include "Default/RigElements/MPAS_Limb.h"
#include "MPAS_Handler.h"
#include "Engine/SkeletalMesh.h"
#include "Kismet/KismetMathLibrary.h"
#include "Kismet/KismetStringLibrary.h"
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
                PreviousLocation = CurrentState[i - 1].Location;
                PreviousLength = Segments[i - 1].Length;
            }

            NewState.Location = PreviousLocation + FVector(0, 0, 1) * PreviousLength;
            
            if (i > 0)
                CurrentState[i - 1].Rotation = UKismetMathLibrary::FindLookAtRotation(PreviousLocation, NewState.Location);

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

                NewState.Location = Fetch_MeshComponent->GetBoneLocation(ReversedBoneChain[i]);
                NewState.Rotation = (FRotator)Fetch_MeshComponent->GetBoneQuaternion(ReversedBoneChain[i]);

                NewSegment.BoneName = ReversedBoneChain[i];
                NewSegment.Length = (NewState.Location - Fetch_MeshComponent->GetBoneLocation(ReversedBoneChain[i - 1])).Size();
                
                MaxExtent += NewSegment.Length;
                
                Segments.Add(NewSegment);
                CurrentState.Add(NewState);
            }

            // Tip bone state
            FMPAS_LimbSegmentState TipBoneState;

            TipBoneState.Location = Fetch_MeshComponent->GetBoneLocation(ReversedBoneChain[0]);
            TipBoneState.Rotation = (FRotator)Fetch_MeshComponent->GetBoneQuaternion(ReversedBoneChain[0]);

            CurrentState.Add(TipBoneState);

            // Fetching origin position if needed
            if (Fetch_OriginPosition)
            {
                bool HasActiveElements = false;
                FVector ParentLocation = CalculateVectorLayerValue(VectorStacks[0][0], HasActiveElements);
                FVector NewSelfLocation = CurrentState[0].Location - ParentLocation;

                SetVectorSourceValue(0, 1, this, NewSelfLocation);
            }

            Initialized = true;
        }  
    }

    // Merging segment data with additional data
    if (AdditionalSegmentData.Num() > 0)
        for (int i = 0; i < Segments.Num(); i++)
        {
            Segments[i].AngularLimits_Min = AdditionalSegmentData[UKismetMathLibrary::Clamp(i, 0, AdditionalSegmentData.Num() - 1)].AngularLimits_Min;
            Segments[i].AngularLimits_Max = AdditionalSegmentData[UKismetMathLibrary::Clamp(i, 0, AdditionalSegmentData.Num() - 1)].AngularLimits_Max;
            Segments[i].PhysicsMeshExtent = AdditionalSegmentData[UKismetMathLibrary::Clamp(i, 0, AdditionalSegmentData.Num() - 1)].PhysicsMeshExtent;
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

            NewConfig.PhysicsConstraintLimits = FVector(LocationDrift, LocationDrift, LocationDrift);
           /* if (Segments[i].AngularLimits != FRotator(0, 0, 0))
                NewConfig.PhysicsAngularLimits = Segments[i].AngularLimits;*/

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
            NewConfig.PositionStackID = RegisterVectorStack("PhysicsElementPosition_" + UKismetStringLibrary::Conv_IntToString(i));
            NewConfig.RotationStackID = RegisterRotationStack("PhysicsElementRotation_" + UKismetStringLibrary::Conv_IntToString(i));

            RegisterVectorLayer(NewConfig.PositionStackID, "PhysicsElementPosition", EMPAS_LayerBlendingMode::Normal, EMPAS_LayerCombinationMode::Add, 1.f, 0, true);
            RegisterRotationLayer(NewConfig.RotationStackID, "PhysicsElementRotation", EMPAS_LayerBlendingMode::Normal, 1.f, 0, true);


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

        float L_LimbMaxExtent = MaxExtent;

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
        FVector LastCalculatedPoleTarget = (OriginLocation + TargetLocation) / 2 + GetUpVector() * 10000;
        
        for (int32 i = 0; i < Segments.Num(); i++)
        {
            const FMPAS_LimbSegmentData& Seg = Segments[i];
            if (PoleTargets.Contains(i))
                LastCalculatedPoleTarget = CalculatePoleTargetLocation(PoleTargets[i]);
            
            L_PoleTargets.Add(LastCalculatedPoleTarget);
            //L_PoleTargets.Add(FVector(0, 0, 0));
        }
        

        if (EnableAsyncCalculation)
        {
            // Calling the necessary algorithm on a background thread, so it doesn't waste the perfomance of the main one
            AsyncTask( ENamedThreads::AnyBackgroundThreadNormalTask, [L_Algorithm, L_Segments, L_State, OriginLocation, TargetLocation, L_PoleTargets, L_MaxIterations, L_Tollerance, L_UpVector, L_EnableRollRecalculation, L_LimbRoll, L_LimbMaxExtent, this] ()
            {
                // New state declaration
                TArray<FMPAS_LimbSegmentState> NewState;

                // Selecting an algorithm and calling solving 
                switch (L_Algorithm)
                {
                case EMPAS_LimbSolvingAlgorithm::FABRIK_IK: NewState = Solve_FABRIK_IK(OriginLocation, TargetLocation, L_Segments, L_State, L_MaxIterations, L_Tollerance); break;
                case EMPAS_LimbSolvingAlgorithm::FABRIK_Limited_IK: NewState = Solve_FABRIK_Limited_IK(OriginLocation, TargetLocation, L_Segments, L_State, L_MaxIterations, L_Tollerance); break;
                case EMPAS_LimbSolvingAlgorithm::CCD_IK: NewState = Solve_CCD_IK(OriginLocation, TargetLocation, L_Segments, L_State, L_MaxIterations, L_Tollerance); break;
                case EMPAS_LimbSolvingAlgorithm::PoleFABRIK_IK: NewState = Solve_PoleFABRIK_IK(OriginLocation, TargetLocation, L_Segments, L_State, L_PoleTargets, L_MaxIterations, L_Tollerance, L_UpVector); break;
                case EMPAS_LimbSolvingAlgorithm::PoleFABRIK_Limited_IK: NewState = Solve_PoleFABRIK_Limited_IK(OriginLocation, TargetLocation, L_Segments, L_State, L_PoleTargets, L_MaxIterations, L_Tollerance, L_UpVector); break;
                case EMPAS_LimbSolvingAlgorithm::PistonMulti: NewState = Solve_Piston_Multi(OriginLocation, TargetLocation, L_Segments, L_State, L_LimbMaxExtent); break;
                case EMPAS_LimbSolvingAlgorithm::PistonSequential: NewState = Solve_Piston_Sequential(OriginLocation, TargetLocation, L_Segments, L_State); break;
                case EMPAS_LimbSolvingAlgorithm::RotateToTarget: NewState = Solve_RotateToTarget(OriginLocation, TargetLocation, L_Segments, L_State); break;

                //case EMPAS_LimbSolvingAlgorithm::Gauss_Seidel: NewState = Solve_Gauss_Seidel_IK(OriginLocation, TargetLocation, L_Segments, L_State, L_PoleTargets, L_MaxIterations, L_Tollerance, L_UpVector); break;

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
            case EMPAS_LimbSolvingAlgorithm::FABRIK_Limited_IK: NewState = Solve_FABRIK_Limited_IK(OriginLocation, TargetLocation, L_Segments, L_State, L_MaxIterations, L_Tollerance); break;
            case EMPAS_LimbSolvingAlgorithm::CCD_IK: NewState = Solve_CCD_IK(OriginLocation, TargetLocation, L_Segments, L_State, L_MaxIterations, L_Tollerance); break;
            case EMPAS_LimbSolvingAlgorithm::PoleFABRIK_IK: NewState = Solve_PoleFABRIK_IK(OriginLocation, TargetLocation, L_Segments, L_State, L_PoleTargets, L_MaxIterations, L_Tollerance, L_UpVector); break;
            case EMPAS_LimbSolvingAlgorithm::PoleFABRIK_Limited_IK: NewState = Solve_PoleFABRIK_Limited_IK(OriginLocation, TargetLocation, L_Segments, L_State, L_PoleTargets, L_MaxIterations, L_Tollerance, L_UpVector); break;
            case EMPAS_LimbSolvingAlgorithm::PistonMulti: NewState = Solve_Piston_Multi(OriginLocation, TargetLocation, L_Segments, L_State, L_LimbMaxExtent); break;
            case EMPAS_LimbSolvingAlgorithm::PistonSequential: NewState = Solve_Piston_Sequential(OriginLocation, TargetLocation, L_Segments, L_State); break;
            case EMPAS_LimbSolvingAlgorithm::RotateToTarget: NewState = Solve_RotateToTarget(OriginLocation, TargetLocation, L_Segments, L_State); break;

            //case EMPAS_LimbSolvingAlgorithm::Gauss_Seidel: NewState = Solve_Gauss_Seidel_IK(OriginLocation, TargetLocation, L_Segments, L_State, L_PoleTargets, L_MaxIterations, L_Tollerance, L_UpVector); break;

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
        GetHandler()->SetBoneLocation(Segments[InSegment].BoneName, InState.Location);
        GetHandler()->SetBoneRotation(Segments[InSegment].BoneName, InState.Rotation);
    }
}


// Handles interpolation of the current state to the target state
void UMPAS_Limb::InterpolateLimb(float DeltaTime)
{
    // Ignore interpolation if LimbInterpolationSpeed is set to 0 or less
    if (LimbInterpolationSpeed > 0)
    {
        FVector NextSegmentLocation = GetComponentLocation();

        for (int i = 0 ; i < CurrentState.Num() - 1; i++)
        {
            CurrentState[i].Rotation = UKismetMathLibrary::RInterpTo_Constant(CurrentState[i].Rotation, TargetState[i].Rotation, DeltaTime, LimbInterpolationSpeed);

            CurrentState[i].Location = NextSegmentLocation;

            NextSegmentLocation = CurrentState[i].Location + Segments[i].Length * UKismetMathLibrary::GetForwardVector(CurrentState[i].Rotation);
        }

        // Fective segment's location update
        CurrentState[CurrentState.Num() - 1].Location = NextSegmentLocation;
    }

    else
        CurrentState = TargetState;


    // Writing states to send data to the handler
    for (int32 i = 0; i < Segments.Num(); i++)
        WriteSegmentState(i, CurrentState[i]);
}

// Calculates resulting location of the pole target in world space
FVector UMPAS_Limb::CalculatePoleTargetLocation(const FMPAS_LimbPoleTarget& InPoleTargetSettings)
{
    // Auto calculation mode
    if (InPoleTargetSettings.LocationMode == EMPAS_LimbPoleTargetLocationMode::AutoCalculation)
    {
        FVector ForwardVector = (GetLimbTarget() - GetComponentLocation()).GetSafeNormal();
        FVector UpVector = GetUpVector();
        FVector RightVector = UKismetMathLibrary::Cross_VectorVector(ForwardVector, UpVector).GetSafeNormal();

        return GetComponentLocation() + (ForwardVector * InPoleTargetSettings.AUTO_PoleTargetOffset.X 
                        + RightVector * InPoleTargetSettings.AUTO_PoleTargetOffset.Y
                        + UpVector * InPoleTargetSettings.AUTO_PoleTargetOffset.Z);
    }

    // Location stack mode
    if (InPoleTargetSettings.LocationMode == EMPAS_LimbPoleTargetLocationMode::PoleTargetLocationStack)
    {
        return CalculateVectorStackValue(InPoleTargetSettings.STACK_VectorStackID);
    }

    return FVector::Zero();
}


// Algorithms
// 'static' because they are going to run in a background thread


// Rotate To Target
TArray<FMPAS_LimbSegmentState> UMPAS_Limb::Solve_RotateToTarget(const FVector& InOriginLocation, const FVector& InTargetLocation, const TArray<FMPAS_LimbSegmentData>& InSegments, const TArray<FMPAS_LimbSegmentState>& InCurrentState)
{
    TArray<FMPAS_LimbSegmentState> State;
    State.SetNum(InCurrentState.Num());

    FVector Direction = (InTargetLocation - InOriginLocation).GetSafeNormal();
    FRotator Rotation = Direction.Rotation();

    State[0].Location = InOriginLocation;
    State[0].Rotation = Rotation;

    for (int32 i = 1; i < State.Num(); i++)
    {
        State[i].Location = State[i - 1].Location + Direction * InSegments[i - 1].Length;
        State[i].Rotation = Rotation;
    }

    return State;
}


// FABRIK IK
TArray<FMPAS_LimbSegmentState> UMPAS_Limb::Solve_FABRIK_IK(const FVector& InOriginLocation, const FVector& InTargetLocation, const TArray<FMPAS_LimbSegmentData>& InSegments, const TArray<FMPAS_LimbSegmentState>& InCurrentState, int32 InMaxIterations, float InTollerance)
{
    TArray<FMPAS_LimbSegmentState> State = InCurrentState;

    size_t Iteration = 0;

    while ((Iteration < InMaxIterations) && (((State[State.Num() - 1].Location - InTargetLocation).Size() > InTollerance) || ((State[0].Location - InOriginLocation).Size() > InTollerance * 0.1f)))
    {
        // Forward-Reaching pass
        State[State.Num() - 1].Location = InTargetLocation;
        for (size_t i = State.Num() - 1; i > 0; i--)
        {
            FVector DirectionVector = (State[i - 1].Location - State[i].Location).GetSafeNormal();

            State[i - 1].Location = State[i].Location + DirectionVector * InSegments[i - 1].Length;
            State[i - 1].Rotation = (-1 * DirectionVector).Rotation();
        }

        // Backward-Reaching pass
        State[0].Location = InOriginLocation;
        for (size_t i = 0; i < State.Num() - 1; i++)
        {
            FVector DirectionVector = (State[i + 1].Location - State[i].Location).GetSafeNormal();

            State[i + 1].Location = State[i].Location + DirectionVector * InSegments[i].Length;
            State[i].Rotation = DirectionVector.Rotation();
        }

        Iteration++;
    }

    return State;
}

// FABRIK Limited
TArray<FMPAS_LimbSegmentState> UMPAS_Limb::Solve_FABRIK_Limited_IK(const FVector& InOriginLocation, const FVector& InTargetLocation, const TArray<FMPAS_LimbSegmentData>& InSegments, const TArray<FMPAS_LimbSegmentState>& InCurrentState, int32 InMaxIterations, float InTollerance)
{
    TArray<FMPAS_LimbSegmentState> State = InCurrentState;

    size_t Iteration = 0;

    while ((Iteration < InMaxIterations) && (((State[State.Num() - 1].Location - InTargetLocation).Size() > InTollerance) || ((State[0].Location - InOriginLocation).Size() > InTollerance * 0.1f)))
    {
        // Forward-Reaching pass
        State[State.Num() - 1].Location = InTargetLocation;
        for (size_t i = State.Num() - 1; i > 0; i--)
        {
            FVector DirectionVector = (State[i - 1].Location - State[i].Location).GetSafeNormal();

            // Angular limits
            if (i > 1)
            {
                FRotator DirectionRotation = (-1 * DirectionVector).Rotation();

                FRotator RelativeRotation = UKismetMathLibrary::NormalizedDeltaRotator(DirectionRotation, State[i - 2].Rotation);
                FRotator ClampedRelativeRotation = FRotator(
                    UKismetMathLibrary::FClamp(RelativeRotation.Pitch, InSegments[i - 1].AngularLimits_Min.Pitch, InSegments[i - 1].AngularLimits_Max.Pitch),
                    UKismetMathLibrary::FClamp(RelativeRotation.Yaw, InSegments[i - 1].AngularLimits_Min.Yaw, InSegments[i - 1].AngularLimits_Max.Yaw),
                    UKismetMathLibrary::FClamp(RelativeRotation.Roll, InSegments[i - 1].AngularLimits_Min.Roll, InSegments[i - 1].AngularLimits_Max.Roll)
                );

                DirectionRotation = State[i - 2].Rotation + ClampedRelativeRotation;
                DirectionVector = -1 * DirectionRotation.Vector();
            }

            State[i - 1].Location = State[i].Location + DirectionVector * InSegments[i - 1].Length;
            State[i - 1].Rotation = (-1 * DirectionVector).Rotation();
        }

        // Backward-Reaching pass
        State[0].Location = InOriginLocation;
        for (size_t i = 0; i < State.Num() - 1; i++)
        {
            FVector DirectionVector = (State[i + 1].Location - State[i].Location).GetSafeNormal();

            // Angular limits
            if (i > 0)
            {
                FRotator DirectionRotation = DirectionVector.Rotation();

                FRotator RelativeRotation = UKismetMathLibrary::NormalizedDeltaRotator(DirectionRotation, State[i - 1].Rotation);
                FRotator ClampedRelativeRotation = FRotator(
                    UKismetMathLibrary::FClamp(RelativeRotation.Pitch, InSegments[i].AngularLimits_Min.Pitch, InSegments[i].AngularLimits_Max.Pitch),
                    UKismetMathLibrary::FClamp(RelativeRotation.Yaw, InSegments[i].AngularLimits_Min.Yaw, InSegments[i].AngularLimits_Max.Yaw),
                    UKismetMathLibrary::FClamp(RelativeRotation.Roll, InSegments[i].AngularLimits_Min.Roll, InSegments[i].AngularLimits_Max.Roll)
                );

                DirectionRotation = State[i - 1].Rotation + ClampedRelativeRotation;
                DirectionVector = DirectionRotation.Vector();
            }

            State[i + 1].Location = State[i].Location + DirectionVector * InSegments[i].Length;
            State[i].Rotation = DirectionVector.Rotation();
        }

        Iteration++;
    }

    return State;
}


// CCD IK
TArray<FMPAS_LimbSegmentState> UMPAS_Limb::Solve_CCD_IK(const FVector& InOriginLocation, const FVector& InTargetLocation, const TArray<FMPAS_LimbSegmentData>& InSegments, const TArray<FMPAS_LimbSegmentState>& InCurrentState, int32 InMaxIterations, float InTollerance)
{
    TArray<FMPAS_LimbSegmentState> State = InCurrentState;

    State[0].Location = InOriginLocation;

    // Initial location recalculation, handling cases, where target location didn't change, but the origin location did
    for (int32 j = 1; j < State.Num(); j++)
        State[j].Location = State[j - 1].Location + UKismetMathLibrary::GetForwardVector(State[j - 1].Rotation) * InSegments[j - 1].Length;
   
    size_t Iteration = 0;
    while ((Iteration < InMaxIterations) && ( (State[State.Num() - 1].Location - InTargetLocation).Size() > InTollerance) )
    {
        // Main CCD pass
        for (int32 i = State.Num() - 2; i >= 0; i--)
        {   
            State[i].Rotation = UKismetMathLibrary::ComposeRotators(UKismetMathLibrary::NormalizedDeltaRotator(UKismetMathLibrary::FindLookAtRotation(State[i].Location, State[State.Num() - 1].Location), UKismetMathLibrary::FindLookAtRotation(State[i].Location, InTargetLocation)), State[i].Rotation);

            // Forward pass to recalculate segment locations
            for (int32 j = 1; j < State.Num(); j++)
                State[j].Location = State[j - 1].Location + UKismetMathLibrary::GetForwardVector(State[j - 1].Rotation) * InSegments[j - 1].Length;
        }

        Iteration++;
    }

    if ((State[State.Num() - 1].Location - InTargetLocation).Size() <= InTollerance)
        return State;

    return InCurrentState;
}


// PoleFABRIK IK - custom version of FABRIK IK, sligtly slower, but implements support for pole targets, making it the most usable algorithm ot of the ones presented here
TArray<FMPAS_LimbSegmentState> UMPAS_Limb::Solve_PoleFABRIK_IK(const FVector& InOriginLocation, const FVector& InTargetLocation, const TArray<FMPAS_LimbSegmentData>& InSegments, const TArray<FMPAS_LimbSegmentState>& InCurrentState, const TArray<FVector>& InPoleTargets, int32 InMaxIterations, float InTollerance, const FVector& InUpVector)
{
    TArray<FMPAS_LimbSegmentState> State;
    State.SetNum(InCurrentState.Num());

    // Reinitiating state to match pole targets

    State[0].Location = InOriginLocation;
    for (int32 i = 0; i < InSegments.Num(); i++)
        State[i + 1].Location = State[i].Location + (InPoleTargets[i] - State[i].Location).GetSafeNormal() * InSegments[i].Length;

    // FABRIK iterating
    // Solved locationss are projected onto a solution plane (see ProjectLocationOnToSolutionPlane description) to keep the movement natural

    size_t Iteration = 0;
    while ((Iteration < InMaxIterations) && (((State[State.Num() - 1].Location - InTargetLocation).Size() > InTollerance) || ((State[0].Location - InOriginLocation).Size() > InTollerance * 0.1f)))
    {
        // Forward-Reaching pass
        State[State.Num() - 1].Location = InTargetLocation;
        for (size_t i = State.Num() - 1; i > 0; i--)
        {
            FVector DirectionVector = (State[i - 1].Location - State[i].Location).GetSafeNormal();

            State[i - 1].Location = State[i].Location + DirectionVector * InSegments[i - 1].Length;
            State[i - 1].Rotation = (-1 * DirectionVector).Rotation();
        }

        // Backward-Reaching pass
        State[0].Location = InOriginLocation;
        for (size_t i = 0; i < State.Num() - 2; i++)
        {
            FVector DirectionVector = (State[i + 1].Location - State[i].Location).GetSafeNormal();

            State[i + 1].Location = State[i].Location + DirectionVector * InSegments[i].Length;
            State[i].Rotation = DirectionVector.Rotation();
        }

        Iteration++;
    }

    return State;
}

// PoleFABRIK IK - custom version of FABRIK IK, sligtly slower, but implements support for pole targets, making it the most usable algorithm ot of the ones presented here
TArray<FMPAS_LimbSegmentState> UMPAS_Limb::Solve_PoleFABRIK_Limited_IK(const FVector& InOriginLocation, const FVector& InTargetLocation, const TArray<FMPAS_LimbSegmentData>& InSegments, const TArray<FMPAS_LimbSegmentState>& InCurrentState, const TArray<FVector>& InPoleTargets, int32 InMaxIterations, float InTollerance, const FVector& InUpVector)
{
    TArray<FMPAS_LimbSegmentState> State;
    State.SetNum(InCurrentState.Num());

    // Reinitiating state to match pole targets

    State[0].Location = InOriginLocation;
    for (int32 i = 0; i < InSegments.Num(); i++)
        State[i + 1].Location = State[i].Location + (InPoleTargets[i] - State[i].Location).GetSafeNormal() * InSegments[i].Length;

    // FABRIK iterating
    // Solved locationss are projected onto a solution plane (see ProjectLocationOnToSolutionPlane description) to keep the movement natural

    size_t Iteration = 0;
    while ((Iteration < InMaxIterations) && (((State[State.Num() - 1].Location - InTargetLocation).Size() > InTollerance) || ((State[0].Location - InOriginLocation).Size() > InTollerance * 0.1f)))
    {
        // Forward-Reaching pass
        State[State.Num() - 1].Location = InTargetLocation;
        for (size_t i = State.Num() - 1; i > 0; i--)
        {
            FVector DirectionVector = (State[i - 1].Location - State[i].Location).GetSafeNormal();

            // Angular limits
            if (i > 1)
            {
                FRotator DirectionRotation = (-1 * DirectionVector).Rotation();

                FRotator RelativeRotation = UKismetMathLibrary::NormalizedDeltaRotator(DirectionRotation, State[i - 2].Rotation);
                FRotator ClampedRelativeRotation = FRotator(
                    UKismetMathLibrary::FClamp(RelativeRotation.Pitch, InSegments[i - 1].AngularLimits_Min.Pitch, InSegments[i - 1].AngularLimits_Max.Pitch),
                    UKismetMathLibrary::FClamp(RelativeRotation.Yaw, InSegments[i - 1].AngularLimits_Min.Yaw, InSegments[i - 1].AngularLimits_Max.Yaw),
                    UKismetMathLibrary::FClamp(RelativeRotation.Roll, InSegments[i - 1].AngularLimits_Min.Roll, InSegments[i - 1].AngularLimits_Max.Roll)
                );

                DirectionRotation = State[i - 2].Rotation + ClampedRelativeRotation;
                DirectionVector = -1 * DirectionRotation.Vector();
            }

            State[i - 1].Location = State[i].Location + DirectionVector * InSegments[i - 1].Length;
            State[i - 1].Rotation = (-1 * DirectionVector).Rotation();
        }

        // Backward-Reaching pass
        State[0].Location = InOriginLocation;
        for (size_t i = 0; i < State.Num() - 2; i++)
        {
            FVector DirectionVector = (State[i + 1].Location - State[i].Location).GetSafeNormal();

            // Angular limits
            if (i > 0)
            {
                FRotator DirectionRotation = DirectionVector.Rotation();

                FRotator RelativeRotation = UKismetMathLibrary::NormalizedDeltaRotator(DirectionRotation, State[i - 1].Rotation);
                FRotator ClampedRelativeRotation = FRotator(
                    UKismetMathLibrary::FClamp(RelativeRotation.Pitch, InSegments[i].AngularLimits_Min.Pitch, InSegments[i].AngularLimits_Max.Pitch),
                    UKismetMathLibrary::FClamp(RelativeRotation.Yaw, InSegments[i].AngularLimits_Min.Yaw, InSegments[i].AngularLimits_Max.Yaw),
                    UKismetMathLibrary::FClamp(RelativeRotation.Roll, InSegments[i].AngularLimits_Min.Roll, InSegments[i].AngularLimits_Max.Roll)
                );

                DirectionRotation = State[i - 1].Rotation + ClampedRelativeRotation;
                DirectionVector = DirectionRotation.Vector();
            }

            State[i + 1].Location = State[i].Location + DirectionVector * InSegments[i].Length;
            State[i].Rotation = DirectionVector.Rotation();
        }

        Iteration++;
    }

    return State;
}

// Turns the limb into a telescopic multi-stage piston for mechanical effects, all segments extend at the same time
TArray<FMPAS_LimbSegmentState> UMPAS_Limb::Solve_Piston_Multi(const FVector& InOriginLocation, const FVector& InTargetLocation, const TArray<FMPAS_LimbSegmentData>& InSegments, const TArray<FMPAS_LimbSegmentState>& InCurrentState, float InLimbMaxExtent)
{
    TArray<FMPAS_LimbSegmentState> State;
    State.SetNum(InCurrentState.Num());

    float ExtentProportion = UKismetMathLibrary::FClamp(FVector::Distance(InOriginLocation, InTargetLocation) / InLimbMaxExtent, 0.f, 1.f);
    FVector ExtentDirection = (InTargetLocation - InOriginLocation).GetSafeNormal();

    FRotator Rotation = ExtentDirection.Rotation();

    State[0].Location = InOriginLocation;
    State[0].Rotation = Rotation;

    for (int32 i = 1; i < State.Num() - 1; i++)
    {
        State[i].Location = State[i - 1].Location + ExtentDirection * InSegments[i - 1].Length - ExtentDirection * (i == 1 ? InSegments[0].Length + InSegments[1].Length : InSegments[i].Length) * (1 - ExtentProportion);

        State[i].Rotation = Rotation;
    }

    State[State.Num() - 1].Location = State[State.Num() - 2].Location + ExtentDirection * InSegments[State.Num() - 2].Length;

    return State;
}


// Turns the limb into a telescopic multi-stage piston for mechanical effects, segments extend one by one
TArray<FMPAS_LimbSegmentState> UMPAS_Limb::Solve_Piston_Sequential(const FVector& InOriginLocation, const FVector& InTargetLocation, const TArray<FMPAS_LimbSegmentData>& InSegments, const TArray<FMPAS_LimbSegmentState>& InCurrentState)
{
    TArray<FMPAS_LimbSegmentState> State = InCurrentState;

    float RequiredDistance = FVector::Distance(InOriginLocation, InTargetLocation);
    FVector ExtentDirection = (InTargetLocation - InOriginLocation).GetSafeNormal();

    FRotator Rotation = ExtentDirection.Rotation();

    State[0].Location = InOriginLocation;
    State[0].Rotation = Rotation;

    RequiredDistance = (RequiredDistance - InSegments[0].Length < 0) ? 0 : (RequiredDistance - InSegments[0].Length);

    for (int32 i = 1; i < State.Num() - 1; i++)
    {
        float ExtentProportion = UKismetMathLibrary::FClamp(RequiredDistance / InSegments[i].Length, 0.f, 1.f);
        
        State[i].Location = State[i - 1].Location + ExtentDirection * InSegments[i - 1].Length - ExtentDirection * InSegments[i].Length * (1 - ExtentProportion);
        State[i].Rotation = Rotation;

        RequiredDistance = (RequiredDistance - InSegments[i].Length * ExtentProportion < 0) ? 0 : (RequiredDistance - InSegments[i].Length * ExtentProportion);
    }

    State[State.Num() - 1].Location = State[State.Num() - 2].Location + ExtentDirection * InSegments[State.Num() - 2].Length;

    return State;
}


//// Gauss-Seidel IK
//TArray<FMPAS_LimbSegmentState> UMPAS_Limb::Solve_Gauss_Seidel_IK(const FVector& InOriginLocation, const FVector& InTargetLocation, const TArray<FMPAS_LimbSegmentData>& InSegments, const TArray<FMPAS_LimbSegmentState>& InCurrentState, const TArray<FVector>& InPoleTargets, int32 InMaxIterations, float InTollerance, const FVector& InUpVector)
//{
//    TArray<FMPAS_LimbSegmentState> State = InCurrentState;
//
//    return State;
//}



// Recalculating segment roll rotation
void UMPAS_Limb::RecalculateRoll(TArray<FMPAS_LimbSegmentState>& InOutState, float InLimbRoll, const FVector& InOriginLocation, const FVector& InTargetLocaiton, const TArray<FMPAS_LimbSegmentData>& InSegments, const TArray<FVector>& InPoleTargets, const FVector& InUpVector)
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
            FVector OriginToStart = InOutState[i].Location - InOriginLocation;
            FVector OriginToEnd = InOutState[i].Location + UKismetMathLibrary::GetForwardVector(InOutState[i].Rotation) * InSegments[i].Length - InOriginLocation;

            SolutionPlaneNormal = UKismetMathLibrary::Cross_VectorVector(OriginToStart, OriginToEnd).GetSafeNormal();

            TargetDirection = (InOutState[i].Location + InOutState[i].Location + UKismetMathLibrary::GetForwardVector(InOutState[i].Rotation) * InSegments[i].Length) / 2.f - InOriginLocation;
        }

        // First segment case
        else
        {
            TargetDirection = InUpVector;

            SolutionPlaneNormal = UKismetMathLibrary::Cross_VectorVector(InUpVector, UKismetMathLibrary::GetForwardVector(InOutState[i].Rotation)).GetSafeNormal();
        }

        // Solution
        FVector TargetNormal = UKismetMathLibrary::Cross_VectorVector(SolutionPlaneNormal, UKismetMathLibrary::GetForwardVector(InOutState[i].Rotation)).GetSafeNormal();

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

    case EMPAS_LimbTargetType::TargetVectorStack:
        Target = CalculateVectorStackValue(TargetStackID);
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

// Changes the element the limb origin is attached to (by default it is the limb parent)
void UMPAS_Limb::OverrideAttachmentParent(UMPAS_RigElement* NewParent)
{
    if (NewParent)
    {
        SetVectorSourceValue(0, 0, ParentElement, FVector::ZeroVector);
        SetRotationSourceValue(0, 0, ParentElement, FRotator::ZeroRotator);

        ParentElement = NewParent;
        FName ParentElementName = ParentElement->RigElementName;

        // Parent location and rotation initial cache
        SetVectorSourceValue(0, 0, ParentElement, ParentElement->GetComponentLocation());
        SetRotationSourceValue(0, 0, ParentElement, ParentElement->GetComponentRotation());

        // Self location and rotation fetching

        // Setting initial location / rotation
        InitialSelfTransform.SetLocation(UKismetMathLibrary::Quat_UnrotateVector(ParentElement->GetComponentRotation().Quaternion(), GetComponentLocation() - ParentElement->GetComponentLocation()));
        InitialSelfTransform.SetRotation(UKismetMathLibrary::NormalizedDeltaRotator(GetComponentRotation(), ParentElement->GetComponentRotation()).Quaternion());

        SetVectorSourceValue(0, 1, this, UKismetMathLibrary::Quat_RotateVector(ParentElement->GetComponentRotation().Quaternion(), InitialSelfTransform.GetLocation()));
        SetRotationSourceValue(0, 1, this, FRotator(InitialSelfTransform.GetRotation()));
    }
}




// CALLED BY THE HANDLER
// Initializing Rig Element
void UMPAS_Limb::InitRigElement(class UMPAS_Handler* InHandler)
{
    Super::InitRigElement(InHandler);

    if (TargetType == EMPAS_LimbTargetType::TargetVectorStack)
        TargetStackID = RegisterVectorStack("LimbTarget");

    for (auto& Pole: PoleTargets)
        if (Pole.Value.LocationMode == EMPAS_LimbPoleTargetLocationMode::PoleTargetLocationStack)
            Pole.Value.STACK_VectorStackID = RegisterVectorStack("PoleTarget_" + UKismetStringLibrary::Conv_IntToString(Pole.Key));

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

    if (IsCoreElement) return;

    SetWorldRotation(FRotator::ZeroRotator);

    if (Initialized && GetRigElementActive() && !GetPhysicsModelEnabled())
    {
        InterpolateLimb(DeltaTime);
        SolveLimb();
    }
}

// CALLED BY THE HANDLER : Synchronizes Rig Element to the most recently fetched bone transforms
void UMPAS_Limb::SyncToFetchedBoneTransforms()
{
    for (int32 i = 0; i < Segments.Num(); i++)
    {
        auto BoneTransform = GetHandler()->GetCachedFetchedBoneTransforms().Find(Segments[i].BoneName);
        if (BoneTransform)
        {
            CurrentState[i].Location = (*BoneTransform).GetLocation();
            CurrentState[i].Rotation = (*BoneTransform).GetRotation().Rotator();
        }
    }
}


// PHYSICS MODEL

// Called when physics model is enabled, applies position and rotation of physics elements to the rig element
void UMPAS_Limb::ApplyPhysicsModelLocationAndRotation_Implementation(float DeltaTime)
{
    for(int32 i = 0; i < PhysicsElementsConfiguration.Num(); i++)
    {
        FMPAS_LimbSegmentState NewState;
        NewState.Location = CalculateVectorStackValue(PhysicsElementsConfiguration[i].PositionStackID);
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
        LimbSegmentTransform.SetLocation(CurrentState[PhysicsElementID].Location);
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

// Projects location vector on to a "solution plane" (plane constructed from 3 points: Limb Origin, Limb Target and Pole Target)
FVector UMPAS_Limb::ProjectLocationOnToSolutionPlane(const FVector& InLocation, const FVector& InLimbOrigin, const FVector& InLimbTarget, const FVector& InPoleTarget)
{
    FVector PlaneNormal = UKismetMathLibrary::Cross_VectorVector(InLimbTarget - InLimbOrigin, InPoleTarget - InLimbOrigin).GetSafeNormal();
    FVector Projection = UKismetMathLibrary::ProjectVectorOnToPlane(InLocation - InLimbOrigin, PlaneNormal);
    
    return Projection + InLimbOrigin;
}