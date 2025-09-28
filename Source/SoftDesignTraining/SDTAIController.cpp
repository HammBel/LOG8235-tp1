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
    ACharacter* playerCharacter = UGameplayStatics::GetPlayerCharacter(GetWorld(), 0);
	TakeDecision(uWorld, pawn, playerCharacter, deltaTime);
}

void ASDTAIController::BeginPlay()
{
    Super::BeginPlay();
    ACharacter* MyCharacter = GetCharacter();
    m_MaxSpeed = MyCharacter->GetCharacterMovement()->MaxWalkSpeed;

    FVector Origin;
    MyCharacter->GetActorBounds(true, Origin, halfExtent);
    halfWidth = halfExtent.Y;
    halfHeight = halfExtent.Z;
}


bool ASDTAIController::MoveToTarget(UWorld* world, APawn* pawn, FVector target, float speed, float deltaTime)
{
    FRotator currentRotation = pawn->GetActorRotation();
    TArray<FHitResult> hitResults;
    FVector const pawnPosition(pawn->GetActorLocation());
    FVector2D toTarget = FVector2D(target) - FVector2D(pawnPosition);
    FVector2D displacement = FMath::Min(toTarget.Size(), speed * deltaTime) * toTarget.GetSafeNormal();
    float InitialYawAngle = FMath::RadiansToDegrees(FMath::Atan2(toTarget.GetSafeNormal().Y, toTarget.GetSafeNormal().X));
    SDTUtils::BoxCast(world, pawn->GetActorLocation(), target, halfExtent, FVector(displacement, 0.f).ToOrientationQuat(), hitResults, false, true);
    if (hitResults.Num() != 0) {
        FVector normal = hitResults[0].ImpactNormal;
        FVector right = FVector::CrossProduct(FVector(FVector2D(hitResults[0].ImpactNormal), 0.0f), pawn->GetActorUpVector()).GetSafeNormal();
        FVector* leftSafePosition = nullptr;
        FVector* rightSafePosition = nullptr;
        float angle = 1;
        FVector2D newToTarget;
        FVector2D newDisplacement;
        FVector newTarget;
        FVector lastRightNormal = hitResults[0].ImpactNormal;
        FVector lastLeftNormal = hitResults[0].ImpactNormal;
        bool ignoreNormal = false;
        while (!leftSafePosition || !rightSafePosition) {
            if (!rightSafePosition) {
                newTarget = pawnPosition + FVector(toTarget.Size() * cos(FMath::DegreesToRadians(InitialYawAngle + angle)), toTarget.Size() * sin(FMath::DegreesToRadians(InitialYawAngle + angle)), 0);
                newToTarget = FVector2D(newTarget) - FVector2D(pawnPosition);
                newDisplacement = FMath::Min(newToTarget.Size(), speed * deltaTime) * newToTarget.GetSafeNormal();
                if (!SDTUtils::BoxCast(world, pawn->GetActorLocation() + FVector(newDisplacement, 0).GetSafeNormal() * halfExtent * 2, newTarget, halfExtent * 2, FVector(newDisplacement, 0.f).ToOrientationQuat(), hitResults, false)) {
                    TArray<FHitResult> rayHits;
                    if (!SDTUtils::CastRay(world, pawn->GetActorLocation(), newTarget, rayHits, false))
                        rightSafePosition = new FVector(newTarget);
                    else {
                        rightSafePosition = new FVector(rayHits[0].ImpactPoint);
                    }
                }
                else if (!ignoreNormal) {
                    if ((hitResults[0].ImpactNormal - lastRightNormal).Size() > .2)
                        ignoreNormal = true;
                    lastRightNormal = hitResults[0].ImpactNormal;
                }
            }
            if (!leftSafePosition) {
                newTarget = pawnPosition + FVector(toTarget.Size() * cos(FMath::DegreesToRadians(InitialYawAngle - angle)), toTarget.Size() * sin(FMath::DegreesToRadians(InitialYawAngle - angle)), 0);
                newToTarget = FVector2D(newTarget) - FVector2D(pawnPosition);
                newDisplacement = FMath::Min(newToTarget.Size(), speed * deltaTime) * newToTarget.GetSafeNormal();
                if (!SDTUtils::BoxCast(world, pawn->GetActorLocation() + FVector(newDisplacement, 0).GetSafeNormal() * halfExtent * 2, newTarget, halfExtent * 2, FVector(newDisplacement, 0.f).ToOrientationQuat(), hitResults, false)) {
                    TArray<FHitResult> rayHits;
                    if (!SDTUtils::CastRay(world, pawn->GetActorLocation(), newTarget, rayHits, false))
                        leftSafePosition = new FVector(newTarget);
                    else {
                        leftSafePosition = new FVector(rayHits[0].ImpactPoint);
                    }
                }
                else if (!ignoreNormal) {
                    if ((hitResults[0].ImpactNormal - lastLeftNormal).Size() > .2)
                        ignoreNormal = true;
                    lastLeftNormal = hitResults[0].ImpactNormal;
                }
            }
            angle += 1;
            if (angle > 180)
                break;
        }
        if (!leftSafePosition && !rightSafePosition) return false;
        else if (!leftSafePosition || !rightSafePosition) target = leftSafePosition ? *leftSafePosition : *rightSafePosition;
        else if (ignoreNormal)
            target = (*leftSafePosition - target).Size() > (*rightSafePosition - target).Size() ? *rightSafePosition : *leftSafePosition;
        else if ((FVector::DotProduct(*leftSafePosition - target, normal) * normal).Size() < (FVector::DotProduct(*rightSafePosition - target, normal) * normal).Size())
            target = *leftSafePosition;
        else target = *rightSafePosition;
    }
    toTarget = FVector2D(target) - FVector2D(pawnPosition);
    displacement = FMath::Min(toTarget.Size(), speed * deltaTime) * toTarget.GetSafeNormal();
    float cosAngle = FVector::DotProduct(pawn->GetActorForwardVector(), FVector(toTarget, 0).GetSafeNormal()); // [-1,1]
    float turnFactor = (cosAngle + 1.f) * 0.5f;            // [0,1]

    float targetSpeed = m_MaxSpeed * turnFactor;

    m_Speed = FMath::Lerp(m_Speed, targetSpeed, 0.25f);
    
    pawn->SetActorRotation(FMath::Lerp(pawn->GetActorRotation(), FVector(displacement, 0.f).ToOrientationRotator(), .5f));
    pawn->AddMovementInput(pawn->GetActorForwardVector(), speed / m_MaxSpeed);
    return toTarget.Size() < speed * deltaTime;
}

