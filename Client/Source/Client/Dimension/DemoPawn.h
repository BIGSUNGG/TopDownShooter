#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Pawn.h"
#include "DemoPawn.generated.h"

class UCameraComponent;
class UCapsuleComponent;
class UFloatingPawnMovement;
class USpringArmComponent;

/** 기술 데모용 스캐폴드 폰이며 정식 캐릭터가 아니다. */
UCLASS()
class CLIENT_API ADemoPawn : public APawn
{
	GENERATED_BODY()

public:
	ADemoPawn();

protected:
	virtual void SetupPlayerInputComponent(UInputComponent* PlayerInputComponent) override;

	/** 디지털 키라 축 바인딩을 쓸 수 없어 눌림 상태를 모아 Tick에서 이동한다. */
	void OnKeyPressed(FKey Key);
	void OnKeyReleased(FKey Key);
	void SetAxisState(FKey Key, bool bDown);
	virtual void Tick(float DeltaSeconds) override;

	bool bMoveForward = false;
	bool bMoveBack = false;
	bool bMoveRight = false;
	bool bMoveLeft = false;

	UPROPERTY(VisibleAnywhere, Category = "Demo")
	TObjectPtr<UCapsuleComponent> Capsule;

	UPROPERTY(VisibleAnywhere, Category = "Demo")
	TObjectPtr<USpringArmComponent> SpringArm;

	UPROPERTY(VisibleAnywhere, Category = "Demo")
	TObjectPtr<UCameraComponent> Camera;

	UPROPERTY(VisibleAnywhere, Category = "Demo")
	TObjectPtr<UFloatingPawnMovement> Movement;
};
