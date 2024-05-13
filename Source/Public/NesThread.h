#pragma once

#include "EmuCore/api/NstApiEmulator.hpp"
#include "EmuCore/api/NstApiVideo.hpp"
#include "EmuCore/api/NstApiSound.hpp"
#include "EmuCore/api/NstApiInput.hpp"
#include "EmuCore/api/NstApiMachine.hpp"
#include "EmuCore/api/NstApiUser.hpp"
#include "EmuCore/api/NstApiCheats.hpp"

#include "UEnes.h"
#include "Misc/ScopeLock.h"

#include <fstream>

using namespace std;

namespace Nes
{
	using namespace Api;
}

class UNesComponent;

class FEmulatorThreaded : public FRunnable, public Nes::Emulator
{
public:
	FEmulatorThreaded(UNesComponent* InNesComponent, const FNesSettings& InSettings);
	~FEmulatorThreaded();

	// FRunnable
	virtual bool Init() override;
	virtual uint32 Run() override;
	virtual void Exit() override;
	virtual void Stop() override;

	void PowerOff();
protected:

#if NES_SUPPORT_MULTIPLE_INSTANCES
	static FCriticalSection CoreCriticalSection;
#endif
	// Reference to the most recent task graph entry for the nes game object callback
	// TODO This should probably be an array if NES_SYNC_THREADS is 0
	FGraphEventRef LastFrameCallbackTask;
	FRunnableThread* Thread;

	FNesSettings NesSettings;

	// The NesComponent that spawned this thread
	UNesComponent* NesComponent = nullptr;

	// If true the thread will shutdown
	bool bShutdown = false;

	// If true the NES emulator is running
	bool bIsRunning = false;

	FString CurrentGamePath = "";

	// Number of audio samples to request from the emulator
	int32 NumSamplesRequested = 0;
	
	double StartTime = 0;
	int32 FrameNumber = 0;
	long double FrameExecuteRate = 1.0 / 60.0;

	// Begin emulator
	void SetCallbacks();

	static void NST_CALLBACK DoFileIO(Nes::User::UserData data, Nes::User::File& context)
	{
		if (FEmulatorThreaded* NesThread = (FEmulatorThreaded*)data)
		{
			if (NesThread->NesSettings.bSaveBatteryBackup)
			{
				ifstream imageStream;
				ofstream imageOutStream;

				FString	SavePath = NesThread->CurrentGamePath.Replace(TEXT(".nes"), TEXT(".sav"));
				Nes::Api::User::File::Action Action = context.GetAction();

				switch (Action)
				{

				case Nes::User::File::LOAD_BATTERY:
					imageStream.open(TCHAR_TO_UTF8(*SavePath), ios::binary);
					context.SetContent(imageStream);
					break;

				case Nes::User::File::SAVE_BATTERY:
					imageOutStream.open(TCHAR_TO_UTF8(*SavePath), ios::binary);
					context.GetContent(imageOutStream);
					break;
				}
			}
		}
	}
	static void NST_CALLBACK OnMachine(Nes::User::UserData data, Nes::Machine::Event event, Nes::Result result)
	{
		if (FEmulatorThreaded* NesThread = (FEmulatorThreaded*)data)
		{
			if (event == Nes::Machine::EVENT_POWER_ON || event == Nes::Machine::EVENT_LOAD)
			{
				if (event == Nes::Machine::EVENT_POWER_ON)
				{
					NesThread->bIsRunning = true;
					NesThread->StartTime = FDateTime::Now().ToUnixTimestampDecimal();
				}
			}
		}
	}

	static bool NST_CALLBACK PollZapper(Nes::Input::UserData data, Nes::Input::Controllers::Zapper& zapper)
	{
		if (FEmulatorThreaded* NesThread = (FEmulatorThreaded*)data)
		{

			zapper.fire = NesThread->bFireZapper;

			if (false)
			{
				zapper.fire = true;
				zapper.x = ~0U;
				zapper.y = ~0U;
			}
			else if (zapper.x != ~0U)
			{
				zapper.x = NesThread->ZapperX;
				zapper.y = NesThread->ZapperY;
			}
			else if (zapper.fire)
			{
				zapper.x = ~1U;
			}
		}
		return true;
	}

	static bool NST_CALLBACK PollPad(Nes::Input::UserData data, Nes::Input::Controllers::Pad& pad, unsigned int index)
	{
		if (FEmulatorThreaded* NesThread = (FEmulatorThreaded*)data)
		{
			//FScopeLock PadsAccessLock(&NesThread->PadsAccessCriticalSection);
			pad.buttons = NesThread->PadButtons[index];
		}
		return true;
	}

	static bool NST_CALLBACK AudioLock(Nes::Sound::UserData data, Nes::Sound::Output& output)
	{
		if (FEmulatorThreaded * NesThread = (FEmulatorThreaded*)data)
		{
			ensureMsgf(&NesThread->SoundOutput == &output, TEXT("NES thread did not match audio output buffer. This is most likely caused by having two NesComponents in the same level, please enable NES_SUPPORT_MULTIPLE_INSTANCES"));

			output.samples[0] = NesThread->AudioBuffer.GetData();
			output.length[0] = NesThread->NumSamplesRequested;

			output.samples[1] = NULL;
			output.length[1] = 0;
		}
		return true;
	}

	static void NST_CALLBACK AudioUnlock(Nes::Sound::UserData data, Nes::Sound::Output& Output)
	{
	}

	static bool NST_CALLBACK ScreenLock(Nes::Video::UserData data, Nes::Video::Output& output)
	{
		if (FEmulatorThreaded* NesThread = (FEmulatorThreaded*)data)
		{
			ensureMsgf(&NesThread->VideoOutput == &output, TEXT("NES thread did not match audio output buffer. This is most likely caused by having two NesComponents in the same level, please enable NES_SUPPORT_MULTIPLE_INSTANCES"));

			output.pixels = NesThread->VideoBuffer.GetData();
			output.pitch = NesThread->NesSettings.ScreenWidth * 4;
		}
		return true;
	}

	static void NST_CALLBACK ScreenUnlock(Nes::Video::UserData data, Nes::Video::Output&)
	{
	}

protected:
	
	TArray<uint8> VideoBuffer;
	TArray<uint8> AudioBuffer;
	
	Nes::Video::Output VideoOutput;
	Nes::Sound::Output SoundOutput;
	Nes::Input::Controllers Input;

public:

#if NES_SYNC_THREADS
	FCriticalSection DataPendingWriteCriticalSection;
	bool bDataPending = false;
#endif
	
	// TODO Make thread safe?
	uint32 PadButtons[4] = { 0, 0, 0, 0 };
	unsigned int ZapperX = 0;
	unsigned int ZapperY = 0;
	bool bFireZapper = false;

	Nes::Result PlayFromFile(FString FileName);
	Nes::Result ExecuteFrame(bool bOutputVideo);
};
