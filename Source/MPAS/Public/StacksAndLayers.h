// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "Kismet/BlueprintFunctionLibrary.h"
#include "StacksAndLayers.generated.h"


// ENUMS

// Blending mode for location/rotation/... layers in stacks
UENUM(BlueprintType)
enum class EMPAS_LayerBlendingMode : uint8
{
	Normal UMETA(DisplayName = "Normal"),
	Add UMETA(DisplayName = "Add"),
	Multiply UMETA(DisplayName = "Multiply")
};

// The rule by which the elements in a single layer are combined
UENUM(BlueprintType)
enum class EMPAS_LayerCombinationMode : uint8
{
	Add UMETA(DisplayName = "Add"),
	Multiply UMETA(DisplayName = "Multiply"),
	Average UMETA(DisplayName = "Average")
};


// CLASS TEMPLATES

template<typename T, const T& BaseValue>
struct FMPAS_LayerBase
{
	bool Enabled = true;

	EMPAS_LayerBlendingMode BlendingMode;

	float BlendingFactor = 1.f;

	EMPAS_LayerCombinationMode CombinationMode;

	TMap<class UMPAS_RigElement*, T> LayerElements;

	// Higher priority -> higher in the stack
	int Priority = 0;

	bool ForceAllElementsActive = false;


	FMPAS_LayerBase(EMPAS_LayerBlendingMode InBlendingMode = EMPAS_LayerBlendingMode::Normal,
		float InBlendingFactor = 1.f,
		EMPAS_LayerCombinationMode InLayerCombinationMode = EMPAS_LayerCombinationMode::Average,
		int InPriority = 0,
		bool InForceAllElementsActive = false) : 
		BlendingMode(InBlendingMode), BlendingFactor(InBlendingFactor), CombinationMode(InLayerCombinationMode), Priority(InPriority), ForceAllElementsActive(InForceAllElementsActive) {}


	// Calculates the resulting value from the layer elements
	T CalculateLayerValue(bool& OutHasActiveElements)
	{
		T OutValue = BaseValue;
		int32 ActiveElementCount = 0;

		switch (CombinationMode)
		{

		case EMPAS_LayerCombinationMode::Average:

			for (auto& Source : LayerElements)
			{
				bool CurrentActive = ForceAllElementsActive || Source.Key->GetRigElementActive();

				OutValue += Source.Value * CurrentActive;
				ActiveElementCount += CurrentActive;
			}

			if (ActiveElementCount > 0)
				OutValue = OutValue / ActiveElementCount;

			break;

		case EMPAS_LayerCombinationMode::Add:

			for (auto& Source : LayerElements)
			{
				bool CurrentActive = ForceAllElementsActive || Source.Key->GetRigElementActive();

				OutValue += Source.Value * CurrentActive;
				ActiveElementCount += CurrentActive;
			}
			break;

		case EMPAS_LayerCombinationMode::Multiply:

			for (auto& Source : LayerElements)
			{
				bool CurrentActive = ForceAllElementsActive || Source.Key->GetRigElementActive();

				ActiveElementCount += CurrentActive;
				if (CurrentActive)
					OutValue *= Source.Value;
			}
			break;

		default:
			OutValue = {};
			break;
		}

		OutHasActiveElements = ActiveElementCount > 0;
		return OutValue;
	}

	// Adds/Updates a sourced value in the layer. If succeded: returns true, false - overwise
	bool SetSourceValue(UMPAS_RigElement* InSourceElement, const T& InSourceValue)
	{
		LayerElements[InSourceElement] = InSourceValue;
	}

	// Removes a source value from the layer. If succeded: returns true, false - overwise
	bool RemoveVectorSourceValue(UMPAS_RigElement* InSourceElement)
	{
		return LayerElements.Remove(InSourceElement) != nullptr;
	}

	// Returns the value associated with the specified source, returns default value if source isn't valid
	const FVector& GetVectorSourceValue(UMPAS_RigElement* InSourceElement)
	{
		T* ValueP = LayerElements.Find(InSourceElement);
		if (ValueP)
			return *ValueP;

		return {};
	}
};


template<typename T, const T& BaseValue>
struct FMPAS_StackBase
{

	// [LayerID] -> <Layer>
	TArray<FMPAS_LayerBase<T, BaseValue>> Layers;

	// [Execution Step] -> <LayerID>
	TArray<int32> StackOrder;

	// Since normal layers override everything that lies beneath them, it is logical to start compuatation from the top-most normal layer with blending factor of 1.0f
	// But that layer may not be active for any number of reasons, so we cache all of the normal layers with blending factor of 1.0f in this array, so we can check if they are active in runtime
	// Layers are identified by their stack order id and placed top to bottom (0 - the top-most such layer)
	TArray<int32> StartingLayersCache;


