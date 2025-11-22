// Fill out your copyright notice in the Description page of Project Settings.

#include "CameraControlComponent.h"
#include "GameFramework/Character.h"
#include "GameFramework/PlayerController.h"
#include "Camera/CameraComponent.h"
#include "GameFramework/SpringArmComponent.h"
#include "Components/SkeletalMeshComponent.h"
#include "Components/CapsuleComponent.h"
#include "Kismet/KismetMathLibrary.h"
#include "Kismet/GameplayStatics.h"
#include "Engine/World.h"
#include "Engine/Engine.h"
#include "DrawDebugHelpers.h"
#include "HAL/IConsoleManager.h"
#include "UObject/UObjectIterator.h"
#include "TargetDetectionComponent.h" // 新增：需要调用 IsTargetStillLockable

// 控制台命令定义
static TAutoConsoleVariable<int32> CVarCameraDebugLevel(
	TEXT("Camera.DebugLevel"),
	0,
	TEXT("Camera debug visualization level (0=Off, 1=Basic, 2=Detailed, 3=Full)"),
	ECVF_Cheat
);

static TAutoConsoleVariable<float> CVarCameraInterpSpeed(
	TEXT("Camera.InterpSpeed"),
	5.0f,
	TEXT("Camera interpolation speed"),
	ECVF_Cheat
);

static TAutoConsoleVariable<bool> CVarCameraShowTargetInfo(
	TEXT("Camera.ShowTargetInfo"),
	false,
	TEXT("Show target information overlay"),
	ECVF_Cheat
);

// 控制台命令函数
static FAutoConsoleCommand CmdResetCamera(
	TEXT("Camera.Reset"),
	TEXT("Reset camera to default position"),
	FConsoleCommandDelegate::CreateLambda([]()
	{
		if (GEngine)
		{
			for (TObjectIterator<UCameraControlComponent> It; It; ++It)
			{
				if (It->GetWorld() && !It->GetWorld()->bIsTearingDown)
				{
					It->ResetCameraToDefault();
					UE_LOG(LogTemp, Warning, TEXT("Camera reset to default"));
				}
			}
		}
	})
);

static FAutoConsoleCommand CmdToggleFreeLook(
	TEXT("Camera.ToggleFreeLook"),
	TEXT("Toggle free look mode"),
	FConsoleCommandDelegate::CreateLambda([]()
	{
		for (TObjectIterator<UCameraControlComponent> It; It; ++It)
		{
			if (It->GetWorld() && !It->GetWorld()->bIsTearingDown)
			{
				It->FreeLookSettings.bEnableFreeLook = !It->FreeLookSettings.bEnableFreeLook;
				UE_LOG(LogTemp, Warning, TEXT("FreeLook: %s"), 
					It->FreeLookSettings.bEnableFreeLook ? TEXT("Enabled") : TEXT("Disabled"));
			}
		}
	})
);

// Sets default values for this component's properties
UCameraControlComponent::UCameraControlComponent()
{
	// Set this component to be ticked every frame
	PrimaryComponentTick.bCanEverTick = true;

	// ==================== 初始化状态变量 ====================
	CurrentCameraState = ECameraState::Normal;
	CurrentLockOnTarget = nullptr;
	PreviousLockOnTarget = nullptr;
	
	// 初始化组件缓存
	CachedSpringArm = nullptr;
	CachedCamera = nullptr;
	
	// 相机跟随初始化
	bShouldCameraFollowTarget = true;
	bShouldCharacterRotateToTarget = true;
	bPlayerIsMoving = false;

	// 平滑切换状态初始化
	bIsSmoothSwitching = false;
	SmoothSwitchStartTime = 0.0f;
	SmoothSwitchStartRotation = FRotator::ZeroRotator;
	SmoothSwitchTargetRotation = FRotator::ZeroRotator;
	bShouldSmoothSwitchCamera = false;
	bShouldSmoothSwitchCharacter = false;

	// 自动修正状态初始化
	bIsCameraAutoCorrection = false;
	CameraCorrectionStartTime = 0.0f;
	CameraCorrectionStartRotation = FRotator::ZeroRotator;
	CameraCorrectionTargetRotation = FRotator::ZeroRotator;
	DelayedCorrectionTarget = nullptr;

	// 重置相机状态初始化
	bIsSmoothCameraReset = false;
	SmoothResetStartTime = 0.0f;
	SmoothResetStartRotation = FRotator::ZeroRotator;
	SmoothResetTargetRotation = FRotator::ZeroRotator;

	// ==================== Spring Arm平滑插值初始化（新增）====================
	bIsInterpolatingArmLength = false;
	ArmLengthInterpStart = 0.0f;
	ArmLengthInterpTarget = 0.0f;
	ArmLengthInterpStartTime = 0.0f;
	ArmLengthInterpSpeed = 5.0f;

	// ==================== Socket Offset 平滑重置初始化（新增）====================
	bIsResetingSocketOffset = false;
	SocketOffsetResetStartTime = 0.0f;
	SocketOffsetResetStart = FVector::ZeroVector;
	SocketOffsetResetTarget = FVector::ZeroVector;
	SocketOffsetResetSpeed = 5.0f;

	// 高级相机距离响应初始化
	bIsAdvancedCameraAdjustment = false;
	LastAdvancedAdjustmentTime = 0.0f;
	CurrentTargetSizeCategory = EEnemySizeCategory::Medium;
	CurrentTargetDistance = 0.0f;

	// 调试控制初始化
	bEnableCameraDebugLogs = true;
	bEnableAdvancedAdjustmentDebugLogs = false;

	// 相机臂动态调整初始化
	BaseArmLength = 0.0f;
	bArmLengthAdjusted = false;

	// 初始化FreeLook
	FreeLookSettings = FFreeLookSettings();
	FreeLookOffset = FRotator::ZeroRotator;
	LastFreeLookInputTime = 0.0f;
	bIsFreeLooking = false;

	// 初始化3D视口控制
	OrbitRotation = FRotator::ZeroRotator;
	bIsPreviewingCamera = false;

	// 初始化递归防护
	RecursionDepth = 0;
}

// Called when the game starts
void UCameraControlComponent::BeginPlay()
{
	Super::BeginPlay();

	// 延迟一帧进行验证，确保PostInitializeComponents已执行
	GetWorld()->GetTimerManager().SetTimerForNextTick([this]()
	{
		if (!CachedSpringArm || !CachedCamera)
		{
			UE_LOG(LogTemp, Error, TEXT("Camera components not initialized properly"));
			// 尝试恢复
			AActor* Owner = GetOwner();
			if (Owner)
			{
				CachedSpringArm = Owner->FindComponentByClass<USpringArmComponent>();
				CachedCamera = Owner->FindComponentByClass<UCameraComponent>();
			}
		}
		
		if (CachedSpringArm)
		{
			BaseArmLength = CachedSpringArm->TargetArmLength;
			BaseSocketOffset = CachedSpringArm->SocketOffset;  // 【新增】缓存初始 SocketOffset
			
			UE_LOG(LogTemp, Warning, TEXT("BeginPlay: Cached initial camera values"));
			UE_LOG(LogTemp, Warning, TEXT("  BaseArmLength = %.1f"), BaseArmLength);
			UE_LOG(LogTemp, Warning, TEXT("  BaseSocketOffset = %s"), *BaseSocketOffset.ToString());
		}
	});

	// 设置初始相机状态
	UpdateCameraState(ECameraState::Normal);

	if (bEnableCameraDebugLogs)
	{
		AActor* Owner = GetOwner();
		ACharacter* OwnerCharacter = Cast<ACharacter>(Owner);
		UE_LOG(LogTemp, Warning, TEXT("CameraControlComponent: Successfully initialized for %s"), 
			OwnerCharacter ? *OwnerCharacter->GetName() : TEXT("Unknown"));
	}
}

