#include "Sarah/SarahCharacter.h"
#include "Components/CapsuleComponent.h"
#include "GameFramework/CharacterMovementComponent.h"
#include "GameFramework/Controller.h"
#include "EnhancedInputComponent.h"
#include "EnhancedInputSubsystems.h"
#include "Engine/LocalPlayer.h"
#include "Camera/CameraComponent.h"
#include "GameFramework/SpringArmComponent.h"

ASarahCharacter::ASarahCharacter()
{
    PrimaryActorTick.bCanEverTick = true;

    // Initialize state
    CurrentState = ESarahMovementState::Idle;
    PreviousState = ESarahMovementState::Idle;

    // Initialize movement system state
    CurrentCameraYaw = 0.0f;
    LockedCameraYaw = 0.0f;
    bUsingCameraRelativeMovement = false;
    CurrentMovementAngle = 0.0f;
    TargetMovementAngle = 0.0f;
    bIsTransitioningAngle = false;

    // Initialize jump state
    bJumpStartCompleted = false;
    bIsFalling = false;
    JumpStartTime = 0.0f;
    JumpAnimationLength = 0.0f;
    PreviousZVelocity = 0.0f;

    // Initialize landing state
    LandingStartTime = 0.0f;
    LandingAnimationLength = 0.0f;
    bLandingAnimationCompleted = false;
    bLandingStateActive = false;

    // Setup character capsule collision
    GetCapsuleComponent()->InitCapsuleSize(42.f, 96.0f);

    // Load character mesh
    static ConstructorHelpers::FObjectFinder<USkeletalMesh> SkeletalMeshFinder(TEXT("/Game/Adventure_Pack/Characters/Sarah/Mesh/SK_Sarah"));
    if (SkeletalMeshFinder.Succeeded())
    {
        GetMesh()->SetSkeletalMesh(SkeletalMeshFinder.Object);
        GetMesh()->SetAnimInstanceClass(nullptr);
    }

    // Position mesh relative to capsule
    GetMesh()->SetRelativeLocation(FVector(0.0f, 0.0f, -90.0f));
    GetMesh()->SetRelativeRotation(FRotator(0.0f, -90.0f, 0.0f));

    // Create camera boom (spring arm)
    CameraBoom = CreateDefaultSubobject<USpringArmComponent>(TEXT("CameraBoom"));
    CameraBoom->SetupAttachment(RootComponent);
    CameraBoom->TargetArmLength = 400.0f;
    CameraBoom->bUsePawnControlRotation = true;
    CameraBoom->bInheritPitch = true;
    CameraBoom->bInheritYaw = true;
    CameraBoom->bInheritRoll = true;

    // Create follow camera
    FollowCamera = CreateDefaultSubobject<UCameraComponent>(TEXT("FollowCamera"));
    FollowCamera->SetupAttachment(CameraBoom, USpringArmComponent::SocketName);
    FollowCamera->bUsePawnControlRotation = false;

    // Configure character movement
    bUseControllerRotationPitch = false;
    bUseControllerRotationYaw = false;
    bUseControllerRotationRoll = false;

    if (GetCharacterMovement())
    {
        GetCharacterMovement()->bOrientRotationToMovement = true;
        GetCharacterMovement()->RotationRate = FRotator(0.0f, 540.0f, 0.0f);
        GetCharacterMovement()->JumpZVelocity = 300.f;
        GetCharacterMovement()->AirControl = 0.2f;
        GetCharacterMovement()->MaxWalkSpeed = WalkSpeed;
    }

    // Load input assets
    static ConstructorHelpers::FObjectFinder<UInputMappingContext> IMC_Finder(TEXT("/Game/Sarah/Inputs/IMC_Sarah"));
    if (IMC_Finder.Succeeded())
    {
        DefaultMappingContext = IMC_Finder.Object;
    }

    static ConstructorHelpers::FObjectFinder<UInputAction> MoveAction_Finder(TEXT("/Game/Sarah/Inputs/IA_Sarah_Move"));
    if (MoveAction_Finder.Succeeded())
    {
        MoveAction = MoveAction_Finder.Object;
    }

    static ConstructorHelpers::FObjectFinder<UInputAction> LookAction_Finder(TEXT("/Game/Sarah/Inputs/IA_Sarah_Look"));
    if (LookAction_Finder.Succeeded())
    {
        LookAction = LookAction_Finder.Object;
    }

    static ConstructorHelpers::FObjectFinder<UInputAction> SprintAction_Finder(TEXT("/Game/Sarah/Inputs/IA_Sarah_Sprint"));
    if (SprintAction_Finder.Succeeded())
    {
        SprintAction = SprintAction_Finder.Object;
    }

    static ConstructorHelpers::FObjectFinder<UInputAction> JumpAction_Finder(TEXT("/Game/Sarah/Inputs/IA_Sarah_Jump"));
    if (JumpAction_Finder.Succeeded())
    {
        JumpAction = JumpAction_Finder.Object;
    }

    // Load animation sequences
    static ConstructorHelpers::FObjectFinder<UAnimSequence> IdleAnimFinder(TEXT("/Game/Adventure_Pack/Characters/Sarah/Animation/AS_Sarah_MF_Idle"));
    if (IdleAnimFinder.Succeeded())
    {
        IdleAnimation = IdleAnimFinder.Object;
    }

    static ConstructorHelpers::FObjectFinder<UAnimSequence> WalkAnimFinder(TEXT("/Game/Adventure_Pack/Characters/Sarah/Animation/AS_Sarah_MF_Walk_Fwd"));
    if (WalkAnimFinder.Succeeded())
    {
        WalkAnimation = WalkAnimFinder.Object;
    }

    static ConstructorHelpers::FObjectFinder<UAnimSequence> RunAnimFinder(TEXT("/Game/Adventure_Pack/Characters/Sarah/Animation/AS_Sarah_MF_Run_Fwd"));
    if (RunAnimFinder.Succeeded())
    {
        RunAnimation = RunAnimFinder.Object;
    }

    static ConstructorHelpers::FObjectFinder<UAnimSequence> JumpStartAnimFinder(TEXT("/Game/Adventure_Pack/Characters/Sarah/Animation/AS_Sarah_MM_Jump"));
    if (JumpStartAnimFinder.Succeeded())
    {
        JumpStartAnimation = JumpStartAnimFinder.Object;
        if (JumpStartAnimation)
        {
            JumpAnimationLength = JumpStartAnimation->GetPlayLength();
        }
    }

    static ConstructorHelpers::FObjectFinder<UAnimSequence> JumpFallAnimFinder(TEXT("/Game/Adventure_Pack/Characters/Sarah/Animation/AS_Sarah_MM_Fall_Loop"));
    if (JumpFallAnimFinder.Succeeded())
    {
        JumpFallAnimation = JumpFallAnimFinder.Object;
    }

    static ConstructorHelpers::FObjectFinder<UAnimSequence> LandingAnimFinder(TEXT("/Game/Adventure_Pack/Characters/Sarah/Animation/AS_Sarah_MM_Land"));
    if (LandingAnimFinder.Succeeded())
    {
        LandingAnimation = LandingAnimFinder.Object;
        if (LandingAnimation)
        {
            LandingAnimationLength = LandingAnimation->GetPlayLength();
        }
    }
}

