#include "Combat/CombatEffects.h"

#include "Combat/CombatAttributeSet.h"
#include "Combat/CombatConfig.h"

UCombatDamageEffect::UCombatDamageEffect()
{
	// 즉시 적용. 체력 속성에 SetByCaller 값을 그대로 더한다(피해는 음수로 들어온다).
	DurationPolicy = EGameplayEffectDurationType::Instant;

	FGameplayModifierInfo& Modifier = Modifiers.AddDefaulted_GetRef();
	Modifier.Attribute = UCombatAttributeSet::GetHealthAttribute();
	Modifier.ModifierOp = EGameplayModOp::Additive;

	FSetByCallerFloat SetByCaller;
	SetByCaller.DataName = CombatEffectKeys::Damage;
	Modifier.ModifierMagnitude = FGameplayEffectModifierMagnitude(SetByCaller);
}

UCombatFireCooldownEffect::UCombatFireCooldownEffect()
{
	// 지속 시간 = 연사 간격. 종료될 때까지 같은 어빌리티를 다시 활성화할 수 없다.
	DurationPolicy = EGameplayEffectDurationType::HasDuration;
	DurationMagnitude = FGameplayEffectModifierMagnitude(FScalableFloat(CombatConfig::Tuning().FireIntervalSeconds));
}
