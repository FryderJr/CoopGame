// Fill out your copyright notice in the Description page of Project Settings.


#include "SWeapon.h"
#include "CoopGame.h"
#include "TimerManager.h"
#include "DrawDebughelpers.h"
#include "Net/UnrealNetwork.h"
#include "Kismet/GameplayStatics.h"
#include "Particles/ParticleSystem.h"
#include "Particles/ParticleSystemComponent.h"
#include "PhysicalMaterials/PhysicalMaterial.h"

// Sets default values
ASWeapon::ASWeapon()
{
	MeshComponent = CreateDefaultSubobject<USkeletalMeshComponent>(FName("MeshComponent"));
	RootComponent = MeshComponent;
	MuzzleSocketName = FName("Muzzle");
	TrailTargetName = FName("BeamEnd");

	BaseDamage = 20.0f;

	FireRate = 600; //Bullets per minute

	//Setting bullet spread parameters
	InitialBulletSpread = 0.05f; 
	BulletSpreadIncreaseRate = 1.5f;
	BulletSpreadDecreaseRate = 3.0f;
	BulletSpreadCoolDownTime = 0.2f;

	//Setup multiplayer
	SetReplicates(true);
	NetUpdateFrequency = 66.0f;
	MinNetUpdateFrequency = 33.0f;
}

void ASWeapon::BeginPlay()
{
	Super::BeginPlay();

	TimeBetweenShots = 60.0f / FireRate;

	CurrentBulletSpread = InitialBulletSpread;

	LastTimeFired = GetWorld()->GetTimeSeconds();
}

void ASWeapon::Fire()
{
	if (GetLocalRole() < ROLE_Authority)
	{
		ServerFire();
	}

	AActor* MyOwner = GetOwner();
	if (!MyOwner)
	{
		return;
	}
	APlayerController* PC = Cast<APlayerController>(Cast<APawn>(MyOwner)->GetController());
	if (PC)
	{
		PC->ClientPlayCameraShake(FireCamShake);
	}
	FVector EyeLocation;
	FRotator EyeRotation;
	MyOwner->GetActorEyesViewPoint(EyeLocation, EyeRotation);

	FVector ShootDirection = EyeRotation.Vector();

	//Calculate bullet spread
	float SecondsPassedFromLastShot = GetWorld()->GetTimeSeconds() - LastTimeFired;
	if (SecondsPassedFromLastShot > BulletSpreadCoolDownTime)
	{
		CurrentBulletSpread += (BulletSpreadCoolDownTime - SecondsPassedFromLastShot) * BulletSpreadDecreaseRate * InitialBulletSpread;
	}
	else
	{
		CurrentBulletSpread += (BulletSpreadCoolDownTime - SecondsPassedFromLastShot) * BulletSpreadIncreaseRate * InitialBulletSpread;
	}

	CurrentBulletSpread = FMath::Clamp(CurrentBulletSpread, InitialBulletSpread, 0.1f);

	//Add bullet spread
	ShootDirection.Z += FMath::RandRange(-CurrentBulletSpread, CurrentBulletSpread);
	ShootDirection.Y += FMath::RandRange(-CurrentBulletSpread, CurrentBulletSpread);

	FVector TraceEnd = EyeLocation + (ShootDirection * 10000);

	//End of trail particle system component
	FVector TrailEnd = TraceEnd;

	FHitResult HitResult;
	FCollisionQueryParams CollisionQueryParams;
	CollisionQueryParams.AddIgnoredActor(MyOwner); //Igrone collision with owner
	CollisionQueryParams.AddIgnoredActor(this);	   //Ignore collision with gun itself
	CollisionQueryParams.bTraceComplex = true;	   //Trace with each individual triangle of complex mesh
	CollisionQueryParams.bReturnPhysicalMaterial = true;   //Return physical material

	EPhysicalSurface SurfaceType = SurfaceType_Default;

	if (GetWorld()->LineTraceSingleByChannel(HitResult, EyeLocation, TraceEnd, COLLISION_WEAPON, CollisionQueryParams))
	{
		AActor* HitActor = HitResult.GetActor();

		TrailEnd = HitResult.ImpactPoint;

		float ActualDamage = BaseDamage;

		SurfaceType = UPhysicalMaterial::DetermineSurfaceType(HitResult.PhysMaterial.Get());

		if (SurfaceType == SURFACE_FLESH_VULNERABLE)
		{
			ActualDamage *= 4.0f;
		}

		UGameplayStatics::ApplyPointDamage(HitActor, ActualDamage, ShootDirection, HitResult, MyOwner->GetInstigatorController(), MyOwner, DamageType);

		PlayImpactEffects(SurfaceType, TrailEnd);
	}

	PlayEffects(TrailEnd);
	

	if (GetLocalRole() == ROLE_Authority)
	{
		HitScanTrace.TraceEnd = TrailEnd;
		HitScanTrace.SurfaceType = SurfaceType;
	}

	LastTimeFired = GetWorld()->GetTimeSeconds();
}

