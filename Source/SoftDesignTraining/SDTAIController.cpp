// Fill out your copyright notice in the Description page of Project Settings.

#include "SDTAIController.h"
#include "SoftDesignTraining.h"
#include "SDTUtils.h"
#include "SDTCollectible.h"
#include "SoftDesignTrainingMainCharacter.h"
#include "Engine/OverlapResult.h"


void ASDTAIController::Tick(float deltaTime)
{
	APawn* pawn = GetPawn();
	UWorld* uWorld = GetWorld();
	TakeDecision(uWorld, pawn, deltaTime);
}

void ASDTAIController::Roam(APawn* pawn, float deltaTime)
{
    AvoidWall(pawn, m_Speed, deltaTime);
	FVector forwardDir = pawn->GetActorForwardVector();
    m_Speed = FMath::Clamp(m_Speed, 50.0f, m_MaxSpeed / 2);
    pawn->AddMovementInput(forwardDir, m_Speed / m_MaxSpeed);
}




void ASDTAIController::TakeDecision(UWorld* world, APawn* pawn, float deltaTime) {
    //Roam(pawn, deltaTime);
    ACharacter* playerCharacter = UGameplayStatics::GetPlayerCharacter(GetWorld(), 0);
    if (playerCharacter && IsPlayerVisible(world, pawn, playerCharacter)) {
        if (SDTUtils::IsPlayerPoweredUp(world)) {
            //if (!m_IsFleeing) {
            //    UE_LOG(LogTemp, Warning, TEXT("START FLEEING"));
            //    m_ObstacleToDodge = nullptr;
            //    m_ObstacleToDodgeNormal = FVector();
            //}
            m_IsFleeing = true;
            m_LastPlayerPositionReached = true;
            FleeFromPlayer(pawn, playerCharacter->GetActorLocation(), CalculateMovement(m_MaxSpeed, m_Acceleration, deltaTime), deltaTime);
            //else pawn->AddMovementInput(pawn->GetActorForwardVector(), m_Speed / m_MaxSpeed);

        }
        else {
            m_IsFleeing = false;
            m_LastPlayerPositionReached = ChasePlayer(pawn, playerCharacter, deltaTime);
        }
    }
    else if (!m_LastPlayerPositionReached) {
        m_IsFleeing = false;
        m_LastPlayerPositionReached = MoveToTarget(pawn, m_LastPlayerPosition, CalculateMovement(m_MaxSpeed, m_Acceleration, deltaTime), deltaTime);

    }
    else if (AActor* pickup = IsPickupVisible(world, pawn)) {
        m_IsFleeing = false;
        MoveToTarget(pawn, pickup->GetActorLocation(), CalculateMovement(m_MaxSpeed, m_Acceleration, deltaTime), deltaTime);
    }

    else {
        m_IsFleeing = false;
        Roam(pawn, deltaTime);
    }
}

void ASDTAIController::DrawVisionCone(UWorld* world, APawn* pawn, float angle) const
{
	DrawDebugCone(world, pawn->GetActorLocation(), pawn->GetActorForwardVector(), 500.f, angle, 0, 32, FColor::Green);
}

bool ASDTAIController::ChasePlayer(APawn* pawn, ACharacter* playerCharacter, float deltaTime) {
	return MoveToTarget(pawn, playerCharacter->GetActorLocation(), CalculateMovement(m_MaxSpeed, m_Acceleration, deltaTime), deltaTime);
}

