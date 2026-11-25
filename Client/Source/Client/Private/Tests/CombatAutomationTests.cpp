// Copyright Epic Games, Inc. All Rights Reserved.

#include "Misc/AutomationTest.h"

#if WITH_DEV_AUTOMATION_TESTS

#include "Combat/CombatConfig.h"
#include "Combat/CombatLog.h"
#include "Combat/CombatMath.h"
#include "UI/DelayedBar.h"
#include "HAL/FileManager.h"
#include "Misc/FileHelper.h"
#include "Misc/Paths.h"

/**
 * 전투 수치·곡선 자동 테스트.
 *
 * 월드도, UObject도 없이 도는 순수 계산 검증이다(headless: UnrealEditor-Cmd -nullrhi).
 * 상태 이름/순서는 기획 문서 표와 같고, Client.Combat.Tuning 은 문서 값을 그대로 고정하며
 * Document/50-worklog/combat/config.json 을 산출물로 남긴다.
 */
namespace CombatTest
{
	void CheckFloat(FAutomationTestBase& Test, const FString& Label, float Actual, float Expected, float Tolerance = 0.001f)
	{
		Test.TestNearlyEqual(*Label, Actual, Expected, Tolerance);
	}

	FRotator YawRotation(float YawDeg)
	{
		return FRotator(0.0f, YawDeg, 0.0f);
	}

	/** 문서 표를 그대로 옮긴 기대값. 코드 값과 어긋나면 빌드 단계에서 실패한다. */
	struct FExpectedState
	{
		const TCHAR* Name;
		float Base;
		float PerShot;
		float Max;
		float MuzzleMax;
		float Track;
		bool bCanFire;
	};

	const FExpectedState ExpectedStates[] = {
		{ TEXT("Stand_Idle"), 0.5f, 0.2f, 3.0f, 1.5f, 90.0f, true },
		{ TEXT("Stand_Walk"), 1.5f, 0.3f, 5.0f, 6.0f, 60.0f, true },
		{ TEXT("Stand_Run"), 4.0f, 0.4f, 8.0f, 12.0f, 35.0f, true },
		{ TEXT("Stand_Aim"), 0.1f, 0.15f, 1.5f, 0.3f, 120.0f, true },
		{ TEXT("Crouch_Idle"), 0.3f, 0.15f, 2.0f, 0.8f, 90.0f, true },
		{ TEXT("Crouch_Walk"), 1.0f, 0.25f, 3.5f, 3.0f, 60.0f, true },
		{ TEXT("Crouch_Aim"), 0.05f, 0.1f, 1.0f, 0.2f, 140.0f, true },
		{ TEXT("Jump_Fall"), 6.0f, 0.4f, 10.0f, 8.0f, 45.0f, true },
		{ TEXT("Roll"), 0.0f, 0.0f, 0.0f, 0.0f, 0.0f, false }
	};
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FCombatSpreadTest, "Client.Combat.Spread", EAutomationTestFlags_ApplicationContextMask | EAutomationTestFlags::ProductFilter)

