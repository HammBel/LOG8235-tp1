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
	//DrawVisionCone(uWorld, pawn, m_VisionAngle);
	TakeDecision(uWorld, pawn, deltaTime);
}

void ASDTAIController::Roam(APawn* pawn, float deltaTime)
{
    // Prepare casts (forward, right, left)
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
    Casts.Add({ pawnLoc, pawnLoc + fwd * m_Speed, {}, FColor::Blue });                        // forward
    Casts.Add({ pawnLoc + rightVec * halfWidth, pawnLoc + rightVec * halfWidth + fwd * m_Speed, {}, FColor::Red });   // right
    Casts.Add({ pawnLoc - rightVec * halfWidth, pawnLoc - rightVec * halfWidth + fwd * m_Speed, {}, FColor::Yellow }); // left

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
    if (Casts.Num() > 0 && Casts[0].Hits.Num() > 0) // 0 = forward
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
        pawn->AddMovementInput(fwd, m_Speed / m_MaxSpeed);
        return;
    }

    // --- Avoidance logic ---
    FVector beforeImpactPoint = (pawnLoc - BestHit->ImpactPoint).GetSafeNormal() * 100 + BestHit->ImpactPoint;
    FVector right = FVector::CrossProduct(BestHit->ImpactNormal, pawn->GetActorUpVector()).GetSafeNormal();
    FVector left = -right;
    FVector directionToRotate;

    bool alreadyDodging = false;

    // Check all casts, not just BestHit
    for (const auto& C : Casts)
    {
        if (C.Hits.Num() > 0)
        {
            if (C.Hits[0].ImpactNormal == m_ObstacleToDodgeNormal ||
                C.Hits[0].GetActor() == m_ObstacleToDodge)
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
    else
    {
        UE_LOG(LogTemp, Warning, TEXT("ROTATION CHANGE"));
        m_ObstacleToDodge = BestHit->GetActor();
        m_ObstacleToDodgeNormal = BestHit->ImpactNormal;

        if (BestIdx == 0) // forward
        {
            TArray<FHitResult> hitRight, hitLeft;
            SDTUtils::CastRay(GetWorld(), beforeImpactPoint, beforeImpactPoint + right * 1000, hitRight, false);
            SDTUtils::CastRay(GetWorld(), beforeImpactPoint, beforeImpactPoint + left * 1000, hitLeft, false);

            DrawDebugDirectionalArrow(GetWorld(), beforeImpactPoint, beforeImpactPoint + right * 1000, 1, FColor::Black);
            DrawDebugDirectionalArrow(GetWorld(), beforeImpactPoint, beforeImpactPoint + left * 1000, 1, FColor::Yellow);

            float distRight = hitRight.Num() > 0 ? FVector::Dist(beforeImpactPoint, hitRight[0].ImpactPoint) : BIG_NUMBER;
            float distLeft = hitLeft.Num() > 0 ? FVector::Dist(beforeImpactPoint, hitLeft[0].ImpactPoint) : BIG_NUMBER;

            UE_LOG(LogTemp, Warning, TEXT("Forward hit detected"));
            UE_LOG(LogTemp, Warning, TEXT("distRight = %f, distLeft = %f"), distRight, distLeft);

            if (distRight > distLeft)
            {
                directionToRotate = right;
                m_isRotatingRight = true;
                UE_LOG(LogTemp, Warning, TEXT("Chose RIGHT (distRight > distLeft)"));
            }
            else
            {
                directionToRotate = left;
                m_isRotatingRight = false;
                UE_LOG(LogTemp, Warning, TEXT("Chose LEFT (distLeft >= distRight)"));
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
    }

    // --- Rotation ---
    FVector TargetLoc = pawnLoc + directionToRotate;
    FVector Dir = (TargetLoc - pawnLoc).GetSafeNormal();
    pawn->SetActorRotation(FMath::Lerp(pawn->GetActorRotation(), Dir.Rotation(), 0.05f));

    // --- Speed control depending on turn ---
    FVector forwardDir = pawn->GetActorForwardVector().GetSafeNormal();
    float cosAngle = FVector::DotProduct(forwardDir, Dir); // [-1,1]
    float turnFactor = (cosAngle + 1.f) * 0.5f;            // [0,1]

    float targetSpeed = m_MaxSpeed * turnFactor;

    if (m_Speed > targetSpeed)
    {
        // Decelerate smoothly
        m_Speed = FMath::Max(m_Speed - m_Acceleration * deltaTime, targetSpeed);
    }
    else
    {
        // Accelerate back up if no sharp turn
        m_Speed = FMath::Min(m_Speed + m_Acceleration * deltaTime, targetSpeed);
    }

    m_Speed = FMath::Clamp(m_Speed, 50.0f, m_MaxSpeed);


    pawn->AddMovementInput(forwardDir, targetSpeed / m_MaxSpeed);
}




void ASDTAIController::TakeDecision(UWorld* world, APawn* pawn, float deltaTime) {
	ACharacter* playerCharacter = UGameplayStatics::GetPlayerCharacter(GetWorld(), 0);
	Roam(pawn, deltaTime);
	/*if (IsPlayerVisible(world, pawn, playerCharacter)) {
		if (SDTUtils::IsPlayerPoweredUp(world)) {
			m_LastPlayerPositionReached = true;
			if (!AvoidWall(pawn, CalculateMovement(m_MaxSpeed, m_Acceleration, deltaTime), deltaTime))
				FleeFromPlayer(pawn, playerCharacter->GetActorLocation(), CalculateMovement(m_MaxSpeed, m_Acceleration, deltaTime), deltaTime);
		}
		else
			m_LastPlayerPositionReached = ChasePlayer(pawn, playerCharacter, deltaTime);
	}
	else if (!m_LastPlayerPositionReached)
		m_LastPlayerPositionReached = MoveToTarget(pawn, m_LastPlayerPosition, CalculateMovement(m_MaxSpeed, m_Acceleration, deltaTime), deltaTime);
	else if (AActor* pickup = IsPickupVisible(world, pawn))
		MoveToTarget(pawn, pickup->GetActorLocation(), CalculateMovement(m_MaxSpeed, m_Acceleration, deltaTime), deltaTime);
	else Roam(pawn, deltaTime);*/

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
	FVector2D const displacement = FMath::Min(toTarget.Size(), speed * deltaTime) * toTarget.GetSafeNormal();
	pawn->SetActorRotation(FMath::Lerp(pawn->GetActorRotation(), FVector(displacement, 0.f).ToOrientationRotator(), 0.05f));
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

float ASDTAIController::CalculateMovement(float maxSpeed, float acceleration, float delaTime) {
	m_Speed = FMath::Clamp(m_Speed + m_Acceleration * delaTime, 0.0f, maxSpeed);
	return m_Speed;
}

bool ASDTAIController::AvoidWall(APawn* pawn, float speed, float delaTime) {
	TArray<struct FHitResult> hitResults;
	if (SDTUtils::CastRay(GetWorld(), pawn->GetActorLocation(), pawn->GetActorLocation() + pawn->GetActorForwardVector() * m_Speed, hitResults, false)) {
		FVector orientation = FVector::CrossProduct(FVector(hitResults[0].ImpactNormal), pawn->GetActorUpVector()).GetSafeNormal();
		FVector MyLoc = pawn->GetActorLocation();
		FVector TargetLoc = pawn->GetActorLocation() + pawn->GetActorForwardVector() + orientation;
		FVector Dir = (TargetLoc - MyLoc);
		Dir.Normalize();
		pawn->SetActorRotation(FMath::Lerp(pawn->GetActorRotation(), Dir.Rotation(), 0.05f));
		pawn->AddMovementInput(pawn->GetActorForwardVector(), speed / m_MaxSpeed);
		return true;
	}
	return false;
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
	SDTUtils::CastRay(world, pawn->GetActorLocation(), targetActor->GetActorLocation(), hitResult, true);

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
	m_LastPlayerPosition = targetActor->GetActorLocation();
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
		if (IsVisible(world, pawn, result.GetActor()))
			return result.GetActor();
	}
	return nullptr;
}