// Called every frame
void UCameraControlComponent::TickComponent(float DeltaTime, ELevelTick TickType, FActorComponentTickFunction* ThisTickFunction)
{
	Super::TickComponent(DeltaTime, TickType, ThisTickFunction);

	// 应用控制台变量
	if (CVarCameraInterpSpeed.GetValueOnGameThread() != CameraSettings.CameraInterpSpeed)
	{
		CameraSettings.CameraInterpSpeed = CVarCameraInterpSpeed.GetValueOnGameThread();
	}
	
	// 检查Debug级别
	int32 DebugLevel = CVarCameraDebugLevel.GetValueOnGameThread();
	if (DebugLevel > 0 && bEnableCameraDebugLogs != (DebugLevel > 0))
	{
		bEnableCameraDebugLogs = DebugLevel > 0;
	}

	// ========== 组件验证监控 ==========
	static float ComponentValidationTimer = 0.0f;
	ComponentValidationTimer += DeltaTime;
	
	// 每5秒验证一次组件有效性
	if (ComponentValidationTimer > ComponentValidationInterval)
	{
		ComponentValidationTimer = 0.0f;
		
		USpringArmComponent* SpringArm = GetSpringArmComponent();
		UCameraComponent* Camera = GetCameraComponent();
		
		if (!SpringArm || !Camera)
		{
			UE_LOG(LogTemp, Error, TEXT("CameraControlComponent: Components lost during runtime!"));
			UE_LOG(LogTemp, Error, TEXT("  SpringArm: %s"), SpringArm ? TEXT("Valid") : TEXT("NULL"));
			UE_LOG(LogTemp, Error, TEXT("  Camera: %s"), Camera ? TEXT("Valid") : TEXT("NULL"));
			
			// 尝试重新获取
			if (!SpringArm)
			{
				SpringArm = GetSpringArmComponentSafe();
				if (SpringArm)
				{
					CachedSpringArm = SpringArm;
					UE_LOG(LogTemp, Warning, TEXT("SpringArm re-acquired successfully"));
				}
			}
			
			if (!Camera)
			{
				Camera = GetCameraComponentSafe();
				if (Camera)
				{
					CachedCamera = Camera;
					UE_LOG(LogTemp, Warning, TEXT("Camera re-acquired successfully"));
				}
			}
		}
	}

	// ========== 调试状态监控 ==========
	static float DebugTimer = 0.0f;
	DebugTimer += DeltaTime;
	
	// 每2秒输出一次状态
	if (DebugTimer > DebugOutputInterval)
	{
		DebugTimer = 0.0f;
		
		if (CurrentLockOnTarget)
		{
			UE_LOG(LogTemp, Warning, TEXT("DEBUG: Locked on target: %s"), *CurrentLockOnTarget->GetName());
			if (GEngine)
			{
				GEngine->AddOnScreenDebugMessage(1, 2.0f, FColor::Cyan, 
					FString::Printf(TEXT("Locked Target: %s"), *CurrentLockOnTarget->GetName()));
			}
		}
		else
		{
			UE_LOG(LogTemp, Warning, TEXT("DEBUG: No locked target"));
			if (GEngine)
			{
				GEngine->AddOnScreenDebugMessage(1, 2.0f, FColor::Yellow, 
					TEXT("No Lock Target"));
			}
		}
	}
	// ========== 调试状态监控结束 ==========

	// ==================== 【新增】检查锁定目标距离，超过范围自动解锁 ====================
	if (CurrentLockOnTarget && IsValid(CurrentLockOnTarget))
	{
		// 定期检查目标距离（每0.2秒检查一次，避免每帧检查）
		static float LastDistanceCheckTime = 0.0f;
		float CurrentTime = GetWorld()->GetTimeSeconds();
		if (CurrentTime - LastDistanceCheckTime > 0.2f)
		{
			LastDistanceCheckTime = CurrentTime;
			
			// 通过TargetDetectionComponent检查目标是否仍然可锁定
			ACharacter* OwnerCharacter = GetOwnerCharacter();
			if (OwnerCharacter)
			{
				// 获取TargetDetectionComponent
				UActorComponent* DetectionComp = OwnerCharacter->GetComponentByClass(UTargetDetectionComponent::StaticClass());
				if (UTargetDetectionComponent* TargetDetection = Cast<UTargetDetectionComponent>(DetectionComp))
				{
					// 调用IsTargetStillLockable检查
					if (!TargetDetection->IsTargetStillLockable(CurrentLockOnTarget))
					{
						if (bEnableCameraDebugLogs)
						{
							UE_LOG(LogTemp, Warning, TEXT("Target out of range - Auto unlock triggered for: %s"), 
								*CurrentLockOnTarget->GetName());
						}
						
						// 自动解锁
						ClearLockOnTarget();
						return; // 解锁后直接返回，不再处理后续逻辑
					}
				}
			}
		}
	}

	// ==================== 新增：优先处理Spring Arm长度插值 ====================
	if (bIsInterpolatingArmLength)
	{
		UpdateSpringArmLengthInterpolation();
	}
	
	// 【新增】处理 Socket Offset 平滑重置
	if (bIsResetingSocketOffset)
	{
		UpdateSocketOffsetReset();
	}

	// ==================== 关键修复：优先处理平滑相机重置 ====================
	// 如果正在进行平滑相机重置，优先由组件自行驱动并退出（避免与锁定更新冲突）
	if (bIsSmoothCameraReset)
	{
		UpdateSmoothCameraReset();
		return; // 重置期间不处理其他相机逻辑，避免冲突
	}

	// 进行摄像机自动修正
	if (bIsCameraAutoCorrection)
	{
		UpdateCameraAutoCorrection();
	}

	// 锁定状态下的摄像机更新
	if (CurrentLockOnTarget && IsValid(CurrentLockOnTarget))
	{
		// 平滑切换目标状态下的摄像机更新
		if (bIsSmoothSwitching)
		{
			UpdateSmoothTargetSwitch();
		}
		else if (!bIsCameraAutoCorrection) // 仅在不是自动修正状态下，进行锁定目标摄像机更新
		{
			UpdateLockOnCamera();
		}

		// 高级相机距离适应响应
		if (AdvancedCameraSettings.bEnableDistanceAdaptiveCamera)
		{
			UpdateAdvancedCameraAdjustment();
		}
	}

	// 每帧调试信息输出
	if (bEnableCameraDebugLogs && CurrentLockOnTarget)
	{
		static float LastDebugTime = 0.0f;
		float CurrentTime = GetWorld()->GetTimeSeconds();
		if (CurrentTime - LastDebugTime > DebugInfoInterval) // 使用配置的调试输出间隔
		{
			LastDebugTime = CurrentTime;
		}
	}
}

// ==================== 接口函数实现 ====================

void UCameraControlComponent::InitializeCameraComponents(USpringArmComponent* InSpringArm, UCameraComponent* InCamera)
{
	CachedSpringArm = InSpringArm;
	CachedCamera = InCamera;
	
	if (CachedSpringArm)
	{
		BaseArmLength = CachedSpringArm->TargetArmLength;
		BaseSocketOffset = CachedSpringArm->SocketOffset;  // 【新增】缓存初始 SocketOffset
		
		UE_LOG(LogTemp, Warning, TEXT("CameraControlComponent: SpringArm initialized successfully"));
		UE_LOG(LogTemp, Warning, TEXT("  BaseArmLength = %.1f"), BaseArmLength);
		UE_LOG(LogTemp, Warning, TEXT("  BaseSocketOffset = %s"), *BaseSocketOffset.ToString());
	}
	else
	{
		UE_LOG(LogTemp, Error, TEXT("CameraControlComponent: SpringArm initialization failed!"));
	}
	
	if (CachedCamera)
	{
		UE_LOG(LogTemp, Warning, TEXT("CameraControlComponent: Camera initialized successfully"));
	}
	else
	{
		UE_LOG(LogTemp, Error, TEXT("CameraControlComponent: Camera initialization failed!"));
	}
}

void UCameraControlComponent::SetCameraSettings(const FCameraSettings& Settings)
{
	CameraSettings = Settings;
	
	if (bEnableCameraDebugLogs)
	{
		UE_LOG(LogTemp, Log, TEXT("CameraControlComponent: Camera settings updated"));
		UE_LOG(LogTemp, Log, TEXT("- InterpSpeed: %.1f"), CameraSettings.CameraInterpSpeed);
		UE_LOG(LogTemp, Log, TEXT("- SmoothTracking: %s"), CameraSettings.bEnableSmoothCameraTracking ? TEXT("ON") : TEXT("OFF"));
		UE_LOG(LogTemp, Log, TEXT("- TrackingMode: %d"), CameraSettings.CameraTrackingMode);
	}
}

void UCameraControlComponent::SetAdvancedCameraSettings(const FAdvancedCameraSettings& Settings)
{
	AdvancedCameraSettings = Settings;
	
	if (bEnableAdvancedAdjustmentDebugLogs)
	{
		UE_LOG(LogTemp, Log, TEXT("CameraControlComponent: Advanced camera settings updated"));
		UE_LOG(LogTemp, Log, TEXT("- DistanceAdaptive: %s"), AdvancedCameraSettings.bEnableDistanceAdaptiveCamera ? TEXT("ON") : TEXT("OFF"));
		UE_LOG(LogTemp, Log, TEXT("- TerrainCompensation: %s"), AdvancedCameraSettings.bEnableTerrainHeightCompensation ? TEXT("ON") : TEXT("OFF"));
		UE_LOG(LogTemp, Log, TEXT("- EnemySizeAdaptation: %s"), AdvancedCameraSettings.bEnableEnemySizeAdaptation ? TEXT("ON") : TEXT("OFF"));
	}
}

// ==================== 状态更新和切换函数 ====================

void UCameraControlComponent::HandlePlayerInput(float TurnInput, float LookUpInput)
{
	// 检测是否需要中断自动控制
	if (ShouldInterruptAutoControl(TurnInput, LookUpInput))
	{
		// 停止自动修正
		if (bIsCameraAutoCorrection)
		{
			bIsCameraAutoCorrection = false;
			DelayedCorrectionTarget = nullptr;
			UpdateCameraState(ECameraState::LockedOn);
			
			if (bEnableCameraDebugLogs)
			{
				UE_LOG(LogTemp, Log, TEXT("Player input detected - stopping camera auto correction"));
			}
		}
		
		// 停止平滑重置
		if (bIsSmoothCameraReset)
		{
			bIsSmoothCameraReset = false;
			UpdateCameraState(ECameraState::Normal);
			
			if (bEnableCameraDebugLogs)
			{
				UE_LOG(LogTemp, Log, TEXT("Player input detected - stopping smooth camera reset"));
			}
		}
	}
}

void UCameraControlComponent::HandlePlayerMovement(bool bIsMoving)
{
	bPlayerIsMoving = bIsMoving;

	if (!bIsMoving || !CurrentLockOnTarget)
	{
		return;
	}

	// 平滑切换期间不中断
	if (bIsSmoothSwitching)
	{
		bShouldSmoothSwitchCharacter = true;
		return;
	}
	
	// 自动修正期间中断
	if (bIsCameraAutoCorrection)
	{
		bIsCameraAutoCorrection = false;
		DelayedCorrectionTarget = nullptr;
		UpdateCameraState(ECameraState::LockedOn);
		
		if (bEnableCameraDebugLogs)
		{
			UE_LOG(LogTemp, Log, TEXT("Player movement interrupted camera auto correction"));
		}
	}
	
	// 恢复相机跟随
	if (!bShouldCameraFollowTarget)
	{
		bShouldCameraFollowTarget = true;
		bShouldCharacterRotateToTarget = true;
		
		if (bEnableCameraDebugLogs)
		{
			UE_LOG(LogTemp, Log, TEXT("Camera follow restored"));
		}
	}
}

// ==================== 锁定相关函数实现 ====================

