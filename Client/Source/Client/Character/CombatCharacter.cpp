#include "Character/CombatCharacter.h"

#include "AbilitySystemComponent.h"
#include "Character/PlayerCombatCharacter.h"
#include "Combat/CombatAbility_Fire.h"
#include "Combat/CombatAbility_Reload.h"
#include "Combat/CombatAttributeSet.h"
#include "Combat/CombatConfig.h"
#include "Combat/CombatEffects.h"
#include "Combat/CombatLog.h"
#include "Combat/CombatMath.h"
#include "Combat/CombatVisuals.h"
#include "Components/CapsuleComponent.h"
#include "Components/StaticMeshComponent.h"
#include "Engine/StaticMesh.h"
#include "Engine/World.h"
#include "GameFramework/CharacterMovementComponent.h"
#include "Kismet/GameplayStatics.h"
#include "Materials/MaterialInstanceDynamic.h"
#include "Sound/SoundBase.h"
#include "TimerManager.h"
#include "UObject/ConstructorHelpers.h"

namespace
{
	/** 연출용 위치값(문서 수치 아님). 총구를 캡슐 중심에서 조금 앞·위에 둔다. */
	constexpr float MuzzleHeightOffset = 10.0f;
	constexpr float MuzzleForwardOffset = 50.0f;

	/** 기본 도형(100uu) 기준 마네킹 몸통 색과 총 색. */
	const FLinearColor BodyColor(0.55f, 0.61f, 0.68f, 1.0f);
	const FLinearColor GunColor(0.15f, 0.15f, 0.17f, 1.0f);
	const FLinearColor AimLineColor(1.0f, 0.05f, 0.05f, 1.0f);
}

ACombatCharacter::ACombatCharacter()
{
	PrimaryActorTick.bCanEverTick = true;

	const FCombatTuning& Tuning = CombatConfig::Tuning();

	UCapsuleComponent* Capsule = GetCapsuleComponent();
	Capsule->InitCapsuleSize(Tuning.CapsuleRadius, Tuning.CapsuleHalfHeightStand);
	Capsule->SetCollisionProfileName(UCollisionProfile::Pawn_ProfileName);

	UCharacterMovementComponent* MovementComponent = GetCharacterMovement();
	MovementComponent->MaxWalkSpeed = Tuning.SpeedStandUU;
	MovementComponent->MaxWalkSpeedCrouched = Tuning.SpeedCrouchUU;
	MovementComponent->JumpZVelocity = Tuning.JumpZVelocity;
	MovementComponent->GravityScale = Tuning.GravityScale;
	MovementComponent->AirControl = Tuning.AirControl;
	MovementComponent->MaxAcceleration = 3000.0f;
	MovementComponent->BrakingDecelerationWalking = 3000.0f;
	MovementComponent->bOrientRotationToMovement = false;

	bUseControllerRotationYaw = false;

	AbilitySystem = CreateDefaultSubobject<UAbilitySystemComponent>(TEXT("AbilitySystem"));
	AbilitySystem->SetIsReplicated(false);

	CombatAttributeSet = CreateDefaultSubobject<UCombatAttributeSet>(TEXT("CombatAttributeSet"));

	BodyMesh = CreateDefaultSubobject<UStaticMeshComponent>(TEXT("BodyMesh"));
	BodyMesh->SetupAttachment(Capsule);
	BodyMesh->SetCollisionEnabled(ECollisionEnabled::NoCollision);
	BodyMesh->SetCastShadow(false);

	GunMesh = CreateDefaultSubobject<UStaticMeshComponent>(TEXT("GunMesh"));
	GunMesh->SetupAttachment(Capsule);
	GunMesh->SetCollisionEnabled(ECollisionEnabled::NoCollision);
	GunMesh->SetCastShadow(false);

	AimLineMesh = CreateDefaultSubobject<UStaticMeshComponent>(TEXT("AimLineMesh"));
	AimLineMesh->SetCollisionEnabled(ECollisionEnabled::NoCollision);
	AimLineMesh->SetCastShadow(false);
	AimLineMesh->SetVisibility(false);
	// 조준선은 월드에 고정된 메시다(캐릭터에 붙어 회전하지 않는다). 매 틱 월드 트랜스폼을 직접 준다.
	AimLineMesh->SetUsingAbsoluteLocation(true);
	AimLineMesh->SetUsingAbsoluteRotation(true);
	AimLineMesh->SetUsingAbsoluteScale(true);

	static ConstructorHelpers::FObjectFinder<UStaticMesh> CylinderMeshFinder(TEXT("/Engine/BasicShapes/Cylinder.Cylinder"));
	if (CylinderMeshFinder.Succeeded())
	{
		BodyMesh->SetStaticMesh(CylinderMeshFinder.Object);
		AimLineMesh->SetStaticMesh(CylinderMeshFinder.Object);
	}

	static ConstructorHelpers::FObjectFinder<UStaticMesh> CubeMeshFinder(TEXT("/Engine/BasicShapes/Cube.Cube"));
	if (CubeMeshFinder.Succeeded())
	{
		GunMesh->SetStaticMesh(CubeMeshFinder.Object);
	}

	static ConstructorHelpers::FObjectFinder<UMaterialInterface> BaseMaterialFinder(CombatVisuals::BaseMaterialPath);
	if (BaseMaterialFinder.Succeeded())
	{
		BaseMaterial = BaseMaterialFinder.Object;
	}
}

