// Fill out your copyright notice in the Description page of Project Settings.

#include "MyCharacter.h"
#include "Components/InputComponent.h"
#include "Components/SphereComponent.h"
#include "Components/CapsuleComponent.h"
#include "Camera/CameraComponent.h"
#include "GameFramework/SpringArmComponent.h"
#include "GameFramework/CharacterMovementComponent.h"
#include "GameFramework/PlayerController.h"
#include "Engine/World.h"
#include "Kismet/KismetMathLibrary.h"
#include "DrawDebugHelpers.h"
#include "Blueprint/UserWidget.h"
#include "Components/WidgetComponent.h"
#include "EngineUtils.h"
#include "TimerManager.h"
#include "Components/SkeletalMeshComponent.h"
#include "UObject/UObjectGlobals.h"
#include "Engine/Engine.h"
#include "UObject/StructOnScope.h"
#include "LockOnConfigComponent.h" // Step 2.5: ���Ի��������
#include "Camera/CameraPipeline.h" // Week 3: 新相机管线
#include "Camera/CameraSystemAdapter.h" // Week 3: 相机系统适配器

// Sets default values
AMyCharacter::AMyCharacter()
{
	// Set this character to call Tick() every frame.  You can turn this off to improve performance if you don't need it.
	PrimaryActorTick.bCanEverTick = true;

	// ==================== ����Ŀ������� ====================
	TargetDetectionComponent = CreateDefaultSubobject<UTargetDetectionComponent>(TEXT("TargetDetectionComponent"));

	// ==================== �������������� ====================
	CameraControlComponent = CreateDefaultSubobject<UCameraControlComponent>(TEXT("CameraControlComponent"));

	// ==================== ����UI������� ====================
	UIManagerComponent = CreateDefaultSubobject<UUIManagerComponent>(TEXT("UIManagerComponent"));

	// ==================== �������������� ====================
	CameraDebugComponent = CreateDefaultSubobject<UCameraDebugComponent>(TEXT("CameraDebugComponent"));

	// ==================== 创建新相机管线组件（实验性） ====================
	CameraPipelineV2 = CreateDefaultSubobject<UCameraPipeline>(TEXT("CameraPipelineV2"));

	// ==================== ����Soul��Ϸϵͳ��� ====================
	PoiseComponent = CreateDefaultSubobject<UPoiseComponent>(TEXT("PoiseComponent"));
	StaminaComponent = CreateDefaultSubobject<UStaminaComponent>(TEXT("StaminaComponent"));
	DodgeComponent = CreateDefaultSubobject<UDodgeComponent>(TEXT("DodgeComponent"));
	ExecutionComponent = CreateDefaultSubobject<UExecutionComponent>(TEXT("ExecutionComponent"));

	// ==================== ������������������ ====================
	// ԭʼ����ע�ͱ������������ڼ��������ĵ���
	LockOnDetectionSphere = CreateDefaultSubobject<USphereComponent>(TEXT("LockOnDetectionSphere"));
	LockOnDetectionSphere->SetupAttachment(RootComponent);
	LockOnDetectionSphere->InitSphereRadius(LockOnRange);  // ʹ�����õ�������Χ
	// ������ײͨ��
	LockOnDetectionSphere->SetCollisionEnabled(ECollisionEnabled::QueryOnly);
	LockOnDetectionSphere->SetCollisionObjectType(ECollisionChannel::ECC_WorldDynamic);
	LockOnDetectionSphere->SetCollisionResponseToAllChannels(ECollisionResponse::ECR_Ignore);
	LockOnDetectionSphere->SetCollisionResponseToChannel(ECollisionChannel::ECC_Pawn, ECollisionResponse::ECR_Overlap);
	// �༭�����ӻ�����
	LockOnDetectionSphere->SetHiddenInGame(true);  // ��Ϸ������
	LockOnDetectionSphere->ShapeColor = FColor::Green;  // �༭������ʾΪ��ɫ

	// ==================== ????????? ====================
	bIsLockedOn = false;
	CurrentLockOnTarget = nullptr;
	LockOnRange = 2000.0f;  // ?????20???????????3??????
	LockOnAngle = 120.0f;   // ?????????????60?????????????
	NormalWalkSpeed = 600.0f;
	LockedWalkSpeed = 600.0f;
	ForwardInputValue = 0.0f;
	RightInputValue = 0.0f;
	LastFindTargetsTime = 0.0f;
	
	// ==================== ????????????????????????? ====================
	CameraInterpSpeed = 5.0f;           // ????????
	bEnableSmoothCameraTracking = true; // ??????????????
	CameraTrackingMode = 0;             // ????????????
	
	// ???��??????????????????eVector??????
	TargetLocationOffset = FVector(0.0f, 0.0f, -250.0f);

	// ==================== ??????????? ====================
	bEnableCameraDebugLogs = false;     // ????????????
	bEnableLockOnDebugLogs = false;     // ?????????????
	
	// ???????????
	bRightStickLeftPressed = false;
	bRightStickRightPressed = false;
	LastRightStickX = 0.0f;
	
	// ????��????????
	bJustSwitchedTarget = false;
	TargetSwitchCooldown = 0.5f;
	LastTargetSwitchTime = 0.0f;

	// ?????��????????����
	bIsSmoothSwitching = false;
	SmoothSwitchStartTime = 0.0f;
	SmoothSwitchStartRotation = FRotator::ZeroRotator;
	SmoothSwitchTargetRotation = FRotator::ZeroRotator;
	bShouldSmoothSwitchCamera = false;
	bShouldSmoothSwitchCharacter = false;

	// ??????????????
	bShouldCameraFollowTarget = true;
	bShouldCharacterRotateToTarget = true; // ?????????????????
	bPlayerIsMoving = false;

	// ???????????????
	bIsCameraAutoCorrection = false;
	CameraCorrectionStartTime = 0.0f;
	CameraCorrectionStartRotation = FRotator::ZeroRotator;
	CameraCorrectionTargetRotation = FRotator::ZeroRotator;
	DelayedCorrectionTarget = nullptr;

	// UMG???????
	LockOnWidgetClass = nullptr;
	LockOnWidgetInstance = nullptr;
	PreviousLockOnTarget = nullptr;

	// ??????��????
	bIsSmoothCameraReset = false;
	SmoothResetStartTime = 0.0f;
	SmoothResetStartRotation = FRotator::ZeroRotator;
	SmoothResetTargetRotation = FRotator::ZeroRotator;

	// ==================== Socket?????????? ====================
	TargetSocketName = TEXT("Spine2Socket");
	ProjectionScale = 1.0f;
	SocketOffset = FVector(0.0f, 0.0f, 50.0f);
	bUseSocketProjection = true; // ???????Socket???

	// ==================== ���������� ====================
	// ԭʼ����ע�ͱ����
	// CameraBoom = CreateDefaultSubobject<USpringArmComponent>(TEXT("CameraBoom"));
	// CameraBoom->SetupAttachment(RootComponent);
	// CameraBoom->TargetArmLength = 450.0f; // �������
	// CameraBoom->bUsePawnControlRotation = true; // �����������ت

	// �޸ĺ�ʹ�����ýṹ���Ĭ��ֵ
	CameraBoom = CreateDefaultSubobject<USpringArmComponent>(TEXT("CameraBoom"));
	CameraBoom->SetupAttachment(RootComponent);
	// ʹ�������е�ֵ����Ӳ����
	CameraBoom->TargetArmLength = CameraSetupConfig.ArmLength;
	CameraBoom->bUsePawnControlRotation = CameraSetupConfig.bUsePawnControlRotation;
	CameraBoom->SetRelativeRotation(CameraSetupConfig.InitialRotation);
	// ����������ӳ�����
	CameraBoom->bEnableCameraLag = CameraSetupConfig.bEnableCameraLag;
	CameraBoom->CameraLagSpeed = CameraSetupConfig.CameraLagSpeed;

	// ԭʼ����ع��ͱ����
	// FollowCamera = CreateDefaultSubobject<UCameraComponent>(TEXT("FollowCamera"));
	// FollowCamera->SetupAttachment(CameraBoom, USpringArmComponent::SocketName);
	// FollowCamera->SetRelativeLocation(FVector(0.0f, 0.0f, 50.0f));

	// �޸ĺ�ʹ�������еĲ���ƫ��
	FollowCamera = CreateDefaultSubobject<UCameraComponent>(TEXT("FollowCamera"));
	FollowCamera->SetupAttachment(CameraBoom, USpringArmComponent::SocketName);
	FollowCamera->SetRelativeLocation(CameraSetupConfig.SocketOffset);
	FollowCamera->bUsePawnControlRotation = false; // ????????????

	// ==================== ........................ ====================
	// ???y????????? ====================
	// ???y??????????????
	bUseControllerRotationPitch = false;
	bUseControllerRotationYaw = false;
	bUseControllerRotationRoll = false;

	// ???y????????
	GetCharacterMovement()->bOrientRotationToMovement = true; // ??????????????
	GetCharacterMovement()->RotationRate = FRotator(0.0f, 540.0f, 0.0f); // ??????
	GetCharacterMovement()->MaxWalkSpeed = NormalWalkSpeed;
}

// PostInitializeComponents - �����������ʼ����ɺ���ã���������ô��ݵ����ʱ����
void AMyCharacter::PostInitializeComponents()
{
	Super::PostInitializeComponents();
	
	UE_LOG(LogTemp, Warning, TEXT("=== MyCharacter PostInitializeComponents ==="));
	
	// �������ʼ������������ã���ʱ����������Ѵ�����ɣ�
	if (CameraControlComponent)
	{
		if (CameraBoom && FollowCamera)
		{
			// ��ʽ������������CameraControlComponent
			CameraControlComponent->InitializeCameraComponents(CameraBoom, FollowCamera);
			UE_LOG(LogTemp, Warning, TEXT("PostInitializeComponents: Camera components passed to CameraControlComponent"));
		}
		else
		{
			UE_LOG(LogTemp, Error, TEXT("PostInitializeComponents: CameraBoom or FollowCamera is NULL!"));
		}
	}
	else
	{
		UE_LOG(LogTemp, Error, TEXT("PostInitializeComponents: CameraControlComponent is NULL!"));
	}
	
	// ͬ������TargetDetectionComponent
	if (TargetDetectionComponent && LockOnDetectionSphere)
	{
		TargetDetectionComponent->SetLockOnDetectionSphere(LockOnDetectionSphere);
		UE_LOG(LogTemp, Warning, TEXT("PostInitializeComponents: Detection sphere passed to TargetDetectionComponent"));
	}
	else
	{
		if (!TargetDetectionComponent)
			UE_LOG(LogTemp, Error, TEXT("PostInitializeComponents: TargetDetectionComponent is NULL!"));
		if (!LockOnDetectionSphere)
			UE_LOG(LogTemp, Error, TEXT("PostInitializeComponents: LockOnDetectionSphere is NULL!"));
	}
	
	UE_LOG(LogTemp, Warning, TEXT("=== PostInitializeComponents Complete ==="));
}

