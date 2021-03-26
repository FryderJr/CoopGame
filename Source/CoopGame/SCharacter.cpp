// Fill out your copyright notice in the Description page of Project Settings.


#include "SCharacter.h"
#include "SWeapon.h"
#include "CoopGame.h"
#include "Camera/CameraComponent.h"
#include "GameFramework/SpringArmComponent.h"
#include "GameFramework/PawnMovementComponent.h"
#include "Components/SkeletalMeshComponent.h"
#include "Components/CapsuleComponent.h"
#include "Net/UnrealNetwork.h"
#include "SHealthComponent.h"

// Sets default values
ASCharacter::ASCharacter()
{
 	// Set this character to call Tick() every frame.  You can turn this off to improve performance if you don't need it.
	PrimaryActorTick.bCanEverTick = true;
	SpringArm = CreateDefaultSubobject<USpringArmComponent>(FName("SpringArm"));
	SpringArm->bUsePawnControlRotation = true;
	SpringArm->SetupAttachment(RootComponent);

	GetMovementComponent()->GetNavAgentPropertiesRef().bCanCrouch = true;

	GetCapsuleComponent()->SetCollisionResponseToChannel(COLLISION_WEAPON, ECR_Ignore);

	CamComp = CreateDefaultSubobject<UCameraComponent>(FName("CamComponent"));
	CamComp->SetupAttachment(SpringArm);

	HealthComponent = CreateDefaultSubobject<USHealthComponent>(FName("HealthComp"));
	HealthComponent->bEditableWhenInherited = true;
	HealthComponent->RegisterComponent();
	HealthComponent->OnHealthChanged.AddDynamic(this, &ASCharacter::OnHealthChanged);

	ZoomFOV = 65.0f;
	ZoomSpeed = 20.0f;

	WeaponSocketName = FName("WeaponSocket");
}

// Called when the game starts or when spawned
void ASCharacter::BeginPlay()
{
	Super::BeginPlay();
	
	DefaultFOV = CamComp->FieldOfView;

	bDied = false;

	if (GetLocalRole() == ROLE_Authority)
	{
		FActorSpawnParameters SpawnParams;
		SpawnParams.SpawnCollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::AlwaysSpawn;

		//Spawn Weapon
		CurrentWeapon = GetWorld()->SpawnActor<ASWeapon>(WeaponClass, FVector::ZeroVector, FRotator::ZeroRotator, SpawnParams);
		if (CurrentWeapon)
		{
			CurrentWeapon->SetOwner(this);
			CurrentWeapon->AttachToComponent(this->GetMesh(), FAttachmentTransformRules::SnapToTargetNotIncludingScale, WeaponSocketName);
		}
	}

	
}

void ASCharacter::OnHealthChanged(USHealthComponent* HealthComp, float Health, float HealthDelta, const UDamageType* DamageType, AController* InstigatedBy, AActor* DamageCauser)
{
	if (Health <= 0.0f && !bDied)
	{
		bDied = true;

		GetMovementComponent()->StopMovementImmediately();
		GetCapsuleComponent()->SetCollisionEnabled(ECollisionEnabled::NoCollision);

		DetachFromControllerPendingDestroy();
		SetLifeSpan(10.0f);
	}
}

// Called every frame
void ASCharacter::Tick(float DeltaTime)
{
	Super::Tick(DeltaTime);

	float TargetFOV = bWantsToZoom ? ZoomFOV : DefaultFOV;
	float CurrentFOV = FMath::FInterpTo(CamComp->FieldOfView, TargetFOV, DeltaTime, ZoomSpeed);

	CamComp->SetFieldOfView(CurrentFOV);
}

// Called to bind functionality to input
void ASCharacter::SetupPlayerInputComponent(UInputComponent* PlayerInputComponent)
{
	Super::SetupPlayerInputComponent(PlayerInputComponent);

	//Movement Axis
	PlayerInputComponent->BindAxis(FName("MoveForward"), this, &ASCharacter::MoveForward);
	PlayerInputComponent->BindAxis(FName("MoveRight"), this, &ASCharacter::MoveRight);

	//Mouse look Axis
	PlayerInputComponent->BindAxis(FName("LookUp"), this, &ASCharacter::AddControllerPitchInput);
	PlayerInputComponent->BindAxis(FName("Turn"), this, &ASCharacter::AddControllerYawInput);

	//Crouch Action
	PlayerInputComponent->BindAction(FName("Crouch"), IE_Pressed, this, &ASCharacter::BeginCrouch);
	PlayerInputComponent->BindAction(FName("Crouch"), IE_Released, this, &ASCharacter::EndCrouch);

	//Jump Action
	PlayerInputComponent->BindAction(FName("Jump"), IE_Pressed, this, &ACharacter::Jump);

	//Zoom Action
	PlayerInputComponent->BindAction(FName("Zoom"), IE_Pressed, this, &ASCharacter::BeginZoom);
	PlayerInputComponent->BindAction(FName("Zoom"), IE_Released, this, &ASCharacter::EndZoom);

	//Fire
	PlayerInputComponent->BindAction(FName("Fire"), IE_Pressed, this, &ASCharacter::StartFire);
	PlayerInputComponent->BindAction(FName("Fire"), IE_Released, this, &ASCharacter::StopFire);
}

void ASCharacter::GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const
{
	Super::GetLifetimeReplicatedProps(OutLifetimeProps);

	DOREPLIFETIME(ASCharacter, CurrentWeapon);
	DOREPLIFETIME(ASCharacter, bDied);
}

FVector ASCharacter::GetPawnViewLocation() const
{
	if (!CamComp)
	{
		return Super::GetPawnViewLocation();
	}
	return CamComp->GetComponentLocation();
}

void ASCharacter::MoveForward(float Value)
{
	AddMovementInput(GetActorForwardVector() * Value);
}

void ASCharacter::MoveRight(float Value)
{
	AddMovementInput(GetActorRightVector() * Value);
}

void ASCharacter::BeginCrouch()
{
	Crouch();
}

void ASCharacter::EndCrouch()
{
	UnCrouch();
}

void ASCharacter::BeginZoom()
{
	bWantsToZoom = true;
}

void ASCharacter::EndZoom()
{
	bWantsToZoom = false;
}

void ASCharacter::StartFire()
{
	if (CurrentWeapon)
	{
		CurrentWeapon->StartFire();
	}
}

void ASCharacter::StopFire()
{
	if (CurrentWeapon)
	{
		CurrentWeapon->StopFire();
	}
}