void ASarahCharacter::BeginPlay()
{
    Super::BeginPlay();

    // Setup Enhanced Input system
    if (APlayerController* PlayerController = Cast<APlayerController>(Controller))
    {
        if (UEnhancedInputLocalPlayerSubsystem* Subsystem = ULocalPlayer::GetSubsystem<UEnhancedInputLocalPlayerSubsystem>(PlayerController->GetLocalPlayer()))
        {
            if (DefaultMappingContext)
            {
                Subsystem->AddMappingContext(DefaultMappingContext, 0);
            }
        }
    }

    // Initialize movement systems with current camera state
    if (FollowCamera)
    {
        CurrentCameraYaw = FollowCamera->GetComponentRotation().Yaw;
        CurrentMovementAngle = CurrentCameraYaw;
        TargetMovementAngle = CurrentCameraYaw;
    }

    // Start in idle state
    ChangeState(ESarahMovementState::Idle);
}

void ASarahCharacter::Tick(float DeltaTime)
{
    Super::Tick(DeltaTime);

    // Update all systems
    UpdateCameraRotationReference();
    UpdateMovement(DeltaTime);
    UpdateStateMachine(DeltaTime);
}

void ASarahCharacter::SetupPlayerInputComponent(UInputComponent* PlayerInputComponent)
{
    Super::SetupPlayerInputComponent(PlayerInputComponent);

    if (UEnhancedInputComponent* EnhancedInputComponent = Cast<UEnhancedInputComponent>(PlayerInputComponent))
    {
        // Bind movement actions
        if (MoveAction)
        {
            EnhancedInputComponent->BindAction(MoveAction, ETriggerEvent::Triggered, this, &ASarahCharacter::HandleMove);
            EnhancedInputComponent->BindAction(MoveAction, ETriggerEvent::Completed, this, &ASarahCharacter::HandleMoveStop);
        }

        // Bind look action
        if (LookAction)
        {
            EnhancedInputComponent->BindAction(LookAction, ETriggerEvent::Triggered, this, &ASarahCharacter::HandleLook);
        }

        // Bind sprint actions
        if (SprintAction)
        {
            EnhancedInputComponent->BindAction(SprintAction, ETriggerEvent::Started, this, &ASarahCharacter::HandleStartSprint);
            EnhancedInputComponent->BindAction(SprintAction, ETriggerEvent::Completed, this, &ASarahCharacter::HandleStopSprint);
        }

        // Bind jump action
        if (JumpAction)
        {
            EnhancedInputComponent->BindAction(JumpAction, ETriggerEvent::Started, this, &ASarahCharacter::HandleJump);
        }
    }
}

