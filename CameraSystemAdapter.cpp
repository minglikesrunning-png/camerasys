// Fill out your copyright notice in the Description page of Project Settings.

#include "Camera/CameraSystemAdapter.h"
#include "GameFramework/SpringArmComponent.h"
#include "Camera/CameraComponent.h"
#include "GameFramework/PlayerController.h"

bool CameraSystemAdapter::ApplyOutputToComponents(
	const FCameraStateOutput& Output,
	USpringArmComponent* SpringArm,
	UCameraComponent* Camera,
	APlayerController* Controller)
{
	// 验证必要组件
	if (!ValidateComponents(SpringArm, Camera))
	{
		UE_LOG(LogTemp, Warning, TEXT("CameraSystemAdapter: Invalid component references"));
		return false;
	}

	// 应用弹簧臂设置
	ApplySpringArmSettings(Output, SpringArm);

	// 应用相机设置
	ApplyCameraSettings(Output, Camera);

	// 应用控制器旋转（如果提供了控制器）
	if (Controller)
	{
		ApplyControlRotation(Output, Controller);
	}

	return true;
}

bool CameraSystemAdapter::ValidateComponents(USpringArmComponent* SpringArm, UCameraComponent* Camera)
{
	if (!SpringArm)
	{
		UE_LOG(LogTemp, Error, TEXT("CameraSystemAdapter: SpringArm is null"));
		return false;
	}

	if (!Camera)
	{
		UE_LOG(LogTemp, Error, TEXT("CameraSystemAdapter: Camera is null"));
		return false;
	}

	return true;
}

void CameraSystemAdapter::ApplySpringArmSettings(const FCameraStateOutput& Output, USpringArmComponent* SpringArm)
{
	if (!SpringArm)
	{
		return;
	}

	// 设置弹簧臂长度（总是应用）
	SpringArm->TargetArmLength = Output.ArmLength;

	// ==================== ✅ 修改：根据标志决定是否强制设置旋转 ====================
	// 只有在状态明确要求时才强制设置旋转（例如锁定状态）
	// 否则让SpringArm通过bUsePawnControlRotation自动跟随控制器旋转（自由状态）
	if (Output.bForceSpringArmRotation)
	{
		// 锁定状态或其他需要强制旋转的状态
		SpringArm->SetWorldRotation(Output.TargetRotation);
	}
	// 自由状态：不设置旋转，SpringArm会自动使用Controller->ControlRotation
	// 这样玩家通过Turn/LookUp输入修改的旋转会正常生效

	// 注意：位置通常由SpringArm自动处理，基于其附加的父组件
	// 如果需要直接设置位置（非常规用法），可以取消注释下面的代码
	// SpringArm->SetWorldLocation(Output.TargetPosition);
}

void CameraSystemAdapter::ApplyCameraSettings(const FCameraStateOutput& Output, UCameraComponent* Camera)
{
	if (!Camera)
	{
		return;
	}

	// 设置相机视野角度
	Camera->SetFieldOfView(Output.FieldOfView);
}

void CameraSystemAdapter::ApplyControlRotation(const FCameraStateOutput& Output, APlayerController* Controller)
{
	if (!Controller)
	{
		return;
	}

	// ==================== ✅ 修改：根据标志决定是否强制设置控制器旋转 ====================
	// 通常不需要强制设置控制器旋转，因为输入系统已经通过
	// AddControllerYawInput/PitchInput 修改了 ControlRotation
	if (Output.bForceControllerRotation)
	{
		// 特殊情况：需要完全覆盖控制器旋转
		Controller->SetControlRotation(Output.TargetRotation);
	}
	// 否则：保持控制器旋转由输入系统（Turn/LookUp函数）控制
	// 这样玩家的鼠标和手柄输入可以正常工作
}
