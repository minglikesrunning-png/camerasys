// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "Components/ActorComponent.h"
#include "LockOnConfigComponent.generated.h"

/**
 * 轻量级组件：为每个敌人实例提供独立的锁定配置覆盖
 * 
 * 使用场景：
 * - 为特定敌人个性化调整锁定偏移值
 * - 覆盖默认的Socket名称
 * - 实现细粒度的锁定位置控制
 * 
 * 特性：
 * - 可选覆盖：通过bOverrideOffset开关控制是否启用
 * - 零性能开销：未启用时不影响性能
 * - 即时生效：运行时修改立即应用
 */
UCLASS(ClassGroup=(Custom), meta=(BlueprintSpawnableComponent))
class SOUL_API ULockOnConfigComponent : public UActorComponent
{
	GENERATED_BODY()

public:
	// Sets default values for this component's properties
	ULockOnConfigComponent();

	// ==================== 覆盖开关 ====================
	/** 是否覆盖默认偏移值（false则使用MyCharacter中的全局配置） */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Lock On Override", meta = (DisplayName = "Enable Custom Offset"))
	bool bOverrideOffset = false;
	
	// ==================== 自定义偏移值 ====================
	/** 自定义锁定偏移（仅在bOverrideOffset=true时生效） */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Lock On Override", 
		meta = (EditCondition = "bOverrideOffset", DisplayName = "Custom Offset"))
	FVector CustomOffset = FVector(0, 0, -50.0f);
	
	// ==================== Socket偏好设置 ====================
	/** 是否优先使用Socket（false则优先使用Capsule） */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Lock On Override", meta = (DisplayName = "Prefer Socket"))
	bool bPreferSocket = true;
	
	/** 自定义Socket名称（仅在bPreferSocket=true时生效） */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Lock On Override", 
		meta = (EditCondition = "bPreferSocket", DisplayName = "Custom Socket Name"))
	FName CustomSocketName = TEXT("LockOnSocket");
	
	// ==================== 辅助函数 ====================
	/** 验证配置是否有效 */
	UFUNCTION(BlueprintCallable, Category = "Lock On Override")
	bool IsConfigValid() const;
	
	/** 获取当前生效的偏移值（考虑覆盖开关） */
	UFUNCTION(BlueprintPure, Category = "Lock On Override")
	FVector GetEffectiveOffset() const;
	
	/** 获取当前生效的Socket名称（考虑偏好设置） */
	UFUNCTION(BlueprintPure, Category = "Lock On Override")
	FName GetEffectiveSocketName() const;

protected:
	// Called when the game starts
	virtual void BeginPlay() override;

public:
	// Called every frame
	virtual void TickComponent(float DeltaTime, ELevelTick TickType, FActorComponentTickFunction* ThisTickFunction) override;
};
