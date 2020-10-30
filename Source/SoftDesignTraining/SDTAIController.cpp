// Fill out your copyright notice in the Description page of Project Settings.

#include "SDTAIController.h"
#include "SoftDesignTraining.h"
#include "SDTCollectible.h"
#include "SDTFleeLocation.h"
#include "SDTPathFollowingComponent.h"
#include "DrawDebugHelpers.h"
#include "Kismet/KismetMathLibrary.h"
#include "NavigationSystem.h"
#include "SDTUtils.h"
#include "EngineUtils.h"
#include "SoftDesignTrainingMainCharacter.h"

ASDTAIController::ASDTAIController(const FObjectInitializer& ObjectInitializer)
    : Super(ObjectInitializer.SetDefaultSubobjectClass<USDTPathFollowingComponent>(TEXT("PathFollowingComponent"))),
    m_currentObjective(PawnObjective::GetCollectibles)
{
}

void ASDTAIController::GoToBestTarget(float deltaTime)
{
    if      (m_currentObjective == PawnObjective::GetCollectibles) GoToBestCollectible();
    else if (m_currentObjective == PawnObjective::ChasePlayer)     GoToPlayer();
    else if (m_currentObjective == PawnObjective::EscapePlayer)    GoToBestFleeLocation();
}

/*
 * Moves the pawn to the best flee location
 */
void ASDTAIController::GoToBestFleeLocation()
{
    if (AActor* bestFleeLocation = GetBestFleeLocation())
    {
        MoveToLocation(bestFleeLocation->GetActorLocation(), -1.0f, true, true, true, true, 0, false);
        OnMoveToTarget(bestFleeLocation);
    }
}

/*
 * Finds and returns the best flee location
 * A flee location is better than another one if it is further from the player.
 * A flee location is acceptable if the trajectory to join it does not cross the player's path.
 */
AActor* ASDTAIController::GetBestFleeLocation()
{
    // get all flee locations
    TArray<AActor*> fleeLocations;
    UGameplayStatics::GetAllActorsOfClass(GetWorld(), ASDTFleeLocation::StaticClass(), fleeLocations);

    // sort flee locations by distance to the target player
    fleeLocations.Sort([&](const AActor& fleeLoc1, const AActor& fleeLoc2) {
        const float fleeDist1 = FVector::DistSquared2D(m_targetPlayer->GetActorLocation(), fleeLoc1.GetActorLocation());
        const float fleeDist2 = FVector::DistSquared2D(m_targetPlayer->GetActorLocation(), fleeLoc2.GetActorLocation());
        return fleeDist1 > fleeDist2;
    });

    // select the first flee location path that does not cross the target player
    AActor* bestFleelocation = nullptr;
    auto navSystem = FNavigationSystem::GetCurrent<UNavigationSystemV1>(this);

    for (auto fleeLoc : fleeLocations)
    {
        auto path = navSystem->FindPathToLocationSynchronously(GetWorld(), GetPawn()->GetActorLocation(), fleeLoc->GetActorLocation());
        if (path->PathPoints.Num() >= 2)
        {
            const FVector pawnLoc = GetPawn()->GetActorLocation();
            const FVector fleeDir = (path->PathPoints[1] - pawnLoc).GetSafeNormal();
            const FVector pawnToPlayer = (m_targetPlayer->GetActorLocation() - pawnLoc).GetSafeNormal();
            const bool pathCrossesPlayer = FMath::RadiansToDegrees(std::acos(FVector::DotProduct(fleeDir, pawnToPlayer))) <= 45.0f;

            if (!pathCrossesPlayer)
            {
                bestFleelocation = fleeLoc;
                break;
            }
        }
    }
    return bestFleelocation;
}

/*
 * Moves the pawn straight to the target player
 */
void ASDTAIController::GoToPlayer()
{
    MoveToLocation(m_targetPlayer->GetActorLocation(), -1.0f, true, true, true, true, 0, false);
    OnMoveToTarget(m_targetPlayer);
}