bool FCombatSpreadTest::RunTest(const FString& Parameters)
{
	using namespace CombatTest;

	const FWeaponStateTuning& Idle = CombatConfig::StateTuning(EWeaponState::Stand_Idle);
	const FWeaponStateTuning& Aim = CombatConfig::StateTuning(EWeaponState::Stand_Aim);
	const FWeaponStateTuning& Run = CombatConfig::StateTuning(EWeaponState::Stand_Run);
	const FWeaponStateTuning& Roll = CombatConfig::StateTuning(EWeaponState::Roll);

	// 1발째는 기본 탄퍼짐, 이후 perShot 씩 누적되고 max 에서 멈춘다.
	CheckFloat(*this, TEXT("Stand_Idle 첫 발 = 기본 탄퍼짐"), CombatMath::SpreadForShot(Idle, 0), 0.5f);
	CheckFloat(*this, TEXT("Stand_Idle 두 번째 발 = 기본 + 1단계"), CombatMath::SpreadForShot(Idle, 1), 0.7f);
	CheckFloat(*this, TEXT("Stand_Idle 세 번째 발 = 기본 + 2단계"), CombatMath::SpreadForShot(Idle, 2), 0.9f);
	CheckFloat(*this, TEXT("Stand_Idle 누적 상한 = 3.0도"), CombatMath::SpreadForShot(Idle, 100), 3.0f);

	CheckFloat(*this, TEXT("Stand_Aim 첫 발 = 0.1도"), CombatMath::SpreadForShot(Aim, 0), 0.1f);
	CheckFloat(*this, TEXT("Stand_Aim 3번째 발 = 0.1 + 2*0.15"), CombatMath::SpreadForShot(Aim, 2), 0.4f);
	CheckFloat(*this, TEXT("Stand_Aim 상한 = 1.5도"), CombatMath::SpreadForShot(Aim, 50), 1.5f);

	CheckFloat(*this, TEXT("Stand_Run 첫 발 = 4.0도"), CombatMath::SpreadForShot(Run, 0), 4.0f);
	CheckFloat(*this, TEXT("Stand_Run 상한 = 8.0도"), CombatMath::SpreadForShot(Run, 50), 8.0f);

	CheckFloat(*this, TEXT("Roll 은 발사 불가라 탄퍼짐 0"), CombatMath::SpreadForShot(Roll, 5), 0.0f);
	TestFalse(TEXT("Roll 상태는 bCanFire=false"), Roll.bCanFire);

	// 감쇠: 0.2초마다 초과분이 e^-1 만큼 남는다(시간 상수 0.2초).
	const float Base = Idle.SpreadBaseDeg;
	const float Current = Idle.SpreadMaxDeg;
	CheckFloat(*this, TEXT("감쇠 0초는 값 유지"), CombatMath::SpreadDecay(Current, Base, 0.0f), Current);
	CheckFloat(*this, TEXT("감쇠 0.2초 뒤 초과분은 e^-1"), CombatMath::SpreadDecay(Current, Base, 0.2f), Base + (Current - Base) * FMath::Exp(-1.0f), 0.002f);
	CheckFloat(*this, TEXT("감쇠 후에도 기본 탄퍼짐 아래로 내려가지 않는다"), CombatMath::SpreadDecay(Base, Base, 1.0f), Base);
	CheckFloat(*this, TEXT("기본보다 작은 값은 기본으로 올라온다"), CombatMath::SpreadDecay(Base - 1.0f, Base, 1.0f), Base);
	TestTrue(TEXT("감쇠는 단조 감소한다"), CombatMath::SpreadDecay(Current, Base, 0.4f) < CombatMath::SpreadDecay(Current, Base, 0.2f));

	// 상태가 바뀌어 상한이 줄면 현재 탄퍼짐을 즉시 새 상한으로 자른다(런 → 아이들).
	const float Running = CombatMath::SpreadForShot(Run, 100);           // 8.0
	const float Truncated = FMath::Clamp(Running, Idle.SpreadBaseDeg, Idle.SpreadMaxDeg);
	CheckFloat(*this, TEXT("달리기 8도 → 서기 상한 3도로 즉시 절단"), Truncated, 3.0f);

	// ApplySpread: 0도면 조준 방향 그대로, 값이 있으면 원뿔 안에서만 흔들린다.
	FRandomStream Stream(20261124);
	const FRotator AimRotation = YawRotation(35.0f);
	const FRotator NoSpread = CombatMath::ApplySpread(AimRotation, 0.0f, Stream);
	CheckFloat(*this, TEXT("탄퍼짐 0도면 조준 방향 유지"), NoSpread.Yaw, AimRotation.Yaw);

	bool bAllWithinCone = true;
	float MaxObserved = 0.0f;
	for (int32 Index = 0; Index < 64; ++Index)
	{
		const FRotator SpreadRotation = CombatMath::ApplySpread(AimRotation, 2.0f, Stream);
		const float OffsetDeg = CombatMath::AngleBetweenDeg(AimRotation.Vector(), SpreadRotation.Vector());
		MaxObserved = FMath::Max(MaxObserved, OffsetDeg);
		if (OffsetDeg > 2.0f + 0.01f)
		{
			bAllWithinCone = false;
		}
	}
	TestTrue(TEXT("ApplySpread 는 탄퍼짐 각도 안에만 머문다"), bAllWithinCone);
	TestTrue(TEXT("ApplySpread 는 실제로 흩어진다(최대 관측값 > 0)"), MaxObserved > 0.1f);

	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FCombatMuzzleTest, "Client.Combat.Muzzle", EAutomationTestFlags_ApplicationContextMask | EAutomationTestFlags::ProductFilter)

