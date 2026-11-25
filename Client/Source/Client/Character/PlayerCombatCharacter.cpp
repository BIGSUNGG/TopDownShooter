#include "Character/PlayerCombatCharacter.h"

#include "Camera/CameraComponent.h"
#include "Combat/CombatConfig.h"
#include "Combat/CombatLog.h"
#include "Components/CapsuleComponent.h"
#include "Components/InputComponent.h"
#include "Engine/EngineTypes.h"
#include "Engine/World.h"
#include "GameFramework/PlayerController.h"
#include "GameFramework/SpringArmComponent.h"
#include "InputCoreTypes.h"
#include "UI/CombatHUDWidget.h"
#include "UObject/ConstructorHelpers.h"

APlayerCombatCharacter::APlayerCombatCharacter()
{
	PrimaryActorTick.bCanEverTick = true;

	// 기존 ADemoPawn 과 같은 탑다운 카메라 구성(고정 요, -75도 피치).
	SpringArm = CreateDefaultSubobject<USpringArmComponent>(TEXT("SpringArm"));
	SpringArm->SetupAttachment(GetCapsuleComponent());
	SpringArm->TargetArmLength = 2000.0f;
	SpringArm->bDoCollisionTest = false;
	SpringArm->bUsePawnControlRotation = false;
	SpringArm->SetRelativeRotation(FRotator(-75.0f, 0.0f, 0.0f));

	Camera = CreateDefaultSubobject<UCameraComponent>(TEXT("Camera"));
	Camera->SetupAttachment(SpringArm);
	Camera->bUsePawnControlRotation = false;

	// 마우스가 가리키는 방향으로 몸이 돌므로 컨트롤러 요 회전은 쓰지 않는다.
	bUseControllerRotationYaw = false;

	// HUD 는 배치만 WBP 가 담당한다. 에셋이 아직 없으면(예: 처음 빌드) HUD 없이 동작한다.
	static ConstructorHelpers::FClassFinder<UCombatHUDWidget> HUDClassFinder(TEXT("/Game/Combat/UI/WBP_HUD"));
	if (HUDClassFinder.Succeeded())
	{
		HUDWidgetClass = HUDClassFinder.Class;
	}
}

void APlayerCombatCharacter::BeginPlay()
{
	Super::BeginPlay();

	if (SpringArm)
	{
		SpringArmBaseLocation = SpringArm->GetRelativeLocation();
		SpringArmBaseRotation = SpringArm->GetRelativeRotation();
	}

	if (APlayerController* PlayerController = Cast<APlayerController>(GetController()))
	{
		PlayerController->bShowMouseCursor = true;
		PlayerController->bEnableClickEvents = false;
		PlayerController->bEnableMouseOverEvents = false;

		FInputModeGameAndUI InputMode;
		InputMode.SetHideCursorDuringCapture(false);
		InputMode.SetLockMouseToViewportBehavior(EMouseLockMode::DoNotLock);
		PlayerController->SetInputMode(InputMode);
	}

	if (!HUDWidgetClass)
	{
		// 생성자가 CDO 시점에 실행되므로, 에디터를 재시작하지 않고 WBP 를 만든 경우에만 여기서 찾는다.
		HUDWidgetClass = LoadClass<UCombatHUDWidget>(nullptr, TEXT("/Game/Combat/UI/WBP_HUD.WBP_HUD_C"));
	}

	CreateHUD();
}

void APlayerCombatCharacter::PossessedBy(AController* NewController)
{
	Super::PossessedBy(NewController);

	// 빙의 전 BeginPlay 에서는 컨트롤러가 없어 위젯을 만들 수 없다.
	CreateHUD();
}

void APlayerCombatCharacter::CreateHUD()
{
	if (HUDWidget || !HUDWidgetClass)
	{
		return;
	}

	APlayerController* PlayerController = Cast<APlayerController>(GetController());
	if (!PlayerController)
	{
		return;
	}

	HUDWidget = CreateWidget<UCombatHUDWidget>(PlayerController, HUDWidgetClass);
	if (HUDWidget)
	{
		HUDWidget->AddToViewport();
		UE_LOG(LogCombat, Log, TEXT("COMBAT_HUD_CREATED pawn=%s"), *GetName());
	}
}

