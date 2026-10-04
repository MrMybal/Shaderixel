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
	void OpenSessionsFolder();
	bool Tick(float DeltaTime);

	static FString LinkDir();

private:
	bool Launch(const FString& SessionFile);
	void ApplyResult(FShaderixelSession& S);
	UTexture* ImportOrReimport(const FShaderixelOutput& Out, bool& bCreated);
	UMaterialInterface* EnsureMaterialInstance(const FString& PackagePath, const FString& Name, const TMap<FString, UTexture*>& Textures);
	static void Notify(const FString& Message, bool bError = false);

	TArray<FShaderixelSession> Sessions;
};