void UCameraControlComponent::UpdateLockOnCamera()
{
	// 安全检查
	if (!CurrentLockOnTarget || !IsValid(CurrentLockOnTarget))
	{
		return;
	}
	
	// 平滑切换期间由切换逻辑控制
	if (bIsSmoothSwitching)
	{
		return;
	}

	APlayerController* PlayerController = GetOwnerController();
	if (!PlayerController)
	{
		return;
	}

	// 只有在应该跟随目标时才更新相机
	if (!bShouldCameraFollowTarget)
	{
		if (bShouldCharacterRotateToTarget)
		{
			UpdateCharacterRotationToTarget();
		}
		return;
	}

	ACharacter* OwnerCharacter = GetOwnerCharacter();
	if (!OwnerCharacter)
		return;

	// ==================== 新增：动态调整SpringArm参数根据敌人尺寸 ====================
	EEnemySizeCategory SizeCategory = GetTargetSizeCategoryV2(CurrentLockOnTarget);
	float TargetDistance = CalculateDistanceToTarget(CurrentLockOnTarget);
	float BoundingHeight = GetActorBoundingHeight(CurrentLockOnTarget);
	
	// 动态调整臂长和偏移
	AdjustSpringArmForSizeDistance(SizeCategory, TargetDistance, BoundingHeight);
	
	// 动态调整相机偏移
	USpringArmComponent* SpringArm = GetSpringArmComponent();
	if (SpringArm)
	{
		FVector DynamicOffset = GetHeightOffsetForEnemySize(SizeCategory);
		// 平滑插值到目标偏移
		FVector CurrentOffset = SpringArm->SocketOffset;
		float DeltaTime = GetWorld()->GetDeltaSeconds();
		FVector NewOffset = FMath::VInterpTo(CurrentOffset, DynamicOffset, DeltaTime, SocketOffsetInterpSpeed);
		SpringArm->SocketOffset = NewOffset;
	}

	// 获取玩家位置
	FVector PlayerLocation = OwnerCharacter->GetActorLocation();
	
	// ==================== 【增强调试LOG】检测目标切换 ====================
	if (LastFrameTarget != CurrentLockOnTarget)
	{
		// ⚠️ 【关键跳变点】目标切换时，强制重新计算
		FVector TargetLocation = GetOptimalLockOnPosition(CurrentLockOnTarget);
		CachedTargetLocation = TargetLocation;
		LastFrameTarget = CurrentLockOnTarget;
		
		// 【增强LOG】详细记录跳变原因
		if (bEnableCameraDebugLogs)
		{
			UE_LOG(LogTemp, Error, TEXT("⚠️ [JUMP-POINT] LastFrameTarget mismatch detected!"));
			UE_LOG(LogTemp, Error, TEXT("  └─ LastFrameTarget: %s"), 
				LastFrameTarget ? *LastFrameTarget->GetName() : TEXT("None"));
			UE_LOG(LogTemp, Error, TEXT("  └─ CurrentLockOnTarget: %s"), 
				CurrentLockOnTarget ? *CurrentLockOnTarget->GetName() : TEXT("None"));
			UE_LOG(LogTemp, Error, TEXT("  └─ Forced recalculation of position: %s"), 
				*TargetLocation.ToString());
			UE_LOG(LogTemp, Error, TEXT("  └─ This may cause camera jump if switch just completed!"));
			
			// 检查是否刚完成平滑切换
			float TimeSinceLastSwitch = GetWorld()->GetTimeSeconds() - SmoothSwitchStartTime;
			if (TimeSinceLastSwitch < 0.5f)
			{
				UE_LOG(LogTemp, Error, TEXT("  └─ ⚠️ WARNING: Switch completed %.3fs ago - likely the cause of jump!"), 
					TimeSinceLastSwitch);
			}
		}
	}
	else
	{
		// 【调试LOG】正常情况
		static int32 NoJumpCounter = 0;
		if (bEnableCameraDebugLogs && (NoJumpCounter++ % 120 == 0))
		{
			UE_LOG(LogTemp, Log, TEXT("✅ [NO-JUMP] LastFrameTarget matches CurrentLockOnTarget: %s"), 
				*CurrentLockOnTarget->GetName());
		}
	}
	
	// 使用缓存的目标位置，避免每帧重新计算
	FVector TargetLocation = CachedTargetLocation;
	
	// 定期更新缓存位置（每0.1秒更新一次，应对目标移动）
	static float LastUpdateTime = 0.0f;
	float CurrentTime = GetWorld()->GetTimeSeconds();
	if (CurrentTime - LastUpdateTime > TargetLocationCacheUpdateInterval)
	{
		FVector NewTargetLocation = GetOptimalLockOnPosition(CurrentLockOnTarget);
		// 使用平滑插值更新缓存，避免突变
		float DeltaTime = GetWorld()->GetDeltaSeconds();
		CachedTargetLocation = FMath::VInterpTo(CachedTargetLocation, NewTargetLocation, 
			DeltaTime, CachedLocationInterpSpeed);
		LastUpdateTime = CurrentTime;
	}

	// 使用稳定插值获取平滑位置
	if (bEnableTargetStableInterpolation)
	{
		TargetLocation = CachedTargetLocation;  // 使用缓存值
	}

	// 计算玩家朝向目标的旋转
	FRotator LookAtRotation = UKismetMathLibrary::FindLookAtRotation(PlayerLocation, TargetLocation);

	// 获取DeltaTime用于插值计算
	float DeltaTime = GetWorld()->GetDeltaSeconds();

	// 应用FreeLook偏移
	if (FreeLookSettings.bEnableFreeLook && bIsFreeLooking)
	{
		// 检查是否应该自动返回
		float TimeSinceInput = GetWorld()->GetTimeSeconds() - LastFreeLookInputTime;
		if (TimeSinceInput > FreeLookSettings.AutoReturnDelay)
		{
			// 平滑返回中心
			FreeLookOffset = FMath::RInterpTo(FreeLookOffset, FRotator::ZeroRotator, 
				DeltaTime, FreeLookSettings.ReturnToCenterSpeed);
			
			if (FreeLookOffset.IsNearlyZero(FreeLookResetThreshold))
			{
				bIsFreeLooking = false;
			}
		}
		
		// 应用FreeLook偏移
		LookAtRotation += FreeLookOffset;
	}

	// 获取当前相机/控制器的旋转
	FRotator CurrentRotation = PlayerController->GetControlRotation();

	// 使用插值进行平滑
	FRotator NewRotation;
	
	if (CameraSettings.bEnableSmoothCameraTracking)
	{
		float SpeedMultiplier = GetCameraSpeedMultiplierForDistance(TargetDistance);
		float AdjustedInterpSpeed = CameraSettings.CameraInterpSpeed * SpeedMultiplier;

		switch (CameraSettings.CameraTrackingMode)
		{
		case 0: // 完全跟踪
			NewRotation = FMath::RInterpTo(CurrentRotation, LookAtRotation, DeltaTime, AdjustedInterpSpeed);
			break;
		case 1: // 仅水平跟踪
			{
				FRotator HorizontalLookAt = FRotator(CurrentRotation.Pitch, LookAtRotation.Yaw, CurrentRotation.Roll);
				NewRotation = FMath::RInterpTo(CurrentRotation, HorizontalLookAt, DeltaTime, AdjustedInterpSpeed);
			}
			break;
		default:
			NewRotation = FMath::RInterpTo(CurrentRotation, LookAtRotation, DeltaTime, AdjustedInterpSpeed);
			break;
		}
	}
	else
	{
		NewRotation = LookAtRotation;
	}
	
	PlayerController->SetControlRotation(NewRotation);

	// 角色旋转
	if (bShouldCharacterRotateToTarget)
	{
		FRotator CharacterRotation = FMath::RInterpTo(OwnerCharacter->GetActorRotation(), 
			FRotator(0, LookAtRotation.Yaw, 0), DeltaTime, CHARACTER_ROTATION_SPEED);
		OwnerCharacter->SetActorRotation(CharacterRotation);
	}
}

void UCameraControlComponent::StartSmoothTargetSwitch(AActor* NewTarget)
{
	// 安全检查
	if (!NewTarget || !ValidateTarget(NewTarget))
	{
		if (bEnableCameraDebugLogs)
		{
			UE_LOG(LogTemp, Warning, TEXT("StartSmoothTargetSwitch: Invalid target"));
		}
		return;
	}

	APlayerController* PlayerController = GetOwnerController();
	ACharacter* OwnerCharacter = GetOwnerCharacter();
	
	if (!PlayerController || !OwnerCharacter)
	{
		if (bEnableCameraDebugLogs)
		{
			UE_LOG(LogTemp, Error, TEXT("StartSmoothTargetSwitch: Missing components"));
		}
		return;
	}

	// ==================== 【方案B核心修复】提前设置标志和正确缓存位置 ====================
	// 1. 提前设置平滑切换标志，防止其他逻辑在切换期间干扰
	bIsSmoothSwitching = true;
	
	// 2. 存储待切换目标
	PendingLockOnTarget = NewTarget;
	
	// 3. 缓存当前旋转（在任何计算前）
	SmoothSwitchStartRotation = PlayerController->GetControlRotation();
	
	// 4. 记录切换开始时间
	SmoothSwitchStartTime = GetWorld()->GetTimeSeconds();
	
	// ==================== 【关键修复点】在更新目标引用之前缓存旧目标位置 ====================
	// 5. ✅ 先缓存旧目标位置（在CurrentLockOnTarget被修改之前）
	if (CurrentLockOnTarget && IsValid(CurrentLockOnTarget))
	{
		FVector PlayerLocation = OwnerCharacter->GetActorLocation();
		FVector OldTargetLocation = GetOptimalLockOnPosition(CurrentLockOnTarget); // 使用旧目标
		CachedTargetLocation = OldTargetLocation;  // 缓存旧目标位置，作为插值起点
		
		if (bEnableCameraDebugLogs)
		{
			UE_LOG(LogTemp, Log, TEXT("✅ [FIX-B] Cached OLD target location BEFORE switch: %s (Target: %s)"), 
				*OldTargetLocation.ToString(),
				*CurrentLockOnTarget->GetName());
		}
	}
	
	// 6. ✅ 现在才更新目标引用
	PreviousLockOnTarget = CurrentLockOnTarget;
	CurrentLockOnTarget = NewTarget;
	
	// 7. 计算新目标的旋转
	FVector PlayerLocation = OwnerCharacter->GetActorLocation();
	FVector NewTargetLocation = GetOptimalLockOnPosition(NewTarget);
	FRotator TargetRotation = UKismetMathLibrary::FindLookAtRotation(PlayerLocation, NewTargetLocation);
	
	// 所有切换都使用Camera Lag平滑
	SmoothSwitchTargetRotation = TargetRotation;
	
	bShouldSmoothSwitchCamera = true;
	bShouldSmoothSwitchCharacter = bPlayerIsMoving;
	bShouldCameraFollowTarget = true;
	
	UpdateCameraState(ECameraState::SmoothSwitching);
	
	if (bEnableCameraDebugLogs)
	{
		float AngleDifference = CalculateAngleToTarget(NewTarget);
		UE_LOG(LogTemp, Log, TEXT("✅ [FIX-B] Smooth switch started: Angle=%.1f°, Old->New: %s->%s"), 
			AngleDifference,
			PreviousLockOnTarget ? *PreviousLockOnTarget->GetName() : TEXT("None"),
			*NewTarget->GetName());
	}
}

