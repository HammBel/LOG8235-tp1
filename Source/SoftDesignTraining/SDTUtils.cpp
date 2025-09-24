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

bool SDTUtils::CastRay(UWorld* uWorld, const FVector& start, const FVector& end, TArray<struct FHitResult>& outHits, bool drawDebug)
{
    if (uWorld == nullptr)
        return false;

    //Draw ray
    if (drawDebug)
        DrawDebugLine(uWorld, start, end, FColor::Green);


    //Multi line trace
    FCollisionObjectQueryParams objectQueryParams;
    objectQueryParams.AddObjectTypesToQuery(ECC_PhysicsBody);
    objectQueryParams.AddObjectTypesToQuery(ECC_WorldStatic);
    objectQueryParams.AddObjectTypesToQuery(ECC_WorldDynamic);
    FCollisionQueryParams queryParams = FCollisionQueryParams::DefaultQueryParam;
    queryParams.bReturnPhysicalMaterial = true;

    uWorld->LineTraceMultiByObjectType(outHits, start, end, objectQueryParams, queryParams);

    //Draw hits
    if (drawDebug)
    {
        for (int32 i = 0; i < outHits.Num(); ++i)
            DebugDrawHitPoint(uWorld, outHits[i]);
    }

    return outHits.Num() > 0;
}


bool SDTUtils::SphereCast(UWorld* uWorld, const FVector& start, const FVector& end, float radius, TArray<struct FHitResult>& outHits, bool drawDebug)
{
    if (uWorld == nullptr)
        return false;

    if (drawDebug)
    {
        FVector center = (start + end) * 0.5f;

        float halfHeight;
        FVector dir;
        (end - start).ToDirectionAndLength(dir, halfHeight);

        halfHeight *= 0.5f;
        halfHeight += radius;


        FQuat rotation = dir.ToOrientationQuat();

        FQuat rotY = FQuat(rotation.GetRightVector(), HALF_PI);

        rotation *= rotY;

        DrawDebugCapsule(uWorld, center, halfHeight, radius, rotation, FColor::Green);



        DrawDebugLine(uWorld, start, end, FColor::Red);
    }



    FCollisionObjectQueryParams objectQueryParams; // All objects
    FCollisionShape collisionShape;
    collisionShape.SetSphere(radius);
    FCollisionQueryParams queryParams = FCollisionQueryParams::DefaultQueryParam;
    queryParams.bReturnPhysicalMaterial = true;

    uWorld->SweepMultiByObjectType(outHits, start, end, FQuat::Identity, objectQueryParams, collisionShape, queryParams);


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


    FCollisionObjectQueryParams objectQueryParams; // All objects
    FCollisionShape collisionShape;
    collisionShape.SetSphere(radius);
    FCollisionQueryParams queryParams = FCollisionQueryParams::DefaultQueryParam;
    queryParams.bReturnPhysicalMaterial = true;

    uWorld->OverlapMultiByObjectType(outOverlaps, pos, FQuat::Identity, objectQueryParams, collisionShape, queryParams);

    //Draw overlap results
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