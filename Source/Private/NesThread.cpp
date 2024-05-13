#include "NesThread.h"
#include "NesThread.h"
#include "UEnes.h"
#include "NesComponent.h"
#include "GenericPlatform/GenericPlatformProperties.h"
#include "EmuCore/NstBase.hpp"
#include "Async/TaskGraphInterfaces.h"

#include <fstream>

#if NES_SUPPORT_MULTIPLE_INSTANCES
FCriticalSection FEmulatorThreaded::CoreCriticalSection;
#endif

class FNesGraphTask
	: public FAsyncGraphTaskBase
{
public:

	/** Creates and initializes a new instance. */
	FNesGraphTask(ENamedThreads::Type InDesiredThread, TUniqueFunction<void()>&& InFunction)
		: DesiredThread(InDesiredThread)
		, Function(MoveTemp(InFunction))
	{ }

	/** Performs the actual task. */
	void DoTask(ENamedThreads::Type CurrentThread, const FGraphEventRef& MyCompletionGraphEvent)
	{
		Function();
	}

	/** Returns the name of the thread that this task should run on. */
	ENamedThreads::Type GetDesiredThread()
	{
		return DesiredThread;
	}

private:

	/** The thread to execute the function on. */
	ENamedThreads::Type DesiredThread;

	/** The function to execute on the Task Graph. */
	TUniqueFunction<void()> Function;
};

FEmulatorThreaded::FEmulatorThreaded(UNesComponent* InNesComponent, const FNesSettings& InSettings) :
	NesSettings(InSettings),
	NesComponent(InNesComponent)
{
	Thread = FRunnableThread::Create(this, *(FString(TEXT("EmulatorThreaded")) + InNesComponent->GetName()));

	VideoBuffer.SetNum(NesSettings.ScreenWidth * NesSettings.ScreenHeight * 4);

	// TODO Allocate slack for variable sized sample requests per frame
	AudioBuffer.SetNum(NesSettings.SamplesPerFrame * 2);
	
	// Set the emulator callbacks
	SetCallbacks();
}

FEmulatorThreaded::~FEmulatorThreaded()
{
	bShutdown = true;
	bIsRunning = false;
	if (Thread)
	{
		Thread->WaitForCompletion();
		delete Thread;
	}
}

bool FEmulatorThreaded::Init() 
{
	return true;
}

void FEmulatorThreaded::SetCallbacks()
{
	Nes::Video::Output::lockCallback.Set(&ScreenLock, this);
	Nes::Video::Output::unlockCallback.Set(&ScreenUnlock, this);

	Nes::Sound::Output::lockCallback.Set(&AudioLock, this);
	Nes::Sound::Output::unlockCallback.Set(&AudioUnlock, this);

	Nes::Machine::eventCallback.Set(&OnMachine, this);

	Nes::User::fileIoCallback.Set(&DoFileIO, this);
}

uint32 FEmulatorThreaded::Run()
{
	while (!bShutdown)
	{
		if (bIsRunning)
		{

#if NES_SYNC_THREADS
			// Wait for game thread to consume the last frame
			while (bIsRunning && !bShutdown && bDataPending)
			{
				FPlatformProcess::Sleep(0.f);
			}
#endif
			double NowTime = FDateTime::Now().ToUnixTimestampDecimal();
			double DeltaTime = NowTime - StartTime;

			if (DeltaTime >= FrameExecuteRate)
			{
#if NES_SUPPORT_MULTIPLE_INSTANCES
				FScopeLock EmulationLock(&CoreCriticalSection);
#endif
				// Run the NES core for one frame
				ExecuteFrame(true);
			
#if NES_SYNC_THREADS
				// Let the game thread know that there is a frame ready
				{
					FScopeLock DataPendingLock(&DataPendingWriteCriticalSection);
					bDataPending = true;
				}
#endif
				FrameNumber++;
				UE_LOG(LogUEnesTiming, VeryVerbose, TEXT("Delta: %0.4f"), DeltaTime);

				if (bIsRunning && !bShutdown && NesComponent != nullptr)
				{
					auto Function = [this, NumSamplesRequestedCopy = NumSamplesRequested, VideoCopy = VideoBuffer, AudioCopy = AudioBuffer]()
						{
							if (bIsRunning && NesComponent && !bShutdown)
							{
								NesComponent->FrameReadyCallback(VideoCopy, AudioCopy, NumSamplesRequestedCopy * 2);
							}
						};

					LastFrameCallbackTask = TGraphTask<FNesGraphTask>::CreateTask().ConstructAndDispatchWhenReady(ENamedThreads::GameThread, MoveTemp(Function));
				}

				StartTime = FDateTime::Now().ToUnixTimestampDecimal();

			}
		}
		else
		{
			Nes::Machine(*this).Power(false);
		}
		FPlatformProcess::Sleep(0.f);
	}
	
	if (LastFrameCallbackTask.IsValid())
	{
		LastFrameCallbackTask->Wait(ENamedThreads::GameThread);
	}
	UE_LOG(LogUEnesTiming, Verbose, TEXT("Thread loop exiting"));
	return 0;
}

