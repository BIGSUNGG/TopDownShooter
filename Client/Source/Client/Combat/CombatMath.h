#pragma once

#include "CoreMinimal.h"
#include "Combat/CombatTypes.h"

/**
 * 전투 연출 계산용 순수 함수 모음.
 *
 * UObject도, 타이머도, 월드도 쓰지 않는다. 입력만으로 결과가 정해지므로 자동 테스트가
 * 월드 없이 호출해 문서 표의 곡선을 검증할 수 있다(Client.Combat.* 테스트).
 * 상수는 전부 CombatConfig 에서 읽는다.
 */
namespace CombatMath
{
	/** 두 방향 사이의 각도(도). 0 벡터는 0도로 본다. */
	float AngleBetweenDeg(const FVector& A, const FVector& B);

	/** `min(base + perShot * ShotIndex, max)`. ShotIndex는 0부터 센다. */
	float SpreadForShot(const FWeaponStateTuning& StateTuning, int32 ShotIndex);

	/**
	 * 발사하지 않을 때 탄퍼짐이 기본값으로 돌아오는 곡선.
	 * 문서의 "0.2초당 1단계 감소"를 지수 감쇠로 옮긴 식이다(구현 주석 참고).
	 */
	float SpreadDecay(float Current, float Base, float DeltaSeconds);

	/** 조준 방향에 반경 SpreadDeg 이내의 임의 오프셋을 더한다(원뿔 내부 균등). */
	FRotator ApplySpread(const FRotator& Aim, float SpreadDeg, FRandomStream& Stream);

	/**
	 * 총구 방향을 조준 방향으로 추종시킨다.
	 * 1) TrackDegPerSec * DeltaSeconds 만큼만 회전하고,
	 * 2) 남은 각도가 MuzzleMaxDeg 를 넘으면 상한까지만 남긴다(상태 변경 시 즉시 잘림).
	 */
	FVector MuzzleTrack(const FVector& CurrentDir, const FVector& TargetDir, float TrackDegPerSec, float MuzzleMaxDeg, float DeltaSeconds);

	/** 호흡이 총구 벌어짐에 더해지는 상태인가(서기·앉기 × 대기·조준). */
	bool BreathAppliesTo(EWeaponState State);

	/** 호흡에 의한 벌어짐(도). 주기 4초, 진폭 0.4도의 사인파. */
	float BreathSwayDeg(double TimeSeconds);

	/** 점프 시작 순간의 총구 충격(도). 2.0초 동안 선형으로 0까지 줄고 그 뒤에는 0. */
	float JumpImpulseDeg(float ElapsedSeconds);

	/**
	 * 최종 총구 벌어짐(도) = clamp(지속 벌어짐 + 호흡, ±상태 상한) + 점프 충격.
	 * 상태 상한은 지속 벌어짐·호흡 성분에만 적용되고 점프 충격은 그 위에 더해진다.
	 */
	float EffectiveMuzzleOffsetDeg(EWeaponState State, float PersistentDeg, double TimeSeconds, float JumpElapsedSeconds);

	/**
	 * 확산 각도(도)를 화면 픽셀 간격으로 바꾼다.
	 * HalfViewportWidthPx 는 뷰포트 폭의 절반, HalfFovDeg 는 수평 FOV 의 절반이다.
	 * 확산 0도면 0px 이고, 각도가 커지면 탄젠트로 증가한다.
	 */
	float CrosshairGapPixels(float SpreadDeg, float HalfViewportWidthPx, float HalfFovDeg);

	/**
	 * 발당 반동 누적값. 위로 0.6도/발, 좌우 ±0.3도/발, 조준 중에는 절반.
	 * 반동 회복이 시작된 뒤 발사 수를 받아 그만큼 누적한 회전을 돌려준다.
	 */
	FRotator RecoilOffset(int32 ShotsSinceRecoveryStart, bool bAiming);
}
