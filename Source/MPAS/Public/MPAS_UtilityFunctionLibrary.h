// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "Kismet/BlueprintFunctionLibrary.h"
#include "CollisionQueryParams.h"
#include "WorldCollision.h"
#include "MPAS_UtilityFunctionLibrary.generated.h"

/**
 * 
 */
UCLASS()
class MPAS_API UMPAS_UtilityFunctionLibrary : public UBlueprintFunctionLibrary
{
	GENERATED_BODY()
	
	// A wrapper around LineTraceByChannel that can be called from direct objects descendants in Blueprints
	UFUNCTION(BlueprintCallable, Category = "RawObjectCallable")
	static void RawObjectLineTraceByChannel(AActor* ContextActor, struct FHitResult& OutHit, const FVector& Start, const FVector& End, ECollisionChannel TraceChannel);
};