// Called when the game starts or when spawned
void AMyCharacter::BeginPlay()
{
	Super::BeginPlay();
	
	// ==================== ����1��ǿ����֤���ϵͳ ====================
	UE_LOG(LogTemp, Warning, TEXT("=== CHARACTER BEGIN PLAY ==="));
	
	// ��֤���������
	ValidateAndCacheComponents();
	
	// NOTE: �����ʽ��ʼ������PostInitializeComponents�����
	// �˴�ֻ������֤������Ӧ��
	
	// ==================== ����2������ҵ������Ӧ�ó�ʼ���� ====================
	if (CameraBoom && FollowCamera)
	{
		// 680.0f; // ����
		GetCharacterMovement()->MaxWalkSpeed = NormalWalkSpeed;
		
		// ==================== Ӧ��������ã������� ====================
		// ֻ������ȷҪ��Ӧ������ʱ�Ÿ�����ͼ����
		if (bApplyCameraConfigOnBeginPlay)
		{
			// ��¼���ǰ״̬
			float OldArmLength = CameraBoom->TargetArmLength;
			FRotator OldRotation = CameraBoom->GetRelativeRotation();
			
			// Ӧ�����������
			CameraBoom->TargetArmLength = CameraSetupConfig.ArmLength;
			CameraBoom->SetRelativeRotation(CameraSetupConfig.InitialRotation);
			CameraBoom->bUsePawnControlRotation = CameraSetupConfig.bUsePawnControlRotation;
			CameraBoom->bEnableCameraLag = CameraSetupConfig.bEnableCameraLag;
			CameraBoom->CameraLagSpeed = CameraSetupConfig.CameraLagSpeed;
			
			// Ӧ���������
			FollowCamera->SetRelativeLocation(CameraSetupConfig.SocketOffset);
			
			// ��֤Ӧ�ý��
			float ActualArmLength = CameraBoom->TargetArmLength;
			if (FMath::IsNearlyEqual(ActualArmLength, CameraSetupConfig.ArmLength, 0.1f))
			{
				UE_LOG(LogTemp, Warning, TEXT("??? CONFIG APPLIED SUCCESSFULLY ???"));
				UE_LOG(LogTemp, Warning, TEXT("  ArmLength: %.1f -> %.1f"), OldArmLength, ActualArmLength);
				UE_LOG(LogTemp, Warning, TEXT("  Rotation: (%.1f,%.1f,%.1f) -> (%.1f,%.1f,%.1f)"),
					OldRotation.Pitch, OldRotation.Yaw, OldRotation.Roll,
					CameraSetupConfig.InitialRotation.Pitch, CameraSetupConfig.InitialRotation.Yaw, CameraSetupConfig.InitialRotation.Roll);
			}
			else
			{
				UE_LOG(LogTemp, Error, TEXT("??? CONFIG VERIFICATION FAILED ???"));
				UE_LOG(LogTemp, Error, TEXT("  Expected: %.1f, Actual: %.1f"), 
					CameraSetupConfig.ArmLength, ActualArmLength);
			}
			
			// ��¼�����³���������̬����ʹ��
			if (CameraControlComponent)
			{
				// ���ݻ������ø�����������
				FCameraSettings CameraSetup;
				CameraSetup.CameraInterpSpeed = CameraInterpSpeed;
				CameraSetup.bEnableSmoothCameraTracking = bEnableSmoothCameraTracking;
				CameraSetup.CameraTrackingMode = CameraTrackingMode;
				CameraSetup.TargetLocationOffset = TargetLocationOffset;
				CameraControlComponent->SetCameraSettings(CameraSetup);
			}
			
			// ��¼����״̬
			LogCameraState(TEXT("BeginPlay Initial"));
		}
		else
		{
			UE_LOG(LogTemp, Warning, TEXT("MyCharacter: Using Blueprint camera settings (bApplyCameraConfigOnBeginPlay=false)"));
		}
	}
	else
	{
		UE_LOG(LogTemp, Error, TEXT("BeginPlay FAILED - Camera components not initialized properly"));
		return;
	}
	
	// ==================== ����3����������ʱ�������� ====================
	SetupDebugCommands();
	
	// ȷ�����������ȷ��ʼ��
	if (LockOnDetectionSphere)
	{
		LockOnDetectionSphere->SetSphereRadius(LockOnRange);
		// ȷ���༭���пɼ�����Ϸ������
		#if WITH_EDITOR
			LockOnDetectionSphere->SetHiddenInGame(false);  // �༭������ʾ
		#else
			LockOnDetectionSphere->SetHiddenInGame(true);   // ��Ϸ������
		#endif
		
		UE_LOG(LogTemp, Log, TEXT("LockOnDetectionSphere initialized with radius: %.1f"), LockOnRange);
	}
	else
	{
		UE_LOG(LogTemp, Error, TEXT("LockOnDetectionSphere is null in BeginPlay!"));
	}

	// ==================== ����Ŀ������� ====================
	if (TargetDetectionComponent)
	{
		// ???��??????????
		TargetDetectionComponent->SetLockOnDetectionSphere(LockOnDetectionSphere);
		
		// ????????????
		TargetDetectionComponent->LockOnSettings.LockOnRange = LockOnRange;
		TargetDetectionComponent->LockOnSettings.LockOnAngle = LockOnAngle;
		
		// ???????????
		TargetDetectionComponent->CameraSettings.CameraInterpSpeed = CameraInterpSpeed;
		TargetDetectionComponent->CameraSettings.bEnableSmoothCameraTracking = bEnableSmoothCameraTracking;
		TargetDetectionComponent->CameraSettings.CameraTrackingMode = CameraTrackingMode;
		TargetDetectionComponent->CameraSettings.TargetLocationOffset = TargetLocationOffset;
		
		// ????Socket??????
		TargetDetectionComponent->SocketProjectionSettings.bUseSocketProjection = bUseSocketProjection;
		TargetDetectionComponent->SocketProjectionSettings.TargetSocketName = TargetSocketName;
		TargetDetectionComponent->SocketProjectionSettings.SocketOffset = SocketOffset;
		TargetDetectionComponent->SocketProjectionSettings.ProjectionScale = ProjectionScale;
		
		// ???????????
		TargetDetectionComponent->bEnableTargetDetectionDebugLogs = bEnableLockOnDebugLogs;
		TargetDetectionComponent->bEnableSizeAnalysisDebugLogs = bEnableLockOnDebugLogs;
		
		UE_LOG(LogTemp, Warning, TEXT("MyCharacter: TargetDetectionComponent configured successfully"));
	}
	else
	{
		UE_LOG(LogTemp, Error, TEXT("MyCharacter: TargetDetectionComponent is null!"));
	}

	// ==================== ??????????????????? ====================
	if (CameraControlComponent)
	{
		// ???????????
		FCameraSettings CameraSetup;
		CameraSetup.CameraInterpSpeed = CameraInterpSpeed;
		CameraSetup.bEnableSmoothCameraTracking = bEnableSmoothCameraTracking;
		CameraSetup.CameraTrackingMode = CameraTrackingMode;
		CameraSetup.TargetLocationOffset = TargetLocationOffset;
		CameraControlComponent->SetCameraSettings(CameraSetup);
		
		// ?????????????
		FAdvancedCameraSettings AdvancedSetup;
		// ???CameraControlComponent?��???????
		CameraControlComponent->SetAdvancedCameraSettings(AdvancedSetup);
		
		UE_LOG(LogTemp, Warning, TEXT("MyCharacter: CameraControlComponent configured successfully"));
	}
	else
	{
		UE_LOG(LogTemp, Error, TEXT("MyCharacter: CameraControlComponent is null!"));
	}

	// ==================== ?????UI??????????? ====================
	if (UIManagerComponent)
	{
		UIManagerComponent->LockOnWidgetClass = LockOnWidgetClass;
		
		// Configure Hybrid Projection Settings (replacing Socket Projection)
		FHybridProjectionSettings HybridSettings;
		HybridSettings.ProjectionMode = EProjectionMode::Hybrid;
		HybridSettings.CustomOffset = SocketOffset;
		HybridSettings.BoundsOffsetRatio = 0.6f;
		HybridSettings.bEnableSizeAdaptive = true;
		UIManagerComponent->SetHybridProjectionSettings(HybridSettings);
		
		UE_LOG(LogTemp, Warning, TEXT("MyCharacter: UIManagerComponent configured successfully with hybrid projection"));
	}
	else
	{
		UE_LOG(LogTemp, Error, TEXT("MyCharacter: UIManagerComponent is null!"));
	}

	// ==================== ��֤���к������ʵ���Ѿ����А����ܼ�飩 ====================
	if (PoiseComponent)
	{
		UE_LOG(LogTemp, Log, TEXT("PoiseComponent initialized successfully"));
	}

	if (StaminaComponent)
	{
		UE_LOG(LogTemp, Log, TEXT("StaminaComponent initialized successfully"));
	}

	if (DodgeComponent)
	{
		UE_LOG(LogTemp, Log, TEXT("DodgeComponent initialized successfully"));
	}

	if (ExecutionComponent)
	{
		UE_LOG(LogTemp, Log, TEXT("ExecutionComponent initialized successfully"));
	}

	// ==================== ����Debug��� ====================
	if (CameraDebugComponent)
	{
#if WITH_EDITOR
		// �༭����Ĭ�Ͽ���Debug
		CameraDebugComponent->bEnableDebugVisualization = true;
		CameraDebugComponent->VisualizationMode = EDebugVisualizationMode::TargetInfo;
#else
		// �����汾Ĭ�Ϲر�
		CameraDebugComponent->bEnableDebugVisualization = false;
#endif
		
		UE_LOG(LogTemp, Warning, TEXT("MyCharacter: CameraDebugComponent�������"));
	}
	else
	{
		UE_LOG(LogTemp, Error, TEXT("MyCharacter: CameraDebugComponent is null!"));
	}
}