void ACombatCharacter::BeginPlay()
{
	Super::BeginPlay();

	const FCombatTuning& Tuning = CombatConfig::Tuning();

	AmmoInMagazine = Tuning.MagazineSize;
	AmmoReserve = Tuning.ReserveAmmo;
	SpawnTransform = GetActorTransform();

	WeaponState = EWeaponState::Stand_Idle;
	SpreadAccumDeg = CombatConfig::StateTuning(WeaponState).SpreadBaseDeg;

	AimRotation = FRotator(0.0f, GetActorRotation().Yaw, 0.0f);
	PersistentMuzzleDir = AimRotation.Vector();
	MuzzleDir = PersistentMuzzleDir;

	if (AbilitySystem)
	{
		AbilitySystem->InitAbilityActorInfo(this, this);
		// 같은 속성 집합을 두 번 넣지 않는다(AddSpawnedAttribute 가 중복을 걸러낸다).
		AbilitySystem->AddSpawnedAttribute(CombatAttributeSet);
	}

	// 기본 원기둥은 지름 100uu·높이 100uu 이므로 캡슐 크기에 맞춰 늘린다.
	if (BodyMesh)
	{
		const float Diameter = Tuning.CapsuleRadius * 2.0f;
		const float Height = Tuning.CapsuleHalfHeightStand * 2.0f;
		BodyMesh->SetRelativeScale3D(FVector(Diameter / 100.0f, Diameter / 100.0f, Height / 100.0f));
		BodyMesh->SetRelativeLocation(FVector(0.0f, 0.0f, -3.0f));

		if (UMaterialInstanceDynamic* Material = UMaterialInstanceDynamic::Create(BaseMaterial, this))
		{
			BodyMaterial = Material;
			BodyMesh->SetMaterial(0, BodyMaterial);
			CombatVisuals::SetColor(BodyMaterial, BodyColor);
		}
	}

	if (GunMesh)
	{
		// 90cm 길이의 막대기 모양 총.
		GunMesh->SetRelativeScale3D(FVector(0.9f, 0.12f, 0.12f));
	}

	if (AimLineMesh && !bIsDummy)
	{
		if (UMaterialInstanceDynamic* Material = UMaterialInstanceDynamic::Create(BaseMaterial, this))
		{
			AimLineMaterial = Material;
			AimLineMesh->SetMaterial(0, AimLineMaterial);
			CombatVisuals::SetColor(AimLineMaterial, AimLineColor);
		}
	}

	ApplyMoveSpeed();
	SyncHudState();
}

void ACombatCharacter::PossessedBy(AController* NewController)
{
	Super::PossessedBy(NewController);

	if (AbilitySystem)
	{
		AbilitySystem->InitAbilityActorInfo(this, this);
		AbilitySystem->AddSpawnedAttribute(CombatAttributeSet);
	}

	GrantCombatAbilities();
}

void ACombatCharacter::GrantCombatAbilities()
{
	if (!AbilitySystem || bWeaponAbilitiesGranted || bIsDummy)
	{
		return;
	}

	AbilitySystem->GiveAbility(FGameplayAbilitySpec(UCombatAbility_Fire::StaticClass(), 1, INDEX_NONE, this));
	AbilitySystem->GiveAbility(FGameplayAbilitySpec(UCombatAbility_Reload::StaticClass(), 1, INDEX_NONE, this));
	bWeaponAbilitiesGranted = true;

	UE_LOG(LogCombat, Log, TEXT("COMBAT_ABILITIES_GRANTED pawn=%s"), *GetName());
}

UAbilitySystemComponent* ACombatCharacter::GetAbilitySystemComponent() const
{
	return AbilitySystem;
}

