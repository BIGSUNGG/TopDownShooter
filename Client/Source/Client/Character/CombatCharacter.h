#pragma once

#include "CoreMinimal.h"
#include "AbilitySystemInterface.h"
#include "Combat/CombatDamageable.h"
#include "Combat/CombatTypes.h"
#include "GameFramework/Character.h"
#include "CombatCharacter.generated.h"

class UAbilitySystemComponent;
class UCombatAttributeSet;
class UMaterialInstanceDynamic;
class UMaterialInterface;
class UStaticMesh;
class UStaticMeshComponent;
class USoundBase;

/** HUD 계약: 체력 변화. WBP가 이 이름으로 바인딩한다. */
DECLARE_DYNAMIC_MULTICAST_DELEGATE_ThreeParams(FOnHealthChanged, float, NewHealth, float, MaxHealth, float, Delta);

/** HUD 계약: 탄약 변화. */
DECLARE_DYNAMIC_MULTICAST_DELEGATE_TwoParams(FOnAmmoChanged, int32, Magazine, int32, Reserve);

/** HUD 계약: 피격(데미지 숫자·화면 플래시용). WorldLocation은 맞은 지점. */
DECLARE_DYNAMIC_MULTICAST_DELEGATE_TwoParams(FOnDamaged, float, Damage, FVector, WorldLocation);

/**
 * 전투 캐릭터의 공통 몸통.
 *
 * - 체력/피해는 GameplayAbilitySystem(UCombatAttributeSet + GameplayEffect)으로만 다룬다.
 * - 자세·속도·탄퍼짐·총구 벌어짐은 CombatConfig 수치와 CombatMath 순수 함수로 계산한다.
 * - 사격·재장전은 어빌리티가 담당하고, 이 클래스는 상태와 탄약만 제공한다.
 *
 * 플레이어(ACombatCharacter)와 더미(ADummyCombatCharacter)가 이 클래스를 공유한다.
 */
UCLASS()
class CLIENT_API ACombatCharacter : public ACharacter, public IAbilitySystemInterface, public ICombatDamageable
{
	GENERATED_BODY()

public:
	ACombatCharacter();

	virtual void Tick(float DeltaSeconds) override;
	virtual void BeginPlay() override;
	virtual void PossessedBy(AController* NewController) override;

	// --- IAbilitySystemInterface ---
	virtual UAbilitySystemComponent* GetAbilitySystemComponent() const override;

	// --- ICombatDamageable ---
	virtual void ApplyCombatDamage(float Damage, const FVector& Direction, AActor* Causer, const FVector& HitLocation) override;

	// --- HUD 계약 ---
	UFUNCTION(BlueprintPure, Category = "Combat|HUD")
	float GetHealth() const;

	UFUNCTION(BlueprintPure, Category = "Combat|HUD")
	float GetMaxHealth() const;

	UFUNCTION(BlueprintPure, Category = "Combat|HUD")
	int32 GetAmmoInMagazine() const;

	UFUNCTION(BlueprintPure, Category = "Combat|HUD")
	int32 GetAmmoReserve() const;

	UFUNCTION(BlueprintPure, Category = "Combat|HUD")
	bool IsAiming() const;

	UFUNCTION(BlueprintPure, Category = "Combat|HUD")
	bool IsReloading() const;

	UPROPERTY(BlueprintAssignable, Category = "Combat|HUD")
	FOnHealthChanged OnHealthChanged;

	UPROPERTY(BlueprintAssignable, Category = "Combat|HUD")
	FOnAmmoChanged OnAmmoChanged;

	UPROPERTY(BlueprintAssignable, Category = "Combat|HUD")
	FOnDamaged OnDamaged;

	// --- 사격·재장전 (어빌리티가 호출한다) ---
	bool CanFireWeapon() const;
	float GetNextShotSpreadDeg() const;