bool FCombatMuzzleTest::RunTest(const FString& Parameters)
{
	using namespace CombatTest;

	const FVector Forward = FVector::ForwardVector;

	// 1) 추종 속도 제한: 90도/초 * 0.05초 = 4.5도만 움직인다.
	const FVector Lagging = YawRotation(10.0f).Vector();
	const FVector Tracked = CombatMath::MuzzleTrack(Lagging, Forward, 90.0f, 90.0f, 0.05f);
	CheckFloat(*this, TEXT("추종은 90도/초 * 0.05초 = 4.5도 이동"), CombatMath::AngleBetweenDeg(Forward, Tracked), 5.5f, 0.01f);
	CheckFloat(*this, TEXT("이번 프레임 이동량은 4.5도"), CombatMath::AngleBetweenDeg(Lagging, Tracked), 4.5f, 0.01f);

	// 2) 충분한 시간이면 조준 방향에 붙는다.
	const FVector TrackedFull = CombatMath::MuzzleTrack(Lagging, Forward, 90.0f, 90.0f, 1.0f);
	CheckFloat(*this, TEXT("충분한 시간 뒤에는 조준 방향과 일치"), CombatMath::AngleBetweenDeg(Forward, TrackedFull), 0.0f, 0.01f);

	// 3) 상태 상한: 30도를 벗어난 총구는 상한 5도까지만 남는다(상태 변경 시 즉시 절단).
	const FVector FarLagging = YawRotation(30.0f).Vector();
	const FVector Capped = CombatMath::MuzzleTrack(FarLagging, Forward, 0.0f, 5.0f, 0.016f);
	CheckFloat(*this, TEXT("상한 5도를 넘는 벌어짐은 5도로 잘린다"), CombatMath::AngleBetweenDeg(Forward, Capped), 5.0f, 0.01f);

	// 4) 상한 0도(구르기)면 총구는 조준 방향과 같다.
	const FVector ZeroCap = CombatMath::MuzzleTrack(FarLagging, Forward, 0.0f, 0.0f, 0.016f);
	CheckFloat(*this, TEXT("상한 0도면 즉시 조준 방향"), CombatMath::AngleBetweenDeg(Forward, ZeroCap), 0.0f, 0.01f);

	// 5) 추종 0 + 상한 90도면 그대로 둔다.
	const FVector Frozen = CombatMath::MuzzleTrack(FarLagging, Forward, 0.0f, 90.0f, 0.016f);
	CheckFloat(*this, TEXT("추종 0도/초면 총구가 움직이지 않는다"), CombatMath::AngleBetweenDeg(FarLagging, Frozen), 0.0f, 0.01f);

	// 6) 상태 상한이 작은 상태(조준)에서는 큰 벌어짐이 즉시 0.3도로 줄어든다.
	const FWeaponStateTuning& AimState = CombatConfig::StateTuning(EWeaponState::Stand_Aim);
	const FVector AimCapped = CombatMath::MuzzleTrack(YawRotation(20.0f).Vector(), Forward, 0.0f, AimState.MuzzleMaxDeg, 0.016f);
	CheckFloat(*this, TEXT("조준 상태 상한 0.3도"), CombatMath::AngleBetweenDeg(Forward, AimCapped), AimState.MuzzleMaxDeg, 0.01f);

	// 7) 점프 충격은 상태 상한 위에 더해진다.
	const float IdleCap = CombatConfig::StateTuning(EWeaponState::Stand_Idle).MuzzleMaxDeg;
	const float WithJump = CombatMath::EffectiveMuzzleOffsetDeg(EWeaponState::Stand_Idle, 10.0f, 0.0, 0.0f);
	CheckFloat(*this, TEXT("점프 직후 오프셋 = 상한 + 점프 충격"), WithJump, IdleCap + CombatConfig::Tuning().JumpImpulseDeg, 0.01f);

	// 8) 지속 벌어짐은 상한으로 잘린다(호흡은 t=0 에서 0).
	const float ClampedOffset = CombatMath::EffectiveMuzzleOffsetDeg(EWeaponState::Stand_Idle, 10.0f, 0.0, 1000.0f);
	CheckFloat(*this, TEXT("지속 벌어짐은 상태 상한으로 잘린다"), ClampedOffset, IdleCap, 0.01f);

	// 9) 호흡은 서기·앉기의 대기/조준 4개 상태에만 더해진다.
	TestTrue(TEXT("Stand_Idle 에 호흡 적용"), CombatMath::BreathAppliesTo(EWeaponState::Stand_Idle));
	TestTrue(TEXT("Stand_Aim 에 호흡 적용"), CombatMath::BreathAppliesTo(EWeaponState::Stand_Aim));
	TestTrue(TEXT("Crouch_Idle 에 호흡 적용"), CombatMath::BreathAppliesTo(EWeaponState::Crouch_Idle));
	TestTrue(TEXT("Crouch_Aim 에 호흡 적용"), CombatMath::BreathAppliesTo(EWeaponState::Crouch_Aim));
	TestFalse(TEXT("Stand_Walk 에는 호흡 미적용"), CombatMath::BreathAppliesTo(EWeaponState::Stand_Walk));
	TestFalse(TEXT("Jump_Fall 에는 호흡 미적용"), CombatMath::BreathAppliesTo(EWeaponState::Jump_Fall));
	TestFalse(TEXT("Roll 에는 호흡 미적용"), CombatMath::BreathAppliesTo(EWeaponState::Roll));

	CheckFloat(*this, TEXT("Stand_Walk 는 호흡이 더해지지 않는다"), CombatMath::EffectiveMuzzleOffsetDeg(EWeaponState::Stand_Walk, 0.0f, 1.0, 1000.0f), 0.0f, 0.01f);
	CheckFloat(*this, TEXT("Roll 은 상한 0이라 지속 벌어짐이 사라진다"), CombatMath::EffectiveMuzzleOffsetDeg(EWeaponState::Roll, 12.0f, 0.0, 1000.0f), 0.0f, 0.01f);

	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FCombatBreathTest, "Client.Combat.Breath", EAutomationTestFlags_ApplicationContextMask | EAutomationTestFlags::ProductFilter)

