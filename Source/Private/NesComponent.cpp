#include "NesComponent.h"
#include "UEnes.h"
#include "NesThread.h"
#include "ImageUtils.h"
#include "AudioMixerTypes.h"
#include "GenericPlatform/GenericPlatformProperties.h"
#include "EmuCore/NstBase.hpp"
#include "Async/TaskGraphInterfaces.h"

void UNesComponent::FrameReadyCallback(const TArray<uint8>& VideoData, const TArray<uint8>& AudioData, int32 AudioByteCount)
{
	if (!IsValid(this))
	{
		return;
	}

	if (EmulationTickThread != nullptr)
	{
#if NES_SYNC_THREADS
		// We shouldn't be here if there is no data ready
		ensure(EmulationTickThread->bDataPending);		
#endif

#if !NES_USE_AUDIO_QUEUE
		// Copy audio buffer
		FMemory::Memcpy(AudioBuffer.GetData(), AudioData.GetData(), AudioByteCount);
#else
		// Copy pending audio data into queue
		// Slow iteration!!
		int NumBytes = AudioData.Num();
		for (int i = 0; i < AudioByteCount; i++)
		{
			AudioBuffer.Enqueue(AudioData[i]);
		}
#endif	
		// Copy video buffer
		FMemory::Memcpy(VideoBuffer.GetData(), VideoData.GetData(), VideoBuffer.Num());

#if NES_SYNC_THREADS
		// Let the NES thread know we've consumed the data
		if (EmulationTickThread->bDataPending)
		{
			FScopeLock DataPendingLock(&EmulationTickThread->DataPendingWriteCriticalSection);
			EmulationTickThread->bDataPending = false;
		}
#endif

	}
	
	FrameNumber++;
	PostExecuteFrame(AudioByteCount);

#if !UE_BUILD_SHIPPING
	if (FrameNumber % 20 == 0)
	{
		UE_LOG(LogUEnesAudio, VeryVerbose, TEXT("Audio Len: %d"), NesSoundStream->GetAvailableAudioByteCount());
	}
#endif
}

void UNesComponent::CreateScreenTexture()
{
	// TODO Make a UTextureRenderTarget2D
	ScreenTexture = UTexture2D::CreateTransient(NesSettings.ScreenWidth, NesSettings.ScreenHeight, EPixelFormat::PF_R8G8B8A8, FName(TEXT("ScreenTexture") + FGuid::NewGuid().ToString()));
	ScreenTexture->Filter = TextureFilter::TF_Nearest;
	ScreenTexture->SRGB = true;
	ScreenTexture->LODGroup = TextureGroup::TEXTUREGROUP_UI;
	ScreenTexture->UpdateResource();
}

void UNesComponent::BeginPlay()
{
	Super::BeginPlay();
	
	// Determine audio parameters
	NesSettings.SampleRate = FAudioPlatformSettings::GetPlatformSettings(FPlatformProperties::GetRuntimeSettingsClassName()).SampleRate;
	NesSettings.SamplesPerFrame = FMath::RoundToInt32((double)NesSettings.SampleRate / (double)NesSettings.FramesPerSecond);

	// Create buffers
	// Video buffer is 32 bits per pixel
	VideoBuffer.SetNum(NesSettings.ScreenWidth * NesSettings.ScreenHeight * 4);
#if !NES_USE_AUDIO_QUEUE
	// Audio buffer is 2 bytes per sample
	AudioBuffer.SetNum(NesSettings.SamplesPerFrame * 2);
#endif

	CreateScreenTexture();
	NesSoundStream = NewObject<UNesSoundStream>();
}

void UNesComponent::EndPlay(const EEndPlayReason::Type EndPlayReason)
{
	Super::EndPlay(EndPlayReason);
	
	PowerOff();

	if (EmulationTickThread != nullptr)
	{
		delete EmulationTickThread;
		EmulationTickThread = nullptr;
	}
}

void UNesComponent::PowerOff()
{
	if (EmulationTickThread != nullptr)
	{
		EmulationTickThread->PowerOff();
	}

	if (bPlayWhiteNoiseWhenOff)
	{
		NesSoundStream->StreamWhiteNoise();
	}
}

