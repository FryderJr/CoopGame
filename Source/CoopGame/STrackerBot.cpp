// Fill out your copyright notice in the Description page of Project Settings.


#include "STrackerBot.h"
#include "NavigationSystem.h"
#include "NavigationPath.h"
#include "Sound/SoundCue.h"
#include "SHealthComponent.h"
#include "DrawDebughelpers.h"
#include "Kismet/GameplayStatics.h"
#include "GameFramework/Character.h"
#include "Components/SphereComponent.h"
#include "Components/StaticMeshComponent.h"


// Sets default values
ASTrackerBot::ASTrackerBot()
{
 	// Set this pawn to call Tick() every frame.  You can turn this off to improve performance if you don't need it.
	PrimaryActorTick.bCanEverTick = true;

	MeshComponent = CreateDefaultSubobject<UStaticMeshComponent>(FName("MeshComp"));
	MeshComponent->SetCanEverAffectNavigation(false);
	MeshComponent->SetSimulatePhysics(true);
	RootComponent = MeshComponent;

	HealthComponent = CreateDefaultSubobject<USHealthComponent>(FName("HealthComp"));
	HealthComponent->OnHealthChanged.AddDynamic(this, &ASTrackerBot::HandleTakeDamage);

	SphereComponent = CreateDefaultSubobject<USphereComponent>(FName("SphereComp"));
	SphereComponent->SetSphereRadius(200);
	SphereComponent->SetCollisionEnabled(ECollisionEnabled::QueryOnly);
	SphereComponent->SetCollisionResponseToAllChannels(ECR_Ignore);
	SphereComponent->SetCollisionResponseToChannel(ECC_Pawn, ECR_Overlap);
	SphereComponent->SetupAttachment(RootComponent);

	MaterialPulseParameterName = FName("LastTimeDamageTaken");

	bUseVelocityChange = false;
	ForceMagnitude = 1000;
	MinDistanceToTarget = 100.0f;
}

// Called when the game starts or when spawned
void ASTrackerBot::BeginPlay()
{
	Super::BeginPlay();
	
	if (GetLocalRole() == ROLE_Authority) // Run only on server
	{
		RefreshPath();
	}

	bExploded = false;

	OnActorBeginOverlap.AddDynamic(this, &ASTrackerBot::HandleSphereOverlap);

}

FVector ASTrackerBot::NextPathPoint()
{
	AActor* BestTargetToAttack = nullptr;
	float NearestTargetDistance = FLT_MAX;

	for (FConstPawnIterator it = GetWorld()->GetPawnIterator(); it; ++it)
	{
		APawn* TestPawn = it->Get();
		if (TestPawn == nullptr || USHealthComponent::IsFriendly(TestPawn, this))
		{
			continue;
		}
		USHealthComponent* HealthComp = Cast<USHealthComponent>(TestPawn->GetComponentByClass(USHealthComponent::StaticClass()));
		if (HealthComp && HealthComp->GetHealth() > 0.0f)
		{
			float Distance = (TestPawn->GetActorLocation() - this->GetActorLocation()).Size();

			if (NearestTargetDistance > Distance)
			{
				BestTargetToAttack = TestPawn;
				NearestTargetDistance = Distance;
			}
		}
	}

	if (BestTargetToAttack)
	{
		UNavigationPath* NavPath = UNavigationSystemV1::FindPathToActorSynchronously(this, GetActorLocation(), BestTargetToAttack);
		GetWorldTimerManager().ClearTimer(FTimeREfreshPath);
		GetWorldTimerManager().SetTimer(FTimeREfreshPath, this, &ASTrackerBot::RefreshPath, 5.0f, false);
		if (NavPath)
		{
			if (NavPath->GetPathLength() > 1)
			{
				return NavPath->PathPoints[1];
			}
		}
	}
	
	return GetActorLocation();
}

void ASTrackerBot::HandleTakeDamage(USHealthComponent* HealthComp, float Health, float HealthDelta, const UDamageType* DamageType, AController* InstigatedBy, AActor* DamageCauser)
{
	if (MatInst == nullptr)
	{
		MatInst = MeshComponent->CreateAndSetMaterialInstanceDynamicFromMaterial(0, MeshComponent->GetMaterial(0));
	}
	if (MatInst)
	{
		MatInst->SetScalarParameterValue(MaterialPulseParameterName, GetWorld()->GetTimeSeconds());
	}
	if (Health <= 0)
	{
		SelfDestruct();
	}
}

void ASTrackerBot::HandleSphereOverlap(AActor* OverlappedActor, AActor* OtherActor)
{
	if (bStartedSelfDestruction || bExploded)
	{
		return;
	}
	if (OtherActor->GetNetOwningPlayer())
	{
		if (GetLocalRole() == ROLE_Authority)
		{
			GetWorldTimerManager().SetTimer(FTimeBeforeSelfDestruct, this, &ASTrackerBot::SelfDamage, 0.5f, true, 0.0f);
		}
		bStartedSelfDestruction = true;
		if (SelfDestructSound)
		{
			UGameplayStatics::SpawnSoundAttached(SelfDestructSound, RootComponent);
		}
	}
}

void ASTrackerBot::SelfDestruct()
{
	if (bExploded)
	{
		return;
	}

	bExploded = true;

	if (SelfDestructEffect)
	{
		UGameplayStatics::SpawnEmitterAtLocation(GetWorld(), SelfDestructEffect, GetActorLocation());
	}
	if (ExplosionSound)
	{
		UGameplayStatics::PlaySoundAtLocation(this, ExplosionSound, GetActorLocation(), 2.0f);
	}

	MeshComponent->SetVisibility(false, true);
	MeshComponent->SetCollisionEnabled(ECollisionEnabled::NoCollision);

	if (GetLocalRole() == ROLE_Authority)
	{
		TArray<AActor*> IgnoredActors;
		IgnoredActors.Add(this);
		UGameplayStatics::ApplyRadialDamage(this, 100.0f, GetActorLocation(), 200.0f, nullptr, IgnoredActors, this, GetInstigatorController(), true);

		SetLifeSpan(2.0f);
	}
}

void ASTrackerBot::SelfDamage()
{
	UGameplayStatics::ApplyDamage(this, 20.0f, GetInstigatorController(), this, nullptr);
}

void ASTrackerBot::RefreshPath()
{
	NextNavPathPoint = NextPathPoint();
}

// Called every frame
void ASTrackerBot::Tick(float DeltaTime)
{
	Super::Tick(DeltaTime);

	if (GetLocalRole() == ROLE_Authority && !bExploded)
	{
		float DistanceToTarget = (GetActorLocation() - NextNavPathPoint).Size();

		if (DistanceToTarget <= MinDistanceToTarget)
		{
			RefreshPath();
		}
		else
		{
			FVector ForceDirection = NextNavPathPoint - GetActorLocation();

			ForceDirection.Normalize();
			ForceDirection *= ForceMagnitude;

			MeshComponent->AddForce(ForceDirection, NAME_None, bUseVelocityChange);
		}
	}
}

