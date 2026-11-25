#include "Combat/CombatAbility_Reload.h"

#include "Character/CombatCharacter.h"
#include "Combat/CombatConfig.h"
#include "Combat/CombatLog.h"
#include "Engine/World.h"
#include "TimerManager.h"

UCombatAbility_Reload::UCombatAbility_Reload()
{
	InstancingPolicy = EGameplayAbilityInstancingPolicy::InstancedPerActor;
	NetExecutionPolicy = EGameplayAbilityNetExecutionPolicy::LocalOnly;
}

bool UCombatAbility_Reload::CanActivateAbility(const FGameplayAbilitySpecHandle Handle, const FGameplayAbilityActorInfo* ActorInfo, const FGameplayTagContainer* SourceTags, const FGameplayTagContainer* TargetTags, OUT FGameplayTagContainer* OptionalRelevantTags) const
{
	if (!Super::CanActivateAbility(Handle, ActorInfo, SourceTags, TargetTags, OptionalRelevantTags))
	{
		return false;
	}

	const ACombatCharacter* Character = ActorInfo ? Cast<ACombatCharacter>(ActorInfo->AvatarActor.Get()) : nullptr;
	return Character != nullptr && Character->CanReload();
}

void UCombatAbility_Reload::ActivateAbility(const FGameplayAbilitySpecHandle Handle, const FGameplayAbilityActorInfo* ActorInfo, const FGameplayAbilityActivationInfo ActivationInfo, const FGameplayEventData* TriggerEventData)
{
	Super::ActivateAbility(Handle, ActorInfo, ActivationInfo, TriggerEventData);

	ACombatCharacter* Character = Cast<ACombatCharacter>(GetAvatarActorFromActorInfo());
	if (!Character || !Character->CanReload())
	{
		// 예비 탄약이 없거나 이미 가득이면 실패하되 아무 것도 하지 않는다(문서: 조용한 실패).
		EndAbility(Handle, ActorInfo, ActivationInfo, true, true);
		return;
	}

	if (!CommitAbility(Handle, ActorInfo, ActivationInfo))
	{
		EndAbility(Handle, ActorInfo, ActivationInfo, true, false);
		return;
	}

	Character->BeginReloadState();

	UWorld* World = GetWorld();
	if (!World)
	{
		Character->CancelReloadState();
		EndAbility(Handle, ActorInfo, ActivationInfo, true, true);
		return;
	}

	World->GetTimerManager().SetTimer(ReloadTimerHandle, this, &UCombatAbility_Reload::OnReloadFinished, CombatConfig::Tuning().ReloadSeconds, false);
}

void UCombatAbility_Reload::OnReloadFinished()
{
	ACombatCharacter* Character = Cast<ACombatCharacter>(GetAvatarActorFromActorInfo());
	if (Character)
	{
		const int32 MovedRounds = Character->CompleteReload();
		UE_LOG(LogCombat, Log, TEXT("COMBAT_RELOAD pawn=%s moved=%d ammo=%d/%d"),
			*Character->GetName(),
			MovedRounds,
			Character->GetAmmoInMagazine(),
			Character->GetAmmoReserve());
	}

	EndAbility(CurrentSpecHandle, CurrentActorInfo, CurrentActivationInfo, true, false);
}

void UCombatAbility_Reload::EndAbility(const FGameplayAbilitySpecHandle Handle, const FGameplayAbilityActorInfo* ActorInfo, const FGameplayAbilityActivationInfo ActivationInfo, bool bReplicateEndAbility, bool bWasCancelled)
{
	if (UWorld* World = GetWorld())
	{
		World->GetTimerManager().ClearTimer(ReloadTimerHandle);
	}

	ACombatCharacter* Character = Cast<ACombatCharacter>(GetAvatarActorFromActorInfo());
	if (Character)
	{
		// 정상 종료·취소·사망 어느 경로로 끝나도 장전 상태를 남기지 않는다.
		Character->CancelReloadState();

		if (bWasCancelled)
		{
			UE_LOG(LogCombat, Log, TEXT("COMBAT_RELOAD_CANCELLED pawn=%s"), *Character->GetName());
		}
	}

	Super::EndAbility(Handle, ActorInfo, ActivationInfo, bReplicateEndAbility, bWasCancelled);
}