bool ASDTAIController::FleeFromPlayer(APawn* pawn, FVector target, float speed, float deltaTime) {
    FVector const pawnPosition(pawn->GetActorLocation());
    FVector2D const toTarget = FVector2D(pawnPosition) - FVector2D(target);
    //if (FVector::Dist(pawn->GetActorLocation(), target) < FVector::Dist(pawn->GetActorLocation() + pawn->GetActorForwardVector(), target)) {
    //    AvoidWall2(pawn, speed, deltaTime);
    //    pawn->AddMovementInput(pawn->GetActorForwardVector(), speed / m_MaxSpeed);
    //    return toTarget.Size() < speed * deltaTime;
    //}
	FVector2D const displacement = FMath::Min(toTarget.Size(), speed * deltaTime) * toTarget.GetSafeNormal();

	pawn->SetActorRotation(FVector(displacement, 0.f).ToOrientationRotator());
    AvoidWall2(pawn, speed, deltaTime);
	pawn->AddMovementInput(pawn->GetActorForwardVector(), speed / m_MaxSpeed);
	return toTarget.Size() < speed * deltaTime;
}

bool ASDTAIController::MoveToTarget(APawn* pawn, FVector target, float speed, float deltaTime)
{
	FVector const pawnPosition(pawn->GetActorLocation());
	FVector2D const toTarget = FVector2D(target) - FVector2D(pawnPosition);
	FVector2D const displacement = FMath::Min(toTarget.Size(), speed * deltaTime) * toTarget.GetSafeNormal();
    FRotator currentRotation = pawn->GetActorRotation();
	pawn->SetActorRotation(FMath::Lerp(pawn->GetActorRotation(),  FVector(displacement, 0.f).ToOrientationRotator(), 0.25f));
    AvoidWall(pawn, m_Speed, deltaTime);
    pawn->AddMovementInput(pawn->GetActorForwardVector(), speed / m_MaxSpeed);
	return toTarget.Size() < speed * deltaTime;
}

float ASDTAIController::CalculateMovement(float maxSpeed, float acceleration, float deltaTime) {
	m_Speed = FMath::Clamp(m_Speed + m_Acceleration * deltaTime, 0.0f, maxSpeed);
	return m_Speed;
}

TArray<ASDTAIController::FCastInfo> ASDTAIController::CheckSurrounding(APawn* pawn) {

    FVector pawnLoc = pawn->GetActorLocation();
    FVector fwd = pawn->GetActorForwardVector();
    FVector rightVec = pawn->GetActorRightVector();

    TArray<FCastInfo> Casts;
    Casts.Add({ pawnLoc, pawnLoc + fwd * FMath::Min(m_Speed + 50, 250), {}, nullptr, FColor::Blue });                        // forward
    Casts.Add({ pawnLoc + rightVec * halfWidth, pawnLoc + rightVec * halfWidth + fwd * FMath::Min(m_Speed + 50, 250), {}, nullptr, FColor::Red });   // right
    Casts.Add({ pawnLoc - rightVec * halfWidth, pawnLoc - rightVec * halfWidth + fwd * FMath::Min(m_Speed + 50, 250), {}, nullptr, FColor::Yellow }); // left
    Casts.Add({ pawnLoc - rightVec * halfWidth, pawnLoc + rightVec * halfWidth, {}, nullptr, FColor::Blue });
    Casts.Add({ pawnLoc + rightVec * halfWidth, pawnLoc - rightVec * halfWidth, {}, nullptr, FColor::Blue });

    FCollisionObjectQueryParams objectQueryParamsFloor = FCollisionObjectQueryParams::DefaultObjectQueryParam;;
    objectQueryParamsFloor.AddObjectTypesToQuery(ECC_GameTraceChannel3);
   
    TArray<FCastInfo> CastsFloor;
    CastsFloor.Add({ pawnLoc, pawnLoc + fwd * FMath::Min(m_Speed + 50, 250) - halfHeight * pawn->GetActorUpVector(), {}, &objectQueryParamsFloor, FColor::Blue});                        // forward
    CastsFloor.Add({ pawnLoc + rightVec * halfWidth, pawnLoc + rightVec * halfWidth + fwd * FMath::Min(m_Speed + 50, 250) - halfHeight * pawn->GetActorUpVector(), {}, &objectQueryParamsFloor, FColor::Red });   // right
    CastsFloor.Add({ pawnLoc - rightVec * halfWidth, pawnLoc - rightVec * halfWidth + fwd * FMath::Min(m_Speed + 50, 250) - halfHeight * pawn->GetActorUpVector(), {}, &objectQueryParamsFloor, FColor::Yellow }); // left
    
    for (auto& C : CastsFloor)
    {
        SDTUtils::CastRay(GetWorld(), C.Start, C.End, C.Hits, false, C.objectQueryParams);
        DrawDebugDirectionalArrow(GetWorld(), C.Start, C.End, 1, C.DebugColor);
        if (C.Hits.Num() > 0) {
            FVector impactPoint = C.Hits[0].ImpactPoint;
            Casts.Add({ FVector(FVector2D(C.Start), impactPoint.Z), impactPoint, {}, &objectQueryParamsFloor, FColor::Black });
        }
    }

    for (auto& C : Casts)
    {
        SDTUtils::CastRay(GetWorld(), C.Start, C.End, C.Hits, false, C.objectQueryParams);
        DrawDebugDirectionalArrow(GetWorld(), C.Start, C.End, 1, C.DebugColor);
    }
    return Casts;
}