/*
 * Moves the pawn to the nearest collectible
 */
void ASDTAIController::GoToBestCollectible()
{
    TArray<AActor*> targetActors;

    // find all collectibles in the level
    UGameplayStatics::GetAllActorsOfClass(GetWorld(), ASDTCollectible::StaticClass(), targetActors);

    // find nearest actor
    float minDistance = MAX_FLT;
    AActor* targetActor = nullptr;

    for (AActor* actor : targetActors)
    {
        float distanceToTarget = 0.0f;

        // check if the distance to the target is partial
        const UNavigationSystemV1* navSystem = FNavigationSystem::GetCurrent<UNavigationSystemV1>(this);
        navSystem->GetPathLength(GetPawn()->GetActorLocation(), actor->GetActorLocation(), distanceToTarget);
        const bool distanceIsPartial = navSystem->FindPathToLocationSynchronously(GetWorld(), GetPawn()->GetActorLocation(), actor->GetActorLocation())->IsPartial();

        // check if target is visible
        ASDTCollectible* collectible = Cast<ASDTCollectible>(actor);
        const bool targetIsVisible = collectible->GetStaticMeshComponent()->IsVisible();

        // check if no other pawn is already heading towards this
        const bool collectibleIsTargeted = !collectible->m_currentSeeker.IsEmpty() && collectible->m_currentSeeker != GetPawn()->GetActorLabel();

        if (distanceToTarget < minDistance && !distanceIsPartial && targetIsVisible && !collectibleIsTargeted) {
            targetActor = actor;
            minDistance = distanceToTarget;
            collectible->SetCurrentSeeker(GetPawn()->GetActorLabel()); // tell the other pawns that this one is ours
        }
    }

    // move the pawn to the collectible
    if (targetActor) {
        MoveToLocation(targetActor->GetActorLocation(), -1.0f, true, true, true, true, 0, false);
        OnMoveToTarget(targetActor);
    }
}

void ASDTAIController::OnMoveToTarget(AActor* targetActor)
{
    if (ASDTCollectible* collectible = Cast<ASDTCollectible>(m_TargetActor)) collectible->ResetCurrentSeeker();
    m_ReachedTarget = false;
    m_TargetActor = targetActor;
}

void ASDTAIController::OnMoveCompleted(FAIRequestID RequestID, const FPathFollowingResult& Result)
{
    Super::OnMoveCompleted(RequestID, Result);

    m_ReachedTarget = true;
}

void ASDTAIController::ShowNavigationPath()
{
    if (m_TargetActor && m_currentObjective == PawnObjective::GetCollectibles)
    {
        // Get the current path
        auto navSystem = FNavigationSystem::GetCurrent<UNavigationSystemV1>(this);
        auto path = navSystem->FindPathToLocationSynchronously(GetWorld(), GetPawn()->GetActorLocation(), m_TargetActor->GetActorLocation());

        // Draw a line between all points on the path
        FVector previousPoint = GetPawn()->GetActorLocation();
        for (FVector point : path->PathPoints)
        {
            DrawDebugLine(GetWorld(), previousPoint, point, FColor::Red);
            previousPoint = point;
        }
    }
}

void ASDTAIController::ChooseBehavior(float deltaTime)
{
    UpdatePlayerInteraction(deltaTime);
}