bool FCombatBreathTest::RunTest(const FString& Parameters)
{
	using namespace CombatTest;

	const float Amplitude = CombatConfig::Tuning().BreathAmplitudeDeg;
	const float Period = CombatConfig::Tuning().BreathPeriodSeconds;

	CheckFloat(*this, TEXT("호흡 진폭 = 0.4도"), Amplitude, 0.4f);
	CheckFloat(*this, TEXT("호흡 주기 = 4초"), Period, 4.0f);

	CheckFloat(*this, TEXT("t=0 에서 호흡 0"), CombatMath::BreathSwayDeg(0.0), 0.0f);
	CheckFloat(*this, TEXT("t=1초(1/4 주기)에서 최대 진폭"), CombatMath::BreathSwayDeg(1.0), Amplitude);
	CheckFloat(*this, TEXT("t=2초(반 주기)에서 0"), CombatMath::BreathSwayDeg(2.0), 0.0f);
	CheckFloat(*this, TEXT("t=3초에서 최소 진폭"), CombatMath::BreathSwayDeg(3.0), -Amplitude);
	CheckFloat(*this, TEXT("t=4초에 한 주기가 끝난다"), CombatMath::BreathSwayDeg(4.0), 0.0f);
	CheckFloat(*this, TEXT("t=5초는 다음 주기의 최대"), CombatMath::BreathSwayDeg(5.0), Amplitude);
	TestTrue(TEXT("호흡은 진폭을 넘지 않는다"), FMath::Abs(CombatMath::BreathSwayDeg(0.7)) <= Amplitude + 0.0001f);

	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FCombatJumpImpulseTest, "Client.Combat.JumpImpulse", EAutomationTestFlags_ApplicationContextMask | EAutomationTestFlags::ProductFilter)