// Called every frame
void AMyCharacter::Tick(float DeltaTime)
{
	Super::Tick(DeltaTime);

	// ==================== Week 3: 新旧相机系统切换逻辑 ====================
	// 使用新相机系统
	if (bUseNewCameraSystem && CameraPipelineV2)
	{
		// 根据锁定状态设置相机状态
		if (bIsLockedOn && CurrentLockOnTarget)
		{
			CameraPipelineV2->SetCameraState(ECameraPipelineState::LockOn);
		}
		else
		{
			CameraPipelineV2->SetCameraState(ECameraPipelineState::Free);
		}
		
		// 应用输出到组件
		FCameraStateOutput Output = CameraPipelineV2->GetFinalOutput();
		CameraSystemAdapter::ApplyOutputToComponents(Output, CameraBoom, FollowCamera, Cast<APlayerController>(GetController()));
		
		// 跳过旧系统逻辑，但仍需要更新目标检测
		float CurrentTime = GetWorld()->GetTimeSeconds();
		if (CurrentTime - LastFindTargetsTime > TARGET_SEARCH_INTERVAL)
		{
			FindLockOnCandidates();
			LastFindTargetsTime = CurrentTime;
		}
		
		// 更新UI
		if (bIsLockedOn)
		{
			UpdateLockOnWidget();
		}
		
		return; // 跳过旧系统逻辑
	}

	// ==================== 以下是原有的相机更新逻辑，保持不变 ====================
	// ==================== �Ƴ�����ƽ�����ü�� ====================
	// NOTE: ƽ��������������� CameraControlComponent �Լ�����������ڲ��� TickComponent �д����
	// ������ﲻ�ٹ�� AMyCharacter::bIsSmoothCameraReset �����ڷ��/

	// ������Զ��������ȼ�����
	if (bIsCameraAutoCorrection)
	{
		UpdateCameraAutoCorrection();
	}

	// ����״̬���߼�
	if (bIsLockedOn)
	{
		UpdateLockOnTarget();
		
		// ƽ���л��ڼ����ł��߼�����
		if (bIsSmoothSwitching)
		{
			UpdateSmoothTargetSwitch();
		}
		else if (!bIsCameraAutoCorrection)
		{
			UpdateLockOnCamera(); 
		}
		
		// ����UI
		UpdateLockOnWidget();

		// �������
		if (bEnableCameraDebugLogs && CurrentLockOnTarget)
		{
			static float LastCameraDebugLogTime = 0.0f;
			float CurrentTime = GetWorld()->GetTimeSeconds();
			if (CurrentTime - LastCameraDebugLogTime > 0.5f)
			{
				UE_LOG(LogTemp, VeryVerbose, TEXT("LockOn Active: Target=%s, CameraFollow=%s, CharacterRotate=%s"), 
					*CurrentLockOnTarget->GetName(),
					bShouldCameraFollowTarget ? TEXT("YES") : TEXT("NO"),
					bShouldCharacterRotateToTarget ? TEXT("YES") : TEXT("NO"));
				LastCameraDebugLogTime = CurrentTime;
			}
		}
	}

	// Ŀ������������ƽ���л��ڼ���У������ͻ��
	if (!bIsSmoothSwitching)
	{
		float CurrentTime = GetWorld()->GetTimeSeconds();
		if (CurrentTime - LastFindTargetsTime > TARGET_SEARCH_INTERVAL)
		{
			FindLockOnCandidates();
			LastFindTargetsTime = CurrentTime;

			if (bEnableLockOnDebugLogs)
			{
				static float LastTargetSearchLogTime = 0.0f;
				if (CurrentTime - LastTargetSearchLogTime > 1.0f)
				{
					UE_LOG(LogTemp, Verbose, TEXT("Target search completed: Found %d candidates"), LockOnCandidates.Num());
					LastTargetSearchLogTime = CurrentTime;
				}
			}
		}
	}
	// === ������������֤���ԣ��ں���ĩβ��ӣ�===
	#if WITH_EDITOR  // ���ڱ༭����ִ��
	if (bEnableCameraDebugLogs && bIsLockedOn)
	{
		static float LastParamDebugTime = 0.0f;
		float CurrentTime = GetWorld()->GetTimeSeconds();
		
		// ÿ�����һ�β���״̬
		if (CurrentTime - LastParamDebugTime > 1.0f)
		{
			if (CameraBoom && FollowCamera)
			{
				UE_LOG(LogTemp, Warning, TEXT("=== CAMERA PARAMS DEBUG ==="));
				UE_LOG(LogTemp, Warning, TEXT("1. ArmLength: %.1f (Config: %.1f)"), 
					CameraBoom->TargetArmLength, CameraSetupConfig.ArmLength);
				UE_LOG(LogTemp, Warning, TEXT("2. InitialRotation: %s (Config: %s)"), 
					*CameraBoom->GetRelativeRotation().ToString(), 
					*CameraSetupConfig.InitialRotation.ToString());
				UE_LOG(LogTemp, Warning, TEXT("3. SocketOffset: %s (Config: %s)"), 
					*FollowCamera->GetRelativeLocation().ToString(), 
					*CameraSetupConfig.SocketOffset.ToString());
				UE_LOG(LogTemp, Warning, TEXT("4. LockOnHeightOffset: %s (Applied: %s)"), 
					*CameraSetupConfig.LockOnHeightOffset.ToString(),
					bIsLockedOn ? TEXT("YES") : TEXT("NO"));
				
				// ��֤ CameraControlComponent ������
				if (CameraControlComponent)
				{
					UE_LOG(LogTemp, Warning, TEXT("5. TargetLocationOffset in Component: %s"), 
						*CameraControlComponent->CameraSettings.TargetLocationOffset.ToString());
				}
				
				UE_LOG(LogTemp, Warning, TEXT("==========================="));
				LastParamDebugTime = CurrentTime;
			}
		}
	}
	#endif
}

// Called to bind functionality to input
void AMyCharacter::SetupPlayerInputComponent(UInputComponent* PlayerInputComponent)
{
	if (!PlayerInputComponent)
	{
		UE_LOG(LogTemp, Error, TEXT("PlayerInputComponent is null!"));
		return;
	}

	Super::SetupPlayerInputComponent(PlayerInputComponent);

	// ?????????
	PlayerInputComponent->BindAxis("MoveForward", this, &AMyCharacter::MoveForward);
	PlayerInputComponent->BindAxis("MoveRight", this, &AMyCharacter::MoveRight);

	// ?????????
	PlayerInputComponent->BindAxis("Turn", this, &AMyCharacter::Turn);
	PlayerInputComponent->BindAxis("LookUp", this, &AMyCharacter::LookUp);

	// ???? ?????
	PlayerInputComponent->BindAction("Jump", IE_Pressed, this, &AMyCharacter::StartJump);
	PlayerInputComponent->BindAction("Jump", IE_Released, this, &AMyCharacter::StopJump);

	// ==================== ????????? ====================
	// ???????? - ?????????????????
	PlayerInputComponent->BindAction("LockOn", IE_Pressed, this, &AMyCharacter::HandleLockOnButton);
	
	// ?????????? ?????????? Gep?
	PlayerInputComponent->BindAxis("RightStickX", this, &AMyCharacter::HandleRightStickX);
	
	// ????????????????
	PlayerInputComponent->BindAction("SwitchTargetLeft", IE_Pressed, this, &AMyCharacter::SwitchLockOnTargetLeft);
	PlayerInputComponent->BindAction("SwitchTargetRight", IE_Pressed, this, &AMyCharacter::SwitchLockOnTargetRight);

	// ==================== ????????? ====================
	// ????????key (F??) ??????????
	PlayerInputComponent->BindAction("DebugInput", IE_Pressed, this, &AMyCharacter::DebugInputTest);
	
	// ??????????????????? (G??)
	PlayerInputComponent->BindAction("DebugTargetSizes", IE_Pressed, this, &AMyCharacter::DebugDisplayTargetSizes);
	
	// ??????????
	UE_LOG(LogTemp, Warning, TEXT("Input bindings set up successfully"));
	UE_LOG(LogTemp, Warning, TEXT("Available debug commands:"));
	UE_LOG(LogTemp, Warning, TEXT("- DebugInput (F): General debug info"));
	UE_LOG(LogTemp, Warning, TEXT("- DebugTargetSizes (G): Enemy size analysis"));
}

// ==================== ?????????? ====================
void AMyCharacter::MoveForward(float Value)
{
	ForwardInputValue = Value;

	// 通知相机控制组件玩家是否移动
	if (CameraControlComponent)
	{
		bool bIsMoving = (FMath::Abs(Value) > 0.1f);
		CameraControlComponent->HandlePlayerMovement(bIsMoving);
	}

	// 更新全局移动状态
	bPlayerIsMoving = (FMath::Abs(Value) > 0.1f || FMath::Abs(RightInputValue) > 0.1f);

	// 玩家移动时中断平滑切换
	if (bPlayerIsMoving && bIsLockedOn)
	{
		if (bIsSmoothSwitching)
		{
			bIsSmoothSwitching = false;
			bShouldSmoothSwitchCamera = false;
			bShouldSmoothSwitchCharacter = false;
			UE_LOG(LogTemp, Warning, TEXT("Player movement interrupted smooth target switch"));
		}
		
		if (bIsCameraAutoCorrection)
		{
			bIsCameraAutoCorrection = false;
			DelayedCorrectionTarget = nullptr;
			UE_LOG(LogTemp, Warning, TEXT("Player movement interrupted camera auto correction"));
		}
		
		if (!bShouldCameraFollowTarget)
		{
			bShouldCameraFollowTarget = true;
			bShouldCharacterRotateToTarget = true;
			UE_LOG(LogTemp, Warning, TEXT("Player started moving - enabling camera follow and character rotation"));
		}
	}

	if (Value != 0.0f && Controller)
	{
		if (bIsLockedOn && CurrentLockOnTarget)
		{
			// ==================== ✅ 锁定状态：相对于相机方向移动（围绕目标）====================
			const FRotator CameraRotation = Controller->GetControlRotation();
			const FRotator YawRotation(0, CameraRotation.Yaw, 0);
			const FVector Direction = FRotationMatrix(YawRotation).GetUnitAxis(EAxis::X);
			AddMovementInput(Direction, Value);
		}
		else
		{
			// 自由状态：相对于相机方向移动
			const FRotator Rotation = Controller->GetControlRotation();
			const FRotator YawRotation(0, Rotation.Yaw, 0);
			const FVector Direction = FRotationMatrix(YawRotation).GetUnitAxis(EAxis::X);
			AddMovementInput(Direction, Value);
		}
	}
}

void AMyCharacter::MoveRight(float Value)
{
	RightInputValue = Value;

	// 通知相机控制组件玩家是否移动
	if (CameraControlComponent)
	{
		bool bIsMoving = (FMath::Abs(Value) > 0.1f);
		CameraControlComponent->HandlePlayerMovement(bIsMoving);
	}

	// 更新全局移动状态
	bPlayerIsMoving = (FMath::Abs(Value) > 0.1f || FMath::Abs(ForwardInputValue) > 0.1f);

	// 玩家移动时中断平滑切换
	if (bPlayerIsMoving && bIsLockedOn)
	{
		if (bIsSmoothSwitching)
		{
			bIsSmoothSwitching = false;
			bShouldSmoothSwitchCamera = false;
			bShouldSmoothSwitchCharacter = false;
			UE_LOG(LogTemp, Warning, TEXT("Player movement interrupted smooth target switch"));
		}
		
		if (bIsCameraAutoCorrection)
		{
			bIsCameraAutoCorrection = false;
			DelayedCorrectionTarget = nullptr;
			UE_LOG(LogTemp, Warning, TEXT("Player movement interrupted camera auto correction"));
		}
		
		if (!bShouldCameraFollowTarget)
		{
			bShouldCameraFollowTarget = true;
			bShouldCharacterRotateToTarget = true;
			UE_LOG(LogTemp, Warning, TEXT("Player started moving - enabling camera follow and character rotation"));
		}
	}

	if (Value != 0.0f && Controller)
	{
		if (bIsLockedOn && CurrentLockOnTarget)
		{
			// ==================== ✅ 锁定状态：相对于相机方向横向移动（围绕目标）====================
			const FRotator CameraRotation = Controller->GetControlRotation();
			const FRotator YawRotation(0, CameraRotation.Yaw, 0);
			const FVector Direction = FRotationMatrix(YawRotation).GetUnitAxis(EAxis::Y);
			AddMovementInput(Direction, Value);
		}
		else
		{
			// 自由状态：相对于相机方向横向移动
			const FRotator Rotation = Controller->GetControlRotation();
			const FRotator YawRotation(0, Rotation.Yaw, 0);
			const FVector Direction = FRotationMatrix(YawRotation).GetUnitAxis(EAxis::Y);
			AddMovementInput(Direction, Value);
		}
	}
}

void AMyCharacter::StartJump()
{
	Jump();
}

void AMyCharacter::StopJump()
{
	StopJumping();
}

// ==================== ??????? ====================
void AMyCharacter::Turn(float Rate)
{
	// 通知相机控制组件有输入
	if (CameraControlComponent)
	{
		CameraControlComponent->HandlePlayerInput(Rate, 0.0f);
	}

	// ==================== 锁定状态下的处理 ====================
	if (bIsLockedOn)
	{
		// 使用可配置参数控制锁定时的左右转向
		if (!bAllowHorizontalTurnInLockOn)
		{
			return; // 完全禁用左右转向
		}

		if (FMath::Abs(Rate) > 0.1f)
		{
			// 中断自动修正和平滑重置
			if (bIsCameraAutoCorrection)
			{
				bIsCameraAutoCorrection = false;
				DelayedCorrectionTarget = nullptr;
				if (bEnableCameraDebugLogs)
				{
					UE_LOG(LogTemp, Log, TEXT("Player camera input - stopping auto correction"));
				}
			}
			
			if (bIsSmoothCameraReset)
			{
				bIsSmoothCameraReset = false;
				if (bEnableCameraDebugLogs)
				{
					UE_LOG(LogTemp, Log, TEXT("Player camera input - stopping smooth camera reset"));
				}
			}
			
			// ✅ 允许水平旋转（围绕锁定目标）
			AddControllerYawInput(Rate);
		}
		return; // 锁定状态处理完毕，不继续执行自由状态逻辑
	}

	// ==================== ✅ 修复：自由状态下正常处理所有输入 ====================
	// 中断自动控制（如果有输入）
	if (FMath::Abs(Rate) > 0.1f)
	{
		if (bIsCameraAutoCorrection)
		{
			bIsCameraAutoCorrection = false;
			DelayedCorrectionTarget = nullptr;
		}
		
		if (bIsSmoothCameraReset)
		{
			bIsSmoothCameraReset = false;
		}
	}
	
	// ✅ 关键修复：直接应用输入，不论大小（支持鼠标和手柄）
	AddControllerYawInput(Rate);
}

