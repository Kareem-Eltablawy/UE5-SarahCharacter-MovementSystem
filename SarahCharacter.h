#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Character.h"
#include "InputActionValue.h"
#include "Camera/CameraComponent.h"
#include "GameFramework/SpringArmComponent.h"
#include "EnhancedInput/Public/InputMappingContext.h"
#include "EnhancedInput/Public/InputAction.h"
#include "Animation/AnimSequence.h"
#include "SarahCharacter.generated.h"

UENUM(BlueprintType)
enum class ESarahMovementState : uint8
{
    Idle,
    Walk,
    Run
};

UCLASS()
class SARAH_API ASarahCharacter : public ACharacter
{
    GENERATED_BODY()

public:
    ASarahCharacter();

    // Camera Components
    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Camera")
    USpringArmComponent* CameraBoom;

    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Camera")
    UCameraComponent* FollowCamera;

    // Animation control functions
    UFUNCTION(BlueprintCallable, Category = "Sarah|Animation")
    bool PlayAnimationDirect(UAnimSequence* Animation);

    UFUNCTION(BlueprintCallable, Category = "Sarah|Animation")
    void StopAnimationDirect();

    // State query functions
    UFUNCTION(BlueprintPure, Category = "SarahFSM")
    bool SarahIsIdle() const { return CurrentState == ESarahMovementState::Idle; }

    UFUNCTION(BlueprintPure, Category = "SarahFSM")
    bool SarahIsWalking() const { return CurrentState == ESarahMovementState::Walk; }

    UFUNCTION(BlueprintPure, Category = "SarahFSM")
    bool SarahIsRunning() const { return CurrentState == ESarahMovementState::Run; }

    UFUNCTION(BlueprintCallable, Category = "Sarah|Movement")
    FString GetMovementDirectionName() const;

    // Movement state getters
    UFUNCTION(BlueprintPure, Category = "Sarah|Movement")
    FVector2D GetSarahMoveInput() const { return MoveInput; }

    UFUNCTION(BlueprintPure, Category = "Sarah|Movement")
    bool GetSarahIsSprinting() const { return bIsSprinting; }

    // Configurable properties
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Sarah|Movement")
    float ContinuousRotationSpeed = 8.0f;

    // Animation assets
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Sarah|Animation")
    UAnimSequence* IdleAnimation;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Sarah|Animation")
    UAnimSequence* WalkAnimation;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Sarah|Animation")
    UAnimSequence* RunAnimation;

protected:
    virtual void BeginPlay() override;
    virtual void Tick(float DeltaTime) override;
    virtual void SetupPlayerInputComponent(class UInputComponent* PlayerInputComponent) override;

    // Enhanced Input System assets
    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Input")
    UInputMappingContext* DefaultMappingContext;

    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Input")
    UInputAction* MoveAction;

    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Input")
    UInputAction* LookAction;

    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Input")
    UInputAction* SprintAction;

    // Movement configuration
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Movement")
    float WalkSpeed = 200.0f;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Movement")
    float RunSpeed = 600.0f;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Movement")
    float RotationInterpSpeed = 13.0f;

    // Camera control settings
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Camera")
    float BaseTurnRate = 45.0f;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Camera")
    float BaseLookUpRate = 45.0f;

private:
    // Core state machine
    ESarahMovementState CurrentState;
    ESarahMovementState PreviousState;

    // Input data
    FVector2D MoveInput;
    bool bIsSprinting = false;

    // Animation system
    UPROPERTY()
    UAnimSequence* CurrentAnimation;

    // Camera-relative movement
    float CurrentCameraYaw;
    float LockedCameraYaw;
    bool bUsingCameraRelativeMovement;

    // Continuous angle movement
    float CurrentMovementAngle;
    float TargetMovementAngle;
    bool bIsTransitioningAngle;

    // Input handlers
    void HandleMove(const FInputActionValue& Value);
    void HandleMoveStop(const FInputActionValue& Value);
    void HandleLook(const FInputActionValue& Value);
    void HandleStartSprint();
    void HandleStopSprint();

    // State machine operations
    void ChangeState(ESarahMovementState NewState);
    void UpdateStateMachine(float DeltaTime);

    // State lifecycle functions
    void EnterIdle();
    void UpdateIdle(float DeltaTime);
    void ExitIdle();
    void EnterWalk();
    void UpdateWalk(float DeltaTime);
    void ExitWalk();
    void EnterRun();
    void UpdateRun(float DeltaTime);
    void ExitRun();

    // Movement functions
    void UpdateMovement(float DeltaTime);
    FVector CalculateCameraRelativeDirection(float CameraYaw, FVector2D Input) const;
    bool StartCameraRelativeMovement();
    void StopCameraRelativeMovement();
    FVector GetMovementDirection() const;
    void UpdateCharacterRotation();
    bool HasMovementInput() const;

    // Continuous angle system
    float CalculateContinuousInputAngle() const;
    void UpdateContinuousMovementAngle(float DeltaTime);
    float FindShortestAnglePath(float CurrentAngle, float TargetAngle) const;

    // Animation functions
    bool PlayAnimationInternal(UAnimSequence* Animation);
    bool PlayAnimationWithSpeed(UAnimSequence* Animation, float Speed);
    void SetMovementSpeed(float Speed);

    // Camera functions
    void UpdateCameraRotationReference();
};