void ASarahCharacter::HandleMove(const FInputActionValue& Value)
{
    FVector2D RawInput = Value.Get<FVector2D>();
    MoveInput = RawInput;

    // Normalize input if magnitude exceeds 1.0 (gamepad circles)
    float InputMagnitude = MoveInput.Size();
    if (InputMagnitude > 1.0f)
    {
        MoveInput = MoveInput.GetSafeNormal();
    }
}

void ASarahCharacter::HandleMoveStop(const FInputActionValue& Value)
{
    MoveInput = FVector2D::ZeroVector;
    bIsTransitioningAngle = false;
}

void ASarahCharacter::HandleLook(const FInputActionValue& Value)
{
    FVector2D LookAxisVector = Value.Get<FVector2D>();

    if (Controller != nullptr)
    {
        // Apply look input with frame rate independent scaling
        AddControllerYawInput(LookAxisVector.X * BaseTurnRate * GetWorld()->GetDeltaSeconds());
        AddControllerPitchInput(LookAxisVector.Y * BaseLookUpRate * GetWorld()->GetDeltaSeconds());

        // Clamp pitch to prevent over-rotation
        FRotator ControlRotation = GetControlRotation();
        ControlRotation.Pitch = FMath::Clamp(ControlRotation.Pitch, -89.0f, 89.0f);
        Controller->SetControlRotation(ControlRotation);
    }
}

void ASarahCharacter::HandleStartSprint()
{
    bIsSprinting = true;
}

void ASarahCharacter::HandleStopSprint()
{
    bIsSprinting = false;
}

void ASarahCharacter::HandleJump()
{
    // Only allow jumping from idle state for now
    if (CurrentState == ESarahMovementState::Idle && IsOnGround())
    {
        ChangeState(ESarahMovementState::Jump);
    }
}