int ASDTAIController::ComputeObstacleToDodge(APawn* pawn,const TArray<FCastInfo>& Casts) {

    FVector pawnLoc = pawn->GetActorLocation();

    // Find the best hit with forward priority
    int BestIdx = -1;
    float ClosestDist = BIG_NUMBER;

     // Step 1: Forward priority
    if (Casts.Num() > 5) {
        BestIdx = 5;
    }
    else if (Casts.Num() > 0 && Casts[0].Hits.Num() > 0) // 0 = forward
    {
        BestIdx = 0;
    }
    else
    {
        // Step 2: fallback to closest among left/right
        for (int i = 1; i < Casts.Num(); i++)
        {
            if (Casts[i].Hits.Num() > 0)
            {
                float d = FVector::Dist(pawnLoc, Casts[i].Hits[0].ImpactPoint);
                if (d < ClosestDist)
                {
                    ClosestDist = d;
                    BestIdx = i;
                }
            }
        }
    }

    return BestIdx;
}

bool ASDTAIController::AvoidWall2(APawn* pawn, float speed, float deltaTime) {

    FVector pawnLoc = pawn->GetActorLocation();
    TArray<FCastInfo> Casts = CheckSurrounding(pawn);
 

    // Find the best hit with forward priority
    FHitResult* BestHit = nullptr;
    int BestIdx = ComputeObstacleToDodge(pawn, Casts);
    if (BestIdx != -1 && BestIdx != 3 && BestIdx != 4) BestHit = &Casts[BestIdx].Hits[0];
    if (!BestHit)
    {
        // No obstacle → accelerate to max
        m_Speed = FMath::Min(m_Speed + m_Acceleration * deltaTime, m_MaxSpeed);
        m_ObstacleToDodge = nullptr;
        m_ObstacleToDodgeNormal = FVector();
        return false;
    }

    // --- Avoidance logic ---
    FVector beforeImpactPoint = (pawnLoc - BestHit->ImpactPoint).GetSafeNormal() * 100 + BestHit->ImpactPoint;
    FVector right = FVector::CrossProduct(BestHit->ImpactNormal, pawn->GetActorUpVector()).GetSafeNormal();
    FVector left = -right;
    FVector directionToRotate;

    float alphaLerp = 0.75f;
    bool alreadyDodging = false;
    for (int i = 0; i < 3; i++)
    {
        if (Casts[i].Hits.Num() > 0)
        {
            if (Casts[i].Hits[0].ImpactNormal == m_ObstacleToDodgeNormal ||
                (m_ObstacleToDodge && Casts[i].Hits[0].GetActor() == m_ObstacleToDodge))
            {
                alreadyDodging = true;
                break;
            }
        }
    }

    if (alreadyDodging)
    {
        directionToRotate = m_isRotatingRight ? right : left;
    }
    else if (BestIdx == 0 || BestIdx > 4) // forward
    {
        m_ObstacleToDodge = BestHit->GetActor();
        m_ObstacleToDodgeNormal = BestHit->ImpactNormal;
        TArray<FHitResult> hitRight, hitLeft;
        SDTUtils::CastRay(GetWorld(), beforeImpactPoint, beforeImpactPoint + right * 1000, hitRight, false, nullptr);
        SDTUtils::CastRay(GetWorld(), beforeImpactPoint, beforeImpactPoint + left * 1000, hitLeft, false, nullptr);

        DrawDebugDirectionalArrow(GetWorld(), beforeImpactPoint, beforeImpactPoint + right * 1000, 1, FColor::Black);
        DrawDebugDirectionalArrow(GetWorld(), beforeImpactPoint, beforeImpactPoint + left * 1000, 1, FColor::Yellow);

        float distRight = hitRight.Num() > 0 ? FVector::Dist(beforeImpactPoint, hitRight[0].ImpactPoint) : BIG_NUMBER;
        float distLeft = hitLeft.Num() > 0 ? FVector::Dist(beforeImpactPoint, hitLeft[0].ImpactPoint) : BIG_NUMBER;

        ACharacter* playerCharacter = UGameplayStatics::GetPlayerCharacter(GetWorld(), 0);
        FVector playerLocation = playerCharacter->GetActorLocation();


        FVector ToPlayer = playerLocation - pawnLoc;
        ToPlayer.Z = 0.0f;

        float Side = FVector::CrossProduct(pawn->GetActorForwardVector(), ToPlayer).Z;


        if (distRight <= 200)
            {
                directionToRotate = left;
                m_isRotatingRight = false;
            }
        else if(distLeft <= 200)
            {
                directionToRotate = right;
                m_isRotatingRight = true;
            }

        else if (Side > 0) {
            directionToRotate = right;
            m_isRotatingRight = true;
        }
        else {
            directionToRotate = left;
            m_isRotatingRight = false;
        }
    }
    else if (BestIdx == 1) // right
    {
        directionToRotate = left;
        m_isRotatingRight = false;
    }
    else if (BestIdx == 2) // left
    {
        directionToRotate = right;
        m_isRotatingRight = true;
    }

    // --- Rotation ---
    FVector TargetLoc = pawnLoc + directionToRotate;
    FVector Dir = (TargetLoc - pawnLoc).GetSafeNormal();

    pawn->SetActorRotation(FMath::Lerp(pawn->GetActorRotation(), Dir.Rotation(), alphaLerp));

    float targetSpeed = m_Speed;
    FVector forwardDir = pawn->GetActorForwardVector().GetSafeNormal();
    return true;
}