void ACombatCharacter::Tick(float DeltaSeconds)
{
	Super::Tick(DeltaSeconds);

	if (bRolling)
	{
		RollRemainingSeconds -= DeltaSeconds;

		// 구르기는 중력 없이 고정 속도로 진행한다(문서 0.45초 / 900uu/s).
		if (UCharacterMovementComponent* MovementComponent = GetCharacterMovement())
		{
			MovementComponent->Velocity = RollDirection * CombatConfig::Tuning().RollSpeedUU;
		}

		if (RollRemainingSeconds <= 0.0f)
		{
			OnRollFinished();
		}
	}

	UpdateWeaponState();
	UpdateMuzzle(DeltaSeconds);
	UpdateAimLine();
	UpdateHitFeedback(DeltaSeconds);
	SyncHudState();
}

void ACombatCharacter::UpdateWeaponState()
{
	EWeaponState DesiredState = EWeaponState::Stand_Idle;

	if (bRolling)
	{
		DesiredState = EWeaponState::Roll;
	}
	else
	{
		const UCharacterMovementComponent* MovementComponent = GetCharacterMovement();
		const bool bAirborne = MovementComponent && MovementComponent->IsFalling();
		const bool bMoving = GetVelocity().SizeSquared2D() > 1.0f;

		if (bAirborne)
		{
			DesiredState = EWeaponState::Jump_Fall;
		}
		else if (bCrouching)
		{
			DesiredState = bAiming ? EWeaponState::Crouch_Aim : (bMoving ? EWeaponState::Crouch_Walk : EWeaponState::Crouch_Idle);
		}
		else if (bAiming)
		{
			DesiredState = EWeaponState::Stand_Aim;
		}
		else if (bSprinting && bMoving)
		{
			DesiredState = EWeaponState::Stand_Run;
		}
		else
		{
			DesiredState = bMoving ? EWeaponState::Stand_Walk : EWeaponState::Stand_Idle;
		}
	}

	if (DesiredState == WeaponState)
	{
		return;
	}

	WeaponState = DesiredState;
	OnWeaponStateChanged();
}

void ACombatCharacter::OnWeaponStateChanged()
{
	// 상태가 바뀌어 상한이 줄면 현재 탄퍼짐을 즉시 새 범위로 자른다(문서: 달리다 멈추면 바로 줄어든다).
	const FWeaponStateTuning& StateTuning = CombatConfig::StateTuning(WeaponState);
	SpreadAccumDeg = FMath::Clamp(SpreadAccumDeg, StateTuning.SpreadBaseDeg, StateTuning.SpreadMaxDeg);

	ApplyMoveSpeed();

	UE_LOG(LogCombat, Verbose, TEXT("COMBAT_STATE pawn=%s state=%s"), *GetName(), CombatConfig::StateName(WeaponState));
}

void ACombatCharacter::ApplyMoveSpeed()
{
	UCharacterMovementComponent* MovementComponent = GetCharacterMovement();
	if (!MovementComponent)
	{
		return;
	}

	const FCombatTuning& Tuning = CombatConfig::Tuning();

	float Speed = Tuning.SpeedStandUU;
	if (bRolling)
	{
		Speed = Tuning.RollSpeedUU;
	}
	else if (MovementComponent->IsFalling())
	{
		// 공중 이동 속도는 서기 속도와 같다(문서: 공중 이동 400).
		Speed = Tuning.SpeedStandUU;
	}
	else if (bCrouching)
	{
		Speed = bAiming ? Tuning.SpeedCrouchAimUU : Tuning.SpeedCrouchUU;
	}
	else if (bAiming)
	{
		Speed = Tuning.SpeedAimUU;
	}
	else if (bSprinting)
	{
		Speed = Tuning.SpeedRunUU;
	}

	MovementComponent->MaxWalkSpeed = Speed;
}

