// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "Camera/CameraStateBase.h"

// Forward declarations
class USpringArmComponent;
class UCameraComponent;
class APlayerController;

/**
 * 相机系统适配器
 * 提供静态工具函数，将FCameraStateOutput应用到实际的相机组件
 * 这是一个纯静态工具类，不需要实例化
 */
class SOUL_API CameraSystemAdapter
{
public:
	/**
	 * 将相机状态输出应用到SpringArm和Camera组件
	 * @param Output - 要应用的相机状态输出
	 * @param SpringArm - 弹簧臂组件
	 * @param Camera - 相机组件
	 * @param Controller - 玩家控制器（用于设置控制旋转）
	 * @return 是否成功应用
	 */
	static bool ApplyOutputToComponents(
		const FCameraStateOutput& Output,
		USpringArmComponent* SpringArm,
		UCameraComponent* Camera,
		APlayerController* Controller
	);

	/**
	 * 验证组件引用是否有效
	 * @param SpringArm - 弹簧臂组件
	 * @param Camera - 相机组件
	 * @return 组件是否都有效
	 */
	static bool ValidateComponents(USpringArmComponent* SpringArm, UCameraComponent* Camera);

	/**
	 * 应用弹簧臂配置
	 * @param Output - 相机状态输出
	 * @param SpringArm - 弹簧臂组件
	 */
	static void ApplySpringArmSettings(const FCameraStateOutput& Output, USpringArmComponent* SpringArm);

	/**
	 * 应用相机配置
	 * @param Output - 相机状态输出
	 * @param Camera - 相机组件
	 */
	static void ApplyCameraSettings(const FCameraStateOutput& Output, UCameraComponent* Camera);

	/**
	 * 应用控制器旋转
	 * @param Output - 相机状态输出
	 * @param Controller - 玩家控制器
	 */
	static void ApplyControlRotation(const FCameraStateOutput& Output, APlayerController* Controller);
};