void ASarahCharacter::ChangeState(ESarahMovementState NewState)
{
    if (CurrentState == NewState) return;

    // Execute exit logic for current state
    switch (CurrentState)
    {
    case ESarahMovementState::Idle: ExitIdle(); break;
    case ESarahMovementState::Walk: ExitWalk(); break;
    case ESarahMovementState::Run: ExitRun(); break;
    case ESarahMovementState::Jump: ExitJump(); break;
    case ESarahMovementState::Landing: ExitLanding(); break;
    }

    PreviousState = CurrentState;
    CurrentState = NewState;

    // Execute enter logic for new state
    switch (NewState)
    {
    case ESarahMovementState::Idle: EnterIdle(); break;
    case ESarahMovementState::Walk: EnterWalk(); break;
    case ESarahMovementState::Run: EnterRun(); break;
    case ESarahMovementState::Jump: EnterJump(); break;
    case ESarahMovementState::Landing: EnterLanding(); break;
    }
}

void ASarahCharacter::UpdateStateMachine(float DeltaTime)
{
    // Delegate to current state's update function
    switch (CurrentState)
    {
    case ESarahMovementState::Idle:
        UpdateIdle(DeltaTime);
        break;
    case ESarahMovementState::Walk:
        UpdateWalk(DeltaTime);
        break;
    case ESarahMovementState::Run:
        UpdateRun(DeltaTime);
        break;
    case ESarahMovementState::Jump:
        UpdateJump(DeltaTime);
        break;
    case ESarahMovementState::Landing:
        UpdateLanding(DeltaTime);
        break;
    }
}

void ASarahCharacter::EnterIdle()
{
    SetMovementSpeed(0.0f);

    if (IdleAnimation)
    {
        PlayAnimationInternal(IdleAnimation);
    }
}

void ASarahCharacter::UpdateIdle(float DeltaTime)
{
    if (HasMovementInput())
    {
        if (bIsSprinting)
        {
            ChangeState(ESarahMovementState::Run);
        }
        else
        {
            ChangeState(ESarahMovementState::Walk);
        }
    }
}

void ASarahCharacter::ExitIdle()
{
    // Cleanup if needed
}

void ASarahCharacter::EnterWalk()
{
    SetMovementSpeed(WalkSpeed);
    PlayAnimationInternal(WalkAnimation);
}

void ASarahCharacter::UpdateWalk(float DeltaTime)
{
    if (!HasMovementInput())
    {
        ChangeState(ESarahMovementState::Idle);
    }
    else if (bIsSprinting)
    {
        ChangeState(ESarahMovementState::Run);
    }
}

void ASarahCharacter::ExitWalk()
{
    // Cleanup if needed
}

void ASarahCharacter::EnterRun()
{
    SetMovementSpeed(RunSpeed);
    PlayAnimationInternal(RunAnimation);
}

void ASarahCharacter::UpdateRun(float DeltaTime)
{
    if (!HasMovementInput())
    {
        ChangeState(ESarahMovementState::Idle);
    }
    else if (!bIsSprinting)
    {
        ChangeState(ESarahMovementState::Walk);
    }
}

void ASarahCharacter::ExitRun()
{
    // Cleanup if needed
}

void ASarahCharacter::EnterJump()
{
    // Reset jump state variables
    bJumpStartCompleted = false;
    bIsFalling = false;
    JumpStartTime = GetWorld()->GetTimeSeconds();
    PreviousZVelocity = 0.0f;

    // Play jump start animation
    if (JumpStartAnimation)
    {
        PlayAnimationInternal(JumpStartAnimation);
    }

    // Trigger the actual jump physics
    if (GetCharacterMovement())
    {
        GetCharacterMovement()->SetMovementMode(MOVE_Falling);
        GetCharacterMovement()->Velocity.Z = FMath::Max(GetCharacterMovement()->Velocity.Z, GetCharacterMovement()->JumpZVelocity);
        PreviousZVelocity = GetCharacterMovement()->Velocity.Z;
    }
}

