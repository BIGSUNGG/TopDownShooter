#pragma once

#include "CoreMinimal.h"
#include "Character/CombatCharacter.h"
#include "PlayerCombatCharacter.generated.h"

class UCameraComponent;
class USpringArmComponent;
class UCombatHUDWidget;

/**
 * 플레이어 전투 캐릭터.
 *
 * 입력은 이 프로젝트의 기존 관례대로 C++ BindKey/BindAxis 로 묶는다(Enhanced Input 에셋 없음,
 * Client/Config 는 수정 금지). Q/E 차원 전환은 ADemoPlayerController 가 직접 받으므로 여기서
 * 가로채지 않는다.
 */
UCLASS()
class CLIENT_API APlayerCombatCharacter : public ACombatCharacter
{
	GENERATED_BODY()

public:
	APlayerCombatCharacter();

	virtual void Tick(float DeltaSeconds) override;
	virtual void BeginPlay() override;
	virtual void PossessedBy(AController* NewController) override;
	virtual void SetupPlayerInputComponent(UInputComponent* PlayerInputComponent) override;

	/** 문서: 셰이크는 전용 클래스/에셋 없이 코드로 처리한다(스프링암 오프셋). */
	virtual void PlayCameraShake() override;

	UFUNCTION(BlueprintPure, Category = "Combat|HUD")
	UCombatHUDWidget* GetHUDWidget() const { return HUDWidget; }

protected:
	void OnMoveKeyPressed(FKey Key);
	void OnMoveKeyReleased(FKey Key);
	void SetMoveKeyState(FKey Key, bool bDown);

	void OnFirePressed();
	void OnAimPressed();
	void OnAimReleased();
	void OnCrouchPressed();
	void OnJumpPressed();
	void OnSprintPressed();
	void OnSprintReleased();
	void OnReloadPressed();

	/** 마우스 커서 아래 월드 지점을 조준점으로 넘긴다. */
	void UpdateAimFromCursor();

	/** 플레이어 컨트롤러가 확정된 뒤 HUD 를 한 번만 만든다. */
	void CreateHUD();

	/** 스프링암 셰이크·카메라 킥을 계산해 적용한다. */
	void UpdateCameraEffects(float DeltaSeconds);

	/** WASD 눌림 상태를 월드 방향 이동 입력으로 바꾼다(카메라 요가 고정이라 월드 축과 같다). */
	void ApplyMoveInput();

	UPROPERTY(VisibleAnywhere, Category = "Camera")
	TObjectPtr<USpringArmComponent> SpringArm;

	UPROPERTY(VisibleAnywhere, Category = "Camera")
	TObjectPtr<UCameraComponent> Camera;

	/**
	 * HUD 위젯 클래스. 기본값은 /Game/Combat/UI/WBP_HUD 이고, 없으면 HUD 없이 동작한다.
	 * 배치·연출은 UCombatHUDWidget 이 처리하므로 이 클래스는 생성만 담당한다.
	 */
	UPROPERTY(EditDefaultsOnly, Category = "Combat|HUD")
	TSubclassOf<UCombatHUDWidget> HUDWidgetClass;

	UPROPERTY(Transient)
	TObjectPtr<UCombatHUDWidget> HUDWidget;

	FVector SpringArmBaseLocation = FVector::ZeroVector;
	FRotator SpringArmBaseRotation = FRotator::ZeroRotator;

	float ShakeRemainingSeconds = 0.0f;
	float CameraKickRemainingSeconds = 0.0f;

	bool bMoveForward = false;
	bool bMoveBack = false;
	bool bMoveRight = false;
	bool bMoveLeft = false;

	/** Shift 두 번 탭 판정용. 마지막 탭 시각(월드 초). */
	double LastSprintTapTime = -1000.0;
};
