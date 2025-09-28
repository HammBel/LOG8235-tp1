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
}


/** 
* Déplace le Pawn vers une cible tout en évitant les obstacles. 
* 
* L’algorithme tente d’abord de se diriger vers la cible directement. 
* S’il y a un obstacle détecté par un BoxCast, il calcule une trajectoire 
* alternative en explorant à gauche et à droite jusqu’à trouver une position 
* sûre pour contourner l’obstacle. 
* 
* Entrées : 
* @param world Monde Unreal (UWorld*) pour effectuer les traces. 
* @param pawn Pawn de l'agent (APawn*). 
* @param target position de la cible (FVector). 
* @param speed Vitesse désirée du déplacement (float). 
* @param deltaTime Temps écoulé depuis la dernière frame (float). 
* 
* Retour : 
* @return true si le Pawn est suffisamment proche de la cible (objectif atteint). 
* @return false sinon (en cours de déplacement ou bloqué). 
*/
bool ASDTAIController::MoveToTarget(UWorld* world, APawn* pawn, FVector target, float speed, float deltaTime)
{
    FRotator currentRotation = pawn->GetActorRotation();
    TArray<FHitResult> hitResults;
    FVector const pawnPosition(pawn->GetActorLocation());
    FVector2D toTarget = FVector2D(target) - FVector2D(pawnPosition);
    FVector2D displacement = FMath::Min(toTarget.Size(), speed * deltaTime) * toTarget.GetSafeNormal();
    float InitialYawAngle = FMath::RadiansToDegrees(FMath::Atan2(toTarget.GetSafeNormal().Y, toTarget.GetSafeNormal().X));
    // Vérifie si une collision avec un death floor existe entre la position actuelle et la cible
    SDTUtils::BoxCast(world, pawn->GetActorLocation(), target, halfExtent, FVector(displacement, 0.f).ToOrientationQuat(), hitResults, false);

    // Si on détecte une collision -> recherche d’un chemin alternatif
    if (hitResults.Num() != 0) {
        FVector normal = hitResults[0].ImpactNormal;
        FVector right = FVector::CrossProduct(FVector(FVector2D(hitResults[0].ImpactNormal), 0.0f), pawn->GetActorUpVector()).GetSafeNormal();
        FVector* leftSafePosition = nullptr;
        FVector* rightSafePosition = nullptr;
        // Angle de recherche incrémental
        float angle = 1;
        FVector2D newToTarget;
        FVector2D newDisplacement;
        FVector newTarget;
        FVector lastRightNormal = hitResults[0].ImpactNormal;
        FVector lastLeftNormal = hitResults[0].ImpactNormal;
        // Détecte si il y'a un grand changement de valeur par rapport a la précedente normal d'impacte
        bool ignoreNormal = false;

        // Recherche d’un chemin à gauche et à droite
        while (!leftSafePosition || !rightSafePosition) {
            // Vérification côté droit
            if (!rightSafePosition) {
                //Recherche d'un chemin possible a droite
                newTarget = pawnPosition + FVector(toTarget.Size() * cos(FMath::DegreesToRadians(InitialYawAngle + angle)), toTarget.Size() * sin(FMath::DegreesToRadians(InitialYawAngle + angle)), 0);
                newToTarget = FVector2D(newTarget) - FVector2D(pawnPosition);
                newDisplacement = FMath::Min(newToTarget.Size(), speed * deltaTime) * newToTarget.GetSafeNormal();
                // Vérifie si le chemin est libre
                if (!SDTUtils::BoxCast(world, pawn->GetActorLocation() + FVector(newDisplacement, 0).GetSafeNormal() * halfExtent * 2, newTarget, halfExtent * 2, FVector(newDisplacement, 0.f).ToOrientationQuat(), hitResults, false)) {
                    TArray<FHitResult> rayHits;
                    // Si aucun obstacle (mur) détecté on peut prendre directement la nouvelle position
                    if (!SDTUtils::CastRay(world, pawn->GetActorLocation(), newTarget, rayHits, false))
                        rightSafePosition = new FVector(newTarget);
                    else {
                        // Si il y a un obstacle on prend pour position la position du point d'impact avec celui-ci
                        rightSafePosition = new FVector(rayHits[0].ImpactPoint);
                    }
                }
                // Vérifie si la nouvelle normal varie beaucoup par rapport a la précédente
                else if (!ignoreNormal) {
                    if ((hitResults[0].ImpactNormal - lastRightNormal).Size() > .2)
                        ignoreNormal = true;
                    lastRightNormal = hitResults[0].ImpactNormal;
                }
            }
            // Vérification côté gauche (symétrique au code précédent)
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
            // Vérification à 360 degrés autour de l'agent (180 degrés de chaque coté, droite et gauche). Pour ne pas boucler a l'infini dans le cas ou aucun chemin n'a été trouvé.
            if (angle > 180) 
                break;
        }
        // Choix final de la nouvelle position vers laquelle l'agent va se déplacer

        //Si aucun chemin n'a été trouvé
        if (!leftSafePosition && !rightSafePosition) return false; 

        // Si uniquement un des cotés (droite ou gauche) possède un nouveau chemin possible
        else if (!leftSafePosition || !rightSafePosition) target = leftSafePosition ? *leftSafePosition : *rightSafePosition;


        // Si un grand changement de la normale on considere simplement la distance la plus petite par rapport a la cible 
        else if (ignoreNormal)
            target = (*leftSafePosition - target).Size() > (*rightSafePosition - target).Size() ? *rightSafePosition : *leftSafePosition;
        // Sinon: projection sur la normale avant de choisir le chemin le plus proche de la cible  
        else if ((FVector::DotProduct(*leftSafePosition - target, normal) * normal).Size() < (FVector::DotProduct(*rightSafePosition - target, normal) * normal).Size())
            target = *leftSafePosition;
        else target = *rightSafePosition;
    }

    // Mise à jour du déplacement après éventuelle redirection
    toTarget = FVector2D(target) - FVector2D(pawnPosition);
    displacement = FMath::Min(toTarget.Size(), speed * deltaTime) * toTarget.GetSafeNormal();
    float cosAngle = FVector::DotProduct(pawn->GetActorForwardVector(), FVector(toTarget, 0).GetSafeNormal());
    float turnFactor = (cosAngle + 1.f) * 0.5f;

    float targetSpeed = m_MaxSpeed * turnFactor;

    m_Speed = FMath::Lerp(m_Speed, targetSpeed, 0.25f);
    
    pawn->SetActorRotation(FMath::Lerp(pawn->GetActorRotation(), FVector(displacement, 0.f).ToOrientationRotator(), .5f));
    pawn->AddMovementInput(pawn->GetActorForwardVector(), speed / m_MaxSpeed);

    // Retourne vrai si la distance à la cible est inférieure au déplacement d’un tick -> objectif atteint
    return toTarget.Size() < speed * deltaTime;
}