bool ASDTAIController::AvoidWall(APawn* pawn, float speed, float deltaTime) {

    FVector pawnLoc = pawn->GetActorLocation();
    TArray<FCastInfo> Casts = CheckSurrounding(pawn);


    // Find the best hit with forward priority
    FHitResult* BestHit = nullptr;
    int BestIdx = ComputeObstacleToDodge(pawn, Casts);

    if (BestIdx != -1 && Casts[BestIdx].Hits.Num() > 0) BestHit = &Casts[BestIdx].Hits[0];
    else
    {
        // No obstacle → accelerate to max
        m_Speed = FMath::Min(m_Speed + m_Acceleration * deltaTime, m_MaxSpeed);
        m_ObstacleToDodge = nullptr;
        m_ObstacleToDodgeNormal = FVector();
        return false;
    }



    // --- Avoidance logic ---
    FVector targetDest = BestIdx < 5 ? FVector(FVector2D(BestHit->ImpactPoint), 0.0f) : pawnLoc;
    FVector displacement = (pawnLoc - FVector(FVector2D(BestHit->ImpactPoint), 0.0f)).GetSafeNormal() * 100;
    FVector beforeImpactPoint = BestIdx < 5 ? displacement + targetDest : targetDest - displacement;
    FVector right = FVector::CrossProduct(FVector(FVector2D(BestHit->ImpactNormal), 0.0f), pawn->GetActorUpVector()).GetSafeNormal();
    FVector left = -right;
    FVector directionToRotate;

    bool shouldDecel = BestIdx > 4;
    float alphaLerp = BestIdx > 4 ? 0.90f : 0.05f;
    bool alreadyDodging = false;
    for (int i = 0; i < Casts.Num(); i++)
    {
        if (i == 3 || i == 4) continue;
        if (Casts[i].Hits.Num() > 0)
        {
            if (Casts[i].Hits[0].ImpactNormal == m_ObstacleToDodgeNormal ||
                (m_ObstacleToDodge && Casts[i].Hits[0].GetActor() == m_ObstacleToDodge))
            {
                alreadyDodging = true;
                break;
            }
        }
    }

    if (alreadyDodging)
    {
        directionToRotate = m_isRotatingRight ? right : left;
    }
    else if (BestIdx == 0 || BestIdx > 4) // forward
    {
        m_ObstacleToDodge = BestHit->GetActor();
        m_ObstacleToDodgeNormal = BestHit->ImpactNormal;
        TArray<FHitResult> hitRight, hitLeft;
        SDTUtils::CastRay(GetWorld(), beforeImpactPoint, beforeImpactPoint + right * 1000, hitRight, false, nullptr);
        SDTUtils::CastRay(GetWorld(), beforeImpactPoint, beforeImpactPoint + left * 1000, hitLeft, false, nullptr);

        FCollisionObjectQueryParams objectQueryParamsFloor = FCollisionObjectQueryParams::DefaultObjectQueryParam;;
        objectQueryParamsFloor.AddObjectTypesToQuery(ECC_GameTraceChannel3);
        TArray<FHitResult> hitFloorRight, hitFloorLeft;
        SDTUtils::CastRay(GetWorld(), pawnLoc, (pawnLoc + right - halfHeight * pawn->GetActorUpVector()) * 1000, hitFloorRight, false, &objectQueryParamsFloor);
        SDTUtils::CastRay(GetWorld(), pawnLoc, (pawnLoc + left - halfHeight * pawn->GetActorUpVector()) * 1000 , hitFloorLeft, false, &objectQueryParamsFloor);
        UE_LOG(LogTemp, Warning, TEXT("Checking death floor"));

        DrawDebugDirectionalArrow(GetWorld(), pawnLoc, (pawnLoc + right - halfHeight * pawn->GetActorUpVector()) * 1000, 1, FColor::Black, true);
        DrawDebugDirectionalArrow(GetWorld(), pawnLoc, (pawnLoc + left - halfHeight * pawn->GetActorUpVector()) * 1000, 1, FColor::Yellow, true);

        float distRight = hitRight.Num() > 0 ? FVector::Dist(beforeImpactPoint, hitRight[0].ImpactPoint) : BIG_NUMBER;
        float distLeft = hitLeft.Num() > 0 ? FVector::Dist(beforeImpactPoint, hitLeft[0].ImpactPoint) : BIG_NUMBER;
        if (hitFloorRight.Num() > 0) {
            UE_LOG(LogTemp, Warning, TEXT("Hit floor right"));
            directionToRotate = left;
            m_isRotatingRight = false;
        }
        else if (hitFloorLeft.Num() > 0) {
            UE_LOG(LogTemp, Warning, TEXT("Hit floor left"));
            directionToRotate = right;
            m_isRotatingRight = true;
        }
        else if (distRight > distLeft)
        {
            directionToRotate = right;
            m_isRotatingRight = true;
        }
        else
        {
            directionToRotate = left;
            m_isRotatingRight = false;
        }
    }
    else if (BestIdx == 1) // right
    {
        directionToRotate = left;
        m_isRotatingRight = false;
    }
    else if (BestIdx == 2) // left
    {
        directionToRotate = right;
        m_isRotatingRight = true;
    }
    else if (BestIdx == 3) // cul right
    {
        directionToRotate = -pawn->GetActorRightVector();
        m_isRotatingRight = false;
        shouldDecel = false;
        alphaLerp = 0.01f;
    }
    else if (BestIdx == 4) // cul left
    {
        directionToRotate = pawn->GetActorRightVector();
        m_isRotatingRight = true;
        shouldDecel = false;
        alphaLerp = 0.01f;
    }

    // --- Rotation ---
    FVector TargetLoc = pawnLoc + directionToRotate;
    FVector Dir = (TargetLoc - pawnLoc).GetSafeNormal();

    pawn->SetActorRotation(FMath::Lerp(pawn->GetActorRotation(), Dir.Rotation(), alphaLerp));
    float targetSpeed = m_Speed;
    FVector forwardDir = pawn->GetActorForwardVector().GetSafeNormal();

    if (shouldDecel) {
        float cosAngle = FVector::DotProduct(forwardDir, Dir); // [-1,1]
        float turnFactor = (cosAngle + 1.f) * 0.5f;            // [0,1]

        targetSpeed = BestIdx > 4 ? m_Speed * turnFactor : m_MaxSpeed * turnFactor;

        m_Speed = FMath::Lerp(m_Speed, targetSpeed, 0.25f);
    }
    return true;
}

