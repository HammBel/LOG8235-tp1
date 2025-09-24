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
	DrawVisionCone(uWorld, pawn, m_VisionAngle);
	TakeDecision(uWorld, pawn, deltaTime);
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