void UCameraControlComponent::UpdateSmoothTargetSwitch()
{
	if (!CurrentLockOnTarget || !IsValid(CurrentLockOnTarget))
	{
		bIsSmoothSwitching = false;
		return;
	}

	APlayerController* PlayerController = GetOwnerController();
	ACharacter* OwnerCharacter = GetOwnerCharacter();
	if (!PlayerController || !OwnerCharacter)
		return;

	// ✅ 重新计算目标位置（应对目标移动）
	FVector PlayerLocation = OwnerCharacter->GetActorLocation();
	FVector TargetLocation = GetOptimalLockOnPosition(CurrentLockOnTarget);
	FRotator TargetRotation = UKismetMathLibrary::FindLookAtRotation(PlayerLocation, TargetLocation);
	
	// ✅ 使用 RInterpTo 进行平滑插值
	float DeltaTime = GetWorld()->GetDeltaSeconds();
	FRotator CurrentRotation = PlayerController->GetControlRotation();
	
	// 使用更高的插值速度进行切换
	float SwitchSpeed = CameraSettings.CameraInterpSpeed * 2.0f;  // 切换时速度加倍
	FRotator NewRotation = FMath::RInterpTo(CurrentRotation, TargetRotation, DeltaTime, SwitchSpeed);
	
	PlayerController->SetControlRotation(NewRotation);
	
	// 检查是否完成
	if (IsInterpolationComplete(NewRotation, TargetRotation, 1.0f))
	{
		// ==================== 【方案A核心修复】在清除标志前同步 LastFrameTarget ====================
		// 1. ✅ 先同步 LastFrameTarget 防止 UpdateLockOnCamera 检测到变化
		if (PendingLockOnTarget && IsValid(PendingLockOnTarget))
		{
			LastFrameTarget = PendingLockOnTarget;  // ✅ 同步到新目标
			
			if (bEnableCameraDebugLogs)
			{
				UE_LOG(LogTemp, Log, TEXT("🔄 [FIX-A] LastFrameTarget synchronized to: %s BEFORE state change"), 
					*PendingLockOnTarget->GetName());
			}
		}
		
		// 2. ✅ 然后清除平滑切换标志
		bIsSmoothSwitching = false;
		
		// 3. ✅ 最后更新目标引用
		if (PendingLockOnTarget && IsValid(PendingLockOnTarget))
		{
			CurrentLockOnTarget = PendingLockOnTarget;
			PendingLockOnTarget = nullptr;
			
			if (bEnableCameraDebugLogs)
			{
				UE_LOG(LogTemp, Log, TEXT("✅ [FIX-A+B] Smooth switch completed - Target updated to: %s, LastFrameTarget synced"), 
					*CurrentLockOnTarget->GetName());
			}
		}
		
		UpdateCameraState(ECameraState::LockedOn);
		
		if (bEnableCameraDebugLogs)
		{
			UE_LOG(LogTemp, Log, TEXT("✅ [FIX-A+B] Smooth switch completed WITHOUT target change detection"));
		}
	}
	
	// 角色旋转
	if (bShouldCharacterRotateToTarget)
	{
		FRotator CharacterRotation = FMath::RInterpTo(
			OwnerCharacter->GetActorRotation(), 
			FRotator(0, TargetRotation.Yaw, 0), 
			DeltaTime, 
			CHARACTER_ROTATION_SPEED
		);
		OwnerCharacter->SetActorRotation(CharacterRotation);
	}
}

// ==================== 状态管理函数实现 ====================

void UCameraControlComponent::SetLockOnTarget(AActor* Target)
{
	// === 步骤2修复：防止重复设置 ===
	// 安全宪法：防御性检查
	if (CurrentLockOnTarget == Target)
	{
		return; // 相同目标，直接返回
	}
	
	// ==================== Step 2.4: 目标变更时清除缓存 ====================
	// 清除MyCharacter中的三层锁定系统缓存
	ACharacter* OwnerCharacter = GetOwnerCharacter();
	if (OwnerCharacter)
	{
		// 通过反射或强制转换获取 MyCharacter 实例
		// 假设 OwnerCharacter 是 AMyCharacter 类型
		// 注意：这里需要根据实际项目结构调整
		
		// 方案A：如果能直接访问 MyCharacter（需要包含头文件）
		// AMyCharacter* MyChar = Cast<AMyCharacter>(OwnerCharacter);
		// if (MyChar)
		// {
		//     MyChar->CachedLockMethod = AMyCharacter::ELockMethod::None;
		//     MyChar->CachedSocketName = NAME_None;
		//     MyChar->CachedMethodOffset = FVector::ZeroVector;
		// }
		
		// 方案B：通过组件自身管理缓存（推荐）
		// 清除缓存状态，下次调用 GetOptimalLockOnPosition 会重新计算
		CachedTargetLocation = FVector::ZeroVector;
		LastFrameTarget = nullptr;
		
		if (bEnableCameraDebugLogs)
		{
			UE_LOG(LogTemp, Log, TEXT("SetLockOnTarget: Cache cleared for target change"));
		}
	}
	
	// 记录变更
	AActor* OldTarget = CurrentLockOnTarget;
	CurrentLockOnTarget = Target;
	
	// 只在真正改变时记录日志
	if (bEnableCameraDebugLogs)
	{
		if (Target)
		{
			UE_LOG(LogTemp, Log, TEXT("CameraControlComponent: Target changed from [%s] to [%s]"), 
				OldTarget ? *OldTarget->GetName() : TEXT("None"),
				*Target->GetName());
		}
		else if (OldTarget)
		{
			UE_LOG(LogTemp, Log, TEXT("CameraControlComponent: Target cleared (was [%s])"),
				*OldTarget->GetName());
		}
	}
}

void UCameraControlComponent::ClearLockOnTarget()
{
	if (bEnableCameraDebugLogs && CurrentLockOnTarget)
	{
		UE_LOG(LogTemp, Log, TEXT("CameraControlComponent: Clearing lock-on target: %s"), *CurrentLockOnTarget->GetName());
	}
	
	// 保存上一个目标用于UI管理
	PreviousLockOnTarget = CurrentLockOnTarget;
	
	// 清除目标引用
	CurrentLockOnTarget = nullptr;
	
	// 停止所有其他相机控制状态
	bIsSmoothSwitching = false;
	bShouldSmoothSwitchCamera = false;
	bShouldSmoothSwitchCharacter = false;
	bIsCameraAutoCorrection = false;
	DelayedCorrectionTarget = nullptr;
	
	// 恢复相机跟随状态
	bShouldCameraFollowTarget = true;
	bShouldCharacterRotateToTarget = true;
	
	// 【新增】重置 Spring Arm Length 到初始值
	USpringArmComponent* SpringArm = GetSpringArmComponent();
	if (SpringArm && BaseArmLength > 0.0f)
	{
		SetSpringArmLengthSmooth(BaseArmLength, ArmLengthInterpSpeed);
		bArmLengthAdjusted = false;
		
		if (bEnableCameraDebugLogs)
		{
			UE_LOG(LogTemp, Warning, TEXT("Unlock: Resetting ArmLength %.1f -> %.1f"), 
				SpringArm->TargetArmLength, BaseArmLength);
		}
	}
	
	// 【新增】重置 Socket Offset 到初始值（使用平滑插值）
	if (SpringArm)
	{
		// 启动 SocketOffset 的平滑插值重置
		// 注意：这里不能直接赋值，需要在 Tick 中平滑插值
		// 我们使用一个标志来触发重置
		bIsResetingSocketOffset = true;
		SocketOffsetResetStartTime = GetWorld()->GetTimeSeconds();
		SocketOffsetResetStart = SpringArm->SocketOffset;
		SocketOffsetResetTarget = BaseSocketOffset;
		
		if (bEnableCameraDebugLogs)
		{
			UE_LOG(LogTemp, Warning, TEXT("Unlock: Resetting SocketOffset %s -> %s"), 
				*SpringArm->SocketOffset.ToString(), *BaseSocketOffset.ToString());
		}
	}
	
	// 【关键修复】启动平滑重置，而非直接切换到Normal状态
	// bFromUnlock = true，使用UnlockResetSpeed（较慢的速度）
	StartSmoothCameraReset(true);
	
	if (bEnableCameraDebugLogs)
	{
		UE_LOG(LogTemp, Log, TEXT("CameraControlComponent: Started smooth camera reset from unlock (Speed: %.1f)"), UnlockResetSpeed);
	}
}