void AMyCharacter::LookUp(float Rate)
{
	// 通知相机控制组件有输入
	if (CameraControlComponent)
	{
		CameraControlComponent->HandlePlayerInput(0.0f, Rate);
	}

	// ==================== 锁定状态下的处理 ====================
	if (bIsLockedOn)
	{
		// 使用可配置参数控制锁定时的上下看
		if (!bAllowVerticalLookInLockOn)
		{
			// 仅中断自动修正，但不应用垂直输入
			if (FMath::Abs(Rate) > 0.1f)
			{
				if (bIsCameraAutoCorrection)
				{
					bIsCameraAutoCorrection = false;
					DelayedCorrectionTarget = nullptr;
					if (bEnableCameraDebugLogs)
					{
						UE_LOG(LogTemp, Log, TEXT("Player camera input - stopping auto correction"));
					}
				}
				
				if (bIsSmoothCameraReset)
				{
					bIsSmoothCameraReset = false;
					if (bEnableCameraDebugLogs)
					{
						UE_LOG(LogTemp, Log, TEXT("Player camera input - stopping smooth camera reset"));
					}
				}
			}
			return; // 禁用上下看
		}

		// 允许上下看
		if (FMath::Abs(Rate) > 0.1f)
		{
			if (bIsCameraAutoCorrection)
			{
				bIsCameraAutoCorrection = false;
				DelayedCorrectionTarget = nullptr;
			}
			
			if (bIsSmoothCameraReset)
			{
				bIsSmoothCameraReset = false;
			}
		}
		
		AddControllerPitchInput(Rate);
		return; // 锁定状态处理完毕
	}

	// ==================== ✅ 修复：自由状态下正常处理所有输入 ====================
	// 中断自动控制（如果有输入）
	if (FMath::Abs(Rate) > 0.1f)
	{
		if (bIsCameraAutoCorrection)
		{
			bIsCameraAutoCorrection = false;
			DelayedCorrectionTarget = nullptr;
		}
		
		if (bIsSmoothCameraReset)
		{
			bIsSmoothCameraReset = false;
		}
	}
	
	// ✅ 关键修复：直接应用输入，不论大小（支持鼠标和手柄）
	AddControllerPitchInput(Rate);
}

// ==================== ????????? ====================
void AMyCharacter::ToggleLockOn()
{
	if (bEnableLockOnDebugLogs)
	{
		UE_LOG(LogTemp, Warning, TEXT("ToggleLockOn called - Current state: %s"), 
			bIsLockedOn ? TEXT("LOCKED") : TEXT("UNLOCKED"));
	}
	
	if (bIsLockedOn)
	{
		// ���UI��� - �ڵ���CancelLockOn֮ǰ�������UI
		if (bEnableLockOnDebugLogs)
		{
			UE_LOG(LogTemp, Log, TEXT("Cancelling lock-on..."));
		}
		
		HideAllLockOnWidgets();
		if (LockOnWidgetInstance && LockOnWidgetInstance->IsInViewport()) 
		{
			LockOnWidgetInstance->RemoveFromViewport();
			LockOnWidgetInstance = nullptr;
		}
		
		// ȡ�������߼����ᴥ��ƽ�����ã�
		CancelLockOn();
	}
	else
	{
		if (bEnableLockOnDebugLogs)
		{
			UE_LOG(LogTemp, Log, TEXT("Attempting to start lock-on..."));
		}
		
		FindLockOnCandidates();
		
		if (bEnableLockOnDebugLogs)
		{
			UE_LOG(LogTemp, Log, TEXT("Found %d lock-on candidates"), LockOnCandidates.Num());
		}
		
		// ֻ�������������ڵ�Ŀ��
		AActor* SectorTarget = TryGetSectorLockTarget();
		
		if (SectorTarget)
		{
			if (bEnableLockOnDebugLogs)
			{
				UE_LOG(LogTemp, Log, TEXT("Found sector target: %s"), *SectorTarget->GetName());
			}
			// ���1���ҵ���������Ŀ�꣬��ʼ����
			StartLockOn(SectorTarget);
		}
		else
		{
			if (bEnableLockOnDebugLogs)
			{
				UE_LOG(LogTemp, Log, TEXT("No valid sector target found, performing camera reset"));
			}
			// ==================== �ؼ��޸������2û�и���ʱʹ��ƽ������ ====================
			// ����ƽ�����ö���������ت
			if (CameraControlComponent)
			{
				CameraControlComponent->StartSmoothCameraReset();
			}
			else
			{
				PerformSimpleCameraReset();
			}
		}
	}
	
	UE_LOG(LogTemp, Warning, TEXT("=== ToggleLockOn complete ==="));
}

// ????????m???????????????????��??
void AMyCharacter::PerformSimpleCameraReset()
{
	if (CameraControlComponent)
	{
		CameraControlComponent->PerformSimpleCameraReset();
	}
	else
	{
		UE_LOG(LogTemp, Error, TEXT("MyCharacter::PerformSimpleCameraReset: CameraControlComponent is null!"));
	}
}

// ???????????????????
void AMyCharacter::StartSmoothCameraReset()
{
	if (CameraControlComponent)
	{
		CameraControlComponent->StartSmoothCameraReset();
	}
	else
	{
		UE_LOG(LogTemp, Error, TEXT("MyCharacter::StartSmoothCameraReset: CameraControlComponent is null!"));
	}
}

// ???????????????????
void AMyCharacter::UpdateSmoothCameraReset()
{
	if (CameraControlComponent)
	{
		CameraControlComponent->UpdateSmoothCameraReset();
	}
	else
	{
		UE_LOG(LogTemp, Error, TEXT("MyCharacter::UpdateSmoothCameraReset: CameraControlComponent is null!"));
	}
}

// ==================== ��ʼ����Ŀ��
void AMyCharacter::StartLockOn(AActor* Target)
{
	if (!Target)
		return;
	
	// ==================== Step 2.4: 目标切换时清除缓存 ====================
	if (CurrentLockOnTarget != Target)
	{
		CachedLockMethod = ELockMethod::None;
		CachedSocketName = NAME_None;
		CachedMethodOffset = FVector::ZeroVector;
		
		if (bEnableLockOnDebugLogs)
		{
			UE_LOG(LogTemp, Log, TEXT("StartLockOn: Cache cleared for target change from [%s] to [%s]"), 
				CurrentLockOnTarget ? *CurrentLockOnTarget->GetName() : TEXT("None"),
				*Target->GetName());
		}
	}
		
	bIsLockedOn = true;
	CurrentLockOnTarget = Target;
	
	// 使用配置的臂长调整
	if (CameraControlComponent && CameraBoom)
	{
		float TargetLength = CameraSetupConfig.ArmLength * CameraSetupConfig.LockOnArmLengthMultiplier;
		CameraControlComponent->SetSpringArmLengthSmooth(TargetLength);
		
		if (bEnableLockOnDebugLogs)
		{
			UE_LOG(LogTemp, Warning, TEXT("StartLockOn: Smoothly transitioning ArmLength to %.1f"), TargetLength);
		}
	}
	else if (CameraBoom)
	{
		CameraBoom->TargetArmLength = CameraSetupConfig.ArmLength * CameraSetupConfig.LockOnArmLengthMultiplier;
		
		if (bEnableLockOnDebugLogs)
		{
			UE_LOG(LogTemp, Warning, TEXT("StartLockOn: Fallback - immediate ArmLength set to %.1f"), 
				CameraBoom->TargetArmLength);
		}
	}
	
	// ==================== ✅ 关键修改：禁用角色自动转向移动方向 ====================
	GetCharacterMovement()->MaxWalkSpeed = LockedWalkSpeed;
	GetCharacterMovement()->bOrientRotationToMovement = false;  // 禁用自动转向
	bUseControllerRotationYaw = false;  // 不使用控制器Yaw旋转角色
	
	// 显示锁定UI
	ShowLockOnWidget();
	
	UE_LOG(LogTemp, Warning, TEXT("Started lock-on with target: %s"), *Target->GetName());
}

// ?????????????
void AMyCharacter::CancelLockOn()
{
	if (!bIsLockedOn)
		return;
	
	// ==================== 关键修复：强制清理所有UI ====================
	// 第一部：通过UIManagerComponent清理UI
	if (UIManagerComponent)
	{
		UIManagerComponent->HideLockOnWidget();
		if (bEnableLockOnDebugLogs)
		{
			UE_LOG(LogTemp, Warning, TEXT("CancelLockOn: UI hidden via UIManagerComponent"));
		}
	}
	
	// 第二步：双重保险 - 直接清理传统方式的UI
	HideAllLockOnWidgets();
	
	// 第三步：如果还有Widget实例残留，强制移除
	if (LockOnWidgetInstance)
	{
		if (LockOnWidgetInstance->IsInViewport())
		{
			LockOnWidgetInstance->SetVisibility(ESlateVisibility::Hidden);
			LockOnWidgetInstance->RemoveFromViewport();
			if (bEnableLockOnDebugLogs)
			{
				UE_LOG(LogTemp, Warning, TEXT("CancelLockOn: Widget forcefully removed from viewport"));
			}
		}
		LockOnWidgetInstance = nullptr;
	}
	
	// 重置相机到默认配置（如需要）
	ResetCameraToDefaultConfig();
	
	// 清除锁定状态
	bIsLockedOn = false;
	PreviousLockOnTarget = CurrentLockOnTarget;
	CurrentLockOnTarget = nullptr;
	
	// 清除相机跟随状态
	bShouldCameraFollowTarget = true;
	bShouldCharacterRotateToTarget = true;
	
	// 停止任何正在进行的平滑切换或自动修正
	bIsSmoothSwitching = false;
	bShouldSmoothSwitchCamera = false;
	bShouldSmoothSwitchCharacter = false;
	bIsCameraAutoCorrection = false;
	DelayedCorrectionTarget = nullptr;
	
	// ==================== ✅ 恢复角色自动转向移动方向 ====================
	GetCharacterMovement()->bOrientRotationToMovement = true;  // 恢复自动转向
	bUseControllerRotationYaw = false; // 确保不使用控制器旋转
	GetCharacterMovement()->MaxWalkSpeed = NormalWalkSpeed;
	
	// 执行平滑相机重置
	if (CameraControlComponent)
	{
		// 清除组件内部的锁定目标
		CameraControlComponent->ClearLockOnTarget();
		
		// 执行平滑相机重置，让相机平滑返回到正常状态
		CameraControlComponent->StartSmoothCameraReset();
		
		if (bEnableLockOnDebugLogs)
		{
			UE_LOG(LogTemp, Warning, TEXT("CancelLockOn: Smooth camera reset initiated via CameraControlComponent"));
		}
	}
	else
	{
		// 降级方案：如果组件不可用，使用传统快速重置
		PerformSimpleCameraReset();
		
		if (bEnableLockOnDebugLogs)
		{
			UE_LOG(LogTemp, Warning, TEXT("CancelLockOn: Using fallback instant reset method"));
		}
	}
	
	UE_LOG(LogTemp, Warning, TEXT("Lock-on cancelled - All UI cleaned up"));
}

// ?????????????
void AMyCharacter::ResetCamera()
{
	// ==================== �ؼ��޸�����Ϊʹ�������ƽ������ ====================
	// ���� CameraControlComponent ִ��ƽ��������ã�����ʹ��ƽ�����ɣ�
	if (CameraControlComponent)
	{
		CameraControlComponent->StartSmoothCameraReset();
		
		if (bEnableLockOnDebugLogs)
		{
			UE_LOG(LogTemp, Log, TEXT("ResetCamera: Starting smooth camera reset"));
		}
	}
	else
	{
		// �������������ʱ�Ż��˵���������
		PerformSimpleCameraReset();
		
		if (bEnableLockOnDebugLogs)
		{
			UE_LOG(LogTemp, Warning, TEXT("ResetCamera: Fallback to instant reset (CameraControlComponent not found)"));
		}
	}
}