void ACombatCharacter::UpdateMuzzle(float DeltaSeconds)
{
	const FWeaponStateTuning& StateTuning = CombatConfig::StateTuning(WeaponState);

	// 사격하지 않으면 탄퍼짐이 기본값으로 돌아온다(0.2초당 1단계 상당).
	SpreadAccumDeg = CombatMath::SpreadDecay(SpreadAccumDeg, StateTuning.SpreadBaseDeg, DeltaSeconds);
	SpreadAccumDeg = FMath::Min(SpreadAccumDeg, StateTuning.SpreadMaxDeg);

	// 지속 벌어짐: 조준 방향을 추종 속도로 따라가되 상태 상한을 넘지 않는다.
	const FVector AimDir = AimRotation.Vector();
	PersistentMuzzleDir = CombatMath::MuzzleTrack(PersistentMuzzleDir, AimDir, StateTuning.MuzzleTrackDegPerSec, StateTuning.MuzzleMaxDeg, DeltaSeconds);

	const float AimYaw = AimRotation.Yaw;
	const float TrackedYaw = PersistentMuzzleDir.Rotation().Yaw;
	const float DeviationDeg = FMath::FindDeltaAngleDegrees(AimYaw, TrackedYaw);

	JumpImpulseElapsedSeconds += DeltaSeconds;

	const UWorld* World = GetWorld();
	const double TimeSeconds = World ? World->GetTimeSeconds() : 0.0;

	// 최종 오프셋 = clamp(지속 벌어짐 + 호흡, ±상한) + 점프 충격.
	// 좌우(요) 회전으로 적용해 "조준선이 좌우로 흔들린다"는 문서 서술과 화면을 맞춘다.
	const float OffsetDeg = CombatMath::EffectiveMuzzleOffsetDeg(WeaponState, DeviationDeg, TimeSeconds, JumpImpulseElapsedSeconds);

	// 반동은 조준 방향에만 더한다(캐릭터 몸 회전은 건드리지 않는다).
	const FRotator Recoil = CombatMath::RecoilOffset(ShotsSinceRecoilStart, bAiming);

	const FRotator FinalRotation(Recoil.Pitch, AimYaw + OffsetDeg + Recoil.Yaw, 0.0f);
	MuzzleDir = FinalRotation.Vector();

	if (GunMesh)
	{
		const FVector MuzzleLocation = GetMuzzleLocation();
		GunMesh->SetWorldLocation(MuzzleLocation - MuzzleDir * (50.0f * 0.5f));
		GunMesh->SetWorldRotation(MuzzleDir.Rotation());
	}
}

FVector ACombatCharacter::GetMuzzleLocation() const
{
	return GetActorLocation() + FVector(0.0f, 0.0f, MuzzleHeightOffset) + MuzzleDir * MuzzleForwardOffset;
}

void ACombatCharacter::UpdateAimLine()
{
	if (!AimLineMesh)
	{
		return;
	}

	if (bIsDummy)
	{
		AimLineMesh->SetVisibility(false);
		return;
	}

	UWorld* World = GetWorld();
	if (!World)
	{
		return;
	}

	const FCombatTuning& Tuning = CombatConfig::Tuning();
	const FVector Start = GetMuzzleLocation();
	const FVector End = Start + MuzzleDir * Tuning.WeaponRangeUU;

	FCollisionQueryParams QueryParams(SCENE_QUERY_STAT(CombatAimLine), false, this);
	FHitResult Hit;
	const bool bHit = World->LineTraceSingleByChannel(Hit, Start, End, ECC_Visibility, QueryParams);

	const float Length = bHit ? FVector::Dist(Start, Hit.ImpactPoint) : Tuning.WeaponRangeUU;

	// 막힘 상태가 바뀔 때만 로그를 남긴다(매 틱 로그 금지).
	if (bHit != bAimLineBlocked)
	{
		bAimLineBlocked = bHit;
		if (bAimLineBlocked)
		{
			UE_LOG(LogCombat, Log, TEXT("COMBAT_AIMLINE_BLOCKED pawn=%s distance=%.1f"), *GetName(), Length);
		}
	}

	AimLineMesh->SetVisibility(true);
	AimLineMesh->SetWorldLocation(Start + MuzzleDir * (Length * 0.5f));
	AimLineMesh->SetWorldRotation(MuzzleDir.Rotation());
	AimLineMesh->SetWorldScale3D(FVector(
		Tuning.AimLineThicknessUU / 100.0f,
		Tuning.AimLineThicknessUU / 100.0f,
		Length / 100.0f));
}

void ACombatCharacter::UpdateHitFeedback(float DeltaSeconds)
{
	const FCombatTuning& Tuning = CombatConfig::Tuning();

	if (KnockbackRemainingSeconds > 0.0f)
	{
		const float Alpha = (Tuning.KnockbackSeconds > 0.0f) ? FMath::Clamp(KnockbackRemainingSeconds / Tuning.KnockbackSeconds, 0.0f, 1.0f) : 0.0f;
		AddActorWorldOffset(KnockbackDirection * Tuning.KnockbackSpeedUU * Alpha * DeltaSeconds, true);
		KnockbackRemainingSeconds -= DeltaSeconds;
	}

	UpdateBodyColor(DeltaSeconds);
}

