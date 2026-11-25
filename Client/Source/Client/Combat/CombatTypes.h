#pragma once

#include "CoreMinimal.h"
#include "CombatTypes.generated.h"

/**
 * 무기 상태 9종. 이름과 순서는 Document/30-design/combat-system.md 의 벌어짐 표와 1:1로 대응한다.
 * 자동 테스트(Client.Combat.Tuning)가 이 순서를 검사하므로 항목을 재배열하거나 추가하지 않는다.
 */
UENUM(BlueprintType)
enum class EWeaponState : uint8
{
	Stand_Idle = 0,
	Stand_Walk = 1,
	Stand_Run = 2,
	Stand_Aim = 3,
	Crouch_Idle = 4,
	Crouch_Walk = 5,
	Crouch_Aim = 6,
	Jump_Fall = 7,
	Roll = 8
};

/** 상태별 탄퍼짐·총구 벌어짐 수치. 실제 값은 CombatConfig.cpp 한 곳에만 있다. */
USTRUCT(BlueprintType)
struct FWeaponStateTuning
{
	GENERATED_BODY()

	/** 발사 순간 더해지는 기본 탄퍼짐(도). */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Combat")
	float SpreadBaseDeg = 0.0f;

	/** 연속 발사 1발당 누적 탄퍼짐(도). */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Combat")
	float SpreadPerShotDeg = 0.0f;

	/** 누적 탄퍼짐 상한(도). */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Combat")
	float SpreadMaxDeg = 0.0f;

	/** 지속 벌어짐 + 호흡 성분의 상한(도). 점프 충격은 이 상한을 넘을 수 있다. */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Combat")
	float MuzzleMaxDeg = 0.0f;

	/** 마우스 방향을 따라잡는 속도(도/초). */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Combat")
	float MuzzleTrackDegPerSec = 0.0f;

	/** 이 상태에서 사격할 수 있는가. 구르기만 false다. */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Combat")
	bool bCanFire = false;
};

/**
 * 문서로 확정된 모든 스칼라. 값은 CombatConfig.cpp 에서만 채운다(단일 출처).
 * 기본값은 0이며 "밸런싱 값"이 아니라 초기화 자리표시자다.
 */
USTRUCT(BlueprintType)
struct FCombatTuning
{
	GENERATED_BODY()

	// --- 체력 ---
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Health")
	float MaxHealth = 0.0f;

	// --- 피스톨 ---
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Pistol")
	float PistolDamage = 0.0f;
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Pistol")
	int32 MagazineSize = 0;
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Pistol")
	int32 ReserveAmmo = 0;
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Pistol")
	float FireIntervalSeconds = 0.0f;
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Pistol")
	float ReloadSeconds = 0.0f;
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Pistol")
	float WeaponRangeUU = 0.0f;
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Pistol")
	float ProjectileSpeedUUPerSec = 0.0f;

	// --- 반동 ---
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Recoil")
	float RecoilVerticalDegPerShot = 0.0f;
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Recoil")
	float RecoilHorizontalDegPerShot = 0.0f;
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Recoil")
	float RecoilRecoverySeconds = 0.0f;
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Recoil")
	float CameraKickDeg = 0.0f;
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Recoil")
	float CameraKickSeconds = 0.0f;
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Recoil")
	float AimRecoilScale = 0.0f;

	// --- 호흡 ---
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Breath")
	float BreathPeriodSeconds = 0.0f;
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Breath")
	float BreathAmplitudeDeg = 0.0f;

	// --- 점프 충격 ---
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Jump")
	float JumpImpulseDeg = 0.0f;
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Jump")
	float JumpImpulseRecoverySeconds = 0.0f;
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Jump")
	float JumpZVelocity = 0.0f;
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Jump")
	float GravityScale = 0.0f;

	// --- 이동 ---
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Movement")
	float SpeedStandUU = 0.0f;
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Movement")
	float SpeedRunUU = 0.0f;
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Movement")
	float SpeedCrouchUU = 0.0f;
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Movement")
	float SpeedAimUU = 0.0f;
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Movement")
	float SpeedCrouchAimUU = 0.0f;
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Movement")
	float RollSpeedUU = 0.0f;
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Movement")
	float RollSeconds = 0.0f;
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Movement")
	float RollInputWindowSeconds = 0.0f;
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Movement")
	float AirControl = 0.0f;
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Movement")
	float CapsuleRadius = 0.0f;
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Movement")
	float CapsuleHalfHeightStand = 0.0f;
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Movement")
	float CapsuleHalfHeightCrouch = 0.0f;
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Movement")
	float CameraCrouchDrop = 0.0f;

	// --- 타격감 ---
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "HitFeel")
	float HitFlashWhiteSeconds = 0.0f;
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "HitFeel")
	float HitFlashReturnSeconds = 0.0f;
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "HitFeel")
	float HitStopSeconds = 0.0f;
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "HitFeel")
	float HitStopDilation = 0.0f;
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "HitFeel")
	float ShakeAmplitude = 0.0f;
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "HitFeel")
	float ShakeSeconds = 0.0f;
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "HitFeel")
	float KnockbackSpeedUU = 0.0f;
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "HitFeel")
	float KnockbackSeconds = 0.0f;
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "HitFeel")
	float ImpactBurstSeconds = 0.0f;
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "HitFeel")
	float DelayedBarHoldSeconds = 0.0f;
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "HitFeel")
	float DelayedBarCatchUpSeconds = 0.0f;
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "HitFeel")
	float DamageNumberSeconds = 0.0f;
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "HitFeel")
	float LowHealthFlashSeconds = 0.0f;

	/** 사격하지 않을 때 탄퍼짐 1단계가 줄어드는 시간(초). */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "AimLine")
	float SpreadDecayStepSeconds = 0.0f;

	// --- 조준선 ---
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "AimLine")
	float AimLineThicknessUU = 0.0f;

	// --- 사망 ---
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Death")
	float PlayerRespawnSeconds = 0.0f;
};
