// Fill out your copyright notice in the Description page of Project Settings.

using UnrealBuildTool;

using System;
using System.IO;

public class FractalTerrainV2 : ModuleRules
{
	public FractalTerrainV2(ReadOnlyTargetRules Target) : base(Target)
	{
		PCHUsage = PCHUsageMode.UseExplicitOrSharedPCHs;

        PublicDependencyModuleNames.AddRange(new string[] { "Core", "CoreUObject", "Engine", "InputCore", "ProceduralMeshComponent" });

        PrivateDependencyModuleNames.AddRange(new string[] {  });

        // Uncomment if you are using Slate UI
        // PrivateDependencyModuleNames.AddRange(new string[] { "Slate", "SlateCore" });

        // Uncomment if you are using online features
        // PrivateDependencyModuleNames.Add("OnlineSubsystem");

        //*
        // Not sure if needed
        Definitions.Add("_CRT_SECURE_NO_WARNINGS=1");
        Definitions.Add("BOOST_DISABLE_ABI_HEADERS=1");

        // Needed configurations in order to run Boost
        bUseRTTI = true;
        bEnableExceptions = true;

        // To include OnlineSubsystemSteam, add it to the plugins section in your uproject file with the Enabled attribute set to true
        LoadLib("boost", new string[] { "boost_system-vc142-mt-x64-1_70", "boost_iostreams-vc142-mt-x64-1_70", "boost_thread-vc142-mt-x64-1_70" });
        LoadLib("OpenEXR", new string[] { "Half-2_3", "Iex-2_3", "IexMath-2_3", "IlmImf-2_3", "IlmImfUtil-2_3", "IlmThread-2_3", "Imath-2_3" });
        LoadLib("tbb", new string[] { "tbb" });
        LoadLib("OpenVDB", new string[] { "openvdb" });
        LoadLib("libnoise", new string[] { "noise", "noiseutils-static" });
        //*/
    }

    private void LoadLib(string name, string[] libs)
    {
        PublicIncludePaths.Add(Path.Combine(ModuleDirectory, @"../../ThirdParty/" + name + @"/include"));
        PublicLibraryPaths.Add(Path.Combine(ModuleDirectory, @"../../ThirdParty/" + name + @"/lib"));
        foreach (string lib in libs)
        {
            PublicAdditionalLibraries.Add(lib + ".lib");
        }
    }
}