void ACombatCharacter::UpdateBodyColor(float DeltaSeconds)
{
	if (!BodyMaterial || FlashElapsedSeconds < 0.0f)
	{
		return;
	}

	const FCombatTuning& Tuning = CombatConfig::Tuning();
	FlashElapsedSeconds += DeltaSeconds;

	if (FlashElapsedSeconds <= Tuning.HitFlashWhiteSeconds)
	{
		CombatVisuals::SetColor(BodyMaterial, FLinearColor::White);
		return;
	}

	const float ReturnElapsed = FlashElapsedSeconds - Tuning.HitFlashWhiteSeconds;
	const float Alpha = (Tuning.HitFlashReturnSeconds > 0.0f) ? FMath::Clamp(ReturnElapsed / Tuning.HitFlashReturnSeconds, 0.0f, 1.0f) : 1.0f;

	// 커브로 돌아온다: 흰색에서 빠르게 벗어나 천천히 원래 색에 붙는다.
	const float WhiteAmount = 1.0f - FMath::Sqrt(Alpha);
	CombatVisuals::SetColor(BodyMaterial, FMath::Lerp(BodyColor, FLinearColor::White, WhiteAmount));

	if (Alpha >= 1.0f)
	{
		FlashElapsedSeconds = -1.0f;
	}
}

void ACombatCharacter::SetAimPoint(const FVector& WorldAimPoint)
{
	const FVector Origin = GetActorLocation() + FVector(0.0f, 0.0f, MuzzleHeightOffset);
	const FVector ToTarget = WorldAimPoint - Origin;
	if (ToTarget.IsNearlyZero())
	{
		return;
	}

	AimRotation = ToTarget.Rotation();
}

void ACombatCharacter::SetAiming(bool bNewAiming)
{
	if (bDead || bRolling || bAiming == bNewAiming)
	{
		return;
	}

	bAiming = bNewAiming;
	UpdateWeaponState();
	ApplyMoveSpeed();
}

void ACombatCharacter::SetCrouching(bool bNewCrouching)
{
	if (bDead)
	{
		return;
	}

	if (bRolling)
	{
		// 구르기 중 C 입력은 선입력으로만 기록하고, 구르기가 끝날 때 소비한다(문서).
		if (bNewCrouching && !bCrouching)
		{
			bCrouchPreInput = true;
		}
		return;
	}

	if (bNewCrouching == bCrouching)
	{
		return;
	}

	if (!bNewCrouching && !HasHeadroomToStand())
	{
		// 머리 위 공간이 없으면 앉은 상태를 유지한다.
		return;
	}

	bCrouching = bNewCrouching;
	bCrouchPreInput = false;
	UpdateCrouchCapsule();
	UpdateWeaponState();
	ApplyMoveSpeed();
}

void ACombatCharacter::SetSprinting(bool bNewSprinting)
{
	if (bDead || bRolling)
	{
		return;
	}

	bSprinting = bNewSprinting;
	UpdateWeaponState();
	ApplyMoveSpeed();
}

void ACombatCharacter::UpdateCrouchCapsule()
{
	const FCombatTuning& Tuning = CombatConfig::Tuning();
	UCapsuleComponent* Capsule = GetCapsuleComponent();
	if (!Capsule)
	{
		return;
	}

	const float CurrentHalfHeight = Capsule->GetUnscaledCapsuleHalfHeight();
	const float TargetHalfHeight = bCrouching ? Tuning.CapsuleHalfHeightCrouch : Tuning.CapsuleHalfHeightStand;
	const float Delta = TargetHalfHeight - CurrentHalfHeight;

	Capsule->SetCapsuleHalfHeight(TargetHalfHeight, true);

	// 캡슐 중심은 액터 위치라서, 반높이가 줄면 발이 떠 버린다. 그만큼 액터를 내려 발을 바닥에 붙인다.
	if (!FMath::IsNearlyZero(Delta))
	{
		AddActorWorldOffset(FVector(0.0f, 0.0f, Delta), true);
	}

	if (BodyMesh)
	{
		const float Diameter = Tuning.CapsuleRadius * 2.0f;
		BodyMesh->SetRelativeScale3D(FVector(Diameter / 100.0f, Diameter / 100.0f, TargetHalfHeight * 2.0f / 100.0f));
		BodyMesh->SetRelativeLocation(FVector(0.0f, 0.0f, -3.0f));
	}
}

bool ACombatCharacter::HasHeadroomToStand() const
{
	const UWorld* World = GetWorld();
	const UCapsuleComponent* Capsule = GetCapsuleComponent();
	if (!World || !Capsule)
	{
		return true;
	}

	const FCombatTuning& Tuning = CombatConfig::Tuning();
	const float Delta = Tuning.CapsuleHalfHeightStand - Tuning.CapsuleHalfHeightCrouch;
	if (Delta <= 0.0f)
	{
		return true;
	}

	const FVector StandCenter = GetActorLocation() + FVector(0.0f, 0.0f, Delta);
	const FCollisionShape StandShape = FCollisionShape::MakeCapsule(Tuning.CapsuleRadius, Tuning.CapsuleHalfHeightStand);
	FCollisionQueryParams QueryParams(SCENE_QUERY_STAT(CombatStandHeadroom), false, this);

	return !World->OverlapBlockingTestByChannel(StandCenter, FQuat::Identity, ECC_Pawn, StandShape, QueryParams);
}

