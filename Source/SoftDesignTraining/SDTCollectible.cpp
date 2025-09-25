// Fill out your copyright notice in the Description page of Project Settings.

#include "SDTCollectible.h"
#include "SoftDesignTraining.h"

ASDTCollectible::ASDTCollectible()
{
    PrimaryActorTick.bCanEverTick = true;
}

void ASDTCollectible::BeginPlay()
{
    Super::BeginPlay();
}

void ASDTCollectible::Collect(AController* pawn)
{
    
    if (Cast<APlayerController>(pawn) && playerCollectSound){
        UGameplayStatics::PlaySoundAtLocation(GetWorld(), playerCollectSound, GetActorLocation());
    }
    else if (agentCollectSound) {
        UGameplayStatics::PlaySoundAtLocation(GetWorld(), agentCollectSound, GetActorLocation());
    }

    if (ParticleEffect)
    {
        UGameplayStatics::SpawnEmitterAtLocation(
            GetWorld(),
            ParticleEffect,
            GetActorLocation(),
            GetActorRotation()
        );
    }
  
    GetWorld()->GetTimerManager().SetTimer(m_CollectCooldownTimer, this, &ASDTCollectible::OnCooldownDone, m_CollectCooldownDuration, false);

    GetStaticMeshComponent()->SetVisibility(false);
}


void ASDTCollectible::OnCooldownDone()
{
    GetWorld()->GetTimerManager().ClearTimer(m_CollectCooldownTimer);

    GetStaticMeshComponent()->SetVisibility(true);
}

bool ASDTCollectible::IsOnCooldown()
{
    return m_CollectCooldownTimer.IsValid();
}

void ASDTCollectible::Tick(float deltaTime)
{
    Super::Tick(deltaTime);
}