bool FCombatJumpImpulseTest::RunTest(const FString& Parameters)
{
	using namespace CombatTest;

	const float Peak = CombatConfig::Tuning().JumpImpulseDeg;
	const float Recovery = CombatConfig::Tuning().JumpImpulseRecoverySeconds;

	CheckFloat(*this, TEXT("점프 충격 최대 = 25도"), Peak, 25.0f);
	CheckFloat(*this, TEXT("점프 충격 회복 시간 = 2초"), Recovery, 2.0f);

	CheckFloat(*this, TEXT("점프 시작 순간 25도"), CombatMath::JumpImpulseDeg(0.0f), 25.0f);
	CheckFloat(*this, TEXT("1초 뒤 절반"), CombatMath::JumpImpulseDeg(1.0f), 12.5f);
	CheckFloat(*this, TEXT("2초 뒤 0"), CombatMath::JumpImpulseDeg(2.0f), 0.0f);
	CheckFloat(*this, TEXT("2초 이후에도 0"), CombatMath::JumpImpulseDeg(5.0f), 0.0f);
	TestTrue(TEXT("점프 충격은 선형으로 줄어든다"), CombatMath::JumpImpulseDeg(0.5f) > CombatMath::JumpImpulseDeg(1.5f));
	TestTrue(TEXT("점프 충격은 음수가 되지 않는다"), CombatMath::JumpImpulseDeg(10.0f) >= 0.0f);

	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FCombatRecoilTest, "Client.Combat.Recoil", EAutomationTestFlags_ApplicationContextMask | EAutomationTestFlags::ProductFilter)

bool FCombatRecoilTest::RunTest(const FString& Parameters)
{
	using namespace CombatTest;

	const FCombatTuning& Tuning = CombatConfig::Tuning();
	CheckFloat(*this, TEXT("반동 수직 = 0.6도/발"), Tuning.RecoilVerticalDegPerShot, 0.6f);
	CheckFloat(*this, TEXT("반동 수평 = 0.3도/발"), Tuning.RecoilHorizontalDegPerShot, 0.3f);
	CheckFloat(*this, TEXT("반동 회복 = 0.12초"), Tuning.RecoilRecoverySeconds, 0.12f);
	CheckFloat(*this, TEXT("카메라 킥 = -1.2도"), Tuning.CameraKickDeg, -1.2f);
	CheckFloat(*this, TEXT("카메라 킥 복귀 = 0.1초"), Tuning.CameraKickSeconds, 0.1f);
	CheckFloat(*this, TEXT("조준 중 반동 배율 = 0.5"), Tuning.AimRecoilScale, 0.5f);

	const FRotator First = CombatMath::RecoilOffset(1, false);
	CheckFloat(*this, TEXT("1발째 수직 반동 0.6도"), First.Pitch, 0.6f);
	CheckFloat(*this, TEXT("1발째 수평 반동 +0.3도"), First.Yaw, 0.3f);

	const FRotator Second = CombatMath::RecoilOffset(2, false);
	CheckFloat(*this, TEXT("2발째 수직 반동 1.2도"), Second.Pitch, 1.2f);
	CheckFloat(*this, TEXT("2발째 수평 반동은 반대쪽(-0.6도)"), Second.Yaw, -0.6f);

	const FRotator Aimed = CombatMath::RecoilOffset(1, true);
	CheckFloat(*this, TEXT("조준 중 수직 반동은 절반"), Aimed.Pitch, 0.3f);
	CheckFloat(*this, TEXT("조준 중 수평 반동은 절반"), Aimed.Yaw, 0.15f);

	const FRotator Recovered = CombatMath::RecoilOffset(0, false);
	CheckFloat(*this, TEXT("회복 후 수직 반동 0"), Recovered.Pitch, 0.0f);
	CheckFloat(*this, TEXT("회복 후 수평 반동 0"), Recovered.Yaw, 0.0f);

	TestTrue(TEXT("수평 반동은 ±0.3도를 넘지 않는다"), FMath::Abs(CombatMath::RecoilOffset(1, false).Yaw) <= 0.3f + 0.0001f);

	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FCombatTuningTest, "Client.Combat.Tuning", EAutomationTestFlags_ApplicationContextMask | EAutomationTestFlags::ProductFilter)

