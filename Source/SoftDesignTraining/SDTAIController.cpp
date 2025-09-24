// Fill out your copyright notice in the Description page of Project Settings.

#include "SDTAIController.h"
#include "SoftDesignTraining.h"
#include "SDTUtils.h"

void ASDTAIController::Tick(float deltaTime)
{
	ChasePlayer(deltaTime);
}

bool ASDTAIController::ChasePlayer(float deltaTime) {
	ACharacter* playerCharacter = UGameplayStatics::GetPlayerCharacter(GetWorld(), 0);
	return MoveToTarget(playerCharacter->GetActorLocation(), CalculateMovement(m_MaxSpeed, m_Acceleration, deltaTime), deltaTime);
}

bool ASDTAIController::MoveToTarget(FVector target, float speed, float deltaTime)
{
	APawn* pawn = GetPawn();
	FVector const pawnPosition(pawn->GetActorLocation());
	FVector const toTarget = target - pawnPosition;
	FVector const displacement = FMath::Min(toTarget.Size(), speed * deltaTime) * toTarget.GetSafeNormal();
	pawn->SetActorRotation(displacement.ToOrientationQuat());
	pawn->AddMovementInput(pawn->GetActorForwardVector(), speed / m_MaxSpeed);
	return toTarget.Size() < speed * deltaTime;
}

float ASDTAIController::CalculateMovement(float maxSpeed, float acceleration, float delaTime) {
	m_Speed = FMath::Clamp(m_Speed + m_Acceleration * delaTime, 0.0f, maxSpeed);
	return m_Speed;
}

float ASDTAIController::AvoidWall(float delaTime) {
	APawn* pawn = GetPawn();
	TArray<struct FHitResult> hitResults;
	if (SDTUtils::CastRay(GetWorld(), pawn->GetActorLocation(), pawn->GetActorLocation() + pawn->GetActorForwardVector() * m_Speed, hitResults, false)) {
		FVector orientation = FVector::CrossProduct(FVector(hitResults[0].ImpactNormal), pawn->GetActorUpVector()).GetSafeNormal();
		FVector MyLoc = pawn->GetActorLocation();
		FVector TargetLoc = pawn->GetActorLocation() + pawn->GetActorForwardVector() + orientation;
		FVector Dir = (TargetLoc - MyLoc);
		Dir.Normalize();
		pawn->SetActorRotation(FMath::Lerp(pawn->GetActorRotation(), Dir.Rotation(), 0.05f));
	}
	return 0.f;
}

void ASDTAIController::BeginPlay()
{
	Super::BeginPlay();
	ACharacter* MyCharacter = GetCharacter();
	m_MaxSpeed = MyCharacter->GetCharacterMovement()->MaxWalkSpeed;
}

void ASDTAIController::ResetSpeed() {
	m_Speed = 0;
}

TArray<FOverlapResult> ASDTAIController::CollectTargetActorsInFrontOfCharacter(APawn const* pawn) const
{
	TArray<FOverlapResult> results;

	SDTUtils::SphereOverlap(GetWorld(), pawn->GetActorLocation() + pawn->GetActorForwardVector() * 750.f, 1000.f, results, true);

	return results;
}

bool ASDTAIController::IsInsideCone(APawn* pawn, AActor* targetActor) const
{
	if (FVector::Dist2D(pawn->GetActorLocation(), targetActor->GetActorLocation()) > 500.f)
	{
		return false;
	}

	auto pawnForwardVector = pawn->GetActorForwardVector();
	auto dir = targetActor->GetActorLocation() - pawn->GetActorLocation();

	auto value = FVector::DotProduct(dir.GetSafeNormal(), pawnForwardVector.GetSafeNormal());
	auto angle = FMath::Acos(value);

	auto isVisible = FMath::Abs(angle) <= m_VisionAngle;

	if (isVisible)
	{
		DrawDebugSphere(GetWorld(), targetActor->GetActorLocation(), 100.f, 32, FColor::Magenta);
	}

	GEngine->AddOnScreenDebugMessage(-1, 0.16f, FColor::Red, FString::Printf(TEXT("Actor : %s is %s"), *targetActor->GetName(), isVisible ? TEXT("visible") : TEXT("not visible")));

	return isVisible;
}