// ==================== ��������ϵͳ����ʵ�� ====================

FVector AMyCharacter::GetOptimalLockPosition(AActor* Target) const
{
	if (!Target) return FVector::ZeroVector;
	
	// ==================== Step 2.5: �����Ի�������� ====================
	if (ULockOnConfigComponent* Config = Target->FindComponentByClass<ULockOnConfigComponent>())
	{
		if (Config->bOverrideOffset && Config->IsConfigValid())
		{
			// ʹ�ø��Ի�����
			if (Config->bPreferSocket)
			{
				// ���ȳ����Զ���Socket
				if (USkeletalMeshComponent* TargetMeshComp = Target->FindComponentByClass<USkeletalMeshComponent>())
				{
					if (TargetMeshComp->DoesSocketExist(Config->CustomSocketName))
					{
						FVector SocketLoc = TargetMeshComp->GetSocketLocation(Config->CustomSocketName) + Config->CustomOffset;
						
						if (bEnableLockOnDebugLogs)
						{
							UE_LOG(LogTemp, Log, TEXT("GetOptimalLockPosition: Using custom Socket '%s' with offset %s for %s"),
								*Config->CustomSocketName.ToString(), *Config->CustomOffset.ToString(), *Target->GetName());
						}
						
						return SocketLoc;
					}
				}
			}
			
			// ������Capsule��ʹ���Զ���ƫ��
			if (UCapsuleComponent* Capsule = Target->FindComponentByClass<UCapsuleComponent>())
			{
				FVector Center = Capsule->GetComponentLocation();
				float Height = Capsule->GetScaledCapsuleHalfHeight() * (CapsuleHeightRatio - 0.5f) * 2.0f;
				FVector CapsuleLoc = Center + FVector(0, 0, Height) + Config->CustomOffset;
				
				if (bEnableLockOnDebugLogs)
				{
					UE_LOG(LogTemp, Log, TEXT("GetOptimalLockPosition: Using custom Capsule offset %s for %s"),
						*Config->CustomOffset.ToString(), *Target->GetName());
				}
				
				return CapsuleLoc;
			}
			
			// ��󽵼���Root + �Զ���ƫ��
			if (bEnableLockOnDebugLogs)
			{
				UE_LOG(LogTemp, Log, TEXT("GetOptimalLockPosition: Using custom Root offset %s for %s"),
					*Config->CustomOffset.ToString(), *Target->GetName());
			}
			
			return Target->GetActorLocation() + Config->CustomOffset;
		}
	}
	
	// ==================== ԭ�е�����Ĭ���߼���δ���ø��Ի����ʱʹ�ã�====================
	
	// �����ͬһĿ�����ѻ��棬ʹ�û��淽��
	if (Target == CurrentLockOnTarget && CachedLockMethod != ELockMethod::None)
	{
		switch (CachedLockMethod)
		{
			case ELockMethod::Socket:
				if (USkeletalMeshComponent* TargetMesh = Target->FindComponentByClass<USkeletalMeshComponent>())
				{
					if (TargetMesh->DoesSocketExist(CachedSocketName))
						return TargetMesh->GetSocketLocation(CachedSocketName) + CachedMethodOffset;
				}
				break;
			case ELockMethod::Capsule:
				if (UCapsuleComponent* TargetCapsule = Target->FindComponentByClass<UCapsuleComponent>())
				{
					FVector Center = TargetCapsule->GetComponentLocation();
					float Height = TargetCapsule->GetScaledCapsuleHalfHeight() * (CapsuleHeightRatio - 0.5f) * 2.0f;
					return Center + FVector(0, 0, Height) + CachedMethodOffset;
				}
				break;
			case ELockMethod::Root:
				return Target->GetActorLocation() + CachedMethodOffset;
		}
	}
	
	// ���ȼ�1��Socket
	if (USkeletalMeshComponent* TargetMesh = Target->FindComponentByClass<USkeletalMeshComponent>())
	{
		// ����Ĭ��Socket
		if (TargetMesh->DoesSocketExist(DefaultLockOnSocketName))
		{
			const_cast<AMyCharacter*>(this)->CachedLockMethod = ELockMethod::Socket;
			const_cast<AMyCharacter*>(this)->CachedSocketName = DefaultLockOnSocketName;
			const_cast<AMyCharacter*>(this)->CachedMethodOffset = GetSizeBasedSocketOffset(Target);
			return TargetMesh->GetSocketLocation(DefaultLockOnSocketName) + CachedMethodOffset;
		}
		
		// ���Α���Socket
		for (const FName& SocketName : FallbackSocketNames)
		{
			if (TargetMesh->DoesSocketExist(SocketName))
			{
				const_cast<AMyCharacter*>(this)->CachedLockMethod = ELockMethod::Socket;
				const_cast<AMyCharacter*>(this)->CachedSocketName = SocketName;
				const_cast<AMyCharacter*>(this)->CachedMethodOffset = GetSizeBasedSocketOffset(Target);
				return TargetMesh->GetSocketLocation(SocketName) + CachedMethodOffset;
			}
		}
	}
	
	// ���ȼ�2��Capsule
	if (UCapsuleComponent* TargetCapsule = Target->FindComponentByClass<UCapsuleComponent>())
	{
		const_cast<AMyCharacter*>(this)->CachedLockMethod = ELockMethod::Capsule;
		const_cast<AMyCharacter*>(this)->CachedMethodOffset = CapsuleBaseOffset;
		FVector Center = TargetCapsule->GetComponentLocation();
		float Height = TargetCapsule->GetScaledCapsuleHalfHeight() * (CapsuleHeightRatio - 0.5f) * 2.0f;
		return Center + FVector(0, 0, Height) + CapsuleBaseOffset;
	}
	
	// ���ȼ�3��Root
	const_cast<AMyCharacter*>(this)->CachedLockMethod = ELockMethod::Root;
	const_cast<AMyCharacter*>(this)->CachedMethodOffset = DefaultRootOffset;
	return Target->GetActorLocation() + DefaultRootOffset;
}

FVector AMyCharacter::GetSizeBasedSocketOffset(AActor* Target) const
{
	FVector Origin, BoxExtent;
	Target->GetActorBounds(false, Origin, BoxExtent);
	float MaxDimension = FMath::Max3(BoxExtent.X, BoxExtent.Y, BoxExtent.Z);
	
	if (MaxDimension < 100.0f)
		return SmallEnemySocketOffset;
	else if (MaxDimension < 300.0f)
		return MediumEnemySocketOffset;
	else
		return LargeEnemySocketOffset;
}

// ==================== ���������Ҹ�������ʵ�� ====================

USpringArmComponent* AMyCharacter::FindCameraBoomComponent()
{
	// ����1: ֱ��ʹ�ó�Ա����������ѳ�ʼ����
	if (CameraBoom && CameraBoom->IsValidLowLevel())
	{
		UE_LOG(LogTemp, Log, TEXT("FindCameraBoom: Using existing member variable"));
		return CameraBoom;
	}
	
	// ����2: ͨ�����ֲ���
	USpringArmComponent* FoundBoom = FindComponentByClass<USpringArmComponent>();
	if (FoundBoom)
	{
		UE_LOG(LogTemp, Warning, TEXT("FindCameraBoom: Found by class search"));
		return FoundBoom;
	}
	
	// ����3: ���������������
	TArray<UActorComponent*> Components;
	GetComponents(Components);
	for (UActorComponent* Comp : Components)
	{
		USpringArmComponent* SpringArm = Cast<USpringArmComponent>(Comp);
		if (SpringArm)
		{
			UE_LOG(LogTemp, Warning, TEXT("FindCameraBoom: Found by component iteration: %s"), *SpringArm->GetName());
			return SpringArm;
		}
	}
	
	UE_LOG(LogTemp, Error, TEXT("FindCameraBoom: No SpringArmComponent found!"));
	return nullptr;
}

UCameraComponent* AMyCharacter::FindFollowCameraComponent()
{
	// ����1: ֱ��ʹ�ó�Ա����������ѳ�ʼ����
	if (FollowCamera && FollowCamera->IsValidLowLevel())
	{
		UE_LOG(LogTemp, Log, TEXT("FindFollowCamera: Using existing member variable"));
		return FollowCamera;
	}
	
	// ����2: ͨ�������
	UCameraComponent* FoundCamera = FindComponentByClass<UCameraComponent>();
	if (FoundCamera)
	{
		UE_LOG(LogTemp, Warning, TEXT("FindFollowCamera: Found by class search"));
		return FoundCamera;
	}
	
	// ����3: ���������������
	TArray<UActorComponent*> Components;
	GetComponents(Components);
	for (UActorComponent* Comp : Components)
	{
		UCameraComponent* Camera = Cast<UCameraComponent>(Comp);
		if (Camera)
		{
			UE_LOG(LogTemp, Warning, TEXT("FindFollowCamera: Found by component iteration: %s"), *Camera->GetName());
			return Camera;
		}
	}
	
	UE_LOG(LogTemp, Error, TEXT("FindFollowCamera: No CameraComponent found!"));
	return nullptr;
}

void AMyCharacter::ValidateAndCacheComponents()
{
	UE_LOG(LogTemp, Warning, TEXT("=== VALIDATING CAMERA COMPONENTS ==="));
	
	// ����CameraBoom
	if (!CameraBoom || !CameraBoom->IsValidLowLevel())
	{
		UE_LOG(LogTemp, Warning, TEXT("CameraBoom is null or invalid, attempting to find..."));
		CameraBoom = FindCameraBoomComponent();
	}
	
	// ����FollowCamera
	if (!FollowCamera || !FollowCamera->IsValidLowLevel())
	{
		UE_LOG(LogTemp, Warning, TEXT("FollowCamera is null or invalid, attempting to find..."));
		FollowCamera = FindFollowCameraComponent();
	}
	
	// ��֤���
	if (CameraBoom && FollowCamera)
	{
		UE_LOG(LogTemp, Warning, TEXT("? Camera components validated successfully"));
		UE_LOG(LogTemp, Warning, TEXT("  - CameraBoom: %s"), *CameraBoom->GetName());
		UE_LOG(LogTemp, Warning, TEXT("  - FollowCamera: %s"), *FollowCamera->GetName());
		
		// ��¼��ǰ״̬
		LogCameraState(TEXT("Validation"));
	}
	else
	{
		UE_LOG(LogTemp, Error, TEXT("? Camera component validation FAILED"));
		if (!CameraBoom) UE_LOG(LogTemp, Error, TEXT("  - CameraBoom is NULL"));
		if (!FollowCamera) UE_LOG(LogTemp, Error, TEXT("  - FollowCamera is NULL"));
		
		// �����������԰�������
		LogAllComponents();
	}
	
	UE_LOG(LogTemp, Warning, TEXT("===================================="));
}

// ==================== ������ö�̬�����ӿ�ʵ�� ====================

