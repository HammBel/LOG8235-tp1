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
    float m_VisionAngle = PI / 3;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = AI)
    float m_ViewDistance = 1000.f;

    virtual void Tick(float deltaTime) override;
    virtual void BeginPlay() override;
    void Reset();

    enum AIState {
        ROAMING,
        FLEEING_PLAYER,
        CHASING_PLAYER,
        CHASING_PICKUP
    };
    
private:
    struct FCastInfo
    {
        FVector Start;
        FVector End;
        TArray<FHitResult> Hits;
        FCollisionObjectQueryParams* objectQueryParams;
        FColor DebugColor;
    };

    void SetState(UWorld* world, APawn* pawn, ACharacter* playerCharacter);
    AIState  m_State = ROAMING;
    float m_Speed = 0.f;
    float m_MaxSpeed = 0.f;
    bool m_IsFleeing = false;
    FVector halfExtent;
    float halfWidth;
    float halfHeight;
    AActor* m_ObstacleToDodge;
    FVector m_ObstacleToDodgeNormal;
    bool m_isRotatingRight;
    FVector m_LastPlayerPosition = FVector();
    bool m_LastPlayerPositionReached = true;
    float CalculateMovement(float maxSpeed, float acceleration, float deltaTime);
    bool AvoidWall(APawn* pawn, float speed, float deltaTime);
    bool MoveToTarget(UWorld* world, APawn* pawn, FVector target, float speed, float deltaTime);
    bool ChasePlayer(APawn* pawn, ACharacter* playerCharacter, float deltaTime);
    bool FleeFromPlayer(APawn* pawn, FVector target, float speed, float deltaTime);
    TArray<FOverlapResult> CollectTargetActorsInFrontOfCharacter(APawn const* pawn) const;
    void DrawVisionCone(UWorld* world, APawn* pawn, float angle) const;
    void TakeDecision(UWorld* world, APawn* pawn, ACharacter* playerCharacter, float deltaTime);
    bool IsVisible(UWorld* world, APawn* pawn, AActor* targetActor);
    bool IsPlayerVisible(UWorld* world, APawn* pawn, AActor* targetActor);
    AActor* IsPickupVisible(UWorld* world, APawn* pawn);
    void Roam(APawn* pawn, float deltaTime);
    TArray<FCastInfo> CheckSurrounding(APawn* pawn);
    int ComputeObstacleToDodge(APawn* pawn, const TArray<FCastInfo>& Casts);
    bool AvoidWall2(APawn* pawn, float speed, float deltaTime);
};
