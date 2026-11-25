#include "Combat/CombatAbility_Fire.h"

#include "Character/CombatCharacter.h"
#include "Combat/CombatConfig.h"
#include "Combat/CombatEffects.h"
#include "Combat/CombatLog.h"
#include "Combat/CombatMath.h"
#include "Combat/CombatProjectile.h"
#include "Engine/World.h"

UCombatAbility_Fire::UCombatAbility_Fire()
{
	InstancingPolicy = EGameplayAbilityInstancingPolicy::InstancedPerActor;
	NetExecutionPolicy = EGameplayAbilityNetExecutionPolicy::LocalOnly;
	CooldownGameplayEffectClass = UCombatFireCooldownEffect::StaticClass();
}

bool UCombatAbility_Fire::CanActivateAbility(const FGameplayAbilitySpecHandle Handle, const FGameplayAbilityActorInfo* ActorInfo, const FGameplayTagContainer* SourceTags, const FGameplayTagContainer* TargetTags, OUT FGameplayTagContainer* OptionalRelevantTags) const
{
	if (!Super::CanActivateAbility(Handle, ActorInfo, SourceTags, TargetTags, OptionalRelevantTags))
	{
		return false;
	}

	const ACombatCharacter* Character = ActorInfo ? Cast<ACombatCharacter>(ActorInfo->AvatarActor.Get()) : nullptr;
	return Character != nullptr && Character->CanFireWeapon();
}

void UCombatAbility_Fire::ActivateAbility(const FGameplayAbilitySpecHandle Handle, const FGameplayAbilityActorInfo* ActorInfo, const FGameplayAbilityActivationInfo ActivationInfo, const FGameplayEventData* TriggerEventData)
{
	Super::ActivateAbility(Handle, ActorInfo, ActivationInfo, TriggerEventData);

	ACombatCharacter* Character = Cast<ACombatCharacter>(GetAvatarActorFromActorInfo());
	if (!Character || !Character->CanFireWeapon())
	{
		EndAbility(Handle, ActorInfo, ActivationInfo, true, true);
		return;
	}

	// 연사 간격(쿨다운)을 먼저 커밋한다. 0.15초가 지나지 않은 입력은 여기서 걸러진다.
	if (!CommitAbility(Handle, ActorInfo, ActivationInfo))
	{
		EndAbility(Handle, ActorInfo, ActivationInfo, true, false);
		return;
	}

	// 이번 발에 적용할 탄퍼짐은 누적 후가 아니라 누적 전 값을 쓴다.
	const float SpreadDeg = Character->GetNextShotSpreadDeg();
	if (!Character->NotifyWeaponFired())
	{
		EndAbility(Handle, ActorInfo, ActivationInfo, true, true);
		return;
	}

	UWorld* World = GetWorld();
	if (!World)
	{
		EndAbility(Handle, ActorInfo, ActivationInfo, true, false);
		return;
	}

	// 탄은 마우스 방향이 아니라 총구가 향한 방향(MuzzleDir)으로 날아가고, 여기에 상태별 탄퍼짐을 더한다.
	FRandomStream& SpreadStream = Character->GetSpreadRandomStream();
	const FRotator FiringRotation = CombatMath::ApplySpread(Character->GetMuzzleDir().Rotation(), SpreadDeg, SpreadStream);
	const FVector FiringDirection = FiringRotation.Vector();

	FActorSpawnParameters SpawnParameters;
	SpawnParameters.Owner = Character;
	SpawnParameters.Instigator = Character;
	SpawnParameters.SpawnCollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::AlwaysSpawn;

	const FVector MuzzleLocation = Character->GetMuzzleLocation();
	if (ACombatProjectile* Projectile = World->SpawnActor<ACombatProjectile>(ACombatProjectile::StaticClass(), MuzzleLocation, FiringDirection.Rotation(), SpawnParameters))
	{
		Projectile->InitProjectile(FiringDirection, Character);
	}

	UE_LOG(LogCombat, Log, TEXT("COMBAT_FIRE pawn=%s ammo=%d/%d spreadDeg=%.2f muzzleDir=%s"),
		*Character->GetName(),
		Character->GetAmmoInMagazine(),
		Character->GetAmmoReserve(),
		SpreadDeg,
		*FiringDirection.ToCompactString());

	EndAbility(Handle, ActorInfo, ActivationInfo, true, false);
}