void AMyCharacter::ApplyCameraConfig(const FCameraSetupConfig& NewConfig)
{
	// === ��ȫ��飨�����Ա�̣�===
	if (!CameraBoom || !FollowCamera)
	{
		UE_LOG(LogTemp, Error, TEXT("ApplyCameraConfig: Camera components not found"));
		ValidateAndCacheComponents();
		if (!CameraBoom || !FollowCamera)
		{
			UE_LOG(LogTemp, Error, TEXT("ApplyCameraConfig: Failed to find camera components"));
			return;
		}
	}
	
	// === ȷ��ǰ��ֵ�����ڵ��ԣ�===
	float OldArmLength = CameraBoom->TargetArmLength;
	FRotator OldRotation = CameraBoom->GetRelativeRotation();
	FVector OldCameraOffset = FollowCamera->GetRelativeLocation();
	
	// === ���ؼ��޸���Ӧ������۳��� - ʹ��ƽ����ֵ ===
	if (CameraControlComponent)
	{
		// ʹ��Ĭ�ϲ�ֵ�ٶȣ�5.0f��
		CameraControlComponent->SetSpringArmLengthSmooth(NewConfig.ArmLength);
		
		if (bEnableCameraDebugLogs)
		{
			UE_LOG(LogTemp, Warning, TEXT("ApplyCameraConfig: Smoothly transitioning ArmLength from %.1f to %.1f"), 
				OldArmLength, NewConfig.ArmLength);
		}
	}
	else
	{
		// ���÷�����ֱ�����ã������ݣ�
		CameraBoom->TargetArmLength = NewConfig.ArmLength;
		
		if (bEnableCameraDebugLogs)
		{
			UE_LOG(LogTemp, Warning, TEXT("ApplyCameraConfig: Fallback - immediate ArmLength set to %.1f"), NewConfig.ArmLength);
		}
	}
	
	// === Ӧ��������������� ===
	CameraBoom->SetRelativeRotation(NewConfig.InitialRotation);
	CameraBoom->bUsePawnControlRotation = NewConfig.bUsePawnControlRotation;
	CameraBoom->bEnableCameraLag = NewConfig.bEnableCameraLag;
	CameraBoom->CameraLagSpeed = NewConfig.CameraLagSpeed;
	
	// === �޸���2����ȷӦ�������߶�ƫ�� ===
	FVector CameraOffset = NewConfig.SocketOffset;
	
	if (bIsLockedOn)
	{
		// �������ۼ������߶�ƫ�Ƶ����λ��
		CameraOffset += NewConfig.LockOnHeightOffset;
		
		UE_LOG(LogTemp, Warning, TEXT("ApplyCameraConfig: Lock-on active, applying height offset: %s"), 
			*NewConfig.LockOnHeightOffset.ToString());
	}
	
	// Ӧ�����յ����ƫ��
	FollowCamera->SetRelativeLocation(CameraOffset);
	
	// === ͬ������ CameraControlComponent ===
	if (CameraControlComponent)
	{
		FCameraSettings UpdatedSettings = CameraControlComponent->CameraSettings;
		// ȷ�� TargetLocationOffset ����ȷ����
		UpdatedSettings.TargetLocationOffset = TargetLocationOffset;
		CameraControlComponent->SetCameraSettings(UpdatedSettings);
		
		UE_LOG(LogTemp, Warning, TEXT("ApplyCameraConfig: Synced settings to CameraControlComponent"));
	}
	
	// === ��֤��־ ===
	UE_LOG(LogTemp, Warning, TEXT("=== ApplyCameraConfig Complete ==="));
	UE_LOG(LogTemp, Warning, TEXT("  ArmLength: %.1f -> %.1f (Smooth: %s)"), 
		OldArmLength, NewConfig.ArmLength, CameraControlComponent ? TEXT("YES") : TEXT("NO"));
	UE_LOG(LogTemp, Warning, TEXT("  Rotation: %s -> %s"), *OldRotation.ToString(), *NewConfig.InitialRotation.ToString());
	UE_LOG(LogTemp, Warning, TEXT("  CameraOffset: %s -> %s"), *OldCameraOffset.ToString(), *CameraOffset.ToString());
	UE_LOG(LogTemp, Warning, TEXT("  LockOnHeightOffset applied: %s"), bIsLockedOn ? TEXT("YES") : TEXT("NO"));
	UE_LOG(LogTemp, Warning, TEXT("==================================="));
}

void AMyCharacter::ResetCameraToDefaultConfig()
{
	ApplyCameraConfig(CameraSetupConfig);
	
	// ��������߶�ƫ��
	if (CameraBoom)
	{
		CameraBoom->TargetOffset = FVector::ZeroVector;
	}
	
	UE_LOG(LogTemp, Log, TEXT("Reset camera to default configuration"));
}

FCameraSetupConfig AMyCharacter::GetCurrentCameraConfig() const
{
	FCameraSetupConfig CurrentConfig;
	
	if (CameraBoom && FollowCamera)
	{
		CurrentConfig.ArmLength = CameraBoom->TargetArmLength;
		CurrentConfig.InitialRotation = CameraBoom->GetRelativeRotation();
		CurrentConfig.SocketOffset = FollowCamera->GetRelativeLocation();
		CurrentConfig.bUsePawnControlRotation = CameraBoom->bUsePawnControlRotation;
		CurrentConfig.bEnableCameraLag = CameraBoom->bEnableCameraLag;
		CurrentConfig.CameraLagSpeed = CameraBoom->CameraLagSpeed;
		CurrentConfig.LockOnHeightOffset = CameraBoom->TargetOffset;
	}
	
	return CurrentConfig;
}

// ==================== UI��������ʵ�� ====================

void AMyCharacter::HideAllLockOnWidgets()
{
	// �������к�ѡĿ���UI
	for (AActor* Candidate : LockOnCandidates)
	{
		if (IsValid(Candidate))
		{
			UActorComponent* WidgetComp = Candidate->GetComponentByClass(UWidgetComponent::StaticClass());
			if (WidgetComp)
			{
				UWidgetComponent* WidgetComponent = Cast<UWidgetComponent>(WidgetComp);
				if (WidgetComponent && WidgetComponent->IsVisible())
				{
					WidgetComponent->SetVisibility(false);
				}
			}
		}
	}
	
	// ���ʹ��SocketͶ��UI��Ҳ������
	if (bUseSocketProjection && LockOnWidgetInstance && LockOnWidgetInstance->IsInViewport())
	{
		LockOnWidgetInstance->SetVisibility(ESlateVisibility::Hidden);
	}
}

// ==================== ������������ηʵ�� ====================

bool AMyCharacter::HasCandidatesInSphere()
{
	// ȷ����ѡ�б������µ�
	if (LockOnCandidates.Num() == 0)
	{
		FindLockOnCandidates();
	}
	
	return LockOnCandidates.Num() > 0;
}

AActor* AMyCharacter::TryGetSectorLockTarget()
{
	if (TargetDetectionComponent)
	{
		return TargetDetectionComponent->TryGetSectorLockTarget();
	}
	else
	{
		UE_LOG(LogTemp, Error, TEXT("TryGetSectorLockTarget: TargetDetectionComponent is null!"));
		return nullptr;
	}
}

AActor* AMyCharacter::TryGetCameraCorrectionTarget()
{
	if (TargetDetectionComponent)
	{
		return TargetDetectionComponent->TryGetCameraCorrectionTarget();
	}
	else
	{
		UE_LOG(LogTemp, Error, TEXT("TryGetCameraCorrectionTarget: TargetDetectionComponent is null!"));
		return nullptr;
	}
}

void AMyCharacter::StartCameraCorrectionForTarget(AActor* Target)
{
	if (CameraControlComponent && Target)
	{
		CameraControlComponent->StartCameraCorrectionForTarget(Target);
	}
	else
	{
		UE_LOG(LogTemp, Error, TEXT("StartCameraCorrectionForTarget: CameraControlComponent or Target is null!"));
	}
}

void AMyCharacter::StartCameraAutoCorrection(AActor* Target)
{
	if (CameraControlComponent && Target)
	{
		CameraControlComponent->StartCameraAutoCorrection(Target);
	}
	else
	{
		UE_LOG(LogTemp, Error, TEXT("StartCameraAutoCorrection: CameraControlComponent or Target is null!"));
	}
}

void AMyCharacter::UpdateCameraAutoCorrection()
{
	if (CameraControlComponent)
	{
		CameraControlComponent->UpdateCameraAutoCorrection();
	}
	else
	{
		UE_LOG(LogTemp, Error, TEXT("UpdateCameraAutoCorrection: CameraControlComponent is null!"));
	}
}

void AMyCharacter::DelayedCameraCorrection()
{
	if (CameraControlComponent)
	{
		CameraControlComponent->DelayedCameraCorrection();
	}
	else
	{
		UE_LOG(LogTemp, Error, TEXT("DelayedCameraCorrection: CameraControlComponent is null!"));
	}
}

void AMyCharacter::RestoreCameraFollowState()
{
	if (CameraControlComponent)
	{
		CameraControlComponent->RestoreCameraFollowState();
	}
	else
	{
		UE_LOG(LogTemp, Error, TEXT("RestoreCameraFollowState: CameraControlComponent is null!"));
	}
}

// ==================== ������������ ====================

void AMyCharacter::SetupDebugCommands()
{
	UE_LOG(LogTemp, Warning, TEXT("=== DEBUG COMMANDS AVAILABLE ==="));
	UE_LOG(LogTemp, Warning, TEXT("Press F: General debug info"));
	UE_LOG(LogTemp, Warning, TEXT("Press G: Display target sizes"));
	UE_LOG(LogTemp, Warning, TEXT("================================"));
}

// ==================== �༭��ʵʱ���½ӿ�ʵ�� ====================

#if WITH_EDITOR

void AMyCharacter::PostEditChangeProperty(FPropertyChangedEvent& PropertyChangedEvent)
{
	Super::PostEditChangeProperty(PropertyChangedEvent);
	
	if (!PropertyChangedEvent.Property)
		return;
	
	FName PropertyName = PropertyChangedEvent.Property->GetFName();
	
	// ����������ýṹ����޸�
	if (PropertyName == GET_MEMBER_NAME_CHECKED(AMyCharacter, CameraSetupConfig))
	{
		// ������������ڣ�Ӧ��������
		if (CameraBoom && FollowCamera)
		{
			ApplyCameraConfig(CameraSetupConfig);
			
			// ǿ��ˢ�±༭���ӿ�
			CameraBoom->MarkRenderStateDirty();
			FollowCamera->MarkRenderStateDirty();
			
			UE_LOG(LogTemp, Warning, TEXT("Editor: Camera config struct updated - ArmLength=%.1f"), 
				CameraSetupConfig.ArmLength);
		}
	}
	
	// ԭʼ���뱣���������Χ�Ĵ���
	if (PropertyName == GET_MEMBER_NAME_CHECKED(AMyCharacter, LockOnRange))
	{
		if (LockOnDetectionSphere)
		{
			LockOnDetectionSphere->SetSphereRadius(LockOnRange);
			UE_LOG(LogTemp, Warning, TEXT("Editor: Detection sphere radius updated to %.1f"), 
				LockOnRange);
		}
		
		if (TargetDetectionComponent)
		{
			TargetDetectionComponent->LockOnSettings.LockOnRange = LockOnRange;
		}
	}
}

void AMyCharacter::PreviewCameraConfigInEditor()
{
	// ��ȫ��飨��ȫ�ܷ��������Α�̣�
	if (!CameraBoom || !FollowCamera)
	{
		UE_LOG(LogTemp, Error, TEXT("PreviewCameraConfig: Camera components not found"));
		ValidateAndCacheComponents();
		return;
	}
	
	// Ӧ�õ�ǰ���ã�ʹ�����еİ�ȫ������
	ApplyCameraConfig(CameraSetupConfig);
	
	// ǿ��ˢ���ӿ���ʾ
	CameraBoom->MarkRenderStateDirty();
	FollowCamera->MarkRenderStateDirty();
	
	// �����ǰ������Ϣ
	UE_LOG(LogTemp, Warning, TEXT("=== Camera Config Preview Applied ==="));
	UE_LOG(LogTemp, Warning, TEXT("Arm Length: %.1f"), CameraSetupConfig.ArmLength);
	UE_LOG(LogTemp, Warning, TEXT("Initial Rotation: Pitch=%.1f, Yaw=%.1f, Roll=%.1f"), 
		CameraSetupConfig.InitialRotation.Pitch,
		CameraSetupConfig.InitialRotation.Yaw,
		CameraSetupConfig.InitialRotation.Roll);
	UE_LOG(LogTemp, Warning, TEXT("Socket Offset: X=%.1f, Y=%.1f, Z=%.1f"), 
		CameraSetupConfig.SocketOffset.X,
		CameraSetupConfig.SocketOffset.Y,
		CameraSetupConfig.SocketOffset.Z);
	UE_LOG(LogTemp, Warning, TEXT("Camera Lag: %s, Speed: %.1f"),
		CameraSetupConfig.bEnableCameraLag ? TEXT("Enabled") : TEXT("Disabled"),
		CameraSetupConfig.CameraLagSpeed);
	UE_LOG(LogTemp, Warning, TEXT("====================================="));
}