bool ASDTAIController::FleeToTarget(UWorld* world, APawn* pawn, FVector playerPosition, float speed, float deltaTime)
{
    FVector target = pawn->GetActorLocation() - (playerPosition - pawn->GetActorLocation()).GetSafeNormal() * speed;
    FRotator currentRotation = pawn->GetActorRotation();
    TArray<FHitResult> hitResults;
    FVector const pawnPosition(pawn->GetActorLocation());
    FVector2D toTarget = FVector2D(target) - FVector2D(pawnPosition);
    FVector2D displacement = FMath::Min(toTarget.Size(), speed * deltaTime) * toTarget.GetSafeNormal();
    float InitialYawAngle = FMath::RadiansToDegrees(FMath::Atan2(toTarget.GetSafeNormal().Y, toTarget.GetSafeNormal().X));
    SDTUtils::BoxCast(world, pawn->GetActorLocation(), target, halfExtent, FVector(displacement, 0.f).ToOrientationQuat(), hitResults, true);
    FVector farthestPosition = target;
    if (hitResults.Num() != 0) {
        float angle = 1;
        FVector2D newToTarget;
        FVector2D newDisplacement;
        FVector newTarget;
        float farthestDistance = 0;
        while (angle < 100) {
            newTarget = pawnPosition + FVector(toTarget.Size() * cos(FMath::DegreesToRadians(InitialYawAngle + angle)), toTarget.Size() * sin(FMath::DegreesToRadians(InitialYawAngle + angle)), 0);
            newToTarget = FVector2D(newTarget) - FVector2D(pawnPosition);
            newDisplacement = FMath::Min(newToTarget.Size(), speed * deltaTime) * newToTarget.GetSafeNormal();
            if (!SDTUtils::BoxCast(world, pawn->GetActorLocation() + FVector(newDisplacement, 0).GetSafeNormal() * halfExtent * 2, newTarget, halfExtent * 2, FVector(newDisplacement, 0.f).ToOrientationQuat(), hitResults, false)) {
                if (!SDTUtils::BoxCast(world, pawn->GetActorLocation() + FVector(newDisplacement, 0).GetSafeNormal() * halfExtent * 2, newTarget, halfExtent * 2, FVector(newDisplacement, 0.f).ToOrientationQuat(), hitResults, true)) {
                    float distanceToPlayer = FVector::Distance(newTarget, playerPosition);
                    if (farthestDistance < distanceToPlayer) {
                        farthestDistance = distanceToPlayer;
                        farthestPosition = newTarget;
                    }
                }
                else {
                    float distanceToPlayer = FVector::Distance(hitResults[0].ImpactPoint, playerPosition);
                    if (farthestDistance < distanceToPlayer) {
                        farthestDistance = distanceToPlayer;
                        farthestPosition = newTarget;
                    }
                }
            }
          
            newTarget = pawnPosition + FVector(toTarget.Size() * cos(FMath::DegreesToRadians(InitialYawAngle - angle)), toTarget.Size() * sin(FMath::DegreesToRadians(InitialYawAngle - angle)), 0);
            newToTarget = FVector2D(newTarget) - FVector2D(pawnPosition);
            newDisplacement = FMath::Min(newToTarget.Size(), speed * deltaTime) * newToTarget.GetSafeNormal();
            if (!SDTUtils::BoxCast(world, pawn->GetActorLocation() + FVector(newDisplacement, 0).GetSafeNormal() * halfExtent * 2, newTarget, halfExtent * 2, FVector(newDisplacement, 0.f).ToOrientationQuat(), hitResults, false)) {
                if (!SDTUtils::BoxCast(world, pawn->GetActorLocation() + FVector(newDisplacement, 0).GetSafeNormal() * halfExtent * 2, newTarget, halfExtent * 2, FVector(newDisplacement, 0.f).ToOrientationQuat(), hitResults, true)) {
                    float distanceToPlayer = FVector::Distance(newTarget, playerPosition);
                    if (farthestDistance < distanceToPlayer) {
                        farthestDistance = distanceToPlayer;
                        farthestPosition = newTarget;
                    }
                }
                else {
                    float distanceToPlayer = FVector::Distance(hitResults[0].ImpactPoint, playerPosition);
                    if (farthestDistance < distanceToPlayer) {
                        farthestDistance = distanceToPlayer;
                        farthestPosition = newTarget;
                    }
                }
            }
            angle += 1;
        }

    }
    toTarget = FVector2D(farthestPosition) - FVector2D(pawnPosition);
    displacement = FMath::Min(toTarget.Size(), speed * deltaTime) * toTarget.GetSafeNormal();
    float cosAngle = FVector::DotProduct(pawn->GetActorForwardVector(), FVector(toTarget, 0).GetSafeNormal()); // [-1,1]
    float turnFactor = (cosAngle + 1.f) * 0.5f;            // [0,1]

    float targetSpeed = m_MaxSpeed * turnFactor;

    m_Speed = FMath::Lerp(m_Speed, targetSpeed, 0.25f);

    pawn->SetActorRotation(FMath::Lerp(pawn->GetActorRotation(), FVector(displacement, 0.f).ToOrientationRotator(), .5f));
    pawn->AddMovementInput(pawn->GetActorForwardVector(), speed / m_MaxSpeed);
    return toTarget.Size() < speed * deltaTime;
}

