#include "Combat/CombatMath.h"

#include "Combat/CombatConfig.h"

namespace CombatMath
{
	float AngleBetweenDeg(const FVector& A, const FVector& B)
	{
		const FVector NormalA = A.GetSafeNormal();
		const FVector NormalB = B.GetSafeNormal();
		if (NormalA.IsNearlyZero() || NormalB.IsNearlyZero())
		{
			return 0.0f;
		}

		const float Dot = FMath::Clamp(FVector::DotProduct(NormalA, NormalB), -1.0f, 1.0f);
		return FMath::RadiansToDegrees(FMath::Acos(Dot));
	}

	float SpreadForShot(const FWeaponStateTuning& StateTuning, int32 ShotIndex)
	{
		const int32 Steps = FMath::Max(0, ShotIndex);
		const float Accumulated = StateTuning.SpreadBaseDeg + StateTuning.SpreadPerShotDeg * static_cast<float>(Steps);
		return FMath::Min(Accumulated, StateTuning.SpreadMaxDeg);
	}

	float SpreadDecay(float Current, float Base, float DeltaSeconds)
	{
		if (Current <= Base)
		{
			return Base;
		}

		if (DeltaSeconds <= 0.0f)
		{
			return Current;
		}

		// 문서: "발사하지 않으면 0.2초당 1단계씩 줄어든다". 1단계의 크기는 상태별 SpreadPerShotDeg라
		// 이 함수의 입력에 없다. 그래서 "0.2초마다 남은 초과분을 한 단계씩 덜어낸다"를 연속 시간으로
		// 옮겨, 시간 상수 0.2초의 지수 감쇠로 근사한다: 0.2초 뒤 초과분은 e^-1(약 37%)만 남는다.
		// 선형으로 두면 초과분 크기와 무관하게 같은 양이 빠져 상태별 표(1단계 = perShot)와 어긋난다.
		const float StepSeconds = CombatConfig::Tuning().SpreadDecayStepSeconds;
		if (StepSeconds <= 0.0f)
		{
			return Base;
		}

		const float Excess = Current - Base;
		const float Decayed = Base + Excess * FMath::Exp(-DeltaSeconds / StepSeconds);
		return FMath::Max(Decayed, Base);
	}

	FRotator ApplySpread(const FRotator& Aim, float SpreadDeg, FRandomStream& Stream)
	{
		if (SpreadDeg <= 0.0f)
		{
			return Aim;
		}

		const FVector AimDir = Aim.Vector();

		// 원뿔 안에서 균등하도록 반지름을 sqrt(U)로 뽑는다(원판 위 균등 분포).
		const float ConeAngleDeg = SpreadDeg * FMath::Sqrt(FMath::Clamp(Stream.FRand(), 0.0f, 1.0f));
		const float AzimuthDeg = Stream.FRandRange(0.0f, 360.0f);

		FVector Right = FVector::CrossProduct(AimDir, FVector::UpVector).GetSafeNormal();
		if (Right.IsNearlyZero())
		{
			Right = FVector::CrossProduct(AimDir, FVector::ForwardVector).GetSafeNormal();
		}
		const FVector Up = FVector::CrossProduct(Right, AimDir).GetSafeNormal();

		const float AzimuthRad = FMath::DegreesToRadians(AzimuthDeg);
		const FVector OffsetDir = (Right * FMath::Cos(AzimuthRad) + Up * FMath::Sin(AzimuthRad)).GetSafeNormal();

		// AimDir × OffsetDir 축으로 돌리면 AimDir가 OffsetDir 쪽으로 이동한다.
		const FVector Axis = FVector::CrossProduct(AimDir, OffsetDir).GetSafeNormal();
		const FVector Tilted = Axis.IsNearlyZero() ? AimDir : AimDir.RotateAngleAxis(ConeAngleDeg, Axis);

		return Tilted.Rotation();
	}

	FVector MuzzleTrack(const FVector& CurrentDir, const FVector& TargetDir, float TrackDegPerSec, float MuzzleMaxDeg, float DeltaSeconds)
	{
		const FVector Target = TargetDir.GetSafeNormal();
		if (Target.IsNearlyZero())
		{
			return CurrentDir.GetSafeNormal();
		}

		FVector Current = CurrentDir.GetSafeNormal();
		if (Current.IsNearlyZero())
		{
			Current = Target;
		}

		float RemainingDeg = AngleBetweenDeg(Current, Target);
		const float MaxStepDeg = FMath::Max(0.0f, TrackDegPerSec) * FMath::Max(0.0f, DeltaSeconds);

		// 1) 추종 속도 제한: 이번 프레임에 회전할 수 있는 각도만큼만 조준 방향으로 간다.
		if (RemainingDeg > MaxStepDeg && RemainingDeg > UE_KINDA_SMALL_NUMBER)
		{
			const FVector Axis = FVector::CrossProduct(Current, Target).GetSafeNormal();
			if (!Axis.IsNearlyZero())
			{
				Current = Current.RotateAngleAxis(MaxStepDeg, Axis).GetSafeNormal();
				RemainingDeg = AngleBetweenDeg(Current, Target);
			}
		}
		else
		{
			Current = Target;
			RemainingDeg = 0.0f;
		}

		// 2) 상태 상한: 남은 각도가 상한보다 크면 상한만 남기고 나머지는 잘라낸다.
		//    상태가 바뀌어 상한이 줄어든 경우도 이 경로로 즉시 반영된다.
		const float MaxDeviationDeg = FMath::Max(0.0f, MuzzleMaxDeg);
		if (RemainingDeg > MaxDeviationDeg + UE_KINDA_SMALL_NUMBER)
		{
			const FVector Axis = FVector::CrossProduct(Current, Target).GetSafeNormal();
			if (Axis.IsNearlyZero())
			{
				return Target;
			}

			Current = Current.RotateAngleAxis(RemainingDeg - MaxDeviationDeg, Axis).GetSafeNormal();
		}

		return Current;
	}