void ACombatCharacter::TryStartRoll()
{
	const FCombatTuning& Tuning = CombatConfig::Tuning();
	if (bDead || bRolling || bCrouching)
	{
		return;
	}

	if (const UCharacterMovementComponent* MovementComponent = GetCharacterMovement())
	{
		if (MovementComponent->IsFalling())
		{
			return;
		}
	}

	const FVector Velocity2D(GetVelocity().X, GetVelocity().Y, 0.0f);
	if (Velocity2D.SizeSquared() <= 1.0f)
	{
		// 제자리 두 번 탭은 달리기로 처리한다(문서).
		return;
	}

	bRolling = true;
	RollRemainingSeconds = Tuning.RollSeconds;
	RollDirection = Velocity2D.GetSafeNormal();
	bCrouchPreInput = false;

	// 구르기 중에는 중력이 적용되지 않고, 끝나는 순간 바닥에 착지한다.
	if (UCharacterMovementComponent* MovementComponent = GetCharacterMovement())
	{
		MovementComponent->SetMovementMode(MOVE_Flying);
		MovementComponent->Velocity = RollDirection * Tuning.RollSpeedUU;
	}

	UpdateWeaponState();
	ApplyMoveSpeed();

	UE_LOG(LogCombat, Log, TEXT("COMBAT_ROLL pawn=%s direction=%s"), *GetName(), *RollDirection.ToCompactString());
}

void ACombatCharacter::OnRollFinished()
{
	bRolling = false;
	RollRemainingSeconds = 0.0f;

	if (UCharacterMovementComponent* MovementComponent = GetCharacterMovement())
	{
		MovementComponent->SetMovementMode(MOVE_Walking);
	}

	// 구르기 중 C 선입력이 있으면 앉기로, 없으면 서기로 끝난다(문서).
	const bool bEndCrouched = bCrouchPreInput;
	bCrouchPreInput = false;
	if (bEndCrouched != bCrouching)
	{
		SetCrouching(bEndCrouched);
	}

	UpdateWeaponState();
	ApplyMoveSpeed();
}

void ACombatCharacter::NotifyJumped()
{
	if (bDead || bRolling || bCrouching)
	{
		return;
	}

	// 점프 시작 순간 총구 충격 +25도가 걸리고 2초에 걸쳐 회복된다.
	JumpImpulseElapsedSeconds = 0.0f;

	UpdateWeaponState();
	ApplyMoveSpeed();

	UE_LOG(LogCombat, Verbose, TEXT("COMBAT_JUMP pawn=%s"), *GetName());
}

float ACombatCharacter::GetHealth() const
{
	return CombatAttributeSet ? CombatAttributeSet->GetHealth() : 0.0f;
}

float ACombatCharacter::GetMaxHealth() const
{
	return CombatAttributeSet ? CombatAttributeSet->GetMaxHealth() : 0.0f;
}

int32 ACombatCharacter::GetAmmoInMagazine() const
{
	return AmmoInMagazine;
}

int32 ACombatCharacter::GetAmmoReserve() const
{
	return AmmoReserve;
}

bool ACombatCharacter::IsAiming() const
{
	return bAiming;
}

bool ACombatCharacter::IsReloading() const
{
	return bReloading;
}

bool ACombatCharacter::CanFireWeapon() const
{
	const FWeaponStateTuning& StateTuning = CombatConfig::StateTuning(WeaponState);
	return !bDead && !bRolling && !bReloading && StateTuning.bCanFire && AmmoInMagazine > 0;
}

float ACombatCharacter::GetNextShotSpreadDeg() const
{
	return FMath::Min(SpreadAccumDeg, CombatConfig::StateTuning(WeaponState).SpreadMaxDeg);
}

bool ACombatCharacter::NotifyWeaponFired()
{
	if (bDead || AmmoInMagazine <= 0)
	{
		return false;
	}

	--AmmoInMagazine;

	const FWeaponStateTuning& StateTuning = CombatConfig::StateTuning(WeaponState);
	SpreadAccumDeg = FMath::Min(SpreadAccumDeg + StateTuning.SpreadPerShotDeg, StateTuning.SpreadMaxDeg);

	++ShotsSinceRecoilStart;

	// 반동 회복: 0.12초 동안 발사가 없으면 누적을 버린다. 새 발사는 회복을 멈추고 다시 더한다.
	if (UWorld* World = GetWorld())
	{
		World->GetTimerManager().SetTimer(RecoilRecoveryTimerHandle, this, &ACombatCharacter::OnRecoilRecoveryFinished, CombatConfig::Tuning().RecoilRecoverySeconds, false);
	}

	if (FireSound)
	{
		UGameplayStatics::PlaySoundAtLocation(this, FireSound, GetMuzzleLocation());
	}

	SyncHudState();
	return true;
}

