// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "Components/ActorComponent.h"
#include "CameraStateBase.h"
#include "CameraPipeline.generated.h"

// Forward declarations
class AMyCharacter;
class USpringArmComponent;
class UCameraComponent;
class UCameraStateBase;

/**
 * 相机管线状态枚举
 * 定义所有可能的相机状态类型
 */
UENUM(BlueprintType)
enum class ECameraPipelineState : uint8
{
	/** 自由相机状态（默认） */
	Free UMETA(DisplayName = "Free Camera"),
	
	/** 锁定目标状态 */
	LockOn UMETA(DisplayName = "Lock-On Camera"),
	
	/** 处决动画状态 */
	Execution UMETA(DisplayName = "Execution Camera"),
	
	/** 死亡状态 */
	Death UMETA(DisplayName = "Death Camera"),
	
	/** 无效状态 */
	None UMETA(Hidden)
};

/**
 * 相机混合信息
 * 用于配置不同状态之间的过渡混合
 */
USTRUCT(BlueprintType)
struct SOUL_API FCameraBlendInfo
{
	GENERATED_BODY()

	/** 混合时间（秒） */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Camera Blend", meta = (ClampMin = "0.0", ClampMax = "5.0"))
	float BlendTime = 0.5f;

	/** 混合曲线指数（1.0=线性，>1.0=缓入缓出） */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Camera Blend", meta = (ClampMin = "0.1", ClampMax = "5.0"))
	float BlendExponent = 2.0f;

	/** 是否在混合期间锁定输入 */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Camera Blend")
	bool bLockInputDuringBlend = false;

	/** 默认构造函数 */
	FCameraBlendInfo()
		: BlendTime(0.5f)
		, BlendExponent(2.0f)
		, bLockInputDuringBlend(false)
	{
	}

	/** 带参数的构造函数 */
	FCameraBlendInfo(float InBlendTime, float InBlendExponent, bool bInLockInput)
		: BlendTime(InBlendTime)
		, BlendExponent(InBlendExponent)
		, bLockInputDuringBlend(bInLockInput)
	{
	}
};

/**
 * 相机管线组件
 * 负责管理不同相机状态，处理状态切换和混合
 */
UCLASS(ClassGroup = (Camera), meta = (BlueprintSpawnableComponent))
class SOUL_API UCameraPipeline : public UActorComponent
{
	GENERATED_BODY()

public:
	/** 构造函数 */
	UCameraPipeline();

	/** 组件开始时调用 */
	virtual void BeginPlay() override;

	/** 每帧更新 */
	virtual void TickComponent(float DeltaTime, ELevelTick TickType, FActorComponentTickFunction* ThisTickFunction) override;

	/**
	 * 设置相机状态
	 * @param NewState - 新的相机状态
	 * @param bForceChange - 是否强制切换（忽略优先级检查）
	 * @return 是否成功切换状态
	 */
	UFUNCTION(BlueprintCallable, Category = "Camera Pipeline")
	bool SetCameraState(ECameraPipelineState NewState, bool bForceChange = false);

	/**
	 * 获取当前相机状态
	 * @return 当前相机状态枚举
	 */
	UFUNCTION(BlueprintPure, Category = "Camera Pipeline")
	ECameraPipelineState GetCurrentCameraState() const { return CurrentState; }

	/**
	 * 获取最终相机输出
	 * @return 当前帧的相机状态输出
	 */
	UFUNCTION(BlueprintPure, Category = "Camera Pipeline")
	FCameraStateOutput GetFinalOutput() const { return FinalOutput; }

	/**
	 * 应用相机输出到实际组件
	 * 将计算好的相机数据应用到SpringArm和Camera组件
	 */
	UFUNCTION(BlueprintCallable, Category = "Camera Pipeline")
	void ApplyCameraOutput();

	/**
	 * 获取指定状态的实例
	 * @param State - 要获取的状态类型
	 * @return 状态实例指针（如果不存在返回nullptr）
	 */
	UFUNCTION(BlueprintPure, Category = "Camera Pipeline")
	UCameraStateBase* GetStateInstance(ECameraPipelineState State) const;

	/**
	 * 获取当前是否正在混合状态
	 * @return 是否正在混合
	 */
	UFUNCTION(BlueprintPure, Category = "Camera Pipeline")
	bool IsBlending() const { return bIsBlending; }

protected:
	/**
	 * 初始化所有状态实例
	 * 根据StateClasses配置创建状态对象
	 */
	void InitializeStates();

	/**
	 * 切换到新状态的内部实现
	 * @param NewState - 新状态
	 * @return 是否成功切换
	 */
	bool SwitchToState(ECameraPipelineState NewState);

	/**
	 * 查找并缓存组件引用
	 */
	void CacheComponents();