	/** HUD 크로스헤어가 쓰는 현재 확산 각도(도). 탄퍼짐과 같은 값이다. */
	UFUNCTION(BlueprintPure, Category = "Combat|Weapon")
	float GetCurrentSpreadDeg() const { return GetNextShotSpreadDeg(); }

	/** 1발 소모 + 탄퍼짐 누적 + 반동 + HUD 알림. 실패하면 false. */
	bool NotifyWeaponFired();

	FVector GetMuzzleLocation() const;
	FVector GetMuzzleDir() const { return MuzzleDir; }
	FRandomStream& GetSpreadRandomStream() { return SpreadRandomStream; }

	bool CanReload() const;
	void BeginReloadState();
	void CancelReloadState();

	/** 탄창을 min(부족분, 예비) 만큼 채우고 실제로 옮긴 수를 돌려준다. */
	int32 CompleteReload();

	/** 입력에서 호출: 쿨다운이 남아 있으면 어빌리티가 스스로 거절한다. */
	void TryFire();
	void TryReload();

	// --- 자세·이동 ---
	void SetAiming(bool bNewAiming);
	void SetCrouching(bool bNewCrouching);
	void SetSprinting(bool bNewSprinting);
	void TryStartRoll();

	/** 점프 시작 순간 총구 충격을 건다. */
	void NotifyJumped();

	/** 플레이어가 마우스로 고른 조준점(월드). 조준 방향·총구 방향 계산의 기준. */
	void SetAimPoint(const FVector& WorldAimPoint);

	EWeaponState GetWeaponState() const { return WeaponState; }
	bool IsCrouching() const { return bCrouching; }
	bool IsRolling() const { return bRolling; }
	bool IsDead() const { return bDead; }

	UCombatAttributeSet* GetCombatAttributeSet() const { return CombatAttributeSet; }

	/** 카메라 연출 훅. 기본 캐릭터(더미)는 아무 것도 하지 않는다. */
	virtual void PlayCameraShake();

protected:
	/** 사망 처리. 기본은 플레이어 규칙(3초 뒤 시작 지점 부활). 더미는 재정의한다. */
	virtual void HandleDeath();

	/** 시작 지점/스폰 지점으로 되돌리고 체력을 채운다. */
	virtual void RespawnFromDeath();

	/** 어빌리티 부여. 플레이어만 사격·재장전을 받는다. */
	virtual void GrantCombatAbilities();

	void UpdateWeaponState();
	void ApplyMoveSpeed();
	void UpdateMuzzle(float DeltaSeconds);
	void UpdateAimLine();
	void UpdateHitFeedback(float DeltaSeconds);
	void UpdateCrouchCapsule();
	void SyncHudState();
	void ApplyHitStop();

	// --- 컴포넌트 ---
	UPROPERTY(VisibleAnywhere, Category = "Combat")
	TObjectPtr<UAbilitySystemComponent> AbilitySystem;

	UPROPERTY(VisibleAnywhere, Category = "Combat")
	TObjectPtr<UCombatAttributeSet> CombatAttributeSet;

	/** 기본 원기둥(100uu)을 캡슐에 맞춰 늘린 마네킹 몸통. */
	UPROPERTY(VisibleAnywhere, Category = "Combat")
	TObjectPtr<UStaticMeshComponent> BodyMesh;

	/** 총구에 붙는 큐브 "총". */
	UPROPERTY(VisibleAnywhere, Category = "Combat")
	TObjectPtr<UStaticMeshComponent> GunMesh;

	/** 조준선(두께 2cm 빨간 원기둥). 더미는 숨긴다. */
	UPROPERTY(VisibleAnywhere, Category = "Combat")
	TObjectPtr<UStaticMeshComponent> AimLineMesh;

	UPROPERTY()
	TObjectPtr<UMaterialInstanceDynamic> BodyMaterial;

