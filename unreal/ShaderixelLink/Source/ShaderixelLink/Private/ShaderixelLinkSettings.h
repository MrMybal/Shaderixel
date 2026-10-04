// Project settings > Plugins > Shaderixel Link (stored per user).
#pragma once

#include "CoreMinimal.h"
#include "Engine/DeveloperSettings.h"
#include "ShaderixelLinkSettings.generated.h"

UCLASS(config = EditorPerProjectUserSettings, meta = (DisplayName = "Shaderixel Link"))
class UShaderixelLinkSettings : public UDeveloperSettings
{
	GENERATED_BODY()

public:
	/** Shaderixel.exe. Empty: the last Shaderixel started on this computer. */
	UPROPERTY(config, EditAnywhere, Category = "Shaderixel", meta = (FilePathFilter = "exe"))
	FFilePath ExecutablePath;

	/** Texture coordinates painted in Shaderixel (UV channel of the static mesh). */
	UPROPERTY(config, EditAnywhere, Category = "Shaderixel", meta = (ClampMin = "0", ClampMax = "7"))
	int32 UVChannel = 0;

	/** Resolution of new textures (meshes whose material has no texture yet). */
	UPROPERTY(config, EditAnywhere, Category = "Shaderixel", meta = (ClampMin = "256", ClampMax = "8192"))
	int32 NewTextureResolution = 2048;

	/** Folder of new textures and material instances, next to the mesh. */
	UPROPERTY(config, EditAnywhere, Category = "Shaderixel")
	FString NewAssetsFolder = TEXT("Shaderixel");

	/** New textures: create a material instance of the Shaderixel master material and assign it to the painted slot. */
	UPROPERTY(config, EditAnywhere, Category = "Shaderixel")
	bool bAssignMaterial = true;

	virtual FName GetCategoryName() const override { return TEXT("Plugins"); }
};
