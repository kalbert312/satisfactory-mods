#pragma once

#include "CoreMinimal.h"

namespace AutoSupportConstants
{
	static const FName ModReference = FName(TEXT("AutoSupport"));

	static const FName ModuleName_BuildGunExtensions = FName(TEXT("AutoSupportBuildGunExtensions"));
	static const FName ModuleName_PartPickerConfig = FName(TEXT("AutoSupportPartPickerConfig"));
	static const FName ModuleName_BuildConfig = FName(TEXT("AutoSupportBuildConfig"));

	static const FName TagName_AutoSupport_Trace_Ignore = FName(TEXT("AutoSupport.Trace.Ignore"));
	static const FName TagName_AutoSupport_Trace_Landscape = FName(TEXT("AutoSupport.Trace.Landscape"));

	static const FName TraceTag_BuildableAutoSupport = FName(TEXT("BuildableAutoSupport_Trace"));
}
