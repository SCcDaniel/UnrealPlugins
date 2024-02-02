// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "Kismet/BlueprintFunctionLibrary.h"
#include "HbbRuntimeBPFunctionLibrary.generated.h"

/**
 * 
 */
UCLASS()
class HBBRUNTIMEPLUGINS_API UHbbRuntimeBPFunctionLibrary : public UBlueprintFunctionLibrary
{
	GENERATED_BODY()

	UFUNCTION(BlueprintCallable, Category="Rendering|Material", meta=(Keywords="SetFloatParameterValue", WorldContext="WorldContextObject", MaterialParameterCollectionFunction = "true"))
	static class UDynamicTexture2DArray* CreateDynamicTexture2DArray(UObject* Owner);
	
};