void ASarahCharacter::UpdateJump(float DeltaTime)
{
    if (!GetCharacterMovement()) return;

    float CurrentZVelocity = GetCharacterMovement()->Velocity.Z;

    // Check if we've reached the highest point (when Z velocity changes from positive to negative)
    if (!bJumpStartCompleted && PreviousZVelocity > 0 && CurrentZVelocity <= 0)
    {
        bJumpStartCompleted = true;
        // Switch to fall animation at the apex
        if (JumpFallAnimation)
        {
            PlayAnimationInternal(JumpFallAnimation);
        }
    }

    // Update previous velocity for next frame comparison
    PreviousZVelocity = CurrentZVelocity;

    // Fallback: if we don't detect the apex via velocity, use time-based transition after 1 second
    if (!bJumpStartCompleted && (GetWorld()->GetTimeSeconds() - JumpStartTime) > 1.0f)
    {
        bJumpStartCompleted = true;
        if (JumpFallAnimation)
        {
            PlayAnimationInternal(JumpFallAnimation);
        }
    }

    // Check if we're falling (for state tracking)
    if (GetCharacterMovement()->IsFalling())
    {
        bIsFalling = true;
    }

    // When we detect ground contact, transition to LANDING state
    if (bIsFalling && IsOnGround())
    {
        ChangeState(ESarahMovementState::Landing);
    }
}

void ASarahCharacter::ExitJump()
{
    // Reset any jump-specific state
    bJumpStartCompleted = false;
    bIsFalling = false;
    PreviousZVelocity = 0.0f;
}

void ASarahCharacter::EnterLanding()
{
    // Reset landing state
    bLandingAnimationCompleted = false;
    bLandingStateActive = true;
    LandingStartTime = GetWorld()->GetTimeSeconds();

    // IMPORTANT: Stop ALL movement during landing to prevent state conflicts
    SetMovementSpeed(0.0f);

    // Stop any current animation immediately
    if (GetMesh())
    {
        GetMesh()->Stop();
    }

    // Play the landing animation
    if (LandingAnimation)
    {
        PlayAnimationInternal(LandingAnimation);
    }
    else
    {
        // If no landing animation, go directly to idle
        ChangeState(ESarahMovementState::Idle);
    }
}

void ASarahCharacter::UpdateLanding(float DeltaTime)
{
    // Check if landing animation has completed
    if (LandingAnimation && !bLandingAnimationCompleted && bLandingStateActive)
    {
        float CurrentTime = GetWorld()->GetTimeSeconds();
        float TimeSinceLandingStart = CurrentTime - LandingStartTime;

        // Use a slightly shorter time to ensure we transition BEFORE the animation ends
        float TransitionTime = LandingAnimationLength - 0.05f; // 50ms buffer

        if (TimeSinceLandingStart >= TransitionTime)
        {
            // Mark animation as completed
            bLandingAnimationCompleted = true;
            bLandingStateActive = false;

            // Stop the landing animation and immediately go to idle
            if (GetMesh())
            {
                GetMesh()->Stop();
            }

            // Transition to idle immediately
            ChangeState(ESarahMovementState::Idle);
        }
    }
}

void ASarahCharacter::ExitLanding()
{
    // Reset landing state variables
    bLandingAnimationCompleted = false;
    bLandingStateActive = false;
}

void ASarahCharacter::UpdateMovement(float DeltaTime)
{
    // Don't process normal movement during jump or landing states
    if (CurrentState == ESarahMovementState::Jump || CurrentState == ESarahMovementState::Landing)
    {
        return;
    }

    if (HasMovementInput())
    {
        // Start camera-relative movement system when first moving from idle
        if (CurrentState == ESarahMovementState::Idle)
        {
            StartCameraRelativeMovement();
        }

        // Update continuous angle interpolation
        UpdateContinuousMovementAngle(DeltaTime);

        FVector MovementDirection;
        if (bUsingCameraRelativeMovement)
        {
            // Use interpolated angle for smooth directional changes
            float MovementAngleRad = FMath::DegreesToRadians(CurrentMovementAngle);
            MovementDirection = FVector(
                FMath::Cos(MovementAngleRad),
                FMath::Sin(MovementAngleRad),
                0.0f
            );
        }
        else
        {
            // Standard camera-relative movement
            MovementDirection = CalculateCameraRelativeDirection(CurrentCameraYaw, MoveInput);
        }

        // Apply movement with input magnitude scaling
        float InputMagnitude = MoveInput.Size();
        float MovementIntensity = FMath::Clamp(InputMagnitude, 0.1f, 1.0f);
        AddMovementInput(MovementDirection, MovementIntensity);

        // Update character facing direction
        UpdateCharacterRotation();
    }
    else
    {
        // Stop camera-relative system when no input
        StopCameraRelativeMovement();
    }
}

