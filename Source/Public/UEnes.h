// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Modules/ModuleManager.h"
#include "UEnes.generated.h"

// If non-zero, it will be possible to have more than one emulator in a level. Because Nestopia's callback references aren't
// instanced, each emulator thread will have to lock the staic core mutex before running its frame update. This could slow things down!
#define NES_SUPPORT_MULTIPLE_INSTANCES 1

// If non-zero, the NES emulator thread will wait for its corresponding NesComponent to consume the written frame data before executing another frame.
// While this behavior guarantees that each frame will be presented to the user, it could slow down emulation if the game thread is running under 60 FPS (which may or not be a bad thing).
#define NES_SYNC_THREADS 0

// If non-zero, the audio output of the NES emulator thread will be inserted into a queue rather than consumed immediately. Experimental.
#define NES_USE_AUDIO_QUEUE 0

DECLARE_LOG_CATEGORY_EXTERN(LogUEnesTiming, Verbose, All);
DECLARE_LOG_CATEGORY_EXTERN(LogUEnesAudio, Verbose, All);
DECLARE_LOG_CATEGORY_EXTERN(LogUEnesVideo, Verbose, All);

USTRUCT(BlueprintType)
struct FNesSettings
{
	GENERATED_BODY()

	UPROPERTY(BlueprintReadWrite, EditAnywhere)
	bool bSaveBatteryBackup = true;
	
	UPROPERTY(BlueprintReadOnly, EditAnywhere)
	int32 FramesPerSecond = 60;

	// Nestopia requires specific screen dimensions for use with specific filters, so don't expose to user
	int32 ScreenWidth = 256;
	int32 ScreenHeight = 240;

	// Audio params will be initialized to the appropriate values at runtime
	int32 SampleRate;
	int32 SamplesPerFrame;
};

class FUEnesModule : public IModuleInterface
{
public:

	/** IModuleInterface implementation */
	virtual void StartupModule() override;
	virtual void ShutdownModule() override;
};
