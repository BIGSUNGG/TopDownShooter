#include "Dimension/DemoPawn.h"

#include "Camera/CameraComponent.h"
#include "Components/CapsuleComponent.h"
#include "Components/InputComponent.h"
#include "Engine/CollisionProfile.h"
#include "GameFramework/FloatingPawnMovement.h"
#include "GameFramework/SpringArmComponent.h"

ADemoPawn::ADemoPawn()
{
	PrimaryActorTick.bCanEverTick = true;
	bUseControllerRotationYaw = false;

	Capsule = CreateDefaultSubobject<UCapsuleComponent>(TEXT("Capsule"));
	Capsule->InitCapsuleSize(34.0f, 44.0f);
	// Pawn 프로필: WorldStatic은 막고 다른 Pawn은 무시한다.
	Capsule->SetCollisionProfileName(UCollisionProfile::Pawn_ProfileName);
	RootComponent = Capsule;

	SpringArm = CreateDefaultSubobject<USpringArmComponent>(TEXT("SpringArm"));
	SpringArm->SetupAttachment(Capsule);
	SpringArm->TargetArmLength = 2000.0f;
	SpringArm->bDoCollisionTest = false;
	SpringArm->bUsePawnControlRotation = false;
	SpringArm->SetRelativeRotation(FRotator(-75.0f, 0.0f, 0.0f));

	Camera = CreateDefaultSubobject<UCameraComponent>(TEXT("Camera"));
	Camera->SetupAttachment(SpringArm);
	Camera->bUsePawnControlRotation = false;

	Movement = CreateDefaultSubobject<UFloatingPawnMovement>(TEXT("Movement"));
	Movement->UpdatedComponent = Capsule;
	Movement->MaxSpeed = 400.0f;
	Movement->Acceleration = 3000.0f;
	Movement->Deceleration = 3000.0f;
}

void ADemoPawn::SetupPlayerInputComponent(UInputComponent* PlayerInputComponent)
{
	Super::SetupPlayerInputComponent(PlayerInputComponent);
	check(PlayerInputComponent);

	// ini/EnhancedInput 에셋 없이 키를 직접 묶는다. 디지털 키는 BindAxisKey가 아니라 눌림 이벤트를 받는다.
	PlayerInputComponent->BindKey(EKeys::W, IE_Pressed, this, &ADemoPawn::OnKeyPressed);
	PlayerInputComponent->BindKey(EKeys::S, IE_Pressed, this, &ADemoPawn::OnKeyPressed);
	PlayerInputComponent->BindKey(EKeys::D, IE_Pressed, this, &ADemoPawn::OnKeyPressed);
	PlayerInputComponent->BindKey(EKeys::A, IE_Pressed, this, &ADemoPawn::OnKeyPressed);
	PlayerInputComponent->BindKey(EKeys::W, IE_Released, this, &ADemoPawn::OnKeyReleased);
	PlayerInputComponent->BindKey(EKeys::S, IE_Released, this, &ADemoPawn::OnKeyReleased);
	PlayerInputComponent->BindKey(EKeys::D, IE_Released, this, &ADemoPawn::OnKeyReleased);
	PlayerInputComponent->BindKey(EKeys::A, IE_Released, this, &ADemoPawn::OnKeyReleased);
}

void ADemoPawn::OnKeyPressed(FKey Key)
{
	SetAxisState(Key, true);
}

void ADemoPawn::OnKeyReleased(FKey Key)
{
	SetAxisState(Key, false);
}

void ADemoPawn::SetAxisState(FKey Key, bool bDown)
{
	if (Key == EKeys::W)
	{
		bMoveForward = bDown;
	}
	else if (Key == EKeys::S)
	{
		bMoveBack = bDown;
	}
	else if (Key == EKeys::D)
	{
		bMoveRight = bDown;
	}
	else if (Key == EKeys::A)
	{
		bMoveLeft = bDown;
	}
}

void ADemoPawn::Tick(float DeltaSeconds)
{
	Super::Tick(DeltaSeconds);

	// 카메라가 고정 방향이라 회전을 쓰지 않고 월드 축으로만 이동한다.
	const FVector Direction(
		(bMoveForward ? 1.0f : 0.0f) - (bMoveBack ? 1.0f : 0.0f),
		(bMoveRight ? 1.0f : 0.0f) - (bMoveLeft ? 1.0f : 0.0f),
		0.0f);
	if (!Direction.IsNearlyZero())
	{
		AddMovementInput(Direction.GetSafeNormal(), 1.0f);
	}
}
