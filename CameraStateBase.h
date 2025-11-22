// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "UObject/NoExportTypes.h"
#include "CameraStateBase.generated.h"

// Forward declarations
class AMyCharacter;
class USpringArmComponent;
class UCameraComponent;

/**
 * 相机状态输出结构
 * 包含相机的目标位置、旋转、臂长和视野角度
 */
USTRUCT(BlueprintType)
struct SOUL_API FCameraStateOutput
{
	GENERATED_BODY()

	/** 目标相机位置（世界空间） */
	UPROPERTY(BlueprintReadWrite, Category = "Camera State Output")
	FVector TargetPosition = FVector::ZeroVector;

	/** 目标相机旋转 */
	UPROPERTY(BlueprintReadWrite, Category = "Camera State Output")
	FRotator TargetRotation = FRotator::ZeroRotator;

	/** 目标弹簧臂长度 */
	UPROPERTY(BlueprintReadWrite, Category = "Camera State Output")
	float ArmLength = 300.0f;

	/** 目标视野角度 */
	UPROPERTY(BlueprintReadWrite, Category = "Camera State Output")
	float FieldOfView = 90.0f;

	// ==================== ✅ 新增：旋转控制标志 ====================
	
	/** 
	 * 是否强制设置SpringArm的世界旋转
	 * - true: 强制设置旋转（锁定状态需要，朝向目标）
	 * - false: 让SpringArm通过bUsePawnControlRotation自动跟随控制器（自由状态）
	 */
	UPROPERTY(BlueprintReadWrite, Category = "Camera State Output")
	bool bForceSpringArmRotation = false;
	
	/** 
	 * 是否强制设置控制器旋转
	 * - true: 强制覆盖控制器旋转（通常不需要）
	 * - false: 保持控制器旋转由输入系统控制（推荐）
	 */
	UPROPERTY(BlueprintReadWrite, Category = "Camera State Output")
	bool bForceControllerRotation = false;

	/** 默认构造函数 */
	FCameraStateOutput()
		: TargetPosition(FVector::ZeroVector)
		, TargetRotation(FRotator::ZeroRotator)
		, ArmLength(300.0f)
		, FieldOfView(90.0f)
		, bForceSpringArmRotation(false)
		, bForceControllerRotation(false)
	{
	}

	/** 带参数的构造函数 */
	FCameraStateOutput(const FVector& InPosition, const FRotator& InRotation, float InArmLength, float InFOV)
		: TargetPosition(InPosition)
		, TargetRotation(InRotation)
		, ArmLength(InArmLength)
		, FieldOfView(InFOV)
		, bForceSpringArmRotation(false)
		, bForceControllerRotation(false)
	{
	}
};

/**
 * 相机状态基类
 * 所有相机状态（自由、锁定、处决、死亡等）都继承自此类
 */
UCLASS(Abstract, Blueprintable, BlueprintType)
class SOUL_API UCameraStateBase : public UObject
{
	GENERATED_BODY()

public:
	/** 构造函数 */
	UCameraStateBase();

	/** 
	 * 计算相机状态输出
	 * @param DeltaTime - 帧时间间隔
	 * @param OwnerCharacter - 拥有此相机状态的角色
	 * @param SpringArm - 弹簧臂组件
	 * @param Camera - 相机组件
	 * @return 相机状态输出数据
	 */
	UFUNCTION(BlueprintNativeEvent, Category = "Camera State")
	FCameraStateOutput CalculateState(float DeltaTime, AMyCharacter* OwnerCharacter, USpringArmComponent* SpringArm, UCameraComponent* Camera);
	virtual FCameraStateOutput CalculateState_Implementation(float DeltaTime, AMyCharacter* OwnerCharacter, USpringArmComponent* SpringArm, UCameraComponent* Camera);

	/**
	 * 进入状态时调用
	 * @param OwnerCharacter - 拥有此相机状态的角色
	 * @param PreviousState - 上一个相机状态（可能为nullptr）
	 */
	UFUNCTION(BlueprintNativeEvent, Category = "Camera State")
	void OnEnterState(AMyCharacter* OwnerCharacter, UCameraStateBase* PreviousState);
	virtual void OnEnterState_Implementation(AMyCharacter* OwnerCharacter, UCameraStateBase* PreviousState);

	/**
	 * 退出状态时调用
	 * @param OwnerCharacter - 拥有此相机状态的角色
	 * @param NextState - 下一个相机状态（可能为nullptr）
	 */
	UFUNCTION(BlueprintNativeEvent, Category = "Camera State")
	void OnExitState(AMyCharacter* OwnerCharacter, UCameraStateBase* NextState);
	virtual void OnExitState_Implementation(AMyCharacter* OwnerCharacter, UCameraStateBase* NextState);

	/**
	 * 获取状态优先级
	 * 优先级越高的状态会覆盖优先级低的状态
	 * @return 状态优先级值
	 */
	UFUNCTION(BlueprintPure, Category = "Camera State")
	int32 GetPriority() const { return Priority; }

	/**
	 * 设置状态优先级
	 * @param NewPriority - 新的优先级值
	 */
	UFUNCTION(BlueprintCallable, Category = "Camera State")
	void SetPriority(int32 NewPriority) { Priority = NewPriority; }

	/**
	 * 获取状态是否可被中断
	 * @return 是否可被中断
	 */
	UFUNCTION(BlueprintPure, Category = "Camera State")
	bool GetCanBeInterrupted() const { return bCanBeInterrupted; }

	/**
	 * 设置状态是否可被中断
	 * @param bCanInterrupt - 是否可被中断
	 */
	UFUNCTION(BlueprintCallable, Category = "Camera State")
	void SetCanBeInterrupted(bool bCanInterrupt) { bCanBeInterrupted = bCanInterrupt; }

	/**
	 * 获取状态名称
	 * @return 状态名称
	 */
	UFUNCTION(BlueprintPure, Category = "Camera State")
	FString GetStateName() const { return StateName; }

	/**
	 * 设置状态名称
	 * @param NewName - 新的状态名称
	 */
	UFUNCTION(BlueprintCallable, Category = "Camera State")
	void SetStateName(const FString& NewName) { StateName = NewName; }

protected:
	/** 
	 * 状态优先级
	 * 数值越大优先级越高
	 * 默认优先级：Free=0, LockOn=10, Execution=20, Death=30
	 */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Camera State")
	int32 Priority = 0;

	/**
	 * 状态是否可被中断
	 * 如果为false，则只有更高优先级的状态才能打断当前状态
	 */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Camera State")
	bool bCanBeInterrupted = true;

	/**
	 * 状态调试名称
	 * 用于日志和调试显示
	 */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Camera State")
	FString StateName = TEXT("BaseState");
};
