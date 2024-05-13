// Copyright Epic Games, Inc. All Rights Reserved.

using EpicGames.Core;
using System.IO;
using UnrealBuildTool;
using UnrealBuildTool.Rules;

public class UEnes : ModuleRules
{
	public UEnes(ReadOnlyTargetRules Target) : base(Target)
	{
		PCHUsage = ModuleRules.PCHUsageMode.UseExplicitOrSharedPCHs;
		
		PublicIncludePaths.AddRange(
			new string[] {
				// ... add public include paths required here ...
			}
			);
				
		
		PrivateIncludePaths.AddRange(
			new string[] {
				// ... add other private include paths required here ...
			}
			);
			
		
		PublicDependencyModuleNames.AddRange(
			new string[]
			{
				"Core",
				"Engine",
				"SignalProcessing",
				"AudioExtensions",
				"AudioMixerCore",
				"RenderCore",
				"RHI"
				
				// ... add other public dependencies that you statically link with here ...
			}
			);
			
		
		PrivateDependencyModuleNames.AddRange(
			new string[]
			{
				"CoreUObject",
				"Engine",
				"Slate",
				"SlateCore"
				// ... add private dependencies that you statically link with here ...	
			}
			);
		
		
		DynamicallyLoadedModuleNames.AddRange(
			new string[]
			{
				// ... add any modules that your module loads dynamically here ...
			}
			);
		string PlatformDir = Target.Platform.ToString();

		AddEngineThirdPartyPrivateStaticDependencies(Target, "zlib");
		PublicAdditionalLibraries.Add(Path.Combine(ModuleDirectory, "ThirdParty", PlatformDir, "lib/emucore.lib"));
		PublicAdditionalLibraries.Add(Path.Combine(ModuleDirectory, "ThirdParty", PlatformDir, "lib/zlibstat.lib"));
	}
}