void ACombatCharacter::OnRecoilRecoveryFinished()
{
	ShotsSinceRecoilStart = 0;
}

bool ACombatCharacter::CanReload() const
{
	const FCombatTuning& Tuning = CombatConfig::Tuning();
	return !bDead && !bRolling && !bReloading && AmmoReserve > 0 && AmmoInMagazine < Tuning.MagazineSize;
}

void ACombatCharacter::BeginReloadState()
{
	bReloading = true;
	UE_LOG(LogCombat, Log, TEXT("COMBAT_RELOAD_START pawn=%s ammo=%d/%d"), *GetName(), AmmoInMagazine, AmmoReserve);
}

void ACombatCharacter::CancelReloadState()
{
	bReloading = false;
}

int32 ACombatCharacter::CompleteReload()
{
	const FCombatTuning& Tuning = CombatConfig::Tuning();

	const int32 Missing = FMath::Max(0, Tuning.MagazineSize - AmmoInMagazine);
	const int32 Moved = FMath::Min(Missing, AmmoReserve);

	AmmoInMagazine += Moved;
	AmmoReserve -= Moved;

	SyncHudState();
	return Moved;
}

void ACombatCharacter::TryFire()
{
	if (UAbilitySystemComponent* ASC = GetAbilitySystemComponent())
	{
		ASC->TryActivateAbilityByClass(UCombatAbility_Fire::StaticClass());
	}
}

void ACombatCharacter::TryReload()
{
	if (UAbilitySystemComponent* ASC = GetAbilitySystemComponent())
	{
		ASC->TryActivateAbilityByClass(UCombatAbility_Reload::StaticClass());
	}
}

void ACombatCharacter::ApplyCombatDamage(float Damage, const FVector& Direction, AActor* Causer, const FVector& HitLocation)
{
	if (bDead || Damage <= 0.0f)
	{
		return;
	}

	UAbilitySystemComponent* ASC = GetAbilitySystemComponent();
	if (!ASC)
	{
		return;
	}

	// 피해는 반드시 GameplayEffect 로 넣는다. 체력 속성에 더하기 연산이므로 음수로 전달한다.
	FGameplayEffectContextHandle Context = ASC->MakeEffectContext();
	Context.AddSourceObject(this);

	const FGameplayEffectSpecHandle SpecHandle = ASC->MakeOutgoingSpec(UCombatDamageEffect::StaticClass(), 1.0f, Context);
	if (!SpecHandle.IsValid() || !SpecHandle.Data.IsValid())
	{
		UE_LOG(LogCombat, Warning, TEXT("COMBAT_DAMAGE_FAILED pawn=%s reason=no-spec"), *GetName());
		return;
	}

	SpecHandle.Data->SetSetByCallerMagnitude(CombatEffectKeys::Damage, -Damage);
	ASC->ApplyGameplayEffectSpecToSelf(*SpecHandle.Data.Get());

	const FCombatTuning& Tuning = CombatConfig::Tuning();

	// 넉백: 발사 방향으로 250uu/s, 0.15초 동안 선형 감쇠.
	KnockbackDirection = Direction.GetSafeNormal2D();
	if (KnockbackDirection.IsNearlyZero())
	{
		KnockbackDirection = -GetActorForwardVector();
	}
	KnockbackRemainingSeconds = Tuning.KnockbackSeconds;

	// 피격 색 플래시 시작(0.12초 흰색 → 0.25초 커브 복귀).
	FlashElapsedSeconds = 0.0f;

	if (HitSound)
	{
		UGameplayStatics::PlaySoundAtLocation(this, HitSound, HitLocation);
	}

	// 맞은 캐릭터의 카메라는 흔들지 않는다. 쏜 쪽(플레이어)이 흔들린다.
	if (APlayerCombatCharacter* Shooter = Cast<APlayerCombatCharacter>(Causer))
	{
		Shooter->PlayCameraShake();
	}

	// 히트스톱은 더미를 맞혔을 때만 건다(문서: 명중 순간 전역 시간 배율).
	if (bIsDummy)
	{
		ApplyHitStop();
	}

	const float NewHealth = GetHealth();
	OnDamaged.Broadcast(Damage, HitLocation);

	UE_LOG(LogCombat, Log, TEXT("COMBAT_DAMAGE victim=%s damage=%.1f health=%.1f"), *GetName(), Damage, NewHealth);

	SyncHudState();

	if (NewHealth <= 0.0f)
	{
		HandleDeath();
	}
}