void UCameraControlComponent::ResetCameraToDefault()
{
	// 清除锁定目标
	ClearLockOnTarget();
	
	// 重置FreeLook
	ResetFreeLook();
	
	// 重置相机到角色前方
	ACharacter* OwnerCharacter = GetOwnerCharacter();
	APlayerController* PlayerController = GetOwnerController();
	
	if (OwnerCharacter && PlayerController)
	{
		FRotator CharacterRotation = OwnerCharacter->GetActorRotation();
		PlayerController->SetControlRotation(CharacterRotation);
	}
	
	// ==================== 修复：使用平滑插值重置相机臂长度 ====================
	USpringArmComponent* SpringArm = GetSpringArmComponent();
	if (SpringArm && BaseArmLength > 0.0f)
	{
		// 使用平滑插值而非直接赋值
		SetSpringArmLengthSmooth(BaseArmLength, ArmLengthInterpSpeed);
		bArmLengthAdjusted = false;
		
		// 【新增】同时重置 Socket Offset
		bIsResetingSocketOffset = true;
		SocketOffsetResetStartTime = GetWorld()->GetTimeSeconds();
		SocketOffsetResetStart = SpringArm->SocketOffset;
		SocketOffsetResetTarget = BaseSocketOffset;
		
		if (bEnableCameraDebugLogs)
		{
			UE_LOG(LogTemp, Log, TEXT("ResetCameraToDefault: Resetting both ArmLength and SocketOffset"));
		}
	}
	
	if (bEnableCameraDebugLogs)
	{
		UE_LOG(LogTemp, Log, TEXT("Camera reset to default state with smooth arm length transition"));
	}
}

// ==================== Spring Arm平滑插值函数实现（新增）====================

void UCameraControlComponent::SetSpringArmLengthSmooth(float TargetLength, float InterpSpeed)
{
	USpringArmComponent* SpringArm = GetSpringArmComponent();
	if (!SpringArm)
	{
		UE_LOG(LogTemp, Error, TEXT("SetSpringArmLengthSmooth: SpringArm component not found"));
		return;
	}
	
	// 如果目标长度与当前长度几乎相同，直接设置
	if (FMath::IsNearlyEqual(SpringArm->TargetArmLength, TargetLength, 1.0f))
	{
		SpringArm->TargetArmLength = TargetLength;
		bIsInterpolatingArmLength = false;
		return;
	}
	
	// 开始插值
	bIsInterpolatingArmLength = true;
	ArmLengthInterpStart = SpringArm->TargetArmLength;
	ArmLengthInterpTarget = TargetLength;
	ArmLengthInterpStartTime = GetWorld()->GetTimeSeconds();
	ArmLengthInterpSpeed = InterpSpeed;
	
	if (bEnableCameraDebugLogs)
	{
		UE_LOG(LogTemp, Log, TEXT("SetSpringArmLengthSmooth: Started interpolation from %.1f to %.1f (Speed: %.1f)"), 
			ArmLengthInterpStart, ArmLengthInterpTarget, InterpSpeed);
	}
}

void UCameraControlComponent::UpdateSpringArmLengthInterpolation()
{
	if (!bIsInterpolatingArmLength)
		return;
	
	USpringArmComponent* SpringArm = GetSpringArmComponent();
	if (!SpringArm)
	{
		bIsInterpolatingArmLength = false;
		return;
	}
	
	float DeltaTime = GetWorld()->GetDeltaSeconds();
	float CurrentLength = SpringArm->TargetArmLength;
	
	// 使用FInterpTo进行平滑插值
	float NewLength = FMath::FInterpTo(CurrentLength, ArmLengthInterpTarget, DeltaTime, ArmLengthInterpSpeed);
	SpringArm->TargetArmLength = NewLength;
	
	// 检查是否完成插值
	if (FMath::IsNearlyEqual(NewLength, ArmLengthInterpTarget, 1.0f))
	{
		SpringArm->TargetArmLength = ArmLengthInterpTarget;
		bIsInterpolatingArmLength = false;
		
		if (bEnableCameraDebugLogs)
		{
			UE_LOG(LogTemp, Log, TEXT("UpdateSpringArmLengthInterpolation: Completed at length %.1f"), 
				ArmLengthInterpTarget);
		}
	}
}

void UCameraControlComponent::UpdateSocketOffsetReset()
{
	if (!bIsResetingSocketOffset)
		return;
	
	USpringArmComponent* SpringArm = GetSpringArmComponent();
	if (!SpringArm)
	{
		bIsResetingSocketOffset = false;
		return;
	}
	
	float DeltaTime = GetWorld()->GetDeltaSeconds();
	FVector CurrentOffset = SpringArm->SocketOffset;
	
	// 使用 VInterpTo 进行平滑插值
	FVector NewOffset = FMath::VInterpTo(CurrentOffset, SocketOffsetResetTarget, DeltaTime, SocketOffsetResetSpeed);
	SpringArm->SocketOffset = NewOffset;
	
	// 检查是否完成插值
	if (NewOffset.Equals(SocketOffsetResetTarget, 1.0f))
	{
		SpringArm->SocketOffset = SocketOffsetResetTarget;
		bIsResetingSocketOffset = false;
		
		if (bEnableCameraDebugLogs)
		{
			UE_LOG(LogTemp, Log, TEXT("UpdateSocketOffsetReset: Completed at offset %s"), 
				*SocketOffsetResetTarget.ToString());
		}
	}
}

void UCameraControlComponent::UpdateCharacterRotationToTarget()
{
	if (!CurrentLockOnTarget || !IsValid(CurrentLockOnTarget))
		return;
	
	ACharacter* OwnerCharacter = GetOwnerCharacter();
	if (!OwnerCharacter)
		return;
	
	FVector PlayerLocation = OwnerCharacter->GetActorLocation();
	FVector TargetLocation = GetOptimalLockOnPosition(CurrentLockOnTarget);
	
	FRotator LookAtRotation = UKismetMathLibrary::FindLookAtRotation(PlayerLocation, TargetLocation);
	float DeltaTime = GetWorld()->GetDeltaSeconds();
	
	FRotator CharacterRotation = FMath::RInterpTo(OwnerCharacter->GetActorRotation(), 
		FRotator(0, LookAtRotation.Yaw, 0), DeltaTime, CHARACTER_ROTATION_SPEED);
	OwnerCharacter->SetActorRotation(CharacterRotation);
}

void UCameraControlComponent::SetSpringArmLengthImmediate(float TargetLength)
{
	USpringArmComponent* SpringArm = GetSpringArmComponent();
	if (!SpringArm)
	{
		UE_LOG(LogTemp, Error, TEXT("SetSpringArmLengthImmediate: SpringArm component not found"));
		return;
	}
	
	SpringArm->TargetArmLength = TargetLength;
	bIsInterpolatingArmLength = false;
	
	if (bEnableCameraDebugLogs)
	{
		UE_LOG(LogTemp, Log, TEXT("SetSpringArmLengthImmediate: Set to %.1f"), TargetLength);
	}
}

void UCameraControlComponent::StartSmoothCameraReset(bool bFromUnlock)
{
	APlayerController* PlayerController = GetOwnerController();
	ACharacter* OwnerCharacter = GetOwnerCharacter();
	if (!PlayerController || !OwnerCharacter)
		return;
	
	// Set the current reset type based on the parameter
	CurrentResetType = bFromUnlock ? ECameraResetType::FromUnlock : ECameraResetType::Normal;
	bIsSmoothCameraReset = true;
	
	FRotator CurrentRotation = PlayerController->GetControlRotation();
	FRotator CharacterRotation = OwnerCharacter->GetActorRotation();
	SmoothResetStartRotation = CurrentRotation;
	SmoothResetTargetRotation = FRotator(CurrentRotation.Pitch, CharacterRotation.Yaw, CurrentRotation.Roll);
	SmoothResetStartTime = GetWorld()->GetTimeSeconds();
	
	UpdateCameraState(ECameraState::SmoothReset);
	
	if (bEnableCameraDebugLogs)
	{
		UE_LOG(LogTemp, Log, TEXT("StartSmoothCameraReset: Started smooth camera reset (Type: %s)"),
			bFromUnlock ? TEXT("FromUnlock") : TEXT("Normal"));
	}
}

void UCameraControlComponent::UpdateSmoothCameraReset()
{
	if (!bIsSmoothCameraReset)
		return;
	
	APlayerController* PlayerController = GetOwnerController();
	if (!PlayerController)
		return;
	
	float DeltaTime = GetWorld()->GetDeltaSeconds();
	FRotator CurrentRotation = PlayerController->GetControlRotation();
	
	// 根据重置类型选择速度
	float ResetSpeed = (CurrentResetType == ECameraResetType::FromUnlock) ? 
					   UnlockResetSpeed : NormalResetSpeed;
	
	// 使用动态速度进行插值
	FRotator NewRotation = FMath::RInterpTo(CurrentRotation, SmoothResetTargetRotation, 
		DeltaTime, ResetSpeed);
	
	PlayerController->SetControlRotation(NewRotation);
	
	// 检查是否完成重置
	if (IsInterpolationComplete(NewRotation, SmoothResetTargetRotation, CAMERA_RESET_ANGLE_THRESHOLD))
	{
		bIsSmoothCameraReset = false;
		CurrentResetType = ECameraResetType::Normal; // 重置类型
		UpdateCameraState(ECameraState::Normal);
		OnCameraResetCompleted.Broadcast();
		
		if (bEnableCameraDebugLogs)
		{
			UE_LOG(LogTemp, Log, TEXT("UpdateSmoothCameraReset: Smooth camera reset completed"));
		}
	}
}

void UCameraControlComponent::StartCameraReset(const FRotator& TargetRotation)
{
	APlayerController* PlayerController = GetOwnerController();
	if (!PlayerController)
		return;
	
	bIsSmoothCameraReset = true;
	SmoothResetStartTime = GetWorld()->GetTimeSeconds();
	SmoothResetStartRotation = PlayerController->GetControlRotation();
	SmoothResetTargetRotation = TargetRotation;
	
	UpdateCameraState(ECameraState::SmoothReset);
	
	if (bEnableCameraDebugLogs)
	{
		UE_LOG(LogTemp, Log, TEXT("StartCameraReset: Started camera reset to target rotation"));
	}
}

