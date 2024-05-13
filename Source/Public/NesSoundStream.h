#pragma once

#include "AudioMixerTypes.h"
#include "Components/AudioComponent.h"
#include "CoreMinimal.h"
#include "Engine/EngineTypes.h"
#include "IAudioExtensionPlugin.h"
#include "Sound/SoundWaveProcedural.h"
#include "UObject/ObjectMacros.h"
#include "DSP/Noise.h"

#include "NesSoundStream.generated.h"

UCLASS(BlueprintType)
class UENES_API UNesSoundStream : public USoundWaveProcedural
{
	GENERATED_UCLASS_BODY()
	
protected:
	bool bGenerateWhiteNoise;
	Audio::FWhiteNoise WhiteNoise;

public:
	
	UFUNCTION()
	void StreamWhiteNoise();
	void StreamGameAudio(float SamplesPerFrame);
	
	UPROPERTY(EditAnywhere, BlueprintReadWrite)
	float NoiseVolume;

	/** Begin USoundWave */
	//virtual void OnBeginGenerate() override;
	virtual int32 OnGeneratePCMAudio(TArray<uint8>& OutAudio, int32 NumSamples) override;
	//virtual void OnEndGenerate() override;
	virtual Audio::EAudioMixerStreamDataFormat::Type GetGeneratedPCMDataFormat() const override 
	{ 
		return bGenerateWhiteNoise ? Audio::EAudioMixerStreamDataFormat::Float : Audio::EAudioMixerStreamDataFormat::Int16;		
	}
	/** End USoundWave */

};