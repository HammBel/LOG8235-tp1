// Fill out your copyright notice in the Description page of Project Settings.

#include "SDTUtils.h"
#include "SoftDesignTraining.h"
#include "SoftDesignTrainingMainCharacter.h"
#include "DrawDebugHelpers.h"
#include "Engine/World.h"
#include "Engine/OverlapResult.h"

/*static*/ bool SDTUtils::Raycast(UWorld* uWorld, FVector sourcePoint, FVector targetPoint)
{
    FHitResult hitData;
    FCollisionQueryParams TraceParams(FName(TEXT("VictoreCore Trace")), true);

    return uWorld->LineTraceSingleByChannel(hitData, sourcePoint, targetPoint, ECC_Pawn, TraceParams);
}

bool SDTUtils::IsPlayerPoweredUp(UWorld * uWorld)
{
    ACharacter* playerCharacter = UGameplayStatics::GetPlayerCharacter(uWorld, 0);
    if (!playerCharacter)
        return false;

    ASoftDesignTrainingMainCharacter* castedPlayerCharacter = Cast<ASoftDesignTrainingMainCharacter>(playerCharacter);
    if (!castedPlayerCharacter)
        return false;

    return castedPlayerCharacter->IsPoweredUp();
}

// Fonction RayCast initialement présente dans le projet ne nous permettais pas d'obtenir les hits obtenue lors du raycast, alors nous avons implémenté une nouvelle.
bool SDTUtils::CastRay(UWorld* uWorld, const FVector& start, const FVector& end, TArray<struct FHitResult>& outHits, bool drawDebug, FCollisionObjectQueryParams* params)
{
    if (uWorld == nullptr)
        return false;

    //Draw ray
    if (drawDebug)
        DrawDebugLine(uWorld, start, end, FColor::Green);

    FCollisionObjectQueryParams* objectQueryParams = params;
    //Multi line trace
    if (!params) {
        FCollisionObjectQueryParams objectParams = FCollisionObjectQueryParams::DefaultObjectQueryParam;
        objectParams.AddObjectTypesToQuery(ECC_WorldStatic);
        objectParams.AddObjectTypesToQuery(ECC_WorldDynamic);
        objectQueryParams = &objectParams;
    }
    FCollisionQueryParams queryParams = FCollisionQueryParams::DefaultQueryParam;
    queryParams.bReturnPhysicalMaterial = true;
 
    uWorld->LineTraceMultiByObjectType(outHits, start, end, *objectQueryParams, queryParams);

    //Draw hits
    if (drawDebug)
    {
        for (int32 i = 0; i < outHits.Num(); ++i)
            DebugDrawHitPoint(uWorld, outHits[i]);
    }

    return outHits.Num() > 0;
}


bool SDTUtils::SphereOverlap(UWorld* uWorld, const FVector& pos, float radius, TArray<struct FOverlapResult>& outOverlaps, bool drawDebug)
{
    if (uWorld == nullptr)
        return false;

    if (drawDebug)
        DrawDebugSphere(uWorld, pos, radius, 24, FColor::Green);


    FCollisionObjectQueryParams objectQueryParams;
    FCollisionShape collisionShape;
    collisionShape.SetSphere(radius);
    FCollisionQueryParams queryParams = FCollisionQueryParams::DefaultQueryParam;
    queryParams.bReturnPhysicalMaterial = true;

    uWorld->OverlapMultiByObjectType(outOverlaps, pos, FQuat::Identity, objectQueryParams, collisionShape, queryParams);

    if (drawDebug)
    {
        for (int32 i = 0; i < outOverlaps.Num(); ++i)
        {
            if (outOverlaps[i].GetComponent())
                DebugDrawPrimitive(uWorld , *(outOverlaps[i].GetComponent()));
        }
    }

    return outOverlaps.Num() > 0;
}

void SDTUtils::DebugDrawHitPoint(UWorld* uWorld, const FHitResult& hit)
{
    DrawDebugPoint(uWorld, hit.ImpactPoint, 10.0f, FColor::Red, false, 1.0f, 0);

    const float arrowLength = 20.0f;
    DrawDebugDirectionalArrow(uWorld, hit.ImpactPoint, hit.ImpactPoint + hit.ImpactNormal * arrowLength, 2, FColor::Blue, false, 1.0f, 0, 2);
}

void SDTUtils::DebugDrawPrimitive(UWorld* uWorld, const UPrimitiveComponent& primitive)
{
    FVector center = primitive.Bounds.Origin;
    FVector extent = primitive.Bounds.BoxExtent;

    DrawDebugBox(uWorld, center, extent, FColor::Red);
}


/**
 * Effectue un "Box Cast" (sweep avec une boîte 3D) entre deux points dans le monde.
 *
 * Entrées :
 *   @param uWorld        Monde de jeu actuel (UWorld*).
 *   @param start         Position de départ du balayage.
 *   @param end           Position d’arrivée du balayage.
 *   @param halfExtents   Demi-tailles de la box (X,Y,Z).
 *   @param orientation   Orientation de la box (FQuat).
 *   @param outHits       Tableau des impacts détectés (résultat rempli).
 *   @param shouldCheckWall Indique si on doit aussi vérifier les murs (WorldStatic).
 *   @param drawDebug     Active/désactive le dessin de debug (box, ligne, impacts).
 *
 * Retour :
 *   @return true  si au moins une collision est détectée.
 *   @return false sinon.
 */

bool SDTUtils::BoxCast(
    UWorld* uWorld,
    const FVector& start,
    const FVector& end,
    const FVector& halfExtents, 
    const FQuat& orientation,  
    TArray<FHitResult>& outHits,
    bool shouldCheckWall,
    bool drawDebug
)
{
    if (uWorld == nullptr)
        return false;

    // Debug draw
    if (drawDebug)
    {
        FVector center = (start + end) * 0.5f;
        FVector dir;
        float length;
        (end - start).ToDirectionAndLength(dir, length);

        DrawDebugBox(uWorld, start, halfExtents, orientation, FColor::Green);
        DrawDebugBox(uWorld, end, halfExtents, orientation, FColor::Green);
        DrawDebugLine(uWorld, start, end, FColor::Red);
    }

    FCollisionObjectQueryParams objectQueryParams;
    objectQueryParams.AddObjectTypesToQuery(ECC_GameTraceChannel3);                 // Death Floor
    if (shouldCheckWall) objectQueryParams.AddObjectTypesToQuery(ECC_WorldStatic);  // Wall
    FCollisionShape collisionShape;
    collisionShape = FCollisionShape::MakeBox(halfExtents);

    FCollisionQueryParams queryParams = FCollisionQueryParams::DefaultQueryParam;
    queryParams.bReturnPhysicalMaterial = true;

    bool hit = uWorld->SweepMultiByObjectType(
        outHits,
        start,
        end,
        orientation,
        objectQueryParams,
        collisionShape,
        queryParams
    );

    // Debug draw hit points
    if (drawDebug)
    {
        for (int32 i = 0; i < outHits.Num(); ++i)
        {
            DebugDrawHitPoint(uWorld, outHits[i]);
        }
    }

    return hit;
}