void ACombatCharacter::ApplyHitStop()
{
	UWorld* World = GetWorld();
	if (!World)
	{
		return;
	}

	const FCombatTuning& Tuning = CombatConfig::Tuning();

	UGameplayStatics::SetGlobalTimeDilation(World, Tuning.HitStopDilation);

	// 월드 타이머는 감속된 게임 시간으로 흐르므로, 실제 0.06초를 원하면 0.06 * 0.15 게임초를 건다.
	// (0.06초 동안 게임 시간은 0.06 * 0.15 만큼만 흐른다.)
	const float GameSeconds = Tuning.HitStopSeconds * Tuning.HitStopDilation;
	World->GetTimerManager().SetTimer(HitStopTimerHandle, this, &ACombatCharacter::RestoreHitStop, FMath::Max(GameSeconds, 0.001f), false);

	UE_LOG(LogCombat, Log, TEXT("COMBAT_HITSTOP dilation=%.2f seconds=%.2f"), Tuning.HitStopDilation, Tuning.HitStopSeconds);
}

void ACombatCharacter::RestoreHitStop()
{
	// 히트스톱은 반드시 원래 배율로 되돌린다(중첩되면 남은 시간만 갱신된다).
	if (UWorld* World = GetWorld())
	{
		UGameplayStatics::SetGlobalTimeDilation(World, 1.0f);
	}
}

void ACombatCharacter::PlayCameraShake()
{
	// 기본 캐릭터(더미)는 카메라가 없으므로 아무 것도 하지 않는다.
}

void ACombatCharacter::HandleDeath()
{
	if (bDead)
	{
		return;
	}

	bDead = true;
	bRolling = false;
	RollRemainingSeconds = 0.0f;
	bReloading = false;

	// 진행 중인 사격·재장전을 취소한다(사망 시 재장전 취소 규칙).
	if (AbilitySystem)
	{
		AbilitySystem->CancelAbilities();
	}

	if (UCharacterMovementComponent* MovementComponent = GetCharacterMovement())
	{
		MovementComponent->DisableMovement();
	}

	if (DeathSound)
	{
		UGameplayStatics::PlaySoundAtLocation(this, DeathSound, GetActorLocation());
	}

	UWorld* World = GetWorld();
	if (World)
	{
		World->GetTimerManager().ClearTimer(RecoilRecoveryTimerHandle);
		World->GetTimerManager().SetTimer(RespawnTimerHandle, this, &ACombatCharacter::RespawnFromDeath, CombatConfig::Tuning().PlayerRespawnSeconds, false);
	}

	UE_LOG(LogCombat, Log, TEXT("COMBAT_DEATH pawn=%s respawnInSeconds=%.1f"), *GetName(), CombatConfig::Tuning().PlayerRespawnSeconds);
}

void ACombatCharacter::RespawnFromDeath()
{
	const FCombatTuning& Tuning = CombatConfig::Tuning();

	SetActorTransform(SpawnTransform, false, nullptr, ETeleportType::TeleportPhysics);

	if (CombatAttributeSet)
	{
		CombatAttributeSet->SetHealth(Tuning.MaxHealth);
	}

	AmmoInMagazine = Tuning.MagazineSize;
	AmmoReserve = Tuning.ReserveAmmo;

	bDead = false;
	bAiming = false;
	bSprinting = false;
	FlashElapsedSeconds = -1.0f;
	KnockbackRemainingSeconds = 0.0f;
	ShotsSinceRecoilStart = 0;

	if (UCharacterMovementComponent* MovementComponent = GetCharacterMovement())
	{
		MovementComponent->SetMovementMode(MOVE_Walking);
	}

	UpdateCrouchCapsule();
	UpdateWeaponState();
	ApplyMoveSpeed();
	SyncHudState();

	UE_LOG(LogCombat, Log, TEXT("COMBAT_RESPAWN pawn=%s"), *GetName());
}

void ACombatCharacter::SyncHudState()
{
	const float HealthValue = GetHealth();
	const float MaxHealthValue = GetMaxHealth();

	if (!FMath::IsNearlyEqual(HealthValue, CachedHealth) || !FMath::IsNearlyEqual(MaxHealthValue, CachedMaxHealth))
	{
		const float Delta = (CachedHealth < 0.0f) ? 0.0f : (HealthValue - CachedHealth);
		CachedHealth = HealthValue;
		CachedMaxHealth = MaxHealthValue;
		OnHealthChanged.Broadcast(HealthValue, MaxHealthValue, Delta);
	}

	if (AmmoInMagazine != CachedMagazine || AmmoReserve != CachedReserve)
	{
		CachedMagazine = AmmoInMagazine;
		CachedReserve = AmmoReserve;
		OnAmmoChanged.Broadcast(AmmoInMagazine, AmmoReserve);
	}
}
