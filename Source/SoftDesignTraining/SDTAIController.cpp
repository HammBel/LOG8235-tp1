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
    m_IsFleeing = false;
	if (IsPlayerVisible(world, pawn, playerCharacter)) {
		if (SDTUtils::IsPlayerPoweredUp(world)) {
            m_IsFleeing = true;
			m_LastPlayerPositionReached = true;
			if (!AvoidWall(pawn, CalculateMovement(m_MaxSpeed, m_Acceleration, deltaTime), deltaTime))
				FleeFromPlayer(pawn, playerCharacter->GetActorLocation(), CalculateMovement(m_MaxSpeed, m_Acceleration, deltaTime), deltaTime);
            else pawn->AddMovementInput(pawn->GetActorForwardVector(), m_Speed / m_MaxSpeed);

		}
		else
			m_LastPlayerPositionReached = ChasePlayer(pawn, playerCharacter, deltaTime);
	}
	else if (!m_LastPlayerPositionReached)
		m_LastPlayerPositionReached = MoveToTarget(pawn, m_LastPlayerPosition, CalculateMovement(m_MaxSpeed, m_Acceleration, deltaTime), deltaTime);
	else if (AActor* pickup = IsPickupVisible(world, pawn))
		MoveToTarget(pawn, pickup->GetActorLocation(), CalculateMovement(m_MaxSpeed, m_Acceleration, deltaTime), deltaTime);
	else Roam(pawn, deltaTime);

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
    if (FVector::Dist(pawn->GetActorLocation(), target) < FVector::Dist(pawn->GetActorLocation() + pawn->GetActorForwardVector(), target)) {
        pawn->AddMovementInput(pawn->GetActorForwardVector(), speed / m_MaxSpeed);
        return toTarget.Size() < speed * deltaTime;
    }
	FVector2D const displacement = FMath::Min(toTarget.Size(), speed * deltaTime) * toTarget.GetSafeNormal();
	pawn->SetActorRotation(FVector(displacement, 0.f).ToOrientationRotator());
	pawn->AddMovementInput(pawn->GetActorForwardVector(), speed / m_MaxSpeed);
	return toTarget.Size() < speed * deltaTime;
}

bool ASDTAIController::MoveToTarget(APawn* pawn, FVector target, float speed, float deltaTime)
{
	FVector const pawnPosition(pawn->GetActorLocation());
	FVector2D const toTarget = FVector2D(target) - FVector2D(pawnPosition);
	FVector2D const displacement = FMath::Min(toTarget.Size(), speed * deltaTime) * toTarget.GetSafeNormal();
	pawn->SetActorRotation(FVector(displacement, 0.f).ToOrientationQuat());
	pawn->AddMovementInput(pawn->GetActorForwardVector(), speed / m_MaxSpeed);
	return toTarget.Size() < speed * deltaTime;
}

float ASDTAIController::CalculateMovement(float maxSpeed, float acceleration, float deltaTime) {
	m_Speed = FMath::Clamp(m_Speed + m_Acceleration * deltaTime, 0.0f, maxSpeed);
	return m_Speed;
}

bool ASDTAIController::AvoidWall(APawn* pawn, float speed, float deltaTime) {
    struct FCastInfo
    {
        FVector Start;
        FVector End;
        TArray<FHitResult> Hits;
        FColor DebugColor;
    };

    FVector pawnLoc = pawn->GetActorLocation();
    FVector fwd = pawn->GetActorForwardVector();
    FVector rightVec = pawn->GetActorRightVector();

    TArray<FCastInfo> Casts;
    Casts.Add({ pawnLoc, pawnLoc + fwd * FMath::Min(m_Speed + 50, 250), {}, FColor::Blue });                        // forward
    Casts.Add({ pawnLoc + rightVec * halfWidth, pawnLoc + rightVec * halfWidth + fwd * FMath::Min(m_Speed + 50, 250), {}, FColor::Red });   // right
    Casts.Add({ pawnLoc - rightVec * halfWidth, pawnLoc - rightVec * halfWidth + fwd * FMath::Min(m_Speed + 50, 250), {}, FColor::Yellow }); // left
    Casts.Add({ pawnLoc - rightVec * halfWidth, pawnLoc + rightVec * halfWidth, {}, FColor::Blue });
    Casts.Add({ pawnLoc + rightVec * halfWidth, pawnLoc - rightVec * halfWidth, {}, FColor::Blue });

 
    // Perform the casts
    for (auto& C : Casts)
    {
        SDTUtils::CastRay(GetWorld(), C.Start, C.End, C.Hits, false);
        DrawDebugDirectionalArrow(GetWorld(), C.Start, C.End, 1, C.DebugColor);
    }


    // Find the best hit with forward priority
    FHitResult* BestHit = nullptr;
    int BestIdx = -1;
    float ClosestDist = BIG_NUMBER;

    // Step 1: Forward priority
    if (Casts.Num() > 0 && Casts[0].Hits.Num() > 0 && !m_IsFleeing) // 0 = forward
    {
        BestHit = &Casts[0].Hits[0];
        BestIdx = 0;
    }
    else
    {
        // Step 2: fallback to closest among left/right
        for (int i = 1; i < Casts.Num(); i++) // skip forward
        {
            if (Casts[i].Hits.Num() > 0)
            {
                float d = FVector::Dist(pawnLoc, Casts[i].Hits[0].ImpactPoint);
                if (d < ClosestDist)
                {
                    ClosestDist = d;
                    BestHit = &Casts[i].Hits[0];
                    BestIdx = i;
                }
            }
        }
    }

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

    bool shouldDecel = false;
    float alphaLerp = 0.05f;
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
    else if (BestIdx == 0) // forward
    {
        m_ObstacleToDodge = BestHit->GetActor();
        m_ObstacleToDodgeNormal = BestHit->ImpactNormal;
        TArray<FHitResult> hitRight, hitLeft;
        SDTUtils::CastRay(GetWorld(), beforeImpactPoint, beforeImpactPoint + right * 1000, hitRight, false);
        SDTUtils::CastRay(GetWorld(), beforeImpactPoint, beforeImpactPoint + left * 1000, hitLeft, false);

        DrawDebugDirectionalArrow(GetWorld(), beforeImpactPoint, beforeImpactPoint + right * 1000, 1, FColor::Black);
        DrawDebugDirectionalArrow(GetWorld(), beforeImpactPoint, beforeImpactPoint + left * 1000, 1, FColor::Yellow);

        float distRight = hitRight.Num() > 0 ? FVector::Dist(beforeImpactPoint, hitRight[0].ImpactPoint) : BIG_NUMBER;
        float distLeft = hitLeft.Num() > 0 ? FVector::Dist(beforeImpactPoint, hitLeft[0].ImpactPoint) : BIG_NUMBER;

        if (distRight > distLeft)
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

        targetSpeed = m_MaxSpeed * turnFactor;

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
