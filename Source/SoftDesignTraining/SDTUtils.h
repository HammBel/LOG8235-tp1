// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#define COLLISION_DEATH_OBJECT		ECollisionChannel::ECC_GameTraceChannel3
#define COLLISION_PLAYER        	ECollisionChannel::ECC_GameTraceChannel4
#define COLLISION_COLLECTIBLE     	ECollisionChannel::ECC_GameTraceChannel5

class SOFTDESIGNTRAINING_API SDTUtils
{
public:
    static bool Raycast(UWorld* uWorld, FVector sourcePoint, FVector targetPoint);
    static bool IsPlayerPoweredUp(UWorld* uWorld);

    static bool CastRay(UWorld* uWorld, const FVector& start, const FVector& end, TArray<struct FHitResult>& outHits, bool drawDebug, FCollisionObjectQueryParams* params = nullptr);
    static bool SphereCast(UWorld* uWorld, const FVector& start, const FVector& end, float radius, TArray<struct FHitResult>& outHits, bool drawDebug);
    static bool SphereOverlap(UWorld* uWorld, const FVector& pos, float radius, TArray<struct FOverlapResult>& outOverlaps, bool drawdebug);



protected:

    static void DebugDrawHitPoint(UWorld* uWorld, const FHitResult& hit);
    static void DebugDrawPrimitive(UWorld* uWorld, const UPrimitiveComponent& primitive);
};
