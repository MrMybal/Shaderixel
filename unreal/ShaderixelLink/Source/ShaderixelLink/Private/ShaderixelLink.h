// Shaderixel Link sessions: a static mesh material slot or a texture sent to Shaderixel,
// reimported when Shaderixel writes result.json (file exchange under %LOCALAPPDATA%\Shaderixel\Link).
#pragma once

#include "CoreMinimal.h"
#include "UObject/WeakObjectPtr.h"
#include "Runtime/Launch/Resources/Version.h"

class UStaticMesh;
class UTexture2D;
class UTexture;
class UMaterialInterface;

#define SX_UE5 (ENGINE_MAJOR_VERSION >= 5)

struct FShaderixelOutput
{
	FString Role;       // BaseColor, Normal, ORM, Roughness, Metallic, AO, Emissive, Height, Image
	FString File;       // written by Shaderixel
	FString AssetPath;  // texture object path (existing: reimported in place; missing: created)
	FString Compression;  // material description: Default, Normalmap, Masks, Grayscale ("" = from the role)
	int32 SRGB = -1;      // material description: 0 / 1 (-1 = from the role)
};

// Material of a model exported by Shaderixel (*.sxmat.json).
struct FShaderixelMaterialSettings
{
	FString Parent;                // material to instance ("" = the plugin's M_ShaderixelPBR2)
	bool bTwoSided = false;
	TMap<FString, float> Scalars;  // EmissiveStrength...
};

struct FShaderixelSession
{
	FString Id;
	FString Dir;
	FString Kind;  // "mesh" or "texture"
	FString Name;
	TWeakObjectPtr<UStaticMesh> Mesh;
	int32 Slot = 0;
	TWeakObjectPtr<UTexture2D> Texture;
	TArray<FShaderixelOutput> Outputs;
	bool bNewTextures = false;  // outputs are new assets: material instance on the slot
	int32 LastRevision = 0;
	FDateTime ResultStamp;
};

class FShaderixelLink
{
public:
	static FShaderixelLink& Get();

	void PaintMesh(UStaticMesh* Mesh, int32 Slot);
	void EditTexture(UTexture2D* Texture);
	// Textures exported by Shaderixel (manifest *.sxue.json): imported into a folder, with a material instance.
	void ImportShaderixelExport(const FString& Manifest, const FString& PackagePath);
	// Model + material described by Shaderixel (*.sxmat.json, or a .sx3d project converted by Shaderixel):
	// textures, material instance and the static mesh using it. PackagePath "" = the description's folder.
	UStaticMesh* ImportShaderixelModel(const FString& File, const FString& PackagePath);
	// Any file dialog entry: .sxmat.json / .sx3d (model) or .sxue.json (textures).
	void ImportAny(const FString& File, const FString& PackagePath);
	// A static mesh imported the usual way (FBX / OBJ...) with a <name>.sxmat.json next to its source file:
	// the textures and the material instance are created and applied.
	void ApplySidecarMaterial(UStaticMesh* Mesh);
	void OpenSessionsFolder();
	bool Tick(float DeltaTime);

	static FString LinkDir();

private:
	bool Launch(const FString& SessionFile);
	void ApplyResult(FShaderixelSession& S);
	UTexture* ImportOrReimport(const FShaderixelOutput& Out, bool& bCreated);
	UMaterialInterface* EnsureMaterialInstance(const FString& PackagePath, const FString& Name, const TMap<FString, UTexture*>& Textures,
		const FShaderixelMaterialSettings* Settings = nullptr);
	class UMaterial* EnsureMasterMaterial(const TMap<FString, UTexture*>& Textures);
	FString FindExecutable() const;
	// Material description: textures and material instance in PackagePath (0 textures: nullptr).
	UMaterialInterface* ImportMaterialDescription(const TSharedPtr<class FJsonObject>& Desc, const FString& Dir, const FString& PackagePath, int32& NumTextures);
	TSet<FString> SidecarBusy;
	static void Notify(const FString& Message, bool bError = false);

	TArray<FShaderixelSession> Sessions;
};