void ASDTAIController::Roam(APawn* pawn, float deltaTime)
{
    //AvoidWall(pawn, m_Speed, deltaTime);
	FVector forwardDir = pawn->GetActorForwardVector();
    m_Speed = FMath::Clamp(m_Speed, 50.0f, m_MaxSpeed / 2);
    FleeToTarget(GetWorld(), pawn, pawn->GetActorLocation() - forwardDir * 100, FMath::Clamp(m_Speed, 50.0f, m_MaxSpeed / 2), deltaTime);
    //pawn->AddMovementInput(forwardDir, m_Speed / m_MaxSpeed);
}

void ASDTAIController::TakeDecision(UWorld* world, APawn* pawn, ACharacter* playerCharacter, float deltaTime) {
    if (playerCharacter && IsPlayerVisible(world, pawn, playerCharacter)) {
        if (SDTUtils::IsPlayerPoweredUp(world)) {
            m_IsFleeing = true;
            m_LastPlayerPositionReached = true;
            FleeFromPlayer(pawn, playerCharacter->GetActorLocation(), CalculateMovement(m_MaxSpeed, m_Acceleration, deltaTime), deltaTime);
        }
        else {
            m_IsFleeing = false;
            GetWorld()->GetTimerManager().ClearTimer(TimerHandle);
            GetWorld()->GetTimerManager().SetTimer(TimerHandle, this, &ASDTAIController::ForgetPlayer, 5.0f, false);
            m_LastPlayerPositionReached = ChasePlayer(pawn, playerCharacter, deltaTime);
            
        }
    }
    else if (!m_LastPlayerPositionReached) {
        m_IsFleeing = false;
        m_LastPlayerPositionReached = MoveToTarget(world, pawn, m_LastPlayerPosition, CalculateMovement(m_MaxSpeed, m_Acceleration, deltaTime), deltaTime);

    }
    else if (AActor* pickup = IsPickupVisible(world, pawn)) {
        m_IsFleeing = false;
        MoveToTarget(world, pawn, pickup->GetActorLocation(), CalculateMovement(m_MaxSpeed, m_Acceleration, deltaTime), deltaTime);
    }

    else {
        m_IsFleeing = false;
        Roam(pawn, deltaTime);
    }
}