	/**
	 * 混合两个相机状态输出
	 * @param From - 起始状态输出
	 * @param To - 目标状态输出
	 * @param Alpha - 混合系数（0.0-1.0）
	 * @return 混合后的相机输出
	 */
	FCameraStateOutput BlendStates(const FCameraStateOutput& From, const FCameraStateOutput& To, float Alpha) const;

	/**
	 * 应用后处理效果到相机输出
	 * 包括碰撞检测、防穿墙等
	 * @param Output - 要处理的相机输出（引用传递，会被修改）
	 */
	void ApplyPostProcessing(FCameraStateOutput& Output);

protected:
	// ==================== 状态配置 ====================
	
	/** 
	 * 状态类映射
	 * 将ECameraPipelineState枚举映射到具体的UCameraStateBase子类
	 * 在蓝图中配置
	 */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Camera Pipeline|Configuration")
	TMap<ECameraPipelineState, TSubclassOf<UCameraStateBase>> StateClasses;

	/**
	 * 状态混合配置
	 * 配置不同状态之间的过渡参数
	 */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Camera Pipeline|Configuration")
	TMap<ECameraPipelineState, FCameraBlendInfo> BlendSettings;

	/**
	 * 默认混合配置
	 * 当BlendSettings中没有特定配置时使用
	 */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Camera Pipeline|Configuration")
	FCameraBlendInfo DefaultBlendInfo;

	// ==================== 后处理配置 ====================
	
	/**
	 * 是否启用相机碰撞检测
	 * 防止相机穿墙
	 */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Camera Pipeline|Post Processing")
	bool bEnableCollisionDetection = true;

	/**
	 * 相机碰撞安全距离系数
	 * 检测到碰撞后，臂长会乘以此系数
	 */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Camera Pipeline|Post Processing", meta = (ClampMin = "0.1", ClampMax = "1.0"))
	float CollisionSafetyFactor = 0.9f;

	/**
	 * 相机最小臂长
	 * 防止相机距离过近
	 */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Camera Pipeline|Post Processing", meta = (ClampMin = "10.0", ClampMax = "500.0"))
	float MinimumArmLength = 50.0f;

	// ==================== 运行时状态 ====================
	
	/**
	 * 状态实例缓存
	 * 存储已创建的状态对象实例
	 */
	UPROPERTY(Transient)
	TMap<ECameraPipelineState, UCameraStateBase*> StateInstances;

	/**
	 * 当前激活的相机状态
	 */
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Camera Pipeline|Runtime")
	ECameraPipelineState CurrentState = ECameraPipelineState::Free;

	/**
	 * 上一个相机状态
	 */
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Camera Pipeline|Runtime")
	ECameraPipelineState PreviousState = ECameraPipelineState::None;

	/**
	 * 最终相机输出
	 * 当前帧计算的相机数据
	 */
	UPROPERTY(Transient)
	FCameraStateOutput FinalOutput;

	/**
	 * 当前状态的输出（未混合）
	 * 用于混合计算
	 */
	UPROPERTY(Transient)
	FCameraStateOutput CurrentStateOutput;

	/**
	 * 上一个状态的输出
	 * 用于混合计算
	 */
	UPROPERTY(Transient)
	FCameraStateOutput PreviousStateOutput;

	// ==================== 组件缓存 ====================
	
	/**
	 * 拥有此管线的角色
	 */
	UPROPERTY(Transient)
	AMyCharacter* OwnerCharacter = nullptr;

	/**
	 * 弹簧臂组件缓存
	 */
	UPROPERTY(Transient)
	USpringArmComponent* CachedSpringArm = nullptr;

	/**
	 * 相机组件缓存
	 */
	UPROPERTY(Transient)
	UCameraComponent* CachedCamera = nullptr;

	// ==================== 混合相关 ====================
	
	/**
	 * 是否正在进行状态混合
	 */
	UPROPERTY(Transient)
	bool bIsBlending = false;

	/**
	 * 状态混合计时器
	 * 记录当前混合已进行的时间
	 */
	UPROPERTY(Transient)
	float StateBlendTimer = 0.0f;

	/**
	 * 当前混合持续时间
	 * 从BlendSettings或DefaultBlendInfo获取
	 */
	UPROPERTY(Transient)
	float StateBlendDuration = 0.5f;

	/**
	 * 当前混合曲线指数
	 */
	UPROPERTY(Transient)
	float CurrentBlendExponent = 2.0f;

	// ==================== 调试 ====================
	
	/**
	 * 是否启用相机管线调试日志
	 */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Camera Pipeline|Debug")
	bool bEnableDebugLogs = false;

	/**
	 * 是否在屏幕上显示相机状态信息
	 */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Camera Pipeline|Debug")
	bool bShowDebugInfo = false;

	/**
	 * 是否显示碰撞检测调试线
	 */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Camera Pipeline|Debug")
	bool bShowCollisionDebug = false;
};
