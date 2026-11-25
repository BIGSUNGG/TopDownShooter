#include "Combat/CombatConfig.h"

namespace
{
	/** 값은 Document/30-design/combat-system.md 와 character-movement.md 표에서 그대로 옮긴다. */
	const FCombatTuning& MakeTuning()
	{
		static const FCombatTuning Tuning = []()
		{
			FCombatTuning T;

			// 체력 (플레이어·더미 모두 100)
			T.MaxHealth = 100.0f;

			// 피스톨
			T.PistolDamage = 20.0f;
			T.MagazineSize = 12;
			T.ReserveAmmo = 60;
			T.FireIntervalSeconds = 0.15f;
			T.ReloadSeconds = 1.2f;
			T.WeaponRangeUU = 3000.0f;
			T.ProjectileSpeedUUPerSec = 8000.0f;

			// 반동
			T.RecoilVerticalDegPerShot = 0.6f;
			T.RecoilHorizontalDegPerShot = 0.3f;
			T.RecoilRecoverySeconds = 0.12f;
			T.CameraKickDeg = -1.2f;
			T.CameraKickSeconds = 0.1f;
			T.AimRecoilScale = 0.5f;

			// 호흡
			T.BreathPeriodSeconds = 4.0f;
			T.BreathAmplitudeDeg = 0.4f;

			// 점프 충격
			T.JumpImpulseDeg = 25.0f;
			T.JumpImpulseRecoverySeconds = 2.0f;
			T.JumpZVelocity = 620.0f;
			T.GravityScale = 1.5f;

			// 이동 (공중 이동 속도는 서기 속도와 같다)
			T.SpeedStandUU = 400.0f;
			T.SpeedRunUU = 600.0f;
			T.SpeedCrouchUU = 200.0f;
			T.SpeedAimUU = 250.0f;
			T.SpeedCrouchAimUU = 130.0f;
			T.RollSpeedUU = 900.0f;
			T.RollSeconds = 0.45f;
			T.RollInputWindowSeconds = 0.25f;
			T.AirControl = 0.35f;

			// 탄퍼짐 감소: 발사하지 않으면 0.2초당 1단계
			T.SpreadDecayStepSeconds = 0.2f;
			T.CapsuleRadius = 34.0f;
			T.CapsuleHalfHeightStand = 88.0f;
			T.CapsuleHalfHeightCrouch = 48.0f;
			T.CameraCrouchDrop = 40.0f;

			// 타격감
			T.HitFlashWhiteSeconds = 0.12f;
			T.HitFlashReturnSeconds = 0.25f;
			T.HitStopSeconds = 0.06f;
			T.HitStopDilation = 0.15f;
			T.ShakeAmplitude = 1.5f;
			T.ShakeSeconds = 0.12f;
			T.KnockbackSpeedUU = 250.0f;
			T.KnockbackSeconds = 0.15f;
			T.ImpactBurstSeconds = 0.3f;
			T.DelayedBarHoldSeconds = 0.5f;
			T.DelayedBarCatchUpSeconds = 2.5f;
			T.DamageNumberSeconds = 0.8f;
			T.LowHealthFlashSeconds = 0.1f;

			// 조준선 두께 2cm
			T.AimLineThicknessUU = 2.0f;

			// 사망
			T.PlayerRespawnSeconds = 3.0f;

			return T;
		}();

		return Tuning;
	}

	/** 열거형 순서대로 9개. 값은 위 표와 1:1이며 순서를 바꾸지 않는다. */
	const TArray<FWeaponStateTuning>& MakeStates()
	{
		static const TArray<FWeaponStateTuning> States = []()
		{
			TArray<FWeaponStateTuning> Out;
			Out.Reserve(CombatConfig::NumWeaponStates);

			auto Add = [&Out](float Base, float PerShot, float Max, float MuzzleMax, float Track, bool bCanFire)
			{
				FWeaponStateTuning S;
				S.SpreadBaseDeg = Base;
				S.SpreadPerShotDeg = PerShot;
				S.SpreadMaxDeg = Max;
				S.MuzzleMaxDeg = MuzzleMax;
				S.MuzzleTrackDegPerSec = Track;
				S.bCanFire = bCanFire;
				Out.Add(S);
			};

			//            base  perShot max   muzzleMax track  canFire
			Add(0.5f, 0.2f, 3.0f, 1.5f, 90.0f, true);   // Stand_Idle
			Add(1.5f, 0.3f, 5.0f, 6.0f, 60.0f, true);   // Stand_Walk
			Add(4.0f, 0.4f, 8.0f, 12.0f, 35.0f, true);  // Stand_Run
			Add(0.1f, 0.15f, 1.5f, 0.3f, 120.0f, true); // Stand_Aim
			Add(0.3f, 0.15f, 2.0f, 0.8f, 90.0f, true);  // Crouch_Idle
			Add(1.0f, 0.25f, 3.5f, 3.0f, 60.0f, true);  // Crouch_Walk
			Add(0.05f, 0.1f, 1.0f, 0.2f, 140.0f, true); // Crouch_Aim
			Add(6.0f, 0.4f, 10.0f, 8.0f, 45.0f, true);  // Jump_Fall
			Add(0.0f, 0.0f, 0.0f, 0.0f, 0.0f, false);   // Roll (발사 불가)

			return Out;
		}();

		return States;
	}

