#include "NesSoundStream.h"
#include "AudioDeviceManager.h"
#include "AudioDevice.h"

UNesSoundStream::UNesSoundStream(const FObjectInitializer& ObjectInitializer)
	: Super(ObjectInitializer)
{
	Duration = INDEFINITELY_LOOPING_DURATION;
	bLooping = true;

	VirtualizationMode = EVirtualizationMode::PlayWhenSilent;
	NumChannels = 1;
	SampleRate = 44100;
	SampleByteSize = 2;
	bGenerateWhiteNoise = true;
	NoiseVolume = 0.15;
}

int32 UNesSoundStream::OnGeneratePCMAudio(TArray<uint8>& OutAudio, int32 NumSamples)
{
	if (bGenerateWhiteNoise)
	{
		for (int i = 0; i < NumSamples; i++)
		{
			float Output = WhiteNoise.Generate(NoiseVolume, 0);
			
			uint8* ByteArray = reinterpret_cast<uint8*>(&Output);

			OutAudio.Append(ByteArray, 4);
			OutAudio.Append(ByteArray, 4);
		}
				
	}
	return 0;
}

void UNesSoundStream::StreamWhiteNoise()
{
	ResetAudio();
	bGenerateWhiteNoise = true;
}

void UNesSoundStream::StreamGameAudio(float SamplesPerFrame)
{
	ResetAudio();
	//NumSamplesToGeneratePerCallback = 48000;
	NumSamplesToGeneratePerCallback = SamplesPerFrame;
	bGenerateWhiteNoise = false;
}