void ASDTAIController::UpdatePlayerInteraction(float deltaTime)
{
    //finish jump before updating AI state
    if (AtJumpSegment)
        return;

    APawn* selfPawn = GetPawn();
    if (!selfPawn)
        return;

    ACharacter* playerCharacter = UGameplayStatics::GetPlayerCharacter(GetWorld(), 0);
    if (!playerCharacter)
        return;

    FVector detectionStartLocation = selfPawn->GetActorLocation() + selfPawn->GetActorForwardVector() * m_DetectionCapsuleForwardStartingOffset;
    FVector detectionEndLocation = detectionStartLocation + selfPawn->GetActorForwardVector() * m_DetectionCapsuleHalfLength * 2;

	//Detects all collisions between collectibles and players with the AI within the vision capsule.
    TArray<TEnumAsByte<EObjectTypeQuery>> detectionTraceObjectTypes;
    detectionTraceObjectTypes.Add(UEngineTypes::ConvertToObjectType(COLLISION_COLLECTIBLE));
    detectionTraceObjectTypes.Add(UEngineTypes::ConvertToObjectType(COLLISION_PLAYER));

    TArray<FHitResult> allDetectionHits;
    GetWorld()->SweepMultiByObjectType(allDetectionHits, detectionStartLocation, detectionEndLocation, FQuat::Identity, detectionTraceObjectTypes, FCollisionShape::MakeSphere(m_DetectionCapsuleRadius));

    FHitResult detectionHit;
    GetHightestPriorityDetectionHit(allDetectionHits, detectionHit);

    //Set behavior based on hit
    UpdateBehavior(detectionHit);
    
    // draw the pawn vision capsule
    DrawDebugCapsule(GetWorld(), detectionStartLocation + m_DetectionCapsuleHalfLength * selfPawn->GetActorForwardVector(), m_DetectionCapsuleHalfLength, m_DetectionCapsuleRadius, selfPawn->GetActorQuat() * selfPawn->GetActorUpVector().ToOrientationQuat(), FColor::Blue);
}

/*
 * Updates the pawn state depending on the detection hit results
 */
void ASDTAIController::UpdateBehavior(FHitResult detectionHit)
{
    const PawnObjective oldObjective = m_currentObjective;
    const UPrimitiveComponent* component = detectionHit.GetComponent();
    bool foundVisiblePlayer = false;

    if (component)
    {
        const bool foundPlayer = component->GetCollisionObjectType() == COLLISION_PLAYER;
        const bool playerIsVisible = TargetIsVisible(component->GetComponentLocation());
        foundVisiblePlayer = foundPlayer && playerIsVisible;

        if (foundVisiblePlayer)
        {
            const bool playerIsPoweredUp = SDTUtils::IsPlayerPoweredUp(GetWorld());

            if (playerIsPoweredUp)
            {
                if (m_currentObjective == PawnObjective::EscapePlayer) m_ReachedTarget = true;
                else m_currentObjective = PawnObjective::EscapePlayer;
            }
            else
            {
                m_ReachedTarget = true;
                m_currentObjective = PawnObjective::ChasePlayer;
            }

            m_targetPlayer = detectionHit.GetActor();
        }
    }
    // get collectibles if nothing else to do
    if (!foundVisiblePlayer && m_ReachedTarget) m_currentObjective = PawnObjective::GetCollectibles;

    // interrupt if the objective changed
    if (m_currentObjective != oldObjective) AIStateInterrupted();
}

/*
 * Indicates if the specified location is directly visible to the pawn
 */
bool ASDTAIController::TargetIsVisible(FVector targetLocation)
{
    return !SDTUtils::Raycast(GetWorld(), GetPawn()->GetActorLocation(), targetLocation);
}

void ASDTAIController::GetHightestPriorityDetectionHit(const TArray<FHitResult>& hits, FHitResult& outDetectionHit)
{
    for (const FHitResult& hit : hits)
    {
        if (UPrimitiveComponent* component = hit.GetComponent())
        {
            if (component->GetCollisionObjectType() == COLLISION_PLAYER)
            {
                //we can't get more important than the player
                outDetectionHit = hit;
                return;
            }
            else if (component->GetCollisionObjectType() == COLLISION_COLLECTIBLE)
            {
                outDetectionHit = hit;
            }
        }
    }
}

void ASDTAIController::AIStateInterrupted()
{
    StopMovement();
    m_ReachedTarget = true;
}