void FEmulatorThreaded::Exit() 
{
	/* Post-Run code, threaded */
}

void FEmulatorThreaded::Stop()
{
	bShutdown = true;
	bIsRunning = false;
	UE_LOG(LogUEnesTiming, Verbose, TEXT("Thread Stop()"));
}

void FEmulatorThreaded::PowerOff()
{
	bIsRunning = false;
}

Nes::Result FEmulatorThreaded::PlayFromFile(FString FileName)
{
	bIsRunning = true;
	CurrentGamePath = FileName;
	FrameExecuteRate = 1.0 / FMath::Max(1.0, (double)NesSettings.FramesPerSecond);

	ifstream imageStream(TCHAR_TO_UTF8(*FileName), ios::binary);
	
	const Nes::Result result = Nes::Machine(*this).Load(imageStream, Nes::Machine::FAVORED_NES_NTSC, Nes::Machine::DONT_ASK_PROFILE);

	ensure(result == Nes::RESULT_OK);

	Nes::Machine(*this).SetMode(Nes::Api::Machine::NTSC);
	Nes::Machine(*this).Power(true);

	Nes::Video::RenderState NESRenderState;
	Nes::Video(*this).GetRenderState(NESRenderState);

	NESRenderState.bits.count = 32;
	NESRenderState.width = NesSettings.ScreenWidth;
	NESRenderState.height = NesSettings.ScreenHeight;
	NESRenderState.bits.mask.r = 0x000000ff;
	NESRenderState.bits.mask.g = 0x0000ff00;
	NESRenderState.bits.mask.b = 0x00ff0000;
	NESRenderState.filter = Nes::Video::RenderState::FILTER_NONE;

	Nes::Result VideoResult = Nes::Video(*this).SetRenderState(NESRenderState);
	UE_LOG(LogUEnesVideo, Log, TEXT("Video init result: %d"), VideoResult);
	
	Nes::Sound NesSound(*this);
	
	NesSound.SetSampleRate(NesSettings.SampleRate);
	NesSound.SetSpeaker(Nes::Api::Sound::Speaker::SPEAKER_MONO);
	NesSound.SetVolume(1, 100);

	NumSamplesRequested = NesSettings.SamplesPerFrame;
	
	Nes::Input::Controllers::Pad::callback.Set(&PollPad, this);
	Nes::Input::Controllers::Zapper::callback.Set(&PollZapper, this);

	Nes::Api::Input(*this).ConnectController(1, Nes::Api::Input::Type::ZAPPER);
	Nes::Api::Input(*this).ConnectController(0, Nes::Api::Input::Type::PAD1);
	
	return result;
}

Nes::Result FEmulatorThreaded::ExecuteFrame(bool bOutputVideo)
{
#if NES_SUPPORT_MULTIPLE_INSTANCES
	SetCallbacks();
#endif
	Nes::Result Result;
	NumSamplesRequested = FMath::Clamp(NumSamplesRequested, 0, NesSettings.SamplesPerFrame);
	
	if (bOutputVideo)
	{
		Result = Nes::Emulator::Execute(&VideoOutput, &SoundOutput, &Input);
	}
	else
	{
		Result = Nes::Emulator::Execute(NULL, &SoundOutput, &Input);
	}
	
	FrameNumber++;
	return Result;
}
