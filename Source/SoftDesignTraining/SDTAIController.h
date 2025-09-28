// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "Math/UnrealMathUtility.h"
#include "CoreMinimal.h"
#include "AIController.h"

#include "SDTAIController.generated.h"

/**
 * 
 */
UCLASS(ClassGroup = AI, config = Game)
class SOFTDESIGNTRAINING_API ASDTAIController : public AAIController
{
    GENERATED_BODY()
public:

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = AI)
    float m_Acceleration = 100.f;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = AI)
    float m_ViewDistance = 1000.f;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = AI)
    bool m_ShouldDetectThroughDeathZone = false;

    virtual void Tick(float deltaTime) override;
    virtual void BeginPlay() override;
    void Reset();
    void ForgetPlayer();


private:
    FTimerHandle TimerHandle;
    float m_Speed = 0.f;
    float m_MaxSpeed = 0.f;
    FVector halfExtent;
    FVector m_LastPlayerPosition = FVector();
    bool m_LastPlayerPositionReached = true;
    float CalculateMovement(float maxSpeed, float acceleration, float deltaTime);
    bool MoveToTarget(UWorld* world, APawn* pawn, FVector target, float speed, float deltaTime);
    bool FleeToTarget(UWorld* world, APawn* pawn, FVector playerPosition, float speed, float deltaTime, bool isRoaming = false);
    bool ChasePlayer(APawn* pawn, ACharacter* playerCharacter, float deltaTime);
    bool FleeFromPlayer(APawn* pawn, FVector target, float speed, float deltaTime);
    TArray<FOverlapResult> CollectTargetActorsInFrontOfCharacter(APawn const* pawn) const;
    void TakeDecision(UWorld* world, APawn* pawn, ACharacter* playerCharacter, float deltaTime);
    bool IsVisible(UWorld* world, APawn* pawn, AActor* targetActor);
    bool IsPlayerVisible(UWorld* world, APawn* pawn, AActor* targetActor);
    AActor* IsPickupVisible(UWorld* world, APawn* pawn);
    void Roam(APawn* pawn, float deltaTime);
};
