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
	//if (!AvoidWall(pawn, CalculateMovement(m_MaxSpeed, m_Acceleration, deltaTime), deltaTime)) {
	//	pawn->AddMovementInput(pawn->GetActorForwardVector(), CalculateMovement(m_MaxSpeed, m_Acceleration, deltaTime) / m_MaxSpeed);
	//}
	float distanceToLeftHit, distanceToRightHit;
	float speed = CalculateMovement(m_MaxSpeed, m_Acceleration, deltaTime);

	TArray<struct FHitResult> hitResults;
	pawn->AddMovementInput(pawn->GetActorForwardVector(), speed / m_MaxSpeed);
	SDTUtils::CastRay(GetWorld(), pawn->GetActorLocation(), pawn->GetActorLocation() + pawn->GetActorForwardVector() * m_Speed, hitResults, false);

	if (hitResults.Num() > 0)
	{
		FVector beforeImpactPoint = (pawn->GetActorLocation() - hitResults[0].ImpactPoint).GetSafeNormal() * 100 + hitResults[0].ImpactPoint;
		FVector right = FVector::CrossProduct(FVector(hitResults[0].ImpactNormal), pawn->GetActorUpVector()).GetSafeNormal();
		FVector left = -right;
		FVector directionToRotate;
		FVector MyLoc = pawn->GetActorLocation();
		FVector TargetLoc = pawn->GetActorLocation();

		if(hitResults[0].GetActor() == m_ObstacleToDodge){
			directionToRotate = m_isRotatingRight ? right : left;
		}
		else {
			m_ObstacleToDodge = hitResults[0].GetActor();
			TArray<struct FHitResult> hitRight;
			SDTUtils::CastRay(GetWorld(), beforeImpactPoint, beforeImpactPoint + right * 500, hitRight, false);

			TArray<struct FHitResult> hitLeft;
			SDTUtils::CastRay(GetWorld(), beforeImpactPoint, beforeImpactPoint + left * 500, hitLeft, false);

			bool hasHitLeft = hitLeft.Num() > 0;
			bool hasHitRight = hitRight.Num() > 0;


			if (hasHitLeft) {
				distanceToLeftHit = FVector::Dist(beforeImpactPoint, hitLeft[0].ImpactPoint);
			}
			if (hasHitRight) {
				distanceToRightHit = FVector::Dist(beforeImpactPoint, hitRight[0].ImpactPoint);
			}

			if (hasHitLeft && hasHitRight) {
				if (distanceToRightHit > distanceToLeftHit) {
					directionToRotate = right;
					m_isRotatingRight = true;
				}
				else {
					directionToRotate = left;
					m_isRotatingRight = false;


				}
			}
			else if (hasHitLeft) {
				directionToRotate = right;
				m_isRotatingRight = true;
			}
			else if (hasHitRight) {
				directionToRotate = left;
				m_isRotatingRight = false;
			}
			else {
				directionToRotate = right;
				m_isRotatingRight = true;
			}

		}

		
		TargetLoc += directionToRotate;
		FVector Dir = (TargetLoc - MyLoc);
		Dir.Normalize();
		pawn->SetActorRotation(FMath::Lerp(pawn->GetActorRotation(), Dir.Rotation(), 0.05f));

		DrawDebugDirectionalArrow(GetWorld(), beforeImpactPoint, beforeImpactPoint + right * 500, 1, FColor::Blue);
		DrawDebugDirectionalArrow(GetWorld(), beforeImpactPoint, beforeImpactPoint + left * 500, 1, FColor::Red);
	}

}


void ASDTAIController::TakeDecision(UWorld* world, APawn* pawn, float deltaTime) {
	ACharacter* playerCharacter = UGameplayStatics::GetPlayerCharacter(GetWorld(), 0);
	if (IsPlayerVisible(world, pawn, playerCharacter)) {
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
}

void ASDTAIController::Reset() {
	m_Speed = 0;
	m_LastPlayerPositionReached = true;
}

TArray<FOverlapResult> ASDTAIController::CollectTargetActorsInFrontOfCharacter(APawn const* pawn) const
{
	TArray<FOverlapResult> results;

	SDTUtils::SphereOverlap(GetWorld(), pawn->GetActorLocation(), 1000.f, results, true);

	return results;
}

bool ASDTAIController::IsVisible(UWorld* world, APawn* pawn, AActor* targetActor)
{
	if (pawn->GetDistanceTo(targetActor) > 1000.f)
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
