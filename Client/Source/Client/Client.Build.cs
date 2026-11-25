// Copyright Epic Games, Inc. All Rights Reserved.

using UnrealBuildTool;

public class Client : ModuleRules
{
	public Client(ReadOnlyTargetRules Target) : base(Target)
	{
		PCHUsage = PCHUsageMode.UseExplicitOrSharedPCHs;

		// Dimension/·Combat/·Character/ 하위 헤더를 "Dimension/X.h" 형태로 포함하기 위해 모듈 루트를 include 경로에 추가한다.
		PublicIncludePaths.Add(ModuleDirectory);

		// GameplayAbilitySystem: 체력 속성(AttributeSet), 피해·쿨다운(GameplayEffect), 사격·재장전(GameplayAbility).
		// 공개 헤더(Combat/CombatAttributeSet.h, Character/CombatCharacter.h)가 GAS 헤더를 포함하므로 Public 에 둔다.
		PublicDependencyModuleNames.AddRange(new string[] { "Core", "CoreUObject", "Engine", "InputCore", "EnhancedInput", "GameplayAbilities", "GameplayTasks", "GameplayTags" });

		PrivateDependencyModuleNames.AddRange(new string[] { "UMG", "Slate", "SlateCore" });

		// Uncomment if you are using Slate UI
		// PrivateDependencyModuleNames.AddRange(new string[] { "Slate", "SlateCore" });
		
		// Uncomment if you are using online features
		// PrivateDependencyModuleNames.Add("OnlineSubsystem");

		// To include OnlineSubsystemSteam, add it to the plugins section in your uproject file with the Enabled attribute set to true
	}
}