	bool BreathAppliesTo(EWeaponState State)
	{
		switch (State)
		{
		case EWeaponState::Stand_Idle:
		case EWeaponState::Stand_Aim:
		case EWeaponState::Crouch_Idle:
		case EWeaponState::Crouch_Aim:
			return true;
		default:
			return false;
		}
	}

	float BreathSwayDeg(double TimeSeconds)
	{
		const FCombatTuning& Tuning = CombatConfig::Tuning();
		if (Tuning.BreathPeriodSeconds <= 0.0f)
		{
			return 0.0f;
		}

		const double Phase = 2.0 * static_cast<double>(PI) * TimeSeconds / static_cast<double>(Tuning.BreathPeriodSeconds);
		return Tuning.BreathAmplitudeDeg * static_cast<float>(FMath::Sin(Phase));
	}

	float JumpImpulseDeg(float ElapsedSeconds)
	{
		const FCombatTuning& Tuning = CombatConfig::Tuning();

		if (ElapsedSeconds <= 0.0f)
		{
			return Tuning.JumpImpulseDeg;
		}

		if (Tuning.JumpImpulseRecoverySeconds <= 0.0f)
		{
			return 0.0f;
		}

		const float Alpha = FMath::Clamp(ElapsedSeconds / Tuning.JumpImpulseRecoverySeconds, 0.0f, 1.0f);
		return Tuning.JumpImpulseDeg * (1.0f - Alpha);
	}

	float EffectiveMuzzleOffsetDeg(EWeaponState State, float PersistentDeg, double TimeSeconds, float JumpElapsedSeconds)
	{
		const FWeaponStateTuning& StateTuning = CombatConfig::StateTuning(State);
		const float BreathDeg = BreathAppliesTo(State) ? BreathSwayDeg(TimeSeconds) : 0.0f;

		const float Steady = FMath::Clamp(PersistentDeg + BreathDeg, -StateTuning.MuzzleMaxDeg, StateTuning.MuzzleMaxDeg);
		return Steady + JumpImpulseDeg(JumpElapsedSeconds);
	}

	/**
	 * 확산 각도(도)를 화면 픽셀 간격으로 바꿉다.
	 * HalfViewportWidthPx 는 뷰포트 폭의 절반, HalfFovDeg 는 수평 FOV 의 절반이다.
	 */
	float CrosshairGapPixels(float SpreadDeg, float HalfViewportWidthPx, float HalfFovDeg)
	{
		const float HalfFov = FMath::Max(HalfFovDeg, 1.0f);
		const float Spread = FMath::Clamp(SpreadDeg, 0.0f, 45.0f);
		const float Scale = FMath::Max(HalfViewportWidthPx, 0.0f) / FMath::Tan(FMath::DegreesToRadians(HalfFov));

		return FMath::Max(FMath::Tan(FMath::DegreesToRadians(Spread)) * Scale, 0.0f);
	}

	FRotator RecoilOffset(int32 ShotsSinceRecoveryStart, bool bAiming)
	{
		const FCombatTuning& Tuning = CombatConfig::Tuning();
		const int32 Shots = FMath::Max(0, ShotsSinceRecoveryStart);
		const float Scale = bAiming ? Tuning.AimRecoilScale : 1.0f;

		const float PitchDeg = Tuning.RecoilVerticalDegPerShot * Scale * static_cast<float>(Shots);

		// 수평 튐은 ±0.3도/발이지만 시그니처에 난수 스트림이 없으므로, 테스트할 수 있도록
		// 발사 수의 홀짝에 따라 부호를 교대시키는 결정적 패턴을 쓴다(좌우로 번갈아 튀는 느낌).
		const float Sign = (Shots % 2 == 0) ? -1.0f : 1.0f;
		const float YawDeg = Tuning.RecoilHorizontalDegPerShot * Scale * static_cast<float>(Shots) * Sign;

		return FRotator(PitchDeg, YawDeg, 0.0f);
	}
}
