// Shaderixel Link: editor module, same sources for Unreal Engine 4.25 to 5.8.
using UnrealBuildTool;

public class ShaderixelLink : ModuleRules
{
	public ShaderixelLink(ReadOnlyTargetRules Target) : base(Target)
	{
		PCHUsage = PCHUsageMode.UseExplicitOrSharedPCHs;
		PrivateDependencyModuleNames.AddRange(new string[]
		{
			"Core",
			"CoreUObject",
			"Engine",
			"UnrealEd",
			"Slate",
			"SlateCore",
			"InputCore",
			"ContentBrowser",
			"AssetTools",
			"AssetRegistry",
			"MaterialEditor",
			"ImageWrapper",
			"Json",
			"LevelEditor",
			"DesktopPlatform",
			"ToolMenus",
			"MeshDescription",
			"StaticMeshDescription",
		});
		if (Target.Version.MajorVersion >= 5 || Target.Version.MinorVersion >= 26)
		{
			PrivateDependencyModuleNames.Add("DeveloperSettings");  // UDeveloperSettings (in Engine before 4.26)
		}
		if (Target.Version.MajorVersion >= 5)
		{
			PrivateDependencyModuleNames.Add("ContentBrowserData");  // FContentBrowserItemPath (current folder)
		}
	}
}