bool FCombatTuningTest::RunTest(const FString& Parameters)
{
	using namespace CombatTest;

	// 1) 상태는 9개, 열거형 순서 그대로여야 한다.
	const TArray<FWeaponStateTuning> States = CombatConfig::AllStates();
	TestEqual(TEXT("상태 개수는 9개"), States.Num(), 9);
	TestEqual(TEXT("기대 표의 항목 수도 9개"), static_cast<int32>(UE_ARRAY_COUNT(ExpectedStates)), 9);

	if (States.Num() != 9)
	{
		return false;
	}

	for (int32 Index = 0; Index < States.Num(); ++Index)
	{
		const FExpectedState& Expected = ExpectedStates[Index];
		const FWeaponStateTuning& Actual = States[Index];
		const FString Prefix = FString::Printf(TEXT("[%d] %s"), Index, Expected.Name);

		TestEqual(*FString::Printf(TEXT("%s 상태 이름"), *Prefix), FString(CombatConfig::StateName(static_cast<EWeaponState>(Index))), FString(Expected.Name));
		TestEqual(*FString::Printf(TEXT("%s 열거형 값"), *Prefix), static_cast<int32>(static_cast<EWeaponState>(Index)), Index);

		CheckFloat(*this, *FString::Printf(TEXT("%s 기본 탄퍼짐"), *Prefix), Actual.SpreadBaseDeg, Expected.Base);
		CheckFloat(*this, *FString::Printf(TEXT("%s 1발당 탄퍼짐"), *Prefix), Actual.SpreadPerShotDeg, Expected.PerShot);
		CheckFloat(*this, *FString::Printf(TEXT("%s 최대 탄퍼짐"), *Prefix), Actual.SpreadMaxDeg, Expected.Max);
		CheckFloat(*this, *FString::Printf(TEXT("%s 최대 벌어짐"), *Prefix), Actual.MuzzleMaxDeg, Expected.MuzzleMax);
		CheckFloat(*this, *FString::Printf(TEXT("%s 추종 속도"), *Prefix), Actual.MuzzleTrackDegPerSec, Expected.Track);
		TestTrue(*FString::Printf(TEXT("%s 발사 가능 여부"), *Prefix), Actual.bCanFire == Expected.bCanFire);

		TestTrue(*FString::Printf(TEXT("%s 상한은 기본 이상이다"), *Prefix), Actual.SpreadMaxDeg >= Actual.SpreadBaseDeg);
	}

	// 이름 역조회도 같은 순서를 따라야 한다.
	for (int32 Index = 0; Index < 9; ++Index)
	{
		EWeaponState Parsed = EWeaponState::Roll;
		const bool bParsed = CombatConfig::TryParseStateName(ExpectedStates[Index].Name, Parsed);
		TestTrue(*FString::Printf(TEXT("%s 이름 역조회"), ExpectedStates[Index].Name), bParsed);
		TestEqual(*FString::Printf(TEXT("%s 역조회 결과"), ExpectedStates[Index].Name), static_cast<int32>(Parsed), Index);
	}

	// 2) 나머지 문서 수치도 고정한다.
	const FCombatTuning& Tuning = CombatConfig::Tuning();
	CheckFloat(*this, TEXT("최대 체력 100"), Tuning.MaxHealth, 100.0f);
	CheckFloat(*this, TEXT("피스톨 피해 20"), Tuning.PistolDamage, 20.0f);
	TestEqual(TEXT("탄창 12발"), Tuning.MagazineSize, 12);
	TestEqual(TEXT("예비 탄약 60발"), Tuning.ReserveAmmo, 60);
	CheckFloat(*this, TEXT("연사 간격 0.15초"), Tuning.FireIntervalSeconds, 0.15f);
	CheckFloat(*this, TEXT("재장전 1.2초"), Tuning.ReloadSeconds, 1.2f);
	CheckFloat(*this, TEXT("사거리 3000"), Tuning.WeaponRangeUU, 3000.0f);
	CheckFloat(*this, TEXT("투사체 속도 8000"), Tuning.ProjectileSpeedUUPerSec, 8000.0f);
	CheckFloat(*this, TEXT("달리기 600"), Tuning.SpeedRunUU, 600.0f);
	CheckFloat(*this, TEXT("서기 400"), Tuning.SpeedStandUU, 400.0f);
	CheckFloat(*this, TEXT("앉기 200"), Tuning.SpeedCrouchUU, 200.0f);
	CheckFloat(*this, TEXT("조준 250"), Tuning.SpeedAimUU, 250.0f);
	CheckFloat(*this, TEXT("앉아 조준 130"), Tuning.SpeedCrouchAimUU, 130.0f);
	CheckFloat(*this, TEXT("구르기 900"), Tuning.RollSpeedUU, 900.0f);
	CheckFloat(*this, TEXT("구르기 지속 0.45초"), Tuning.RollSeconds, 0.45f);
	CheckFloat(*this, TEXT("구르기 입력 창 0.25초"), Tuning.RollInputWindowSeconds, 0.25f);
	CheckFloat(*this, TEXT("점프 속도 620"), Tuning.JumpZVelocity, 620.0f);
	CheckFloat(*this, TEXT("중력 배율 1.5"), Tuning.GravityScale, 1.5f);
	CheckFloat(*this, TEXT("넉백 250"), Tuning.KnockbackSpeedUU, 250.0f);
	CheckFloat(*this, TEXT("넉백 지속 0.15초"), Tuning.KnockbackSeconds, 0.15f);
	CheckFloat(*this, TEXT("히트스톱 0.06초"), Tuning.HitStopSeconds, 0.06f);
	CheckFloat(*this, TEXT("히트스톱 배율 0.15"), Tuning.HitStopDilation, 0.15f);
	CheckFloat(*this, TEXT("피격 플래시 0.12초"), Tuning.HitFlashWhiteSeconds, 0.12f);
	CheckFloat(*this, TEXT("피격 복귀 0.25초"), Tuning.HitFlashReturnSeconds, 0.25f);
	CheckFloat(*this, TEXT("셰이크 진폭 1.5"), Tuning.ShakeAmplitude, 1.5f);
	CheckFloat(*this, TEXT("셰이크 0.12초"), Tuning.ShakeSeconds, 0.12f);
	CheckFloat(*this, TEXT("조준선 두께 2cm"), Tuning.AimLineThicknessUU, 2.0f);
	CheckFloat(*this, TEXT("플레이어 부활 3초"), Tuning.PlayerRespawnSeconds, 3.0f);

	// 3) config.json 산출물을 남긴다(테스트 출력이며 손으로 고치는 문서가 아니다).
	const FString Json = CombatConfig::ToJson();
	TestTrue(TEXT("JSON 은 { 로 시작한다"), Json.StartsWith(TEXT("{\n")));
	TestTrue(TEXT("JSON 은 개행으로 끝난다"), Json.EndsWith(TEXT("\n")));
	TestTrue(TEXT("JSON 첫 상태는 Stand_Idle"), Json.Contains(TEXT("\"state\":\"Stand_Idle\"")));
	TestTrue(TEXT("JSON 마지막 상태는 Roll"), Json.Contains(TEXT("\"state\":\"Roll\"")));
	TestTrue(TEXT("JSON 키 이름 baseDeg"), Json.Contains(TEXT("\"baseDeg\":")));
	TestTrue(TEXT("JSON 키 이름 speedDegPerSec"), Json.Contains(TEXT("\"speedDegPerSec\":")));

	int32 StateKeyCount = 0;
	int32 SearchFrom = 0;
	while (true)
	{
		const int32 Found = Json.Find(TEXT("\"state\":"), ESearchCase::CaseSensitive, ESearchDir::FromStart, SearchFrom);
		if (Found == INDEX_NONE)
		{
			break;
		}
		++StateKeyCount;
		SearchFrom = Found + 1;
	}
	TestEqual(TEXT("JSON 에 9개 상태가 들어간다"), StateKeyCount, 9);

	// 문서 루트의 Document/50-worklog/combat/config.json. FPaths::ProjectDir() 는
	// C:/Projects/TopDownShooter/Client/ 이므로 한 단계 위가 저장소 루트다.
	const FString JsonPath = FPaths::ConvertRelativePathToFull(FPaths::ProjectDir() / TEXT("../Document/50-worklog/combat/config.json"));
	const FString JsonDirectory = FPaths::GetPath(JsonPath);

	IFileManager::Get().MakeDirectory(*JsonDirectory, true);

	const bool bSaved = FFileHelper::SaveStringToFile(Json, *JsonPath, FFileHelper::EEncodingOptions::ForceUTF8WithoutBOM);
	TestTrue(*FString::Printf(TEXT("config.json 저장: %s"), *JsonPath), bSaved);
	TestTrue(*FString::Printf(TEXT("config.json 존재: %s"), *JsonPath), FPaths::FileExists(JsonPath));

	UE_LOG(LogCombat, Log, TEXT("COMBAT_CONFIG_JSON path=%s bytes=%d"), *JsonPath, Json.Len());

	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FCombatCrosshairTest, "Client.Combat.Crosshair", EAutomationTestFlags_ApplicationContextMask | EAutomationTestFlags::ProductFilter)

