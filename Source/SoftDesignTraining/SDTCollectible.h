// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "Engine/StaticMeshActor.h"
#include "SDTCollectible.generated.h"

/**
 * 
 */
UCLASS()
class SOFTDESIGNTRAINING_API ASDTCollectible : public AStaticMeshActor
{
	GENERATED_BODY()
public:
    ASDTCollectible();

    void Collect();
    void OnCooldownDone();
    bool IsOnCooldown();
    void SetCurrentSeeker(FString label);
    void ResetCurrentSeeker();
    FString m_currentSeeker;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = AI)
    float m_CollectCooldownDuration = 10.f;

protected:
    FTimerHandle m_CollectCooldownTimer;
	
};
