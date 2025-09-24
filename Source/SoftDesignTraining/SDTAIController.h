// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

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
    float m_VisionAngle = 100.f;

    virtual void Tick(float deltaTime) override;
    virtual void BeginPlay() override;
    void ResetSpeed();
    
private:
    float m_Speed = 0.f;
    float m_MaxSpeed = 0.f;
    float CalculateMovement(float maxSpeed, float acceleration, float delaTime);
    float AvoidWall(float delaTime);
    bool MoveToTarget(FVector target, float speed, float deltaTime);
    bool ChasePlayer(float deltaTime);
    TArray<FOverlapResult> CollectTargetActorsInFrontOfCharacter(APawn const* pawn) const;
    bool IsInsideCone(APawn* pawn, AActor* targetActor) const;
};