bool FCombatCrosshairTest::RunTest(const FString& Parameters)
{
	using namespace CombatTest;

	// 1920 폭 뷰포트(절반 960px), 수평 FOV 90도(절반 45도) 기준.
	CheckFloat(*this, TEXT("확산 0도면 간격 0px"), CombatMath::CrosshairGapPixels(0.0f, 960.0f, 45.0f), 0.0f);
	CheckFloat(*this, TEXT("확산 45도면 화면 절반 폭(960px)"), CombatMath::CrosshairGapPixels(45.0f, 960.0f, 45.0f), 960.0f, 0.5f);
	CheckFloat(*this, TEXT("확산 상한 45도로 자른다"), CombatMath::CrosshairGapPixels(80.0f, 960.0f, 45.0f), 960.0f, 0.5f);

	// 문서 표의 서기 대기 탄퍼짐 0.5도가 실제 픽셀 간격으로 바뀐다.
	CheckFloat(*this, TEXT("확산 0.5도 = 약 8.4px"), CombatMath::CrosshairGapPixels(0.5f, 960.0f, 45.0f), 8.4f, 0.1f);

	// 간격은 확산이 커질수록 단조 증가한다(달리기 8도 > 서기 3도 > 대기 0.5도).
	const FWeaponStateTuning& Idle = CombatConfig::StateTuning(EWeaponState::Stand_Idle);
	const FWeaponStateTuning& Run = CombatConfig::StateTuning(EWeaponState::Stand_Run);
	const float IdleGap = CombatMath::CrosshairGapPixels(Idle.SpreadBaseDeg, 960.0f, 45.0f);
	const float IdleMaxGap = CombatMath::CrosshairGapPixels(Idle.SpreadMaxDeg, 960.0f, 45.0f);
	const float RunMaxGap = CombatMath::CrosshairGapPixels(Run.SpreadMaxDeg, 960.0f, 45.0f);
	TestTrue(TEXT("사격 누적으로 간격이 벌어진다"), IdleMaxGap > IdleGap);
	TestTrue(TEXT("달리기 최대 간격이 가장 넓다"), RunMaxGap > IdleMaxGap);

	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FCombatDelayedBarTest, "Client.Combat.DelayedBar", EAutomationTestFlags_ApplicationContextMask | EAutomationTestFlags::ProductFilter)