void ASDTAIController::BeginPlay()
{
	Super::BeginPlay();
	ACharacter* MyCharacter = GetCharacter();
	m_MaxSpeed = MyCharacter->GetCharacterMovement()->MaxWalkSpeed;

	FVector Origin;
	FVector BoxExtent;
	MyCharacter->GetActorBounds(true, Origin, BoxExtent);
	halfWidth = BoxExtent.Y;
    halfHeight = BoxExtent.Z;
}

void ASDTAIController::Reset() {
	m_Speed = 0;
	m_LastPlayerPositionReached = true;
}

TArray<FOverlapResult> ASDTAIController::CollectTargetActorsInFrontOfCharacter(APawn const* pawn) const
{
	TArray<FOverlapResult> results;

	SDTUtils::SphereOverlap(GetWorld(), pawn->GetActorLocation(), m_ViewDistance, results, true);

	return results;
}

bool ASDTAIController::IsVisible(UWorld* world, APawn* pawn, AActor* targetActor)
{
	if (pawn->GetDistanceTo(targetActor) > m_ViewDistance)
		return false;

	TArray<struct FHitResult> hitResult;
    if (pawn && targetActor) SDTUtils::CastRay(world, pawn->GetActorLocation(), targetActor->GetActorLocation(), hitResult, true);

	for (int j = 0; j < hitResult.Num(); ++j)
	{
		if (hitResult[j].GetActor() != targetActor && !hitResult[j].GetActor()->IsA(ASDTCollectible::StaticClass()))
			return false;
	}
	return true;
}

bool ASDTAIController::IsPlayerVisible(UWorld* world, APawn* pawn, AActor* targetActor)
{
	if (!IsVisible(world, pawn, targetActor))
		return false;
	if(targetActor) m_LastPlayerPosition = targetActor->GetActorLocation();
	return true;
}

AActor* ASDTAIController::IsPickupVisible(UWorld* world, APawn* pawn) {
	AActor* nearestPickup = nullptr;
	TArray<FOverlapResult> results = CollectTargetActorsInFrontOfCharacter(pawn).FilterByPredicate([&](FOverlapResult overlapResult) {
		return overlapResult.GetActor()->IsA(ASDTCollectible::StaticClass()) && !Cast<ASDTCollectible>(overlapResult.GetActor())->IsOnCooldown();
		});
	Algo::Sort(results, [&](FOverlapResult lhs, FOverlapResult rhs)
		{
			return pawn->GetDistanceTo(lhs.GetActor()) < pawn->GetDistanceTo(rhs.GetActor());
		});
	for (FOverlapResult result : results) {
		if (result.GetActor() && IsVisible(world, pawn, result.GetActor()))
			return result.GetActor();
	}
	return nullptr;
}
