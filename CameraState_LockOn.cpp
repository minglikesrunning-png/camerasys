// Fill out your copyright notice in the Description page of Project Settings.

#include "Camera/CameraState_LockOn.h"
#include "MyCharacter.h"
#include "GameFramework/SpringArmComponent.h"
#include "Camera/CameraComponent.h"
#include "Kismet/KismetMathLibrary.h"
#include "TargetDetectionComponent.h"  // ✅ 新增
#include "Camera/CameraPipeline.h"     // ✅ 新增

UCameraState_LockOn::UCameraState_LockOn()
{
	// 设置锁定相机状态的基本属性
	StateName = TEXT("LockOnCamera");
	Priority = 10; // 高于Free状态的优先级
	bCanBeInterrupted = true; // 可以被更高优先级状态中断

	// 初始化配置参数
	LockOnArmLength = 350.0f;
	TargetOffset = FVector(0.0f, 0.0f, 0.0f);
	TrackingSpeed = 5.0f;
	LockOnFieldOfView = 90.0f;
	bEnableSmoothTracking = false;
	CameraPitchOffset = 0.0f;

	// 初始化内部状态
	LastCameraRotation = FRotator::ZeroRotator;
	bLastRotationInitialized = false;

	// ✅ 距离检测配置初始化
	bEnableDistanceAutoUnlock = true;
	MaxLockOnDistance = 2000.0f;
	DistanceCheckInterval = 0.2f;
	LastDistanceCheckTime = 0.0f;
}

FCameraStateOutput UCameraState_LockOn::CalculateState_Implementation(float DeltaTime, AMyCharacter* OwnerCharacter, USpringArmComponent* SpringArm, UCameraComponent* Camera)
{
	FCameraStateOutput Output;

	// 验证输入参数
	if (!OwnerCharacter || !SpringArm || !Camera)
	{
		UE_LOG(LogTemp, Warning, TEXT("UCameraState_LockOn: Invalid parameters in CalculateState"));
		return Output;
	}

	// 获取当前锁定目标
	AActor* LockOnTarget = OwnerCharacter->GetLockOnTarget();
	if (!LockOnTarget)
	{
		UE_LOG(LogTemp, Warning, TEXT("UCameraState_LockOn: No lock-on target found"));
		// 没有锁定目标时，返回当前相机状态
		Output.TargetPosition = SpringArm->GetComponentLocation();
		Output.TargetRotation = SpringArm->GetComponentRotation();
		Output.ArmLength = LockOnArmLength;
		Output.FieldOfView = LockOnFieldOfView;
		return Output;
	}

	// ==================== ✅ 距离检测自动解锁 ====================
	// 只有启用距离自动解锁功能时才执行检测
	if (bEnableDistanceAutoUnlock)
	{
		float CurrentTime = OwnerCharacter->GetWorld()->GetTimeSeconds();
		if (CurrentTime - LastDistanceCheckTime >= DistanceCheckInterval)
		{
			LastDistanceCheckTime = CurrentTime;
			
			// 获取TargetDetectionComponent进行距离检测
			UTargetDetectionComponent* DetectionComp = 
				OwnerCharacter->FindComponentByClass<UTargetDetectionComponent>();
			
			if (DetectionComp)
			{
				// 使用TargetDetectionComponent的IsTargetStillLockable检查目标是否仍然可锁定
				// 该函数会综合判断距离、视线等条件
				if (!DetectionComp->IsTargetStillLockable(LockOnTarget))
				{
					UE_LOG(LogTemp, Warning, TEXT("🔓 LockOn Camera: Target '%s' out of range - triggering auto unlock"), 
						*LockOnTarget->GetName());
					
					if (GEngine)
					{
						GEngine->AddOnScreenDebugMessage(-1, 3.0f, FColor::Red, 
							FString::Printf(TEXT("Target out of range (%.0f units) - Auto Unlock"), 
								FVector::Dist(OwnerCharacter->GetActorLocation(), LockOnTarget->GetActorLocation())));
					}
					
					// 通过CameraPipeline切换到自由相机状态
					// SetCameraState会自动处理状态清理和目标解锁
					UCameraPipeline* Pipeline = OwnerCharacter->FindComponentByClass<UCameraPipeline>();
					if (Pipeline)
					{
						Pipeline->SetCameraState(ECameraPipelineState::Free, false);
					}
					else
					{
						UE_LOG(LogTemp, Error, TEXT("❌ LockOn Camera: CameraPipeline component not found!"));
					}
					
					// 返回自由相机的默认输出
					Output.TargetPosition = SpringArm->GetComponentLocation();
					Output.TargetRotation = SpringArm->GetComponentRotation();
					Output.ArmLength = 400.0f;
					Output.FieldOfView = 90.0f;
					Output.bForceSpringArmRotation = false;
					Output.bForceControllerRotation = false;
					return Output;
				}
			}
			else
			{
				// 仅在首次警告时输出，避免日志刷屏
				static bool bWarningShown = false;
				if (!bWarningShown)
				{
					UE_LOG(LogTemp, Error, TEXT("❌ LockOn Camera: TargetDetectionComponent not found on character!"));
					bWarningShown = true;
				}
			}
		}
	}

	// 使用Character的GetOptimalLockPosition()获取目标锁定位置
	FVector TargetLocation = OwnerCharacter->GetOptimalLockPosition(LockOnTarget);

	// 应用目标偏移
	TargetLocation += TargetOffset;

	// 计算从角色到目标的方向
	FVector CharacterLocation = OwnerCharacter->GetActorLocation();
	FVector DirectionToTarget = (TargetLocation - CharacterLocation).GetSafeNormal();

	// 计算相机应该朝向的旋转
	FRotator LookAtRotation = UKismetMathLibrary::FindLookAtRotation(CharacterLocation, TargetLocation);

	// 应用俯仰角偏移
	LookAtRotation.Pitch += CameraPitchOffset;

	// 确保Pitch在合理范围内
	LookAtRotation.Pitch = FMath::Clamp(LookAtRotation.Pitch, -80.0f, 80.0f);

	// 根据是否启用平滑跟踪决定最终旋转
	FRotator FinalRotation = LookAtRotation;
	if (bEnableSmoothTracking && bLastRotationInitialized)
	{
		// 使用插值平滑相机旋转
		FinalRotation = UKismetMathLibrary::RInterpTo(LastCameraRotation, LookAtRotation, DeltaTime, TrackingSpeed);
	}

	// 更新上一帧旋转记录
	LastCameraRotation = FinalRotation;
	bLastRotationInitialized = true;

	// 填充输出结构
	Output.TargetPosition = SpringArm->GetComponentLocation(); // 位置由SpringArm自动处理
	Output.TargetRotation = FinalRotation;
	Output.ArmLength = LockOnArmLength;
	Output.FieldOfView = LockOnFieldOfView;

	// ==================== ✅ 新增：设置旋转控制标志 ====================
	// 锁定状态：需要强制设置SpringArm旋转来朝向目标
	// 但不需要强制设置控制器旋转（玩家仍可能需要手动调整视角）
	Output.bForceSpringArmRotation = true;  // 强制朝向锁定目标
	Output.bForceControllerRotation = false; // 保持输入系统控制

	return Output;
}