void ASWeapon::PlayEffects(FVector TrailEnd)
{
	if (MuzzleEffect)
	{
		UGameplayStatics::SpawnEmitterAttached(MuzzleEffect, MeshComponent, MuzzleSocketName);
	}
	if (TrailEffect)
	{
		FVector SocketLocation = MeshComponent->GetSocketLocation(MuzzleSocketName);
		UParticleSystemComponent* Trail = UGameplayStatics::SpawnEmitterAtLocation(GetWorld(), TrailEffect, SocketLocation);
		if (Trail)
		{
			Trail->SetVectorParameter(TrailTargetName, TrailEnd);
		}
	}
}

void ASWeapon::PlayImpactEffects(EPhysicalSurface SurfaceType, FVector ImpactPoint)
{
	UParticleSystem* SelectedParticleSystem = nullptr;

	switch (SurfaceType)
	{
	case SurfaceType_Default:
		SelectedParticleSystem = ImpactEffectDefault;
		break;
	case SURFACE_FLESH_DEFAULT:
		SelectedParticleSystem = ImpactEffectFlesh;
		break;
	case SURFACE_FLESH_VULNERABLE:
		SelectedParticleSystem = ImpactEffectFlesh;
		break;
	default:
		SelectedParticleSystem = ImpactEffectDefault;
		break;
	}

	if (SelectedParticleSystem)
	{
		FVector MuzzleLocation = MeshComponent->GetSocketLocation(MuzzleSocketName);

		FVector ShotDirection = ImpactPoint - MuzzleLocation;

		ShotDirection.Normalize();

		UGameplayStatics::SpawnEmitterAtLocation(GetWorld(), SelectedParticleSystem, ImpactPoint, ShotDirection.Rotation());
	}
}

void ASWeapon::OnRep_HitScanTrace()
{
	//Play cosmetic effects
	PlayEffects(HitScanTrace.TraceEnd);

	PlayImpactEffects(HitScanTrace.SurfaceType, HitScanTrace.TraceEnd);
}

void ASWeapon::ServerFire_Implementation()
{
	Fire();
}

bool ASWeapon::ServerFire_Validate()
{
	return true;
}

void ASWeapon::GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const
{
	Super::GetLifetimeReplicatedProps(OutLifetimeProps);

	DOREPLIFETIME(ASWeapon, HitScanTrace);
}


void ASWeapon::StartFire()
{
	float FireDelay = FMath::Max(LastTimeFired + TimeBetweenShots - GetWorld()->GetTimeSeconds(), 0.0f);
	GetWorldTimerManager().SetTimer(FTimeBetweenShots, this, &ASWeapon::Fire, TimeBetweenShots, true, FireDelay);
}

void ASWeapon::StopFire()
{
	GetWorldTimerManager().ClearTimer(FTimeBetweenShots);
}