void ASarahCharacter::UpdateCameraRotationReference()
{
    if (FollowCamera)
    {
        CurrentCameraYaw = FollowCamera->GetComponentRotation().Yaw;
    }
}

FVector ASarahCharacter::CalculateCameraRelativeDirection(float CameraYaw, FVector2D Input) const
{
    if (Input.IsNearlyZero())
        return FVector::ZeroVector;

    // Invert X axis for more intuitive camera-relative controls
    FVector2D InvertedInput = FVector2D(-Input.X, Input.Y);
    float InputMagnitude = InvertedInput.Size();

    // Convert input to world space direction
    float InputAngleRad = FMath::Atan2(InvertedInput.Y, InvertedInput.X);
    float CameraYawRad = FMath::DegreesToRadians(CameraYaw);

    // Adjust for UE coordinate system (forward is X, right is Y)
    float WorldAngleRad = InputAngleRad + CameraYawRad - FMath::DegreesToRadians(90.0f);

    FVector MovementDirection = FVector(
        FMath::Cos(WorldAngleRad) * InputMagnitude,
        FMath::Sin(WorldAngleRad) * InputMagnitude,
        0.0f
    );

    return MovementDirection.GetSafeNormal() * InputMagnitude;
}

bool ASarahCharacter::StartCameraRelativeMovement()
{
    // Only start camera-relative movement from idle state
    if (CurrentState == ESarahMovementState::Idle && !bUsingCameraRelativeMovement)
    {
        if (!FollowCamera) return false;

        LockedCameraYaw = CurrentCameraYaw;
        bUsingCameraRelativeMovement = true;
        CurrentMovementAngle = LockedCameraYaw;
        TargetMovementAngle = LockedCameraYaw;
        bIsTransitioningAngle = false;
        return true;
    }
    return false;
}

void ASarahCharacter::StopCameraRelativeMovement()
{
    if (bUsingCameraRelativeMovement)
    {
        bUsingCameraRelativeMovement = false;
        bIsTransitioningAngle = false;
    }
}

float ASarahCharacter::CalculateContinuousInputAngle() const
{
    if (!HasMovementInput()) return CurrentMovementAngle;

    // Convert input to screen-space angle
    FVector2D InvertedInput = FVector2D(-MoveInput.X, MoveInput.Y);
    float InputAngle = FMath::RadiansToDegrees(FMath::Atan2(InvertedInput.Y, InvertedInput.X));

    // Normalize to [0, 360) range
    if (InputAngle < 0) InputAngle += 360.0f;

    // Convert to world space angle
    float WorldAngle = InputAngle + LockedCameraYaw - 90.0f;

    // Normalize world angle
    while (WorldAngle >= 360.0f) WorldAngle -= 360.0f;
    while (WorldAngle < 0.0f) WorldAngle += 360.0f;

    return WorldAngle;
}

float ASarahCharacter::FindShortestAnglePath(float CurrentAngle, float TargetAngle) const
{
    float Difference = TargetAngle - CurrentAngle;

    // Normalize to [-180, 180] range for shortest path
    if (Difference > 180.0f) {
        Difference -= 360.0f;
    }
    else if (Difference < -180.0f) {
        Difference += 360.0f;
    }
    return Difference;
}