void AMyCharacter::ResetCameraConfigInEditor()
{
	// ����Ĭ������
	FCameraSetupConfig DefaultConfig;
	
	// ���������������־
	FCameraSetupConfig OldConfig = CameraSetupConfig;
	
	// Ӧ��Ĭ������
	CameraSetupConfig = DefaultConfig;
	ApplyCameraConfig(CameraSetupConfig);
	
	UE_LOG(LogTemp, Warning, TEXT("Camera config reset: ArmLength %.1f -> %.1f"), 
		OldConfig.ArmLength, DefaultConfig.ArmLength);
}

void AMyCharacter::PostEditMove(bool bFinished)
{
	Super::PostEditMove(bFinished);
	
	if (bFinished && bEnableCameraDebugLogs)
	{
		UE_LOG(LogTemp, Log, TEXT("Editor: Character moved, final position: %s"), 
			*GetActorLocation().ToString());
	}
}

void AMyCharacter::SyncCameraConfigFromComponents()
{
	if (!CameraBoom || !FollowCamera)
	{
		UE_LOG(LogTemp, Error, TEXT("SyncCameraConfig: Camera components not available"));
		return;
	}
	
	// �������ȡ��ǰ��ֵ����������
	CameraSetupConfig.ArmLength = CameraBoom->TargetArmLength;
	CameraSetupConfig.InitialRotation = CameraBoom->GetRelativeRotation();
	CameraSetupConfig.SocketOffset = FollowCamera->GetRelativeLocation();
	CameraSetupConfig.bUsePawnControlRotation = CameraBoom->bUsePawnControlRotation;
	CameraSetupConfig.bEnableCameraLag = CameraBoom->bEnableCameraLag;
	CameraSetupConfig.CameraLagSpeed = CameraBoom->CameraLagSpeed;
	
	UE_LOG(LogTemp, Warning, TEXT("Synced config from components: ArmLength=%.1f, Pitch=%.1f"), 
		CameraSetupConfig.ArmLength, CameraSetupConfig.InitialRotation.Pitch);
}

#endif // WITH_EDITOR

// ==================== ����ȱʧ�ĺ���ʵ�� ====================

void AMyCharacter::HandleRightStickX(float Value)
{
	// ==================== 非锁定状态：用于相机旋转 ====================
	if (!bIsLockedOn)
	{
		// 直接调用 Turn 函数实现相机水平旋转
		if (FMath::Abs(Value) > 0.1f)
		{
			Turn(Value);
		}
		LastRightStickX = Value;
		return;
	}

	// ==================== 锁定状态：用于切换目标 ====================
	// ==================== 1. 冷却时间检查 ====================
	float CurrentTime = GetWorld()->GetTimeSeconds();
	if (CurrentTime - LastTargetSwitchTime < TargetSwitchCooldown)
	{
		// 还在冷却中，更新状态但不切换
		LastRightStickX = Value;
		return;
	}

	// ==================== 2. 边缘检测（只在跨越阈值瞬间触发）====================
	bool bWasLeftPressed = (LastRightStickX < -THUMBSTICK_THRESHOLD);
	bool bIsLeftPressed = (Value < -THUMBSTICK_THRESHOLD);
	bool bWasRightPressed = (LastRightStickX > THUMBSTICK_THRESHOLD);
	bool bIsRightPressed = (Value > THUMBSTICK_THRESHOLD);

	// ==================== 3. 只在新按下且上一帧未按下的情况下标为"按下"====================
	if (bIsLeftPressed && !bWasLeftPressed)
	{
		// 向左切换目标
		SwitchLockOnTargetLeft();
		LastTargetSwitchTime = CurrentTime; // 记录操作时间，启动冷却
		
		if (bEnableLockOnDebugLogs)
		{
			UE_LOG(LogTemp, Log, TEXT("HandleRightStickX: Switched target LEFT (Value=%.2f)"), Value);
		}
	}
	else if (bIsRightPressed && !bWasRightPressed)
	{
		// 向右切换目标
		SwitchLockOnTargetRight();
		LastTargetSwitchTime = CurrentTime; // 记录操作时间，启动冷却
		
		if (bEnableLockOnDebugLogs)
		{
			UE_LOG(LogTemp, Log, TEXT("HandleRightStickX: Switched target RIGHT (Value=%.2f)"), Value);
		}
	}

	// ==================== 4. 记录这一帧的值（用于下次边缘检测）====================
	LastRightStickX = Value;
}

// ==================== 缺失的函数实现补充 ====================

void AMyCharacter::FindLockOnCandidates()
{
	if (TargetDetectionComponent)
	{
		TargetDetectionComponent->FindLockOnCandidates();
		LockOnCandidates = TargetDetectionComponent->GetLockOnCandidates();
	}
	else
	{
		UE_LOG(LogTemp, Error, TEXT("FindLockOnCandidates: TargetDetectionComponent is null!"));
	}
}

void AMyCharacter::UpdateLockOnTarget()
{
	if (!bIsLockedOn || !CurrentLockOnTarget)
		return;
	
	// 检查目标是否仍然有效
	if (!IsTargetStillLockable(CurrentLockOnTarget))
	{
		float Distance = FVector::Dist(GetActorLocation(), CurrentLockOnTarget->GetActorLocation());
		float ExtendedRange = LockOnRange * EXTENDED_LOCK_RANGE_MULTIPLIER;
		
		if (bEnableLockOnDebugLogs)
		{
			UE_LOG(LogTemp, Warning, TEXT("=== AUTO UNLOCK TRIGGERED ==="));
			UE_LOG(LogTemp, Warning, TEXT("Target '%s' no longer lockable"), *CurrentLockOnTarget->GetName());
			UE_LOG(LogTemp, Warning, TEXT("Distance: %.1f / Extended Range: %.1f"), Distance, ExtendedRange);
			UE_LOG(LogTemp, Warning, TEXT("Calling CancelLockOn()..."));
		}
		
		// 调用CancelLockOn会触发UI清理
		CancelLockOn();
		
		if (bEnableLockOnDebugLogs)
		{
			UE_LOG(LogTemp, Warning, TEXT("CancelLockOn() completed"));
			UE_LOG(LogTemp, Warning, TEXT("bIsLockedOn: %s"), bIsLockedOn ? TEXT("TRUE") : TEXT("FALSE"));
			UE_LOG(LogTemp, Warning, TEXT("CurrentLockOnTarget: %s"), CurrentLockOnTarget ? TEXT("NOT NULL!") : TEXT("NULL"));
			UE_LOG(LogTemp, Warning, TEXT("=============================="));
		}
	}
}

bool AMyCharacter::IsTargetStillLockable(AActor* Target)
{
	if (!Target || !IsValid(Target))
		return false;
	
	float Distance = FVector::Dist(GetActorLocation(), Target->GetActorLocation());
	float ExtendedRange = LockOnRange * EXTENDED_LOCK_RANGE_MULTIPLIER;
	
	return Distance <= ExtendedRange;
}

bool AMyCharacter::IsValidLockOnTarget(AActor* Target)
{
	if (!Target || !IsValid(Target))
		return false;
	
	// 检查距离
	float Distance = FVector::Dist(GetActorLocation(), Target->GetActorLocation());
	if (Distance > LockOnRange)
		return false;
	
	// 检查角度
	FVector ToTarget = (Target->GetActorLocation() - GetActorLocation()).GetSafeNormal();
	FVector Forward = GetActorForwardVector();
	float Dot = FVector::DotProduct(Forward, ToTarget);
	float Angle = FMath::Acos(Dot) * (180.0f / PI);
	
	return Angle <= (LockOnAngle / 2.0f);
}

void AMyCharacter::SwitchLockOnTargetLeft()
{
	if (!bIsLockedOn || LockOnCandidates.Num() == 0)
		return;
	
	// 排序候选目标（从左到右）
	TArray<AActor*> SortedTargets = LockOnCandidates;
	SortCandidatesByDirection(SortedTargets);
	
	// 找到当前目标的索引
	int32 CurrentIndex = SortedTargets.Find(CurrentLockOnTarget);
	if (CurrentIndex == INDEX_NONE)
	{
		// 当前目标不在列表中，选择最左边的目标
		if (SortedTargets.Num() > 0)
		{
			StartSmoothTargetSwitch(SortedTargets[0]);
		}
		return;
	}
	
	// 切换到左边的目标
	int32 NextIndex = (CurrentIndex > 0) ? (CurrentIndex - 1) : (SortedTargets.Num() - 1);
	if (NextIndex != CurrentIndex && SortedTargets.IsValidIndex(NextIndex))
	{
		StartSmoothTargetSwitch(SortedTargets[NextIndex]);
	}
}

void AMyCharacter::SwitchLockOnTargetRight()
{
	if (!bIsLockedOn || LockOnCandidates.Num() == 0)
		return;
	
	// 排序候选目标（从左到右）
	TArray<AActor*> SortedTargets = LockOnCandidates;
	SortCandidatesByDirection(SortedTargets);
	
	// 找到当前目标的索引
	int32 CurrentIndex = SortedTargets.Find(CurrentLockOnTarget);
	if (CurrentIndex == INDEX_NONE)
	{
		// 当前目标不在列表中，选择最右边的目标
		if (SortedTargets.Num() > 0)
		{
			StartSmoothTargetSwitch(SortedTargets.Last());
		}
		return;
	}
	
	// 切换到右边的目标
	int32 NextIndex = (CurrentIndex < SortedTargets.Num() - 1) ? (CurrentIndex + 1) : 0;
	if (NextIndex != CurrentIndex && SortedTargets.IsValidIndex(NextIndex))
	{
		StartSmoothTargetSwitch(SortedTargets[NextIndex]);
	}
}

void AMyCharacter::UpdateCharacterRotationToTarget()
{
	if (CameraControlComponent)
	{
		CameraControlComponent->UpdateCharacterRotationToTarget();
	}
}

float AMyCharacter::CalculateAngleToTarget(AActor* Target) const
{
	if (!Target)
		return 0.0f;
	
	FVector PlayerLocation = GetActorLocation();
	FVector TargetLocation = Target->GetActorLocation();
	FVector PlayerForward = GetActorForwardVector();
	FVector DirectionToTarget = (TargetLocation - PlayerLocation).GetSafeNormal();
	
	float DotProduct = FVector::DotProduct(PlayerForward, DirectionToTarget);
	return FMath::Acos(DotProduct) * (180.0f / PI);
}

float AMyCharacter::CalculateDirectionAngle(AActor* Target) const
{
	if (!Target)
		return 0.0f;
	
	FVector PlayerLocation = GetActorLocation();
	FVector TargetLocation = Target->GetActorLocation();
	FVector DirectionToTarget = (TargetLocation - PlayerLocation).GetSafeNormal();
	
	FVector PlayerForward = GetActorForwardVector();
	FVector PlayerRight = GetActorRightVector();
	
	float ForwardDot = FVector::DotProduct(PlayerForward, DirectionToTarget);
	float RightDot = FVector::DotProduct(PlayerRight, DirectionToTarget);
	
	return FMath::Atan2(RightDot, ForwardDot) * (180.0f / PI);
}