	const TCHAR* const StateNames[CombatConfig::NumWeaponStates] = {
		TEXT("Stand_Idle"),
		TEXT("Stand_Walk"),
		TEXT("Stand_Run"),
		TEXT("Stand_Aim"),
		TEXT("Crouch_Idle"),
		TEXT("Crouch_Walk"),
		TEXT("Crouch_Aim"),
		TEXT("Jump_Fall"),
		TEXT("Roll")
	};

	/**
	 * 2.0 -> "2.0", 0.05 -> "0.05", 90.0 -> "90.0".
	 * 소수점 아래 최소 한 자리를 남기므로 JSON 숫자가 항상 실수 형태로 보인다.
	 */
	FString FormatNumber(float Value)
	{
		FString Text = FString::Printf(TEXT("%.4f"), Value);

		int32 DotIndex = INDEX_NONE;
		if (Text.FindChar(TEXT('.'), DotIndex))
		{
			int32 Trim = Text.Len();
			while (Trim > DotIndex + 2 && Text[Trim - 1] == TEXT('0'))
			{
				--Trim;
			}
			Text.LeftInline(Trim);
		}

		return Text;
	}
}

namespace CombatConfig
{
	const FCombatTuning& Tuning()
	{
		return MakeTuning();
	}

	const FWeaponStateTuning& StateTuning(EWeaponState State)
	{
		const TArray<FWeaponStateTuning>& States = MakeStates();

		const int32 Index = static_cast<int32>(State);
		if (Index < 0 || Index >= States.Num())
		{
			return States[static_cast<int32>(EWeaponState::Stand_Idle)];
		}

		return States[Index];
	}

	TArray<FWeaponStateTuning> AllStates()
	{
		return MakeStates();
	}

	const TCHAR* StateName(EWeaponState State)
	{
		const int32 Index = static_cast<int32>(State);
		if (Index < 0 || Index >= NumWeaponStates)
		{
			return StateNames[0];
		}

		return StateNames[Index];
	}

	bool TryParseStateName(const FString& Name, EWeaponState& OutState)
	{
		for (int32 Index = 0; Index < NumWeaponStates; ++Index)
		{
			if (Name.Equals(StateNames[Index], ESearchCase::CaseSensitive))
			{
				OutState = static_cast<EWeaponState>(Index);
				return true;
			}
		}

		return false;
	}

	FString ToJson()
	{
		const TArray<FWeaponStateTuning> States = AllStates();

		FString Out;
		Out.Reserve(1024);
		Out += TEXT("{\n");
		Out += TEXT("  \"states\": [\n");

		for (int32 Index = 0; Index < States.Num(); ++Index)
		{
			const FWeaponStateTuning& S = States[Index];
			Out += FString::Printf(
				TEXT("    {\"state\":\"%s\",\"spread\":{\"baseDeg\":%s,\"perShotDeg\":%s,\"maxDeg\":%s},\"muzzle\":{\"maxDeg\":%s,\"speedDegPerSec\":%s}}%s\n"),
				StateName(static_cast<EWeaponState>(Index)),
				*FormatNumber(S.SpreadBaseDeg),
				*FormatNumber(S.SpreadPerShotDeg),
				*FormatNumber(S.SpreadMaxDeg),
				*FormatNumber(S.MuzzleMaxDeg),
				*FormatNumber(S.MuzzleTrackDegPerSec),
				Index + 1 < States.Num() ? TEXT(",") : TEXT(""));
		}

		Out += TEXT("  ]\n");
		Out += TEXT("}\n");

		return Out;
	}
}