void ASarahCharacter::UpdateContinuousMovementAngle(float DeltaTime)
{
    if (!HasMovementInput()) return;

    // Calculate desired movement angle from input
    TargetMovementAngle = CalculateContinuousInputAngle();
    float AngleDifference = FindShortestAnglePath(CurrentMovementAngle, TargetMovementAngle);

    // Only interpolate if angle change is significant
    if (FMath::Abs(AngleDifference) > 0.4f)
    {
        bIsTransitioningAngle = true;

        // Dynamic rotation speed based on angle difference
        float TransitionSpeed = ContinuousRotationSpeed * (FMath::Abs(AngleDifference) / 180.0f) * 1.05f;

        // Apply rotation
        float AngleStep = AngleDifference * TransitionSpeed * DeltaTime;
        CurrentMovementAngle += AngleStep;

        // Keep angle in valid range
        while (CurrentMovementAngle >= 360.0f) CurrentMovementAngle -= 360.0f;
        while (CurrentMovementAngle < 0.0f) CurrentMovementAngle += 360.0f;

        // Snap to target when close enough
        if (FMath::Abs(FindShortestAnglePath(CurrentMovementAngle, TargetMovementAngle)) < 1.5f)
        {
            CurrentMovementAngle = TargetMovementAngle;
            bIsTransitioningAngle = false;
        }
    }
    else
    {
        // No significant change needed
        bIsTransitioningAngle = false;
        CurrentMovementAngle = TargetMovementAngle;
    }
}

FVector ASarahCharacter::GetMovementDirection() const
{
    if (!HasMovementInput())
        return FVector::ZeroVector;

    if (bUsingCameraRelativeMovement)
    {
        // Direction from interpolated angle
        float MovementAngleRad = FMath::DegreesToRadians(CurrentMovementAngle);
        return FVector(
            FMath::Cos(MovementAngleRad),
            FMath::Sin(MovementAngleRad),
            0.0f
        );
    }
    else
    {
        // Standard camera-relative direction
        return CalculateCameraRelativeDirection(CurrentCameraYaw, MoveInput);
    }
}

void ASarahCharacter::UpdateCharacterRotation()
{
    FVector MovementDirection = GetMovementDirection();
    if (!MovementDirection.IsNearlyZero())
    {
        // Smoothly interpolate to face movement direction
        FRotator TargetRotation = MovementDirection.Rotation();
        FRotator CurrentRotation = GetActorRotation();
        FRotator NewRotation = FMath::RInterpTo(
            CurrentRotation,
            TargetRotation,
            GetWorld()->GetDeltaSeconds(),
            RotationInterpSpeed
        );

        // Only rotate around Z axis (yaw)
        SetActorRotation(FRotator(0, NewRotation.Yaw, 0));
    }
}

bool ASarahCharacter::HasMovementInput() const
{
    return (FMath::Abs(MoveInput.X) > 0.01f || FMath::Abs(MoveInput.Y) > 0.01f);
}

bool ASarahCharacter::PlayAnimationInternal(UAnimSequence* Animation)
{
    if (!Animation || !GetMesh()) return false;

    // Stop current animation before playing new one
    if (CurrentAnimation) GetMesh()->Stop();

    GetMesh()->PlayAnimation(Animation, true);
    CurrentAnimation = Animation;
    GetMesh()->SetPlayRate(1.0f);
    return true;
}

bool ASarahCharacter::PlayAnimationWithSpeed(UAnimSequence* Animation, float Speed)
{
    if (!Animation || !GetMesh()) return false;

    if (CurrentAnimation) GetMesh()->Stop();

    GetMesh()->PlayAnimation(Animation, true);
    CurrentAnimation = Animation;
    GetMesh()->SetPlayRate(Speed);
    return true;
}

void ASarahCharacter::SetMovementSpeed(float Speed)
{
    if (GetCharacterMovement())
    {
        GetCharacterMovement()->MaxWalkSpeed = Speed;
    }
}

bool ASarahCharacter::IsOnGround() const
{
    if (!GetCharacterMovement()) return false;
    return GetCharacterMovement()->IsMovingOnGround();
}

bool ASarahCharacter::PlayAnimationDirect(UAnimSequence* Animation)
{
    return PlayAnimationInternal(Animation);
}

void ASarahCharacter::StopAnimationDirect()
{
    if (GetMesh())
    {
        GetMesh()->Stop();
        CurrentAnimation = nullptr;
    }
}

FString ASarahCharacter::GetMovementDirectionName() const
{
    if (!HasMovementInput()) return TEXT("None");
    return FString::Printf(TEXT("%.1f°"), CurrentMovementAngle);
}
