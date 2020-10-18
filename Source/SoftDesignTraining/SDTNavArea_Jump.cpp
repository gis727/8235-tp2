// Fill out your copyright notice in the Description page of Project Settings.

#include "SDTNavArea_Jump.h"
#include "SoftDesignTraining.h"

#include "SDTUtils.h"

USDTNavArea_Jump::USDTNavArea_Jump(const FObjectInitializer& ObjectInitializer)
    : Super(ObjectInitializer)
{
    SDTUtils::SetNavTypeFlag(AreaFlags, SDTUtils::Jump);
}

// Fix for a hot reload issue: https://answers.unrealengine.com/questions/197617/nav-link-proxy-not-seting-custom-navarea-flag.html?sort=oldest
void USDTNavArea_Jump::FinishDestroy()
{
    if (HasAnyFlags(RF_ClassDefaultObject)
        #if WITH_HOT_RELOAD
        && !GIsHotReload
        #endif // WITH_HOT_RELOAD
        )
    {
    }
    UObject::FinishDestroy();
}