	/** 동적 인스턴스를 만들 때 쓰는 부모 머티리얼(기본 도형 머티리얼). */
	UPROPERTY()
	TObjectPtr<UMaterialInterface> BaseMaterial;

	UPROPERTY()
	TObjectPtr<UMaterialInstanceDynamic> AimLineMaterial;

	/** 사운드 훅: 에셋이 없으면 재생하지 않는다. */
	UPROPERTY(EditDefaultsOnly, Category = "Combat|Sound")
	TObjectPtr<USoundBase> FireSound;

	UPROPERTY(EditDefaultsOnly, Category = "Combat|Sound")
	TObjectPtr<USoundBase> HitSound;

	UPROPERTY(EditDefaultsOnly, Category = "Combat|Sound")
	TObjectPtr<USoundBase> DeathSound;

	// --- 상태 ---
	EWeaponState WeaponState = EWeaponState::Stand_Idle;

	UPROPERTY(VisibleInstanceOnly, Category = "Combat")
	int32 AmmoInMagazine = 0;

	UPROPERTY(VisibleInstanceOnly, Category = "Combat")
	int32 AmmoReserve = 0;

	bool bAiming = false;
	bool bCrouching = false;
	bool bSprinting = false;
	bool bRolling = false;
	bool bReloading = false;
	bool bDead = false;
	bool bCrouchPreInput = false;
	bool bWeaponAbilitiesGranted = false;

	/** 더미인가. 더미는 즉시 부활하고 조준선·카메라 연출을 쓰지 않는다. */
	bool bIsDummy = false;

	/** 조준 방향(월드). 플레이어는 마우스 커서 기준으로 매 틱 갱신한다. */
	FRotator AimRotation = FRotator::ZeroRotator;

	/** 최종 총구 방향. 발사와 조준선이 같은 값을 쓴다. */
	FVector MuzzleDir = FVector::ForwardVector;

	/** 조준 방향을 추종 속도로 따라가는 지속 벌어짐 방향. */
	FVector PersistentMuzzleDir = FVector::ForwardVector;

	/** 상태가 바뀌면 새 상한으로 잘라 쓴다. */
	float SpreadAccumDeg = 0.0f;

	FRandomStream SpreadRandomStream;

	/** 반동 회복이 시작된 뒤 쏜 수(반동 누적 계산용). */
	int32 ShotsSinceRecoilStart = 0;

	/** 점프 충격 경과 시간. 점프하지 않았으면 큰 값이라 충격이 0이다. */
	float JumpImpulseElapsedSeconds = 1000.0f;

	float KnockbackRemainingSeconds = 0.0f;
	FVector KnockbackDirection = FVector::ZeroVector;

	float FlashElapsedSeconds = -1.0f;

	float RollRemainingSeconds = 0.0f;
	FVector RollDirection = FVector::ForwardVector;

	/** 스폰/시작 지점. 부활 위치로 쓴다. */
	FTransform SpawnTransform = FTransform::Identity;

	// --- HUD 캐시(변화 시에만 브로드캐스트) ---
	float CachedHealth = -1.0f;
	float CachedMaxHealth = -1.0f;
	int32 CachedMagazine = -1;
	int32 CachedReserve = -1;

	// --- 타이머 ---
	FTimerHandle RollTimerHandle;
	FTimerHandle RespawnTimerHandle;
	FTimerHandle HitStopTimerHandle;
	FTimerHandle RecoilRecoveryTimerHandle;

	void OnRollFinished();
	void OnRecoilRecoveryFinished();
	void RestoreHitStop();

	void UpdateBodyColor(float DeltaSeconds);

	/** 머리 위에 서기 공간이 있는가(문서: 공간이 없으면 앉은 상태 유지). */
	bool HasHeadroomToStand() const;

	/** 상태가 실제로 바뀔 때 공통 후처리(탄퍼짐 자르기·속도 갱신). */
	void OnWeaponStateChanged();

	bool bAimLineBlocked = false;
};
