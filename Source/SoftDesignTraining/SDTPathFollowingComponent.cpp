// Fill out your copyright notice in the Description page of Project Settings.

#include "Kismet/KismetMathLibrary.h"
#include "SDTPathFollowingComponent.h"
#include "SoftDesignTraining.h"
#include "SDTUtils.h"
#include "SDTAIController.h"
#include "GameFramework/CharacterMovementComponent.h"

#include "DrawDebugHelpers.h"

USDTPathFollowingComponent::USDTPathFollowingComponent(const FObjectInitializer& ObjectInitializer)
{}

void USDTPathFollowingComponent::FollowPathSegment(float DeltaTime)
{
    const TArray<FNavPathPoint>& points = Path->GetPathPoints();
    const FNavPathPoint& segmentStart = points[MoveSegmentStartIndex];

    if (SDTUtils::HasJumpFlag(segmentStart)) // Update jump
    {
        ASDTAIController* controller = Cast<ASDTAIController>(GetOwner());
        const float progress = controller->m_jumpTime / controller->m_jumpDuration;
        controller->m_jumpProgress = progress;

        if (MoveSegmentStartIndex + 1 < points.Num())
        {
            const FNavPathPoint& segmentEnd = points[MoveSegmentStartIndex + 1];

            // Compute the targeted height and heading
            float curveValue = Cast<ASDTAIController>(GetOwner())->JumpCurve->GetFloatValue(progress);
            FVector heading = (segmentEnd.Location - segmentStart.Location);

            controller->m_jumpTime += DeltaTime;

            // Teleport to the next location
            controller->GetPawn()->SetActorLocation(FVector(
                controller->m_jumpStartingPos.X + progress * heading.X,
                controller->m_jumpStartingPos.Y + progress * heading.Y,
                controller->m_jumpStartingPos.Z + controller->JumpApexHeight * curveValue));
        }
    }
    else
    {
		Super::FollowPathSegment(DeltaTime);
    }
}

void USDTPathFollowingComponent::SetMoveSegment(int32 segmentStartIndex)
{
    Super::SetMoveSegment(segmentStartIndex);

    const TArray<FNavPathPoint>& points = Path->GetPathPoints();
    const FNavPathPoint& segmentStart = points[MoveSegmentStartIndex];
    const FNavPathPoint& segmentEnd = points[MoveSegmentStartIndex + 1];

    ASDTAIController* controller = Cast<ASDTAIController>(GetOwner());
    APawn* pawn = controller->GetPawn();

    if (SDTUtils::HasJumpFlag(segmentStart) && FNavMeshNodeFlags(segmentStart.Flags).IsNavLink()) // Handle starting jump
    {
        // Set the pawn in flying mode
        Cast<UCharacterMovementComponent>(MovementComp)->SetMovementMode(MOVE_Flying);

        // Update the controller jump states
        controller->AtJumpSegment = true;
        controller->m_jumpTime = 0.0f;
        controller->m_jumpStartingPos = pawn->GetActorLocation();

        // Find the jump heading
        FVector jumpHeading = (segmentEnd.Location - segmentStart.Location);
        jumpHeading.Z = 0;
        jumpHeading.Normalize();

        // Turn the pawn towards the jump heading
        pawn->SetActorRotation(UKismetMathLibrary::FindLookAtRotation(FVector::ZeroVector, jumpHeading));
    }
    else
    {
        controller->AtJumpSegment = false;

        // Set the pawn in walking mode
        Cast<UCharacterMovementComponent>(MovementComp)->SetMovementMode(MOVE_Walking);
    }
}
