// Fill out your copyright notice in the Description page of Project Settings.


#include "SPowerupActor.h"
#include "Net/UnrealNetwork.h"

// Sets default values
ASPowerupActor::ASPowerupActor()
{
	PowerUpInterval = 0.0f;
	TotalNumberOfTicks = 0;

	bIsPowerupActive = false;

	// Net replication
	SetReplicates(true); 
}

void ASPowerupActor::GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const
{
	Super::GetLifetimeReplicatedProps(OutLifetimeProps);

	DOREPLIFETIME(ASPowerupActor, bIsPowerupActive);
}

void ASPowerupActor::ActivatePowerup(AActor* OtherActor)
{
	if (OtherActor == nullptr)
	{
		return;
	}
	OnActivated(OtherActor);

	bIsPowerupActive = true;
	OnRep_PowerupActive();

	if (PowerUpInterval > 0.0f)
	{
		GetWorldTimerManager().SetTimer(PowerupTimerHandle, this, &ASPowerupActor::OnTickPowerup, PowerUpInterval, true);
	}
	else
	{
		OnTickPowerup();
	}
}

void ASPowerupActor::OnRep_PowerupActive()
{
	OnPowerupStateChanged(bIsPowerupActive);
}

void ASPowerupActor::OnTickPowerup()
{
	TicksProccessed++;

	OnPowerupTicked();

	if (TicksProccessed >= TotalNumberOfTicks)
	{
		OnExpired();

		bIsPowerupActive = false;
		OnRep_PowerupActive();

		GetWorldTimerManager().ClearTimer(PowerupTimerHandle);
	}
}