void UCameraState_LockOn::OnEnterState_Implementation(AMyCharacter* OwnerCharacter, UCameraStateBase* PreviousState)
{
	Super::OnEnterState_Implementation(OwnerCharacter, PreviousState);

	// 重置平滑跟踪状态
	bLastRotationInitialized = false;

	// ✅ 新增：重置距离检测计时器
	LastDistanceCheckTime = 0.0f;

	if (GEngine)
	{
		GEngine->AddOnScreenDebugMessage(
			-1, 
			2.0f, 
			FColor::Yellow, 
			FString::Printf(TEXT("Camera State: Entered Lock-On Camera Mode"))
		);
	}

	UE_LOG(LogTemp, Log, TEXT("UCameraState_LockOn: Entered state from %s"), 
		PreviousState ? *PreviousState->GetStateName() : TEXT("None"));

	// 如果有有效的锁定目标，显示目标信息
	if (OwnerCharacter)
	{
		AActor* LockOnTarget = OwnerCharacter->GetLockOnTarget();
		if (LockOnTarget)
		{
			UE_LOG(LogTemp, Log, TEXT("UCameraState_LockOn: Tracking target %s"), *LockOnTarget->GetName());
		}
	}
}

void UCameraState_LockOn::OnExitState_Implementation(AMyCharacter* OwnerCharacter, UCameraStateBase* NextState)
{
	Super::OnExitState_Implementation(OwnerCharacter, NextState);

	// 清理状态
	bLastRotationInitialized = false;

	// ✅ 新增：重置距离检测计时器
	LastDistanceCheckTime = 0.0f;

	UE_LOG(LogTemp, Log, TEXT("UCameraState_LockOn: Exiting to %s"), 
		NextState ? *NextState->GetStateName() : TEXT("None"));
}
