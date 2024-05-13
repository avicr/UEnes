#pragma once

#include "UEnes.h"
#include "Components/ActorComponent.h"
#include "Engine/Texture2D.h"
#include "NesSoundStream.h"
#include "Containers/CircularQueue.h"
#include "NesComponent.generated.h"

USTRUCT(BlueprintType)
struct FCheatCode 
{
	GENERATED_BODY()
public:
	UPROPERTY(BlueprintReadWrite, EditAnywhere)
	int Address;

	UPROPERTY(BlueprintReadWrite, EditAnywhere)
	uint8 Value;
	
	UPROPERTY(BlueprintReadWrite, EditAnywhere)
	FString Description;
};


UENUM(BlueprintType)
enum class PadButton : uint8
{
	A,
	B,
	SELECT,
	START,
	UP,
	DOWN,
	LEFT,
	RIGHT
};

class FEmulatorThreaded;

UCLASS(BlueprintType, Config=Game, meta = (BlueprintSpawnableComponent))
class UENES_API UNesComponent : public UActorComponent
{
	GENERATED_BODY()

public:		
	virtual void BeginPlay() override;
	virtual void EndPlay(const EEndPlayReason::Type EndPlayReason) override;

	void FrameReadyCallback(const TArray<uint8>& VideoData, const TArray<uint8>& AudioData, int32 AudioByteCount);
	void PostExecuteFrame(int32 AudioByteCount);
		
protected:
	int FrameNumber = 0;
	FEmulatorThreaded* EmulationTickThread = nullptr;

	void CreateScreenTexture();

	UFUNCTION(BlueprintCallable)
	void PowerOff();

	UFUNCTION(BlueprintCallable)
	void PlayFromFile(FString FileName);

	UFUNCTION(BlueprintCallable)
	void PullZapperTrigger();

	UFUNCTION(BlueprintCallable)
	void ReleaseZapperTrigger(float X, float Y);

	// TODO Make a UTextureRenderTarget2D
	UPROPERTY(BlueprintReadOnly, Transient)
	UTexture2D* ScreenTexture;

	UPROPERTY(BlueprintReadOnly, Transient)
	UNesSoundStream* NesSoundStream;

	UPROPERTY(BlueprintReadOnly, EditAnywhere, Category = "Emulation")
	FNesSettings NesSettings;

	UPROPERTY(BlueprintReadWrite, EditAnywhere)
	bool bPlayWhiteNoiseWhenOff = false;

	/* If true the render target will be updated on the render thread.  
	 * Updating on the render thread may result in some input delay. While updating in the game thread feels better, it seems
	 * to cause the render thread to be flushed every time the emulator runs a cycle.
	 */
	UPROPERTY(BlueprintReadOnly, EditAnywhere, AdvancedDisplay)
	bool bUpdateVideoOnRenderThread = true;

	UFUNCTION(BlueprintCallable)
	void SetUpdateVideoOnRenderThread(bool bNewValue);
	
	UFUNCTION(BlueprintCallable)
	void SetPadButtonState(int PadNumber, PadButton Button, bool bPressed);

	// TODO Lock with critical section?
	uint32 PadButtons[4] = { 0, 0, 0, 0 };

	UFUNCTION(BlueprintCallable)
	void ResetAudioBuffer();

public:
	TArray<uint8> VideoBuffer;
#if	NES_USE_AUDIO_QUEUE
	TCircularQueue<uint8>AudioBuffer = TCircularQueue<uint8>(100600);
	int32 AudioFramesToBuffer = 4;
#else
	TArray<uint8> AudioBuffer;
#endif
	

};