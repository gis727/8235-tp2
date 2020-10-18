// Fill out your copyright notice in the Description page of Project Settings.

#include "SDTAIController.h"
#include "SoftDesignTraining.h"
#include "SDTCollectible.h"
#include "SDTFleeLocation.h"
#include "SDTPathFollowingComponent.h"
#include "DrawDebugHelpers.h"
#include "Kismet/KismetMathLibrary.h"
#include "NavigationSystem.h"
//#include "UnrealMathUtility.h"
#include "SDTUtils.h"
#include "EngineUtils.h"

ASDTAIController::ASDTAIController(const FObjectInitializer& ObjectInitializer)
    : Super(ObjectInitializer.SetDefaultSubobjectClass<USDTPathFollowingComponent>(TEXT("PathFollowingComponent")))
{
}

void ASDTAIController::GoToBestTarget(float deltaTime)
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
        ASDTCollectible* collectible = dynamic_cast <ASDTCollectible*>(actor);
        const bool targetIsVisible = collectible->GetStaticMeshComponent()->IsVisible();
        
        // check if no other pawn is already heading towards this
        const bool targetIsTargeted = !collectible->m_currentSeeker.IsEmpty() && collectible->m_currentSeeker != GetPawn()->GetActorLabel();
        
        if (distanceToTarget < minDistance && !distanceIsPartial && targetIsVisible && !targetIsTargeted) {
            targetActor = actor;
			minDistance = distanceToTarget;
            collectible->m_currentSeeker = GetPawn()->GetActorLabel(); // tell the other pawns that this one is ours
		}
	}

	if (targetActor) { // go to nearest actor
		MoveToLocation(targetActor->GetActorLocation(), -1.0f, true, true, true, true, 0, false);
        OnMoveToTarget(targetActor);
    }
}

void ASDTAIController::OnMoveToTarget(AActor* targetActor)
{
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
    if (!m_TargetActor) return;

    // Get the current path
    auto navSystem = FNavigationSystem::GetCurrent<UNavigationSystemV1>(this);
    auto path = navSystem->FindPathToLocationSynchronously(GetWorld(), GetPawn()->GetActorLocation(), m_TargetActor->GetActorLocation());
    
    // Draw a line between all points on the path
    FVector previousPoint = GetPawn()->GetActorLocation();
    for (FVector point: path->PathPoints)
    {
        DrawDebugLine(GetWorld(), previousPoint, point, FColor::Red);
        previousPoint = point;
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
	if (UPrimitiveComponent* component = detectionHit.GetComponent())
	{
		if (component->GetCollisionObjectType() != COLLISION_PLAYER)
		{
			GoToBestTarget(deltaTime);
		}
		else {
			// Look for an escape if powered or kill if not powered
		}
	}

    DrawDebugCapsule(GetWorld(), detectionStartLocation + m_DetectionCapsuleHalfLength * selfPawn->GetActorForwardVector(), m_DetectionCapsuleHalfLength, m_DetectionCapsuleRadius, selfPawn->GetActorQuat() * selfPawn->GetActorUpVector().ToOrientationQuat(), FColor::Blue);
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