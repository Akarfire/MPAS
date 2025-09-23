// Fill out your copyright notice in the Description page of Project Settings.


#include "MPAS_UtilityFunctionLibrary.h"

// A wrapper around LineTraceByChannel that can be called from direct objects descendants in Blueprints
bool UMPAS_UtilityFunctionLibrary::RawObjectLineTraceByChannel(AActor* ContextActor, FHitResult& OutHit, const FVector& Start, const FVector& End, ECollisionChannel TraceChannel)
{
	if (!ContextActor) return false;
	return ContextActor->GetWorld()->LineTraceSingleByChannel(OutHit, Start, End, TraceChannel);
}