/** 
* Fait fuir le Pawn par rapport à la position du joueur en choisissant 
* une trajectoire qui maximise la distance avec lui tout en évitant 
* les obstacles. 
* 
* L’algorithme calcule d’abord une direction opposée au joueur. 
* Si un obstacle bloque cette direction, il explore des alternatives 
* à gauche et à droite jusqu’à trouver une position « sûre »,
* et choisit celle qui est la plus éloignée du joueur. 
* 
* Entrées : 
* @param world Monde Unreal (UWorld*). 
* @param pawn Pawn de l'agent (APawn*). 
* @param playerPosition Position du joueur à fuir (FVector). 
* @param speed Vitesse désirée (float). 
* @param deltaTime Temps écoulé depuis la dernière frame (float). 
* 
* Retour : 
* @return true si le Pawn est suffisamment éloigné de la cible actuelle (a atteint le point de fuite). 
* @return false sinon 
*/
bool ASDTAIController::FleeToTarget(UWorld* world, APawn* pawn, FVector playerPosition, float speed, float deltaTime)
{
    // Calcul d’un premier point de fuite directement opposé au joueur
    FVector target = pawn->GetActorLocation() - (playerPosition - pawn->GetActorLocation()).GetSafeNormal() * speed;
    FRotator currentRotation = pawn->GetActorRotation();
    TArray<FHitResult> hitResults;
    FVector const pawnPosition(pawn->GetActorLocation());
    FVector2D toTarget = FVector2D(target) - FVector2D(pawnPosition);
    FVector2D displacement = FMath::Min(toTarget.Size(), speed * deltaTime) * toTarget.GetSafeNormal();
    float InitialYawAngle = FMath::RadiansToDegrees(FMath::Atan2(toTarget.GetSafeNormal().Y, toTarget.GetSafeNormal().X));
    SDTUtils::BoxCast(world, pawn->GetActorLocation(), target, halfExtent, FVector(displacement, 0.f).ToOrientationQuat(), hitResults, true);
    FVector farthestPosition = target;
    // Si la trajectoire est bloquée -> recherche d’alternatives
    if (hitResults.Num() != 0) {
        float angle = 1;
        FVector2D newToTarget;
        FVector2D newDisplacement;
        FVector newTarget;
        float farthestDistance = 0;
        // Explore des chemins a gauche et a droite selon l'angle jusqu’à 100° (positions a l'arriere ignorées)
        while (angle < 100) {
            // Vérification du coté droit
            newTarget = pawnPosition + FVector(toTarget.Size() * cos(FMath::DegreesToRadians(InitialYawAngle + angle)), toTarget.Size() * sin(FMath::DegreesToRadians(InitialYawAngle + angle)), 0);
            newToTarget = FVector2D(newTarget) - FVector2D(pawnPosition);
            newDisplacement = FMath::Min(newToTarget.Size(), speed * deltaTime) * newToTarget.GetSafeNormal();
            // Vérifie uniquement la présence de deathfloor initialement
            if (!SDTUtils::BoxCast(world, pawn->GetActorLocation() + FVector(newDisplacement, 0).GetSafeNormal() * halfExtent * 2, newTarget, halfExtent * 2, FVector(newDisplacement, 0.f).ToOrientationQuat(), hitResults, false)) {
                // Vérifie la présence de mur
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
          
            // Vérification du coté gauche
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

    // Mise à jour de la rotation et de la vitesse de l'agent selon le chemin trouvé
    toTarget = FVector2D(farthestPosition) - FVector2D(pawnPosition);
    displacement = FMath::Min(toTarget.Size(), speed * deltaTime) * toTarget.GetSafeNormal();
    float cosAngle = FVector::DotProduct(pawn->GetActorForwardVector(), FVector(toTarget, 0).GetSafeNormal()); 
    float turnFactor = (cosAngle + 1.f) * 0.5f;          

    float targetSpeed = m_MaxSpeed * turnFactor;

    m_Speed = FMath::Lerp(m_Speed, targetSpeed, 0.25f);

    pawn->SetActorRotation(FMath::Lerp(pawn->GetActorRotation(), FVector(displacement, 0.f).ToOrientationRotator(), .5f));
    pawn->AddMovementInput(pawn->GetActorForwardVector(), speed / m_MaxSpeed);
    return toTarget.Size() < speed * deltaTime;
}

/** 
* Fait errer l'agent en avançant continuellement
* tout en s’appuyant sur la logique de fuite pour 
* éviter les obstacles (murs et death floors). 
* 
* Entrées : 
* @param pawn Pawn de l'agent (APawn*). 
* @param deltaTime Temps écoulé depuis la dernière frame (float). 
*/
void ASDTAIController::Roam(APawn* pawn, float deltaTime)
{
	FVector forwardDir = pawn->GetActorForwardVector();
    m_Speed = FMath::Clamp(m_Speed, 50.0f, m_MaxSpeed / 2);
    FleeToTarget(GetWorld(), pawn, pawn->GetActorLocation() - forwardDir * 100, FMath::Clamp(m_Speed, 50.0f, m_MaxSpeed / 2), deltaTime);
}


/** 
* Prend une décision de comportement pour l’IA en fonction du contexte (présence du joueur, 
* pickups visibles, dernière position du joueur, etc.). 
* 
* Logique : 
* - Si le joueur est visible : 
* • Si le joueur est "power-up" -> l’IA fuit. 
* • Sinon -> l’IA poursuit le joueur. 
* - Si le joueur n’est plus visible mais qu’une dernière position connue existe -> l’IA s’y déplace. 
* - Si un pickup est visible -> l’IA se dirige vers lui. 
* - Sinon -> l’IA erre (roam). 
* 
* Entrées : 
* @param world Monde de jeu actuel (UWorld*). 
* @param pawn Pawn de l'agent (APawn*). 
* @param playerCharacter Joueur (ACharacter*)
* @param deltaTime Temps écoulé depuis la dernière frame (float). 
*/
void ASDTAIController::TakeDecision(UWorld* world, APawn* pawn, ACharacter* playerCharacter, float deltaTime) {
    if (playerCharacter && IsPlayerVisible(world, pawn, playerCharacter)) {
        if (SDTUtils::IsPlayerPoweredUp(world)) {
            m_LastPlayerPositionReached = true;
            FleeFromPlayer(pawn, playerCharacter->GetActorLocation(), CalculateMovement(m_MaxSpeed, m_Acceleration, deltaTime), deltaTime);
        }
        else {
            GetWorld()->GetTimerManager().ClearTimer(TimerHandle);
            GetWorld()->GetTimerManager().SetTimer(TimerHandle, this, &ASDTAIController::ForgetPlayer, 5.0f, false);
            m_LastPlayerPositionReached = ChasePlayer(pawn, playerCharacter, deltaTime);
            
        }
    }
    else if (!m_LastPlayerPositionReached) {
        m_LastPlayerPositionReached = MoveToTarget(world, pawn, m_LastPlayerPosition, CalculateMovement(m_MaxSpeed, m_Acceleration, deltaTime), deltaTime);

    }
    else if (AActor* pickup = IsPickupVisible(world, pawn)) {
        MoveToTarget(world, pawn, pickup->GetActorLocation(), CalculateMovement(m_MaxSpeed, m_Acceleration, deltaTime), deltaTime);
    }

    else {
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


/** 
* Vérifie si une cible est visible par le Pawn. 
* 
* Logique : 
* - Si la cible est trop éloignée -> non visible. 
* - On lance un raycast (CastRay) entre le Pawn et la cible. 
* - Si un mur bloque la vision -> non visible. 
* - Sinon -> la cible est visible. 
* 
* Entrées : 
* @param world Monde de jeu actuel (UWorld*). 
* @param pawn Pawn de l'agent (APawn*). 
* @param targetActor Acteur dont on veut vérifier la visibilité (AActor*). 
* 
* Retour : 
* @return true si la cible est visible (aucun obstacle bloquant la ligne de vue). 
* @return false sinon (hors portée ou obstacle rencontré). 
*/
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


/**
 * Détermine si un collectible (pickup) est visible par le Pawn et renvoie le plus proche.
 *
 * Logique :
 *   - Récupère tous les acteurs devant le Pawn.
 *   - Filtre pour ne garder que les collectibles disponibles (non en cooldown).
 *   - Trie les résultats par distance (le plus proche en premier).
 *   - Pour chaque collectible trié, vérifie la visibilité.
 *   - Retourne le premier pickup visible trouvé.
 *
 * Entrées :
 *   @param world  Monde de jeu actuel (UWorld*).
 *   @param pawn   Pawn de l'agent (APawn*).
 *
 * Retour :
 *   @return Pointeur vers l’acteur collectible le plus proche et visible.
 *   @return nullptr si aucun pickup visible.
 */

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