void UCameraControlComponent::PerformSimpleCameraReset()
{
	APlayerController* PlayerController = GetOwnerController();
	ACharacter* OwnerCharacter = GetOwnerCharacter();
	if (!PlayerController || !OwnerCharacter)
		return;
	
	FRotator CharacterRotation = OwnerCharacter->GetActorRotation();
	PlayerController->SetControlRotation(CharacterRotation);
	
	UpdateCameraState(ECameraState::Normal);
	OnCameraResetCompleted.Broadcast();
	
	if (bEnableCameraDebugLogs)
	{
		UE_LOG(LogTemp, Log, TEXT("PerformSimpleCameraReset: Camera reset completed"));
	}
}

void UCameraControlComponent::StartCameraAutoCorrection(AActor* Target)
{
	if (!Target || !ValidateTarget(Target))
		return;
	
	APlayerController* PlayerController = GetOwnerController();
	ACharacter* OwnerCharacter = GetOwnerCharacter();
	if (!PlayerController || !OwnerCharacter)
		return;
	
	bIsCameraAutoCorrection = true;
	CameraCorrectionStartTime = GetWorld()->GetTimeSeconds();
	CameraCorrectionStartRotation = PlayerController->GetControlRotation();
	
	FVector PlayerLocation = OwnerCharacter->GetActorLocation();
	FVector TargetLocation = Target->GetActorLocation();
	FRotator DirectionToTarget = UKismetMathLibrary::FindLookAtRotation(PlayerLocation, TargetLocation);
	
	CameraCorrectionTargetRotation = DirectionToTarget;
	
	UpdateCameraState(ECameraState::AutoCorrection);
	OnCameraCorrectionStarted.Broadcast(Target);
	
	if (bEnableCameraDebugLogs)
	{
		UE_LOG(LogTemp, Log, TEXT("StartCameraAutoCorrection: Started auto correction to target: %s"), *Target->GetName());
	}
}

void UCameraControlComponent::UpdateCameraAutoCorrection()
{
	if (!bIsCameraAutoCorrection)
		return;
	
	APlayerController* PlayerController = GetOwnerController();
	if (!PlayerController)
		return;
	
	float DeltaTime = GetWorld()->GetDeltaSeconds();
	FRotator CurrentRotation = PlayerController->GetControlRotation();
	FRotator NewRotation = FMath::RInterpTo(CurrentRotation, CameraCorrectionTargetRotation, 
		DeltaTime, CAMERA_AUTO_CORRECTION_SPEED);
	
	PlayerController->SetControlRotation(NewRotation);
	
	// 检查是否完成
	if (IsInterpolationComplete(NewRotation, CameraCorrectionTargetRotation, LOCK_COMPLETION_THRESHOLD))
	{
		bIsCameraAutoCorrection = false;
		UpdateCameraState(ECameraState::LockedOn);
		
		if (bEnableCameraDebugLogs)
		{
			UE_LOG(LogTemp, Log, TEXT("UpdateCameraAutoCorrection: Auto correction completed"));
		}
	}
}

void UCameraControlComponent::StartCameraCorrectionForTarget(AActor* Target)
{
	if (!Target)
		return;
	
	DelayedCorrectionTarget = Target;
	StartCameraAutoCorrection(Target);
}

void UCameraControlComponent::DelayedCameraCorrection()
{
	if (DelayedCorrectionTarget && IsValid(DelayedCorrectionTarget))
	{
		StartCameraAutoCorrection(DelayedCorrectionTarget);
		DelayedCorrectionTarget = nullptr;
	}
}

void UCameraControlComponent::RestoreCameraFollowState()
{
	// 恢复相机跟随状态
	bShouldCameraFollowTarget = true;
	bShouldCharacterRotateToTarget = true;
	
	// 清除延迟修正目标
	DelayedCorrectionTarget = nullptr;
	
	// 停止自动修正
	if (bIsCameraAutoCorrection)
	{
		bIsCameraAutoCorrection = false;
	}
	
	// 停止平滑切换
	if (bIsSmoothSwitching)
	{
		bIsSmoothSwitching = false;
		bShouldSmoothSwitchCamera = false;
		bShouldSmoothSwitchCharacter = false;
	}
	
	if (bEnableCameraDebugLogs)
	{
		UE_LOG(LogTemp, Log, TEXT("Camera follow state restored"));
	}
}

// ==================== FreeLook函数实现 ====================

void UCameraControlComponent::ApplyFreeLookInput(float YawInput, float PitchInput)
{
	if (!FreeLookSettings.bEnableFreeLook || !CurrentLockOnTarget)
		return;
	
	// 更新FreeLook偏移
	FreeLookOffset.Yaw = FMath::Clamp(FreeLookOffset.Yaw + YawInput, -FreeLookSettings.HorizontalLimit, FreeLookSettings.HorizontalLimit);
	FreeLookOffset.Pitch = FMath::Clamp(FreeLookOffset.Pitch + PitchInput, -FreeLookSettings.VerticalLimit, FreeLookSettings.VerticalLimit);
	
	// 记录最后输入时间
	if (FMath::Abs(YawInput) > 0.01f || FMath::Abs(PitchInput) > 0.01f)
	{
		LastFreeLookInputTime = GetWorld()->GetTimeSeconds();
		bIsFreeLooking = true;
	}
}

void UCameraControlComponent::ResetFreeLook()
{
	FreeLookOffset = FRotator::ZeroRotator;
	bIsFreeLooking = false;
}

void UCameraControlComponent::UpdateAdvancedCameraAdjustment()
{
	if (!CurrentLockOnTarget || !IsValid(CurrentLockOnTarget))
		return;
	
	float CurrentTime = GetWorld()->GetTimeSeconds();
	if (CurrentTime - LastAdvancedAdjustmentTime < ADVANCED_ADJUSTMENT_INTERVAL)
		return;
	
	LastAdvancedAdjustmentTime = CurrentTime;
	
	EEnemySizeCategory SizeCategory = GetTargetSizeCategoryV2(CurrentLockOnTarget);
	float Distance = CalculateDistanceToTarget(CurrentLockOnTarget);
	
	FVector AdjustedLocation = CalculateAdvancedTargetLocation(CurrentLockOnTarget, SizeCategory, Distance);
	
	OnCameraAdjusted.Broadcast(SizeCategory, Distance, AdjustedLocation);
	
	if (bEnableAdvancedAdjustmentDebugLogs)
	{
		UE_LOG(LogTemp, VeryVerbose, TEXT("Advanced Camera Adjustment: Size=%s, Distance=%.1f"), 
			*UEnum::GetValueAsString(SizeCategory), Distance);
	}
}

FVector UCameraControlComponent::CalculateAdvancedTargetLocation(AActor* Target, EEnemySizeCategory SizeCategory, float Distance) const
{
	if (!Target)
		return FVector::ZeroVector;
	
	// ✅ 使用新的三层系统获取基础位置
	FVector BaseLocation = GetOptimalLockOnPosition(Target);
	
	// 额外的尺寸偏移（高级调整）
	FVector SizeOffset = CalculateSizeBasedOffset(Target, SizeCategory);
	
	// 地形补偿
	FVector TerrainCompensation = FVector::ZeroVector;
	if (AdvancedCameraSettings.bEnableTerrainHeightCompensation)
	{
		TerrainCompensation = ApplyTerrainHeightCompensation(BaseLocation, Target);
	}
	
	return BaseLocation + SizeOffset + TerrainCompensation;
}

// ==================== 目标位置计算系统 ====================

FVector UCameraControlComponent::GetOptimalLockOnPosition(AActor* Target) const
{
	if (!Target)
		return FVector::ZeroVector;
	
	// 递归防护
	FRecursionGuard Guard(RecursionDepth, MAX_RECURSION_DEPTH);
	if (!Guard.IsValid())
	{
		UE_LOG(LogTemp, Error, TEXT("GetOptimalLockOnPosition: Max recursion depth exceeded!"));
		return Target->GetActorLocation();
	}
	
	// 三层锁定系统
	// 第一优先级：检查LockOnSocketComponent
	USceneComponent* LockOnSocket = Cast<USceneComponent>(
		Target->GetDefaultSubobjectByName(TEXT("LockOnSocketComponent"))
	);
	
	if (LockOnSocket)
	{
		FVector SocketWorldLocation = LockOnSocket->GetComponentLocation();
		FVector Offset = GetSizeBasedSocketOffset(Target);
		return SocketWorldLocation + Offset;
	}
	
	// 第二优先级：检查SkeletalMesh和Socket
	USkeletalMeshComponent* TargetMesh = Target->FindComponentByClass<USkeletalMeshComponent>();
	if (TargetMesh && TargetMesh->DoesSocketExist(FName("LockOnSocket")))
	{
		FVector SocketWorldLocation = TargetMesh->GetSocketLocation(FName("LockOnSocket"));
		FVector Offset = GetSizeBasedSocketOffset(Target);
		return SocketWorldLocation + Offset;
	}
	
	// 第三优先级：Static Mesh Component
	else if (UStaticMeshComponent* StaticMesh = Target->FindComponentByClass<UStaticMeshComponent>())
	{
		FBoxSphereBounds MeshBounds = StaticMesh->CalcBounds(StaticMesh->GetComponentTransform());
		FVector Center = MeshBounds.Origin;
		FVector Extent = MeshBounds.BoxExtent;
		
		FVector SocketLocation = Center + FVector(0, 0, Extent.Z);
		return SocketLocation;
	}
	// 最后降级：RootComponent
	else if (USceneComponent* RootComp = Target->GetRootComponent())
	{
		FBoxSphereBounds RootBounds = RootComp->CalcBounds(RootComp->GetComponentTransform());
		FVector Center = RootBounds.Origin;
		FVector Extent = RootBounds.BoxExtent;
		
		FVector SocketLocation = Center + FVector(0, 0, Extent.Z);
		return SocketLocation;
	}
	
	return Target->GetActorLocation();
}