void APlayerCombatCharacter::SetupPlayerInputComponent(UInputComponent* PlayerInputComponent)
{
	Super::SetupPlayerInputComponent(PlayerInputComponent);
	check(PlayerInputComponent);

	// 디지털 키라 축 바인딩을 쓰지 않고 눌림 상태를 모아 Tick 에서 이동한다(DemoPawn 과 같은 방식).
	PlayerInputComponent->BindKey(EKeys::W, IE_Pressed, this, &APlayerCombatCharacter::OnMoveKeyPressed);
	PlayerInputComponent->BindKey(EKeys::S, IE_Pressed, this, &APlayerCombatCharacter::OnMoveKeyPressed);
	PlayerInputComponent->BindKey(EKeys::A, IE_Pressed, this, &APlayerCombatCharacter::OnMoveKeyPressed);
	PlayerInputComponent->BindKey(EKeys::D, IE_Pressed, this, &APlayerCombatCharacter::OnMoveKeyPressed);
	PlayerInputComponent->BindKey(EKeys::W, IE_Released, this, &APlayerCombatCharacter::OnMoveKeyReleased);
	PlayerInputComponent->BindKey(EKeys::S, IE_Released, this, &APlayerCombatCharacter::OnMoveKeyReleased);
	PlayerInputComponent->BindKey(EKeys::A, IE_Released, this, &APlayerCombatCharacter::OnMoveKeyReleased);
	PlayerInputComponent->BindKey(EKeys::D, IE_Released, this, &APlayerCombatCharacter::OnMoveKeyReleased);

	PlayerInputComponent->BindKey(EKeys::LeftMouseButton, IE_Pressed, this, &APlayerCombatCharacter::OnFirePressed);
	PlayerInputComponent->BindKey(EKeys::RightMouseButton, IE_Pressed, this, &APlayerCombatCharacter::OnAimPressed);
	PlayerInputComponent->BindKey(EKeys::RightMouseButton, IE_Released, this, &APlayerCombatCharacter::OnAimReleased);

	PlayerInputComponent->BindKey(EKeys::C, IE_Pressed, this, &APlayerCombatCharacter::OnCrouchPressed);
	PlayerInputComponent->BindKey(EKeys::SpaceBar, IE_Pressed, this, &APlayerCombatCharacter::OnJumpPressed);
	PlayerInputComponent->BindKey(EKeys::LeftShift, IE_Pressed, this, &APlayerCombatCharacter::OnSprintPressed);
	PlayerInputComponent->BindKey(EKeys::LeftShift, IE_Released, this, &APlayerCombatCharacter::OnSprintReleased);
	PlayerInputComponent->BindKey(EKeys::R, IE_Pressed, this, &APlayerCombatCharacter::OnReloadPressed);
}

void APlayerCombatCharacter::Tick(float DeltaSeconds)
{
	// 조준점을 먼저 갱신해야 이번 프레임 총구 방향이 커서를 따라간다.
	UpdateAimFromCursor();
	ApplyMoveInput();

	Super::Tick(DeltaSeconds);

	UpdateCameraEffects(DeltaSeconds);
}

void APlayerCombatCharacter::OnMoveKeyPressed(FKey Key)
{
	SetMoveKeyState(Key, true);
}

void APlayerCombatCharacter::OnMoveKeyReleased(FKey Key)
{
	SetMoveKeyState(Key, false);
}

void APlayerCombatCharacter::SetMoveKeyState(FKey Key, bool bDown)
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

void APlayerCombatCharacter::ApplyMoveInput()
{
	if (IsDead() || IsRolling())
	{
		return;
	}

	const FVector Direction(
		(bMoveForward ? 1.0f : 0.0f) - (bMoveBack ? 1.0f : 0.0f),
		(bMoveRight ? 1.0f : 0.0f) - (bMoveLeft ? 1.0f : 0.0f),
		0.0f);

	if (!Direction.IsNearlyZero())
	{
		AddMovementInput(Direction.GetSafeNormal(), 1.0f);
	}
}

