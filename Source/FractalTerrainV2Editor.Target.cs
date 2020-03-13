// Fill out your copyright notice in the Description page of Project Settings.

using UnrealBuildTool;
using System.Collections.Generic;

public class FractalTerrainV2EditorTarget : TargetRules
{
	public FractalTerrainV2EditorTarget(TargetInfo Target) : base(Target)
	{
		Type = TargetType.Editor;

		ExtraModuleNames.AddRange( new string[] { "FractalTerrainV2" } );
	}
}