FVector UCameraControlComponent::CalculateSizeBasedOffset(AActor* Target, EEnemySizeCategory SizeCategory) const
{
	if (!Target)
		return FVector::ZeroVector;
	
	float BoundingHeight = GetActorBoundingHeight(Target);
	
	// 根据体型调整高度偏移
	switch (SizeCategory)
	{
	case EEnemySizeCategory::Large:
		return FVector(0, 0, BoundingHeight * 0.6f);
	case EEnemySizeCategory::Medium:
		return FVector(0, 0, BoundingHeight * 0.5f);
	case EEnemySizeCategory::Small:
		return FVector(0, 0, BoundingHeight * 0.4f);
	default:
		return FVector(0, 0, BoundingHeight * 0.5f);
	}
}

FVector UCameraControlComponent::GetSizeBasedSocketOffset(AActor* Target) const
{
	if (!Target)
		return FVector::ZeroVector;
	
	EEnemySizeCategory SizeCategory = GetTargetSizeCategoryV2(Target);
	float BoundingHeight = GetActorBoundingHeight(Target);
	
	// 为不同体型提供额外的偏移量
	switch (SizeCategory)
	{
	case EEnemySizeCategory::Large:
		return FVector(0, 0, BoundingHeight * 0.1f);
	case EEnemySizeCategory::Medium:
		return FVector(0, 0, 0.0f);
	case EEnemySizeCategory::Small:
		return FVector(0, 0, -BoundingHeight * 0.1f);
	default:
		return FVector::ZeroVector;
	}
}

FVector UCameraControlComponent::GetStableTargetLocation(AActor* Target)
{
	if (!Target)
		return FVector::ZeroVector;
	
	// 如果目标切换了，重置缓存
	if (LastFrameTarget != Target)
	{
		FVector NewLocation = GetOptimalLockOnPosition(Target);
		CachedTargetLocation = NewLocation;
		LastFrameTarget = Target;
		return NewLocation;
	}
	
	// 使用插值更新缓存位置
	UpdateCachedTargetLocation(Target, GetWorld()->GetDeltaSeconds());
	
	return CachedTargetLocation;
}

// ==================== 编辑器安全组件获取 ====================

USpringArmComponent* UCameraControlComponent::GetSpringArmComponentSafe() const
{
	AActor* Owner = GetOwner();
	if (!Owner)
		return nullptr;
	
	// 尝试从Owner查找SpringArm组件
	USpringArmComponent* SpringArm = Owner->FindComponentByClass<USpringArmComponent>();
	if (SpringArm)
		return SpringArm;
	
	// 如果是编辑器环境，可能需要从Character获取
	ACharacter* OwnerCharacter = Cast<ACharacter>(Owner);
	if (OwnerCharacter)
	{
		SpringArm = OwnerCharacter->FindComponentByClass<USpringArmComponent>();
	}
	
	return SpringArm;
}

UCameraComponent* UCameraControlComponent::GetCameraComponentSafe() const
{
	AActor* Owner = GetOwner();
	if (!Owner)
		return nullptr;
	
	// 尝试从Owner查找Camera组件
	UCameraComponent* Camera = Owner->FindComponentByClass<UCameraComponent>();
	if (Camera)
		return Camera;
	
	// 如果是编辑器环境，可能需要从Character获取
	ACharacter* OwnerCharacter = Cast<ACharacter>(Owner);
	if (OwnerCharacter)
	{
		Camera = OwnerCharacter->FindComponentByClass<UCameraComponent>();
	}
	
	return Camera;
}

// ==================== 3D视口控制函数实现 ====================

void UCameraControlComponent::SetOrbitMode(bool bEnable)
{
	Viewport3DControl.bEnableOrbitMode = bEnable;
	
	if (bEnableCameraDebugLogs)
	{
		UE_LOG(LogTemp, Log, TEXT("Orbit mode: %s"), bEnable ? TEXT("Enabled") : TEXT("Disabled"));
	}
}

void UCameraControlComponent::UpdateOrbitPosition(float DeltaYaw, float DeltaPitch)
{
	if (!Viewport3DControl.bEnableOrbitMode)
		return;
	
	OrbitRotation.Yaw += DeltaYaw * Viewport3DControl.OrbitSpeed;
	OrbitRotation.Pitch += DeltaPitch * Viewport3DControl.OrbitSpeed;
	
	// 限制Pitch范围
	OrbitRotation.Pitch = FMath::Clamp(OrbitRotation.Pitch, -80.0f, 80.0f);
	
	if (bEnableCameraDebugLogs)
	{
		UE_LOG(LogTemp, VeryVerbose, TEXT("Orbit rotation: %s"), *OrbitRotation.ToString());
	}
}

#if WITH_EDITOR
void UCameraControlComponent::PreviewCameraPosition()
{
	USpringArmComponent* SpringArm = GetSpringArmComponentSafe();
	if (!SpringArm)
	{
		UE_LOG(LogTemp, Warning, TEXT("PreviewCameraPosition: SpringArm not found"));
		return;
	}
	
	bIsPreviewingCamera = true;
	
	// 计算轨道位置
	FVector OwnerLocation = GetOwner()->GetActorLocation();
	FVector OrbitOffset = OrbitRotation.RotateVector(FVector(Viewport3DControl.OrbitRadius, 0, 0));
	OrbitOffset.Z += Viewport3DControl.OrbitHeightOffset;
	
	FVector TargetLocation = OwnerLocation + OrbitOffset;
	
	// 临时调整相机位置用于预览
	SpringArm->SetWorldLocation(TargetLocation);
	SpringArm->SetWorldRotation((-OrbitOffset).Rotation());
	
	UE_LOG(LogTemp, Log, TEXT("Camera preview started at: %s"), *TargetLocation.ToString());
}

void UCameraControlComponent::ResetCameraPreview()
{
	bIsPreviewingCamera = false;
	OrbitRotation = FRotator::ZeroRotator;
	
	// 重置相机到默认位置
	USpringArmComponent* SpringArm = GetSpringArmComponentSafe();
	if (SpringArm)
	{
		SpringArm->SetRelativeRotation(FRotator::ZeroRotator);
		SpringArm->SetRelativeLocation(FVector::ZeroVector);
	}
	
	UE_LOG(LogTemp, Log, TEXT("Camera preview reset"));
}

void UCameraControlComponent::RotatePreviewLeft()
{
	UpdateOrbitPosition(-45.0f, 0.0f);
	if (bIsPreviewingCamera)
	{
		PreviewCameraPosition();
	}
}

void UCameraControlComponent::RotatePreviewRight()
{
	UpdateOrbitPosition(45.0f, 0.0f);
	if (bIsPreviewingCamera)
	{
		PreviewCameraPosition();
	}
}

void UCameraControlComponent::RotatePreviewUp()
{
	UpdateOrbitPosition(0.0f, 15.0f);
	if (bIsPreviewingCamera)
	{
		PreviewCameraPosition();
	}
}

void UCameraControlComponent::RotatePreviewDown()
{
	UpdateOrbitPosition(0.0f, -15.0f);
	if (bIsPreviewingCamera)
	{
		PreviewCameraPosition();
	}
}
#endif

// ==================== 私有辅助函数实现 ====================

void UCameraControlComponent::PerformCameraInterpolation(const FRotator& TargetRotation, float InterpSpeed)
{
	APlayerController* PlayerController = GetOwnerController();
	if (!PlayerController)
		return;
	
	FRotator CurrentRotation = PlayerController->GetControlRotation();
	float DeltaTime = GetWorld()->GetDeltaSeconds();
	FRotator NewRotation = FMath::RInterpTo(CurrentRotation, TargetRotation, DeltaTime, InterpSpeed);
	PlayerController->SetControlRotation(NewRotation);
}

void UCameraControlComponent::PerformCharacterRotationInterpolation(const FRotator& TargetRotation, float InterpSpeed)
{
	ACharacter* OwnerCharacter = GetOwnerCharacter();
	if (!OwnerCharacter)
		return;
	
	FRotator CurrentRotation = OwnerCharacter->GetActorRotation();
	float DeltaTime = GetWorld()->GetDeltaSeconds();
	FRotator NewRotation = FMath::RInterpTo(CurrentRotation, TargetRotation, DeltaTime, InterpSpeed);
	OwnerCharacter->SetActorRotation(NewRotation);
}

bool UCameraControlComponent::IsInterpolationComplete(const FRotator& Current, const FRotator& Target, float Threshold) const
{
	float YawDiff = FMath::Abs(FMath::FindDeltaAngleDegrees(Current.Yaw, Target.Yaw));
	float PitchDiff = FMath::Abs(FMath::FindDeltaAngleDegrees(Current.Pitch, Target.Pitch));
	return YawDiff < Threshold && PitchDiff < Threshold;
}

float UCameraControlComponent::NormalizeAngleDifference(float AngleDiff) const
{
	while (AngleDiff > 180.0f)
		AngleDiff -= 360.0f;
	while (AngleDiff < -180.0f)
		AngleDiff += 360.0f;
	return AngleDiff;
}

float UCameraControlComponent::CalculateDistanceWeight(float Distance, float MaxDistance) const
{
	return FMath::Clamp(Distance / MaxDistance, 0.0f, 1.0f);
}

FVector UCameraControlComponent::ApplyTerrainHeightCompensation(const FVector& BaseLocation, AActor* Target) const
{
	if (!Target)
		return FVector::ZeroVector;
	
	ACharacter* OwnerCharacter = GetOwnerCharacter();
	if (!OwnerCharacter)
		return FVector::ZeroVector;
	
	// 计算玩家和目标的高度差
	float HeightDiff = Target->GetActorLocation().Z - OwnerCharacter->GetActorLocation().Z;
	
	// 如果目标在更高的位置，稍微提升瞄准点
	if (HeightDiff > 100.0f)
	{
		return FVector(0, 0, HeightDiff * 0.3f);
	}
	else if (HeightDiff < -100.0f)
	{
		// 如果目标在更低的位置，稍微降低瞄准点
		return FVector(0, 0, HeightDiff * 0.2f);
	}
	
	return FVector::ZeroVector;
}

