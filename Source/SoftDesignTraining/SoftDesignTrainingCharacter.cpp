// Copyright 1998-2015 Epic Games, Inc. All Rights Reserved.

#include "SoftDesignTrainingCharacter.h"
#include "SoftDesignTraining.h"
#include "SoftDesignTrainingMainCharacter.h"
#include "SDTUtils.h"
#include "DrawDebugHelpers.h"
#include "SDTCollectible.h"
#include "SDTAIController.h"


ASoftDesignTrainingCharacter::ASoftDesignTrainingCharacter()
{
    GetCapsuleComponent()->InitCapsuleSize(42.f, 96.0f);
}

void ASoftDesignTrainingCharacter::BeginPlay()
{
    Super::BeginPlay();

    GetCapsuleComponent()->OnComponentBeginOverlap.AddDynamic(this, &ASoftDesignTrainingCharacter::OnBeginOverlap);
    m_StartingPosition = GetActorLocation();
}

void ASoftDesignTrainingCharacter::OnBeginOverlap(UPrimitiveComponent* OverlappedComponent, AActor* OtherActor, UPrimitiveComponent* OtherComponent, int32 OtherBodyIndex, bool bFromSweep, const FHitResult& SweepResult)
{
    if (OtherComponent->GetCollisionObjectType() == COLLISION_DEATH_OBJECT)
    {
        SetActorLocation(m_StartingPosition);
        if (ASDTAIController* controller = Cast<ASDTAIController>(GetController())) {
            controller->Reset();
        }
        else {
            TArray<AActor*> FoundActors;
            UGameplayStatics::GetAllActorsOfClass(GetWorld(), ASDTAIController::StaticClass(), FoundActors);
            for (auto actor : FoundActors) {
                if (ASDTAIController* AIController = Cast<ASDTAIController>(actor)) {
                    AIController->ForgetPlayer();
                }
            }
        }
    }
    else if(ASDTCollectible* collectibleActor = Cast<ASDTCollectible>(OtherActor))
    {
        if (!collectibleActor->IsOnCooldown())
        {
            OnCollectPowerUp();        
            collectibleActor->Collect(GetController());
        }
    }
    else if (ASoftDesignTrainingMainCharacter* mainCharacter = Cast<ASoftDesignTrainingMainCharacter>(OtherActor))
    {
        if (mainCharacter->IsPoweredUp()) {
            SetActorLocation(m_StartingPosition);
            if (ASDTAIController* controller = Cast<ASDTAIController>(GetController())) {
                controller->Reset();
            }
        }
        else {
            TArray<AActor*> FoundActors;
            UGameplayStatics::GetAllActorsOfClass(GetWorld(), ASDTAIController::StaticClass(), FoundActors);
            for (auto actor : FoundActors) {
                if (ASDTAIController* AIController = Cast<ASDTAIController>(actor)) {
                    AIController->ForgetPlayer();
                }
            }
        }
    }
}