bool FCombatDelayedBarTest::RunTest(const FString& Parameters)
{
	const FCombatTuning& Tuning = CombatConfig::Tuning();
	FCombatDelayedBar Bar;
	Bar.Reset(1.0f);

	// 피해를 맞으면 유지 시간 동안 지연 바가 움직이지 않는다.
	Bar.Report(0.0f, Tuning.DelayedBarHoldSeconds);
	TestNearlyEqual(TEXT("보고 직후 지연 바는 그대로"), Bar.Tick(0.1f, Tuning.DelayedBarCatchUpSeconds), 1.0f);
	TestNearlyEqual(TEXT("유지 시간이 끝나기 전까지 그대로"),
		Bar.Tick(Tuning.DelayedBarHoldSeconds * 0.5f, Tuning.DelayedBarCatchUpSeconds), 1.0f);
	Bar.Tick(Tuning.DelayedBarHoldSeconds, Tuning.DelayedBarCatchUpSeconds);  // 유지 시간 소진
	TestTrue(TEXT("유지 시간이 지나면 줄어든다"),
		Bar.Tick(Tuning.DelayedBarCatchUpSeconds, Tuning.DelayedBarCatchUpSeconds) < 0.5f);

	// 회복·리스폰은 지연 없이 즉시 따라온다.
	Bar.Reset(1.0f);
	Bar.Report(0.5f, Tuning.DelayedBarHoldSeconds);
	Bar.Tick(0.01f, Tuning.DelayedBarCatchUpSeconds);
	Bar.Report(1.0f, Tuning.DelayedBarHoldSeconds);
	TestNearlyEqual(TEXT("값이 늘면 지연 없이 즉시 반영"), Bar.Tick(0.0f, Tuning.DelayedBarCatchUpSeconds), 1.0f);

	return true;
}

#endif // WITH_DEV_AUTOMATION_TESTS