bool ASDTAIController::ChasePlayer(APawn* pawn, ACharacter* playerCharacter, float deltaTime) {
	return MoveToTarget(GetWorld(), pawn, playerCharacter->GetActorLocation(), CalculateMovement(m_MaxSpeed, m_Acceleration, deltaTime), deltaTime);
}

bool ASDTAIController::FleeFromPlayer(APawn* pawn, FVector target, float speed, float deltaTime) {
    return FleeToTarget(GetWorld(), pawn, target, speed, deltaTime);
}

float ASDTAIController::CalculateMovement(float maxSpeed, float acceleration, float deltaTime) {
	m_Speed = FMath::Clamp(m_Speed + m_Acceleration * deltaTime, 0.0f, maxSpeed);
	return m_Speed;
}

void ASDTAIController::Reset() {
	m_Speed = 0;
    ForgetPlayer();
}

void ASDTAIController::ForgetPlayer() {
    m_LastPlayerPositionReached = true;
}

TArray<FOverlapResult> ASDTAIController::CollectTargetActorsInFrontOfCharacter(APawn const* pawn) const
{
	TArray<FOverlapResult> results;

	SDTUtils::SphereOverlap(GetWorld(), pawn->GetActorLocation(), m_ViewDistance, results, false);

	return results;
}

bool ASDTAIController::IsVisible(UWorld* world, APawn* pawn, AActor* targetActor)
{
	if (pawn->GetDistanceTo(targetActor) > m_ViewDistance)
		return false;

	TArray<struct FHitResult> hitResult;
    if (pawn && targetActor) SDTUtils::CastRay(world, pawn->GetActorLocation(), targetActor->GetActorLocation(), hitResult, false);

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