void APlayerCombatCharacter::UpdateAimFromCursor()
{
	APlayerController* PlayerController = Cast<APlayerController>(GetController());
	if (!PlayerController || IsDead())
	{
		return;
	}

	FHitResult CursorHit;
	if (PlayerController->GetHitResultUnderCursorByChannel(UEngineTypes::ConvertToTraceType(ECC_Visibility), false, CursorHit))
	{
		SetAimPoint(CursorHit.ImpactPoint);
		return;
	}

	// 커서 아래에 아무 것도 없으면 사거리만큼 앞의 지점을 조준점으로 쓴다.
	FVector WorldOrigin = FVector::ZeroVector;
	FVector WorldDirection = FVector::ForwardVector;
	if (PlayerController->DeprojectMousePositionToWorld(WorldOrigin, WorldDirection))
	{
		SetAimPoint(WorldOrigin + WorldDirection * CombatConfig::Tuning().WeaponRangeUU);
	}
}

void APlayerCombatCharacter::OnFirePressed()
{
	TryFire();
}

void APlayerCombatCharacter::OnAimPressed()
{
	SetAiming(true);
}

void APlayerCombatCharacter::OnAimReleased()
{
	SetAiming(false);
}

void APlayerCombatCharacter::OnCrouchPressed()
{
	SetCrouching(!IsCrouching());
}

void APlayerCombatCharacter::OnJumpPressed()
{
	if (IsDead() || IsRolling() || IsCrouching())
	{
		return;
	}

	Jump();
	NotifyJumped();
}

void APlayerCombatCharacter::OnSprintPressed()
{
	const UWorld* World = GetWorld();
	const double Now = World ? World->GetTimeSeconds() : 0.0;
	const float Window = CombatConfig::Tuning().RollInputWindowSeconds;
	const bool bMoving = GetVelocity().SizeSquared2D() > 1.0f;

	// 이동 중 Shift 두 번 탭(0.25초 이내)은 구르기, 그 외는 달리기(문서 판정 규칙).
	if (bMoving && (Now - LastSprintTapTime) <= static_cast<double>(Window))
	{
		LastSprintTapTime = -1000.0;
		SetSprinting(false);
		TryStartRoll();
		return;
	}

	LastSprintTapTime = Now;
	SetSprinting(true);
}

void APlayerCombatCharacter::OnSprintReleased()
{
	// 달리기는 Shift 를 떼면 즉시 끝난다.
	SetSprinting(false);
}

void APlayerCombatCharacter::OnReloadPressed()
{
	TryReload();
}

void APlayerCombatCharacter::PlayCameraShake()
{
	const FCombatTuning& Tuning = CombatConfig::Tuning();

	// 명중 순간 진폭 1.5, 0.12초 셰이크 + 카메라 킥(-1.2도, 0.1초 복귀).
	ShakeRemainingSeconds = Tuning.ShakeSeconds;
	CameraKickRemainingSeconds = Tuning.CameraKickSeconds;
}

void APlayerCombatCharacter::UpdateCameraEffects(float DeltaSeconds)
{
	if (!SpringArm)
	{
		return;
	}

	const FCombatTuning& Tuning = CombatConfig::Tuning();

	// 셰이크는 자산(UCameraShakeBase) 없이 스프링암 오프셋으로 처리한다.
	FVector ShakeOffset = FVector::ZeroVector;
	if (ShakeRemainingSeconds > 0.0f)
	{
		ShakeRemainingSeconds -= DeltaSeconds;

		const float Alpha = (Tuning.ShakeSeconds > 0.0f) ? FMath::Clamp(ShakeRemainingSeconds / Tuning.ShakeSeconds, 0.0f, 1.0f) : 0.0f;
		const FVector RandomOffset(FMath::FRandRange(-1.0f, 1.0f), FMath::FRandRange(-1.0f, 1.0f), 0.0f);
		ShakeOffset = RandomOffset * Tuning.ShakeAmplitude * Alpha;
	}

	SpringArm->SetRelativeLocation(SpringArmBaseLocation + ShakeOffset);

	float KickAlpha = 0.0f;
	if (CameraKickRemainingSeconds > 0.0f)
	{
		CameraKickRemainingSeconds -= DeltaSeconds;
		KickAlpha = (Tuning.CameraKickSeconds > 0.0f) ? FMath::Clamp(CameraKickRemainingSeconds / Tuning.CameraKickSeconds, 0.0f, 1.0f) : 0.0f;
	}

	// 카메라 킥은 발사 반동 때 위로 튀었다가 0.1초에 걸쳐 돌아온다.
	SpringArm->SetRelativeRotation(SpringArmBaseRotation + FRotator(Tuning.CameraKickDeg * KickAlpha, 0.0f, 0.0f));
}