	int32 Num() const { return Layers.Num(); }

	FMPAS_LayerBase<T, BaseValue>& operator[] (int32 InID) { return Layers[InID]; }


	// Adds a new layer into the stack and updates the stack order
	int32 AddLayer(FMPAS_LayerBase<T, BaseValue> InLayer)
	{
		// Storing the layer
		int32 ID = Layers.Add(InLayer);

		// Updating stack order
		int32 LastIndex = StackOrder.Add(ID);

		// Insertion sort for just the last element (works in O(N), N - number of elements in the stack)
		int32 i = LastIndex - 1;
		while (i >= 0 && Layers[StackOrder[i]].Priority > InLayer.Priority)
		{
			StackOrder[i + 1] = StackOrder[i];
			i--;
		}

		StackOrder[i + 1] = ID;
		RecalculateStartingLayerCache();

		return ID;
	}

	// Recalucates StartingLayersCache array, should be called whenether a new layer is added/removed or some normal layer's blending factor is changed
	void RecalculateStartingLayerCache()
	{
		StartingLayersCache.Empty();

		for (int32 i = Num() - 1; i >= 0; i--)
			if (Layers[StackOrder[i]].BlendingMode == EMPAS_LayerBlendingMode::Normal && Layers[StackOrder[i]].BlendingFactor == 1.f)
				StartingLayersCache.Add(i);
	}

	// Calculates the final value of the stack based on it's layers
	T CalculateStackValue()
	{
		T FinalValue;

		int32 StartingLayer = -1;
		for (int32 i = 0; i < StartingLayersCache.Num(); i++)
		{
			int32 StackOrderID = StartingLayersCache[i];

			FMPAS_LayerBase<T, BaseValue>& Layer = Layers[StackOrder[StackOrderID]];

			if (!Layer.Enabled) continue;

			bool HasActiveElements = false;
			FinalValue = Layer.CalculateLayerValue(HasActiveElements);

			if (HasActiveElements && Layer.BlendingFactor == 1.0f)
			{
				StartingLayer = StackOrderID + 1; // +1 because we have already calculated this layer's value and stored it in FinalValue
				break;
			}
		}

		if (StartingLayer == -1) // Rare case, when there are no normal layers with blending factor of 1.0f are present
		{
			FinalValue = BaseValue;
			StartingLayer = 0;
		}

		for (int32 LayerOrderID = StartingLayer; LayerOrderID < Num(); LayerOrderID++)
		{
			FMPAS_LayerBase<T, BaseValue>& Layer = Layers[StackOrder[LayerOrderID]];

			if (!Layer.Enabled) continue;

			bool HasActiveElements = false;
			T LayerValue = Layer.CalculateLayerValue(HasActiveElements);

			if (HasActiveElements)
			{
				switch (Layer.BlendingMode)
				{
				case EMPAS_LayerBlendingMode::Normal:
					FinalValue = UKismetMathLibrary::VLerp(FinalValue, LayerValue, Layer.BlendingFactor);
					break;

				case EMPAS_LayerBlendingMode::Add:
					FinalValue += LayerValue * Layer.BlendingFactor;
					break;

				case EMPAS_LayerBlendingMode::Multiply:
					FinalValue = UKismetMathLibrary::VLerp(FinalValue, FinalValue * LayerValue, Layer.BlendingFactor);
					break;

				default: break;
				}
			}
		}

		return FinalValue;
	}
};


// APPLIED STRUCT WRAPPERS
// Needed for blueprint exposure

USTRUCT(BlueprintType)
struct FMPAS_VectorLayer
{
	GENERATED_USTRUCT_BODY()

	// Actual layer data
	FMPAS_LayerBase<FVector> LayerData;

	FMPAS_VectorLayer(EMPAS_LayerBlendingMode InBlendingMode = EMPAS_LayerBlendingMode::Normal,
		float InBlendingFactor = 1.f,
		EMPAS_LayerCombinationMode InLayerCombinationMode = EMPAS_LayerCombinationMode::Average,
		int InPriority = 0,
		bool InForceAllElementsActive = false) :

		LayerData(InBlendingMode, InBlendingFactor, InLayerCombinationMode, InPriority, InForceAllElementsActive) {}
};

USTRUCT(BlueprintType)
struct FMPAS_VectorStack
{
	GENERATED_USTRUCT_BODY()

	// Actual stack data
	FMPAS_StackBase<FVector> StackData;
};


/**
 * 
 */
UCLASS()
class MPAS_API UStacksAndLayers : public UBlueprintFunctionLibrary
{
	GENERATED_BODY()
	
};
