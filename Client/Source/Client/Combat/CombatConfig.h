#pragma once

#include "CoreMinimal.h"
#include "Combat/CombatTypes.h"

/**
 * 전투 수치의 단일 출처(single source of truth).
 *
 * Document/30-design/combat-system.md · character-movement.md 의 확정 값을 여기서만 채운다.
 * 다른 파일은 호출만 하고 숫자를 적지 않는다(자동 테스트 Client.Combat.Tuning 이 ToJson 결과를
 * 문서 표와 대조하고, config.json 을 산출물로 남긴다).
 */
namespace CombatConfig
{
	/** 상태 수. EWeaponState 항목 수와 같아야 한다. */
	constexpr int32 NumWeaponStates = 9;

	/** 문서로 확정된 전투·이동·타격감 스칼라 전부. */
	const FCombatTuning& Tuning();

	/** 상태별 탄퍼짐·총구 벌어짐. 범위를 벗어난 값은 Stand_Idle 로 처리한다. */
	const FWeaponStateTuning& StateTuning(EWeaponState State);

	/** 열거형 순서(Stand_Idle … Roll)대로 9개. */
	TArray<FWeaponStateTuning> AllStates();

	/** 상태 이름 문자열. 기획 문서 표와 JSON 키가 같은 문자열을 쓴다. */
	const TCHAR* StateName(EWeaponState State);

	/** 상태 이름으로 역조회. 모르는 문자열이면 false. */
	bool TryParseStateName(const FString& Name, EWeaponState& OutState);

	/**
	 * config.json 본문. 2칸 들여쓰기, 객체 내부는 공백 없음, 끝에 개행.
	 * 외부 검사기가 키 이름·순서·값을 고정해 비교하므로 형식을 바꾸지 않는다.
	 */
	FString ToJson();
}
