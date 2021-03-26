// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Pawn.h"
#include "STrackerBot.generated.h"

class UStaticMeshComponent;
class USHealthComponent;
class USphereComponent;
class USoundCue;

UCLASS()
class COOPGAME_API ASTrackerBot : public APawn
{
	GENERATED_BODY()

public:
	// Sets default values for this pawn's properties
	ASTrackerBot();

protected:
	// Called when the game starts or when spawned
	virtual void BeginPlay() override;

	UPROPERTY(VisibleDefaultsOnly, Category = "Components")
	UStaticMeshComponent* MeshComponent;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Components")
	USHealthComponent* HealthComponent;

	UPROPERTY(VisibleDefaultsOnly, Category = "Components")
	USphereComponent* SphereComponent;

	FVector NextPathPoint();

	//Next point in navigation path
	FVector NextNavPathPoint;

	UPROPERTY(EditDefaultsOnly, Category = "TrackerBot")
	UParticleSystem* SelfDestructEffect;

	UPROPERTY(EditDefaultsOnly, Category = "TrackerBot")
	USoundCue* SelfDestructSound;

	UPROPERTY(EditDefaultsOnly, Category = "TrackerBot")
	USoundCue* ExplosionSound;

	UPROPERTY(EditDefaultsOnly, Category="TrackerBot")
	float ForceMagnitude;

	UPROPERTY(EditDefaultsOnly, Category = "TrackerBot")
	float MinDistanceToTarget;

	UPROPERTY(EditDefaultsOnly, Category = "TrackerBot")
	bool bUseVelocityChange;

	// Dynamic matrial to pulse
	UMaterialInstanceDynamic* MatInst;

	UPROPERTY(EditDefaultsOnly, Category = "TrackerBot")
	FName MaterialPulseParameterName;

	FTimerHandle FTimeBeforeSelfDestruct;

	FTimerHandle FTimeREfreshPath;

	bool bExploded;

	bool bStartedSelfDestruction;

	UFUNCTION()
	void HandleTakeDamage(USHealthComponent* HealthComp, float Health, float HealthDelta, const class UDamageType* DamageType, class AController* InstigatedBy, AActor* DamageCauser);

	UFUNCTION()
	void HandleSphereOverlap(AActor* OverlappedActor, AActor* OtherActor);

	void SelfDestruct();

	void SelfDamage();

	void RefreshPath();

public:	
	// Called every frame
	virtual void Tick(float DeltaTime) override;
};