void UNesComponent::PlayFromFile(FString FileName)
{
	NesSoundStream->SetSampleRate(NesSettings.SampleRate);
	NesSoundStream->StreamGameAudio(NesSettings.SamplesPerFrame);
	NesSoundStream->ResetAudio();

	if (EmulationTickThread == nullptr)
	{
		EmulationTickThread = new FEmulatorThreaded(this, NesSettings);
	}

	EmulationTickThread->PlayFromFile(FileName);
}

void UNesComponent::SetUpdateVideoOnRenderThread(bool bNewValue)
{
	bUpdateVideoOnRenderThread = bNewValue;

	UE_LOG(LogUEnesVideo, Log, TEXT("bUpdateVideoOnRenderThread: %d"), bUpdateVideoOnRenderThread);
}

void UNesComponent::SetPadButtonState(int PadNumber, PadButton Button, bool bPressed)
{
	// TODO Make thread safe instead of raw access
	if (EmulationTickThread != nullptr)
	{
		if (PadNumber >= 0 && PadNumber < 3)
		{
			if (bPressed)
			{
				EmulationTickThread->PadButtons[PadNumber] |= 1 << (int)Button;
			}
			else
			{
				EmulationTickThread->PadButtons[PadNumber] &= ~(1 << (int)Button);
			}
		}
	}
}

void UNesComponent::ResetAudioBuffer()
{
	NesSoundStream->ResetAudio();

}

void UNesComponent::PullZapperTrigger()
{
	if (EmulationTickThread)
	{
		EmulationTickThread->bFireZapper = true;
	}
}

void UNesComponent::ReleaseZapperTrigger(float X, float Y)
{
	if (EmulationTickThread)
	{
		EmulationTickThread->ZapperX = (int)X;
		EmulationTickThread->ZapperY = (int)Y;
		EmulationTickThread->bFireZapper = false;
	}
}

void UNesComponent::PostExecuteFrame(int32 AudioByteCount)
{
	
#if !NES_USE_AUDIO_QUEUE
	NesSoundStream->QueueAudio(AudioBuffer.GetData(), AudioByteCount);
#else
	// Experimental audio queue
	if (FrameNumber >= AudioFramesToBuffer)
	{
		int32 NumBytesToWrite = AudioBuffer.Count();
		TArray<uint8> Samples;
		
		int32 BytesWritten = NumBytesToWrite;
		for (int32 i = 0; i < NumBytesToWrite; i++)
		{
			uint8 SampleByte;
			if (AudioBuffer.Dequeue(SampleByte))
			{
				Samples.Add(SampleByte);
			}
			else
			{
				BytesWritten = i / 2 * 2;
				break;
			}
		}
		
		NesSoundStream->QueueAudio(Samples.GetData(), BytesWritten);
}
	
	
#endif

	if (bUpdateVideoOnRenderThread)
	{
		ENQUEUE_RENDER_COMMAND(NesScreenUpdate)([ScreenTexturePtr = ScreenTexture, ScreenWidth = NesSettings.ScreenWidth, ScreenHeight = NesSettings.ScreenHeight, VideoBufferCopy = VideoBuffer](FRHICommandListImmediate& RHICmdList)
			{
				FUpdateTextureRegion2D Region(0, 0, 0, 0, ScreenWidth, ScreenHeight);
				RHICmdList.UpdateTexture2D(ScreenTexturePtr->GetResource()->GetTexture2DRHI(), 0, Region, ScreenWidth * 4, VideoBufferCopy.GetData());
			});
	}
	else
	{
		// Draw directly on to the texture. Maniuplating the platfrom data is a bit hacky but should be fine since ScreenTexture is a transient texture and not actually being cooked
		void* Pixels = ScreenTexture->GetPlatformData()->Mips[0].BulkData.Lock(LOCK_READ_WRITE);
		FMemory::Memcpy(Pixels, VideoBuffer.GetData(), VideoBuffer.Num());
		ScreenTexture->GetPlatformData()->Mips[0].BulkData.Unlock();
		ScreenTexture->UpdateResource();
	}
}