bool UCameraControlComponent::ValidateTarget(AActor* Target) const
{
	if (!Target)
		return false;
	
	if (!IsValid(Target))
		return false;
	
	// 检查目标是否有有效的位置
	FVector TargetLocation = Target->GetActorLocation();
	if (TargetLocation.ContainsNaN())
		return false;
	
	return true;
}

FVector UCameraControlComponent::PreCalculateFinalTargetLocation(AActor* Target) const
{
	if (!Target)
		return FVector::ZeroVector;
	
	return GetOptimalLockOnPosition(Target);
}

void UCameraControlComponent::UpdateCachedTargetLocation(AActor* Target, float DeltaTime)
{
	if (!Target)
		return;
	
	FVector NewTargetLocation = GetOptimalLockOnPosition(Target);
	
	// 使用插值更新缓存位置
	CachedTargetLocation = FMath::VInterpTo(
		CachedTargetLocation, 
		NewTargetLocation, 
		DeltaTime, 
		TargetLocationInterpSpeed
	);
}

// ==================== Protected辅助函数实现补充 ====================

FVector UCameraControlComponent::GetHeightOffsetForEnemySize(EEnemySizeCategory SizeCategory) const
{
	switch (SizeCategory)
	{
	case EEnemySizeCategory::Large:
		return FVector(0, 0, LargeEnemyHeightOffsetZ);
	case EEnemySizeCategory::Medium:
		return FVector(0, 0, MediumEnemyHeightOffsetZ);
	case EEnemySizeCategory::Small:
	default:
		return FVector(0, 0, SmallEnemyHeightOffsetZ);
	}
}

void UCameraControlComponent::UpdateCameraState(ECameraState NewState)
{
	if (CurrentCameraState != NewState)
	{
		CurrentCameraState = NewState;
		
		if (bEnableCameraDebugLogs)
		{
			UE_LOG(LogTemp, Log, TEXT("Camera state changed to: %s"), 
				*UEnum::GetValueAsString(NewState));
		}
	}
}

bool UCameraControlComponent::ShouldInterruptAutoControl(float TurnInput, float LookUpInput) const
{
	const float InputThreshold = 0.1f;
	return FMath::Abs(TurnInput) > InputThreshold || FMath::Abs(LookUpInput) > InputThreshold;
}

float UCameraControlComponent::GetActorBoundingHeight(AActor* Actor) const
{
	if (!Actor)
		return 0.0f;
	
	float Height = 200.0f; // 默认高度
	
	// 第一优先级：Capsule Component（最稳定）
	if (UCapsuleComponent* TargetCapsule = Actor->FindComponentByClass<UCapsuleComponent>())
	{
		Height = TargetCapsule->GetScaledCapsuleHalfHeight() * 2.0f;
	}
	// 其次尝试从SkeletalMesh获取高度
	else if (USkeletalMeshComponent* TargetMesh = Actor->FindComponentByClass<USkeletalMeshComponent>())
	{
		FBoxSphereBounds MeshBounds = TargetMesh->CalcBounds(TargetMesh->GetComponentTransform());
		Height = MeshBounds.BoxExtent.Z * 2.0f;
	}
	// 最后尝试从RootComponent获取
	else if (USceneComponent* RootComp = Actor->GetRootComponent())
	{
		FBoxSphereBounds RootBounds = RootComp->CalcBounds(RootComp->GetComponentTransform());
		Height = RootBounds.BoxExtent.Z * 2.0f;
	}
	
	return Height;
}

void UCameraControlComponent::AdjustSpringArmForSizeDistance(EEnemySizeCategory SizeCategory, float Distance, float BoundingHeight)
{
	USpringArmComponent* SpringArm = GetSpringArmComponent();
	if (!SpringArm)
		return;

	float TargetArmLength = BaseArmLength;

	// 根据尺寸调整
	switch (SizeCategory)
	{
	case EEnemySizeCategory::Large:
		TargetArmLength *= LargeSizeArmLengthMultiplier;
		break;
	case EEnemySizeCategory::Medium:
		TargetArmLength *= MediumSizeArmLengthMultiplier;
		break;
	case EEnemySizeCategory::Small:
		TargetArmLength *= SmallSizeArmLengthMultiplier;
		break;
	default:
		break;
	}

	// 平滑的距离响应逻辑
	static float PrevSizeDistanceRatio = 1.0f;
	float TargetSizeDistanceRatio = 1.0f;

	if (Distance <= 150.0f)
	{
		float T = FMath::Clamp(Distance / 150.0f, 0.0f, 1.0f);
		TargetSizeDistanceRatio = FMath::Lerp(0.5f, 0.75f, T);
	}
	else if (Distance <= 300.0f)
	{
		float T = FMath::Clamp((Distance - 150.0f) / 150.0f, 0.0f, 1.0f);
		TargetSizeDistanceRatio = FMath::Lerp(0.75f, 1.0f, T);
	}
	else
	{
		TargetSizeDistanceRatio = 1.0f;
	}

	float DeltaTime = 0.016f;
	if (GetWorld())
	{
		DeltaTime = GetWorld()->GetDeltaSeconds();
	}

	float SizeDistanceRatio = FMath::FInterpTo(PrevSizeDistanceRatio, TargetSizeDistanceRatio, DeltaTime, 5.0f);
	PrevSizeDistanceRatio = SizeDistanceRatio;

	// 将平滑后的比率应用到目标臂长
	TargetArmLength *= SizeDistanceRatio;
	bArmLengthAdjusted = true;

	SpringArm->TargetArmLength = TargetArmLength;
}

// ==================== 更多Protected辅助函数实现 ====================

ACharacter* UCameraControlComponent::GetOwnerCharacter() const
{
	return Cast<ACharacter>(GetOwner());
}

APlayerController* UCameraControlComponent::GetOwnerController() const
{
	ACharacter* OwnerCharacter = GetOwnerCharacter();
	if (OwnerCharacter)
	{
		return Cast<APlayerController>(OwnerCharacter->GetController());
	}
	return nullptr;
}

USpringArmComponent* UCameraControlComponent::GetSpringArmComponent() const
{
	// 优先使用缓存的组件
	if (CachedSpringArm && CachedSpringArm->IsValidLowLevel())
	{
		return CachedSpringArm;
	}
	
	// 回退到原有逻辑
	return GetSpringArmComponentSafe();
}

UCameraComponent* UCameraControlComponent::GetCameraComponent() const
{
	// 优先使用缓存的组件
	if (CachedCamera && CachedCamera->IsValidLowLevel())
	{
		return CachedCamera;
	}
	
	// 回退到原有逻辑
	return GetCameraComponentSafe();
}

float UCameraControlComponent::CalculateAngleToTarget(AActor* Target) const
{
	if (!Target)
		return 0.0f;
	
	ACharacter* OwnerCharacter = GetOwnerCharacter();
	if (!OwnerCharacter)
		return 0.0f;
	
	FVector PlayerLocation = OwnerCharacter->GetActorLocation();
	FVector TargetLocation = Target->GetActorLocation();
	FVector PlayerForward = OwnerCharacter->GetActorForwardVector();
	FVector DirectionToTarget = (TargetLocation - PlayerLocation).GetSafeNormal();
	
	float DotProduct = FVector::DotProduct(PlayerForward, DirectionToTarget);
	return FMath::Acos(DotProduct) * (180.0f / PI);
}

EEnemySizeCategory UCameraControlComponent::GetTargetSizeCategoryV2(AActor* Target) const
{
	if (!Target)
		return EEnemySizeCategory::Medium;
	
	float Height = 200.0f; // 默认高度
	
	// 第一优先级：Capsule Component（最稳定）
	if (UCapsuleComponent* TargetCapsule = Target->FindComponentByClass<UCapsuleComponent>())
	{
		Height = TargetCapsule->GetScaledCapsuleHalfHeight() * 2.0f;
	}
	// 第二优先级：Skeletal Mesh Component
	else if (USkeletalMeshComponent* TargetMesh = Target->FindComponentByClass<USkeletalMeshComponent>())
	{
		FBoxSphereBounds MeshBounds = TargetMesh->CalcBounds(TargetMesh->GetComponentTransform());
		Height = MeshBounds.BoxExtent.Z * 2.0f;
	}
	// 第三优先级：Static Mesh Component
	else if (UStaticMeshComponent* StaticMesh = Target->FindComponentByClass<UStaticMeshComponent>())
	{
		FBoxSphereBounds MeshBounds = StaticMesh->CalcBounds(StaticMesh->GetComponentTransform());
		Height = MeshBounds.BoxExtent.Z * 2.0f;
	}
	// 最后降级：RootComponent
	else if (USceneComponent* RootComp = Target->GetRootComponent())
	{
		FBoxSphereBounds RootBounds = RootComp->CalcBounds(RootComp->GetComponentTransform());
		Height = RootBounds.BoxExtent.Z * 2.0f;
	}
	
	// 简化为3级判断
	if (Height > LargeSizeHeightThreshold)
		return EEnemySizeCategory::Large;
	else if (Height > MediumSizeHeightThreshold)
		return EEnemySizeCategory::Medium;
	else
		return EEnemySizeCategory::Small;
}

float UCameraControlComponent::CalculateDistanceToTarget(AActor* Target) const
{
	if (!Target)
		return 0.0f;
	
	ACharacter* OwnerCharacter = GetOwnerCharacter();
	if (!OwnerCharacter)
		return  0.0f;
	
	return FVector::Dist(OwnerCharacter->GetActorLocation(), Target->GetActorLocation());
}

float UCameraControlComponent::GetCameraSpeedMultiplierForDistance(float Distance) const
{
	// 距离越远，插值速度越快
	if (Distance > VeryFarDistanceThreshold)
		return VeryFarDistanceSpeedMultiplier;
	else if (Distance > MidFarDistanceThreshold)
		return MidFarDistanceSpeedMultiplier;
	else
		return 1.0f;
}