void AMyCharacter::SortCandidatesByDirection(TArray<AActor*>& Targets)
{
	Targets.Sort([this](const AActor& A, const AActor& B)
	{
		float AngleA = CalculateDirectionAngle(const_cast<AActor*>(&A));
		float AngleB = CalculateDirectionAngle(const_cast<AActor*>(&B));
		return AngleA < AngleB;
	});
}

void AMyCharacter::StartSmoothTargetSwitch(AActor* NewTarget)
{
	if (CameraControlComponent && NewTarget)
	{
		CameraControlComponent->StartSmoothTargetSwitch(NewTarget);
	}
}

void AMyCharacter::UpdateSmoothTargetSwitch()
{
	if (CameraControlComponent)
	{
		CameraControlComponent->UpdateSmoothTargetSwitch();
	}
}

void AMyCharacter::UpdateLockOnCamera()
{
	if (CameraControlComponent)
	{
		CameraControlComponent->UpdateLockOnCamera();
	}
}

void AMyCharacter::HandleLockOnButton()
{
	ToggleLockOn();
}

void AMyCharacter::ShowLockOnWidget()
{
	if (UIManagerComponent && CurrentLockOnTarget)
	{
		UIManagerComponent->ShowLockOnWidget(CurrentLockOnTarget);
	}
}

void AMyCharacter::HideLockOnWidget()
{
	if (UIManagerComponent)
	{
		UIManagerComponent->HideLockOnWidget();
	}
}

void AMyCharacter::UpdateLockOnWidget()
{
	if (UIManagerComponent && CurrentLockOnTarget)
	{
		UIManagerComponent->UpdateLockOnWidget(CurrentLockOnTarget, PreviousLockOnTarget);
	}
}

void AMyCharacter::DebugInputTest()
{
	UE_LOG(LogTemp, Warning, TEXT("=== Debug Input Test ==="));
	UE_LOG(LogTemp, Warning, TEXT("Is Locked On: %s"), bIsLockedOn ? TEXT("YES") : TEXT("NO"));
	if (CurrentLockOnTarget)
	{
		UE_LOG(LogTemp, Warning, TEXT("Current Target: %s"), *CurrentLockOnTarget->GetName());
	}
	UE_LOG(LogTemp, Warning, TEXT("Candidates: %d"), LockOnCandidates.Num());
	UE_LOG(LogTemp, Warning, TEXT("======================"));
}

void AMyCharacter::DebugWidgetSetup()
{
	UE_LOG(LogTemp, Warning, TEXT("=== Widget Setup Debug ==="));
	UE_LOG(LogTemp, Warning, TEXT("Widget Class Set: %s"), LockOnWidgetClass ? TEXT("YES") : TEXT("NO"));
	UE_LOG(LogTemp, Warning, TEXT("Widget Instance: %s"), LockOnWidgetInstance ? TEXT("EXISTS") : TEXT("NULL"));
	UE_LOG(LogTemp, Warning, TEXT("========================"));
}

bool AMyCharacter::IsTargetInSectorLockZone(AActor* Target) const
{
	if (!Target)
		return false;
	
	float Angle = CalculateAngleToTarget(Target);
	return Angle <= (SECTOR_LOCK_ANGLE / 2.0f);
}

bool AMyCharacter::IsTargetInEdgeDetectionZone(AActor* Target) const
{
	if (!Target)
		return false;
	
	float Angle = CalculateAngleToTarget(Target);
	return Angle <= (EDGE_DETECTION_ANGLE / 2.0f);
}

AActor* AMyCharacter::GetBestSectorLockTarget()
{
	TArray<AActor*> SectorTargets;
	for (AActor* Target : LockOnCandidates)
	{
		if (IsTargetInSectorLockZone(Target))
		{
			SectorTargets.Add(Target);
		}
	}
	
	return GetBestTargetFromList(SectorTargets);
}

AActor* AMyCharacter::GetBestTargetFromList(const TArray<AActor*>& TargetList)
{
	if (TargetList.Num() == 0)
		return nullptr;
	
	AActor* BestTarget = nullptr;
	float BestScore = -1.0f;
	
	for (AActor* Target : TargetList)
	{
		if (!IsValidLockOnTarget(Target))
			continue;
		
		float Distance = FVector::Dist(GetActorLocation(), Target->GetActorLocation());
		float Angle = CalculateAngleToTarget(Target);
		
		// 评分：距离越近、角度越小越好
		float Score = 1.0f / (Distance + 100.0f) + 1.0f / (Angle + 1.0f);
		
		if (Score > BestScore)
		{
			BestScore = Score;
			BestTarget = Target;
		}
	}
	
	return BestTarget;
}

AActor* AMyCharacter::GetBestLockOnTarget()
{
	return GetBestTargetFromList(LockOnCandidates);
}

void AMyCharacter::DrawLockOnCursor()
{
	// 仅在开发版本中启用
#if !UE_BUILD_SHIPPING
	if (bIsLockedOn && CurrentLockOnTarget && GEngine)
	{
		FVector TargetLocation = GetOptimalLockPosition(CurrentLockOnTarget);
		FVector2D ScreenLocation;
		APlayerController* PC = Cast<APlayerController>(GetController());
		if (PC && PC->ProjectWorldLocationToScreen(TargetLocation, ScreenLocation))
		{
			GEngine->AddOnScreenDebugMessage(-1, 0.0f, FColor::Red, 
				FString::Printf(TEXT("Target: %s"), *CurrentLockOnTarget->GetName()), false);
		}
	}
#endif
}

EEnemySizeCategory AMyCharacter::GetTargetSizeCategory(AActor* Target)
{
	if (TargetDetectionComponent)
	{
		return TargetDetectionComponent->GetTargetSizeCategory(Target);
	}
	return EEnemySizeCategory::Medium;
}

TArray<AActor*> AMyCharacter::GetTargetsBySize(EEnemySizeCategory SizeCategory)
{
	TArray<AActor*> Result;
	for (AActor* Target : LockOnCandidates)
	{
		if (GetTargetSizeCategory(Target) == SizeCategory)
		{
			Result.Add(Target);
		}
	}
	return Result;
}

AActor* AMyCharacter::GetNearestTargetBySize(EEnemySizeCategory SizeCategory)
{
	TArray<AActor*> TargetsOfSize = GetTargetsBySize(SizeCategory);
	AActor* NearestTarget = nullptr;
	float MinDistance = FLT_MAX;
	
	for (AActor* Target : TargetsOfSize)
	{
		float Distance = FVector::Dist(GetActorLocation(), Target->GetActorLocation());
		if (Distance < MinDistance)
		{
			MinDistance = Distance;
			NearestTarget = Target;
		}
	}
	
	return NearestTarget;
}

TMap<EEnemySizeCategory, int32> AMyCharacter::GetSizeCategoryStatistics()
{
	TMap<EEnemySizeCategory, int32> Stats;
	Stats.Add(EEnemySizeCategory::Small, 0);
	Stats.Add(EEnemySizeCategory::Medium, 0);
	Stats.Add(EEnemySizeCategory::Large, 0);
	
	for (AActor* Target : LockOnCandidates)
	{
		EEnemySizeCategory Category = GetTargetSizeCategory(Target);
		Stats[Category]++;
	}
	
	return Stats;
}

void AMyCharacter::TestCameraSystem()
{
	UE_LOG(LogTemp, Warning, TEXT("=== Camera System Test ==="));
	UE_LOG(LogTemp, Warning, TEXT("CameraBoom: %s"), CameraBoom ? TEXT("OK") : TEXT("NULL"));
	UE_LOG(LogTemp, Warning, TEXT("FollowCamera: %s"), FollowCamera ? TEXT("OK") : TEXT("NULL"));
	UE_LOG(LogTemp, Warning, TEXT("CameraControlComponent: %s"), CameraControlComponent ? TEXT("OK") : TEXT("NULL"));
	UE_LOG(LogTemp, Warning, TEXT("=========================="));
}

FVector AMyCharacter::GetTargetSocketWorldLocation(AActor* Target) const
{
	if (!Target)
		return FVector::ZeroVector;
	
	USkeletalMeshComponent* TargetMesh = Target->FindComponentByClass<USkeletalMeshComponent>();
	if (TargetMesh && TargetMesh->DoesSocketExist(TargetSocketName))
	{
		return TargetMesh->GetSocketLocation(TargetSocketName) + SocketOffset;
	}
	
	return Target->GetActorLocation() + SocketOffset;
}

bool AMyCharacter::HasValidSocket(AActor* Target) const
{
	if (!Target)
		return false;
	
	USkeletalMeshComponent* TargetMesh = Target->FindComponentByClass<USkeletalMeshComponent>();
	return TargetMesh && TargetMesh->DoesSocketExist(TargetSocketName);
}

FVector2D AMyCharacter::ProjectSocketToScreen(const FVector& SocketWorldLocation) const
{
	FVector2D ScreenLocation;
	APlayerController* PC = Cast<APlayerController>(GetController());
	if (PC)
	{
		PC->ProjectWorldLocationToScreen(SocketWorldLocation, ScreenLocation);
	}
	return ScreenLocation;
}

void AMyCharacter::ShowSocketProjectionWidget()
{
	if (UIManagerComponent && CurrentLockOnTarget)
	{
		UIManagerComponent->ShowLockOnWidget(CurrentLockOnTarget);
	}
}

void AMyCharacter::UpdateSocketProjectionWidget()
{
	if (UIManagerComponent && CurrentLockOnTarget)
	{
		UIManagerComponent->UpdateProjectionWidget(CurrentLockOnTarget);
	}
}

void AMyCharacter::HideSocketProjectionWidget()
{
	if (UIManagerComponent)
	{
		UIManagerComponent->HideLockOnWidget();
	}
}

FVector AMyCharacter::GetTargetSocketLocation(AActor* Target) const
{
	return GetTargetSocketWorldLocation(Target);
}

void AMyCharacter::DebugDisplayTargetSizes()
{
	UE_LOG(LogTemp, Warning, TEXT("=== Target Size Debug ==="));
	UE_LOG(LogTemp, Warning, TEXT("Total Candidates: %d"), LockOnCandidates.Num());
	
	for (AActor* Target : LockOnCandidates)
	{
		if (Target)
		{
			EEnemySizeCategory Category = GetTargetSizeCategory(Target);
			FString CategoryStr;
			switch (Category)
			{
				case EEnemySizeCategory::Small:
					CategoryStr = TEXT("Small");
					break;
				case EEnemySizeCategory::Medium:
					CategoryStr = TEXT("Medium");
					break;
				case EEnemySizeCategory::Large:
					CategoryStr = TEXT("Large");
					break;
				default:
					CategoryStr = TEXT("Unknown");
			}
			UE_LOG(LogTemp, Warning, TEXT("- %s: %s"), *Target->GetName(), *CategoryStr);
		}
	}
	
	UE_LOG(LogTemp, Warning, TEXT("========================"));
}

void AMyCharacter::LogAllComponents()
{
	UE_LOG(LogTemp, Warning, TEXT("=== All Components ==="));
	TArray<UActorComponent*> Components;
	GetComponents(Components);
	for (UActorComponent* Comp : Components)
	{
		UE_LOG(LogTemp, Warning, TEXT("- %s"), *Comp->GetName());
	}
	UE_LOG(LogTemp, Warning, TEXT("====================="));
}

void AMyCharacter::LogCameraState(const FString& Context)
{
	if (CameraBoom && FollowCamera)
	{
		UE_LOG(LogTemp, Warning, TEXT("=== Camera State (%s) ==="), *Context);
		UE_LOG(LogTemp, Warning, TEXT("Arm Length: %.1f"), CameraBoom->TargetArmLength);
		UE_LOG(LogTemp, Warning, TEXT("Rotation: %s"), *CameraBoom->GetRelativeRotation().ToString());
		UE_LOG(LogTemp, Warning, TEXT("Camera Offset: %s"), *FollowCamera->GetRelativeLocation().ToString());
		UE_LOG(LogTemp, Warning, TEXT("============================"));
	}
}
