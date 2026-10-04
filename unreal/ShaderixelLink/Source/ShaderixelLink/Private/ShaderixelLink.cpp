// Shaderixel Link: session export, Shaderixel launch, result polling and reimport.
#include "ShaderixelLink.h"
#include "ShaderixelLinkSettings.h"

#include "AssetToolsModule.h"
#if ENGINE_MAJOR_VERSION >= 5
#include "AssetRegistry/AssetRegistryModule.h"
#else
#include "AssetRegistryModule.h"
#endif
#include "Factories/TextureFactory.h"
#include "Dom/JsonObject.h"
#include "EditorReimportHandler.h"
#include "Engine/StaticMesh.h"
#include "Engine/Texture2D.h"
#include "Factories/MaterialFactoryNew.h"
#include "Factories/MaterialInstanceConstantFactoryNew.h"
#include "Framework/Application/SlateApplication.h"
#include "Framework/Notifications/NotificationManager.h"
#include "HAL/FileManager.h"
#include "HAL/PlatformProcess.h"
#include "IImageWrapper.h"
#include "IImageWrapperModule.h"
#include "MaterialEditingLibrary.h"
#include "Materials/Material.h"
#include "Materials/MaterialExpressionTextureSampleParameter2D.h"
#include "Materials/MaterialInstanceConstant.h"
#include "Misc/FileHelper.h"
#include "Misc/PackageName.h"
#include "Misc/Paths.h"
#include "Modules/ModuleManager.h"
#include "Serialization/JsonReader.h"
#include "Serialization/JsonSerializer.h"
#include "StaticMeshResources.h"
#include "Widgets/Notifications/SNotificationList.h"

#if PLATFORM_WINDOWS
#include "Windows/WindowsHWrapper.h"
#endif

#define LOCTEXT_NAMESPACE "ShaderixelLink"

namespace
{
const TCHAR* MasterMaterialPath = TEXT("/Game/Shaderixel/M_ShaderixelPBR.M_ShaderixelPBR");

FString Sanitize(const FString& In)
{
	FString Out;
	for (TCHAR C : In)
		Out.AppendChar(FChar::IsAlnum(C) || C == TEXT('_') || C == TEXT('-') ? C : TEXT('_'));
	return Out;
}

// Role of a texture from its parameter / asset name (and its compression).
FString RoleFromName(const FString& InName, const UTexture* Texture)
{
	const FString N = InName.ToLower();
	// Normal first ("normal" contains "orm"); ORM as a whole word only.
	if ((Texture && Texture->CompressionSettings == TC_Normalmap) || N.Contains(TEXT("normal")) || N.EndsWith(TEXT("_n")) || N.Contains(TEXT("nrm")))
		return TEXT("Normal");
	if (N == TEXT("orm") || N.EndsWith(TEXT("_orm")) || N.StartsWith(TEXT("orm_")) || N.Contains(TEXT("_orm_")) || N.Contains(TEXT("occlusionroughnessmetal")) ||
		N.EndsWith(TEXT("_arm")))
		return TEXT("ORM");
	if (N.Contains(TEXT("basecolor")) || N.Contains(TEXT("base_color")) || N.Contains(TEXT("albedo")) || N.Contains(TEXT("diffuse")) || N.EndsWith(TEXT("_bc")) ||
		N.EndsWith(TEXT("_d")) || N.EndsWith(TEXT("_col")) || N == TEXT("color") || N.EndsWith(TEXT("_color")))
		return TEXT("BaseColor");
	if (N.Contains(TEXT("rough")))
		return TEXT("Roughness");
	if (N.Contains(TEXT("metal")))
		return TEXT("Metallic");
	if (N.Contains(TEXT("occlusion")) || N.EndsWith(TEXT("_ao")) || N == TEXT("ao"))
		return TEXT("AO");
	if (N.Contains(TEXT("emiss")))
		return TEXT("Emissive");
	if (N.Contains(TEXT("height")) || N.Contains(TEXT("displace")))
		return TEXT("Height");
	return FString();
}

bool IsDataRole(const FString& Role)
{
	return Role == TEXT("ORM") || Role == TEXT("Roughness") || Role == TEXT("Metallic") || Role == TEXT("AO") || Role == TEXT("Height");
}

bool SaveJson(const TSharedRef<FJsonObject>& Obj, const FString& File)
{
	FString Text;
	TSharedRef<TJsonWriter<>> Writer = TJsonWriterFactory<>::Create(&Text);
	if (!FJsonSerializer::Serialize(Obj, Writer))
		return false;
	return FFileHelper::SaveStringToFile(Text, *File, FFileHelper::EEncodingOptions::ForceUTF8WithoutBOM);
}

TSharedPtr<FJsonObject> LoadJson(const FString& File)
{
	FString Text;
	if (!FFileHelper::LoadFileToString(Text, *File))
		return nullptr;
	TSharedPtr<FJsonObject> Obj;
	TSharedRef<TJsonReader<>> Reader = TJsonReaderFactory<>::Create(Text);
	if (!FJsonSerializer::Deserialize(Reader, Obj))
		return nullptr;
	return Obj;
}

// Mip 0 of the texture's source as PNG (8 bits BGRA, or 16 bits RGBA for 16-bit sources).
bool ExportTexturePng(UTexture2D* Texture, const FString& File, FString& Error)
{
	FTextureSource& Src = Texture->Source;
	if (!Src.IsValid())
	{
		Error = TEXT("pas de données source");
		return false;
	}
	const int32 W = Src.GetSizeX(), H = Src.GetSizeY();
	TArray64<uint8> Data;
	if (!Src.GetMipData(Data, 0))
	{
		Error = TEXT("lecture des pixels impossible");
		return false;
	}
	IImageWrapperModule& Wrappers = FModuleManager::LoadModuleChecked<IImageWrapperModule>(TEXT("ImageWrapper"));
	TSharedPtr<IImageWrapper> Png = Wrappers.CreateImageWrapper(EImageFormat::PNG);
	const int64 Pixels = (int64)W * H;
	bool bOk = false;
	switch (Src.GetFormat())
	{
	case TSF_BGRA8:
		bOk = Png->SetRaw(Data.GetData(), Data.Num(), W, H, ERGBFormat::BGRA, 8);
		break;
	case TSF_G8:
	{
		TArray64<uint8> Bgra;
		Bgra.SetNumUninitialized(Pixels * 4);
		for (int64 i = 0; i < Pixels; ++i)
		{
			Bgra[i * 4] = Bgra[i * 4 + 1] = Bgra[i * 4 + 2] = Data[i];
			Bgra[i * 4 + 3] = 255;
		}
		bOk = Png->SetRaw(Bgra.GetData(), Bgra.Num(), W, H, ERGBFormat::BGRA, 8);
		break;
	}
	case TSF_RGBA16:
		bOk = Png->SetRaw(Data.GetData(), Data.Num(), W, H, ERGBFormat::RGBA, 16);
		break;
	case TSF_G16:
	{
		TArray64<uint16> Rgba;
		Rgba.SetNumUninitialized(Pixels * 4);
		const uint16* G = (const uint16*)Data.GetData();
		for (int64 i = 0; i < Pixels; ++i)
		{
			Rgba[i * 4] = Rgba[i * 4 + 1] = Rgba[i * 4 + 2] = G[i];
			Rgba[i * 4 + 3] = 65535;
		}
		bOk = Png->SetRaw(Rgba.GetData(), Rgba.Num() * 2, W, H, ERGBFormat::RGBA, 16);
		break;
	}
	case TSF_RGBA16F:
	{
		// Float sources: values clamped to 0..1, written as 16-bit (the reimported texture stays 16 bits).
		TArray64<uint16> Rgba;
		Rgba.SetNumUninitialized(Pixels * 4);
		const FFloat16* F = (const FFloat16*)Data.GetData();
		for (int64 i = 0; i < Pixels * 4; ++i)
			Rgba[i] = (uint16)FMath::RoundToInt(FMath::Clamp(F[i].GetFloat(), 0.0f, 1.0f) * 65535.0f);
		bOk = Png->SetRaw(Rgba.GetData(), Rgba.Num() * 2, W, H, ERGBFormat::RGBA, 16);
		break;
	}
	default:
		Error = TEXT("format source non pris en charge (HDR / RGBE)");
		return false;
	}
	if (!bOk)
	{
		Error = TEXT("encodage PNG impossible");
		return false;
	}
	const TArray64<uint8> Bytes = Png->GetCompressed();
	return FFileHelper::SaveArrayToFile(Bytes, *File);
}

// OBJ of the triangles of one material slot (LOD 0), Y up (cyclic axis permutation: no mirroring), meters.
bool BuildMeshObj(UStaticMesh* Mesh, int32 Slot, int32 UVChannel, FString& Out, FString& Error)
{
	const FStaticMeshRenderData* RenderData = Mesh->GetRenderData();
	if (!RenderData || RenderData->LODResources.Num() == 0)
	{
		Error = TEXT("maillage sans données de rendu");
		return false;
	}
	const FStaticMeshLODResources& LOD = RenderData->LODResources[0];
	const uint32 NumVerts = LOD.VertexBuffers.PositionVertexBuffer.GetNumVertices();
	const uint32 NumUV = LOD.VertexBuffers.StaticMeshVertexBuffer.GetNumTexCoords();
	if (NumVerts == 0 || NumUV == 0)
	{
		Error = TEXT("maillage sans UV");
		return false;
	}
	const uint32 UV = (uint32)FMath::Clamp(UVChannel, 0, (int32)NumUV - 1);
	TArray<uint32> Indices;
	LOD.IndexBuffer.GetCopy(Indices);
	TArray<int32> Remap;
	Remap.Init(INDEX_NONE, NumVerts);
	TArray<uint32> Used;
	TArray<int32> Tris;
	for (const FStaticMeshSection& Section : LOD.Sections)
	{
		if (Section.MaterialIndex != Slot)
			continue;
		for (uint32 i = 0; i < Section.NumTriangles * 3; ++i)
		{
			const uint32 V = Indices[Section.FirstIndex + i];
			if (Remap[V] == INDEX_NONE)
			{
				Remap[V] = Used.Num();
				Used.Add(V);
			}
			Tris.Add(Remap[V]);
		}
	}
	if (Tris.Num() == 0)
	{
		Error = TEXT("aucun triangle pour cet emplacement de matériau");
		return false;
	}
	Out.Reset();
	Out.Reserve(Used.Num() * 110 + Tris.Num() * 12);
	Out += FString::Printf(TEXT("# Shaderixel Link: %s, slot %d, UV %u (Y up, meters)\no %s\n"), *Mesh->GetName(), Slot, UV, *Sanitize(Mesh->GetName()));
	for (uint32 V : Used)
	{
		const auto& P = LOD.VertexBuffers.PositionVertexBuffer.VertexPosition(V);
		Out += FString::Printf(TEXT("v %.6f %.6f %.6f\n"), P.Y * 0.01f, P.Z * 0.01f, P.X * 0.01f);
	}
	for (uint32 V : Used)
	{
		const auto N = LOD.VertexBuffers.StaticMeshVertexBuffer.VertexTangentZ(V);
		Out += FString::Printf(TEXT("vn %.5f %.5f %.5f\n"), (float)N.Y, (float)N.Z, (float)N.X);
	}
	for (uint32 V : Used)
	{
		const auto T = LOD.VertexBuffers.StaticMeshVertexBuffer.GetVertexUV(V, UV);
		Out += FString::Printf(TEXT("vt %.6f %.6f\n"), (float)T.X, 1.0f - (float)T.Y);
	}
	for (int32 i = 0; i + 2 < Tris.Num(); i += 3)
	{
		const int32 A = Tris[i] + 1, B = Tris[i + 1] + 1, C = Tris[i + 2] + 1;
		Out += FString::Printf(TEXT("f %d/%d/%d %d/%d/%d %d/%d/%d\n"), A, A, A, B, B, B, C, C, C);
	}
	return true;
}

TSharedRef<FJsonObject> OutputJson(const FString& Role, const FString& File, const FString& Asset)
{
	TSharedRef<FJsonObject> O = MakeShared<FJsonObject>();
	O->SetStringField(TEXT("role"), Role);
	O->SetStringField(TEXT("file"), File);
	O->SetStringField(TEXT("asset"), Asset);
	return O;
}
}  // namespace

FShaderixelLink& FShaderixelLink::Get()
{
	static FShaderixelLink Instance;
	return Instance;
}

FString FShaderixelLink::LinkDir()
{
	return FPaths::Combine(FPlatformMisc::GetEnvironmentVariable(TEXT("LOCALAPPDATA")), TEXT("Shaderixel"), TEXT("Link"));
}

void FShaderixelLink::Notify(const FString& Message, bool bError)
{
	if (!FSlateApplication::IsInitialized())  // commandlets (tests): the log only
	{
		UE_LOG(LogTemp, Display, TEXT("Shaderixel Link: %s"), *Message);
		return;
	}
	FNotificationInfo Info(FText::FromString(Message));
	Info.ExpireDuration = bError ? 8.0f : 4.0f;
	Info.bUseLargeFont = false;
	TSharedPtr<SNotificationItem> Item = FSlateNotificationManager::Get().AddNotification(Info);
	if (Item.IsValid())
		Item->SetCompletionState(bError ? SNotificationItem::CS_Fail : SNotificationItem::CS_Success);
	if (bError)
	{
		UE_LOG(LogTemp, Warning, TEXT("Shaderixel Link: %s"), *Message);
	}
	else
	{
		UE_LOG(LogTemp, Log, TEXT("Shaderixel Link: %s"), *Message);
	}
}

bool FShaderixelLink::Launch(const FString& SessionFile)
{
	// Automated tests: a separate, minimized Shaderixel with extra arguments (never the user's instance).
	const FString TestArgs = FPlatformMisc::GetEnvironmentVariable(TEXT("SHADERIXEL_LINK_TEST_ARGS"));
	// A running Shaderixel takes the session from its inbox.
	if (TSharedPtr<FJsonObject> Inst = TestArgs.IsEmpty() ? LoadJson(FPaths::Combine(LinkDir(), TEXT("instance.json"))) : nullptr)
	{
		const uint32 Pid = (uint32)Inst->GetNumberField(TEXT("pid"));
		if (Pid && FPlatformProcess::IsApplicationRunning(Pid))
		{
			TSharedRef<FJsonObject> Req = MakeShared<FJsonObject>();
			Req->SetStringField(TEXT("session"), SessionFile);
			const FString Inbox = FPaths::Combine(LinkDir(), TEXT("inbox"));
			IFileManager::Get().MakeDirectory(*Inbox, true);
			const FString Tmp = FPaths::Combine(Inbox, FString::Printf(TEXT("%lld.tmp"), FDateTime::UtcNow().GetTicks()));
			if (SaveJson(Req, Tmp) && IFileManager::Get().Move(*FPaths::ChangeExtension(Tmp, TEXT("json")), *Tmp))
			{
#if PLATFORM_WINDOWS
				::AllowSetForegroundWindow((DWORD)Pid);
#endif
				return true;
			}
		}
	}
	FString Exe = FPlatformMisc::GetEnvironmentVariable(TEXT("SHADERIXEL_LINK_EXE"));
	if (Exe.IsEmpty())
		Exe = GetDefault<UShaderixelLinkSettings>()->ExecutablePath.FilePath;
	if (Exe.IsEmpty())
	{
		FFileHelper::LoadFileToString(Exe, *FPaths::Combine(LinkDir(), TEXT("last_exe.txt")));
		Exe.TrimStartAndEndInline();
	}
	if (Exe.IsEmpty() || !FPaths::FileExists(Exe))
	{
		Notify(TEXT("Shaderixel introuvable : lancez Shaderixel une fois, ou réglez son chemin dans Project Settings > Plugins > Shaderixel Link."), true);
		return false;
	}
	const FString Args = FString::Printf(TEXT("--link \"%s\" %s"), *SessionFile, *TestArgs);
	FProcHandle Proc = FPlatformProcess::CreateProc(*Exe, *Args, true, !TestArgs.IsEmpty(), false, nullptr, 0, *FPaths::GetPath(Exe), nullptr);
	if (!Proc.IsValid())
	{
		Notify(TEXT("Impossible de lancer Shaderixel : ") + Exe, true);
		return false;
	}
	FPlatformProcess::CloseProc(Proc);
	return true;
}

void FShaderixelLink::PaintMesh(UStaticMesh* Mesh, int32 Slot)
{
	if (!Mesh)
		return;
	const UShaderixelLinkSettings* Settings = GetDefault<UShaderixelLinkSettings>();
	const TArray<FStaticMaterial>& Materials = Mesh->GetStaticMaterials();
	Slot = FMath::Clamp(Slot, 0, FMath::Max(Materials.Num() - 1, 0));
	FString Name = Mesh->GetName();
	if (Materials.Num() > 1)
		Name += TEXT("_") + (Materials[Slot].MaterialSlotName.IsNone() ? FString::FromInt(Slot) : Materials[Slot].MaterialSlotName.ToString());
	Name = Sanitize(Name);
	const uint32 Hash = FCrc::StrCrc32(*(Mesh->GetPathName() + TEXT("#") + FString::FromInt(Slot)));
	FShaderixelSession S;
	S.Id = FString::Printf(TEXT("%s_%08x"), *Name, Hash);
	S.Dir = FPaths::Combine(LinkDir(), TEXT("sessions"), S.Id);
	S.Kind = TEXT("mesh");
	S.Name = Name;
	S.Mesh = Mesh;
	S.Slot = Slot;
	IFileManager& FM = IFileManager::Get();
	FM.MakeDirectory(*FPaths::Combine(S.Dir, TEXT("in")), true);
	FM.MakeDirectory(*FPaths::Combine(S.Dir, TEXT("out")), true);

	// Geometry of the slot.
	FString Obj, Error;
	if (!BuildMeshObj(Mesh, Slot, Settings->UVChannel, Obj, Error))
	{
		Notify(FString::Printf(TEXT("%s : %s"), *Mesh->GetName(), *Error), true);
		return;
	}
	FFileHelper::SaveStringToFile(Obj, *FPaths::Combine(S.Dir, TEXT("mesh.obj")));
	// The Shaderixel project of an earlier session is reused while the geometry is the same.
	const FString MeshHash = FString::Printf(TEXT("%08x"), FCrc::StrCrc32(*Obj));
	FString OldHash;
	FFileHelper::LoadFileToString(OldHash, *FPaths::Combine(S.Dir, TEXT("mesh.hash")));
	const FString Project = FPaths::Combine(S.Dir, TEXT("project.sx3d"));
	const bool bReuse = FPaths::FileExists(Project) && OldHash == MeshHash;
	if (!bReuse)
		FM.Delete(*Project, false, true, true);
	FFileHelper::SaveStringToFile(MeshHash, *FPaths::Combine(S.Dir, TEXT("mesh.hash")));

	// The slot material's textures (parameters): painted in place.
	TArray<TSharedPtr<FJsonValue>> Inputs, Outputs;
	int32 Resolution = 0;
	if (UMaterialInterface* Material = Mesh->GetMaterial(Slot))
	{
		TArray<FMaterialParameterInfo> Infos;
		TArray<FGuid> Ids;
		Material->GetAllTextureParameterInfo(Infos, Ids);
		TSet<FString> Roles;
		for (const FMaterialParameterInfo& Info : Infos)
		{
			UTexture* Value = nullptr;
			if (!Material->GetTextureParameterValue(Info, Value) || !Value)
				continue;
			UTexture2D* Tex = Cast<UTexture2D>(Value);
			if (!Tex || Tex->GetPathName().StartsWith(TEXT("/Engine/")))
				continue;
			FString Role = RoleFromName(Info.Name.ToString(), Tex);
			if (Role.IsEmpty())
				Role = RoleFromName(Tex->GetName(), Tex);
			if (Role.IsEmpty() || Roles.Contains(Role))
				continue;
			const FString InFile = FPaths::Combine(S.Dir, TEXT("in"), Role + TEXT(".png"));
			if (!bReuse && !ExportTexturePng(Tex, InFile, Error))
			{
				Notify(FString::Printf(TEXT("%s : %s"), *Tex->GetName(), *Error), true);
				continue;
			}
			Roles.Add(Role);
			Resolution = FMath::Max(Resolution, (int32)FMath::Max(Tex->Source.GetSizeX(), Tex->Source.GetSizeY()));
			TSharedRef<FJsonObject> In = OutputJson(Role, TEXT("in/") + Role + TEXT(".png"), Tex->GetPathName());
			Inputs.Add(MakeShared<FJsonValueObject>(In));
			const FString OutFile = FPaths::Combine(S.Dir, TEXT("out"), Tex->GetName() + TEXT(".png"));
			Outputs.Add(MakeShared<FJsonValueObject>(OutputJson(Role, OutFile, Tex->GetPathName())));
			S.Outputs.Add({Role, OutFile, Tex->GetPathName()});
		}
	}
	if (S.Outputs.Num() == 0)
	{
		// No texture yet: new BaseColor / Normal / ORM next to the mesh (and a material instance).
		S.bNewTextures = true;
		const FString Folder = FPackageName::GetLongPackagePath(Mesh->GetOutermost()->GetName()) / Settings->NewAssetsFolder;
		const TCHAR* Roles[][2] = {{TEXT("BaseColor"), TEXT("BC")}, {TEXT("Normal"), TEXT("N")}, {TEXT("ORM"), TEXT("ORM")}};
		for (const auto& R : Roles)
		{
			const FString AssetName = FString::Printf(TEXT("T_%s_%s"), *Name, R[1]);
			const FString Asset = Folder / AssetName + TEXT(".") + AssetName;
			const FString OutFile = FPaths::Combine(S.Dir, TEXT("out"), AssetName + TEXT(".png"));
			Outputs.Add(MakeShared<FJsonValueObject>(OutputJson(R[0], OutFile, Asset)));
			S.Outputs.Add({R[0], OutFile, Asset});
		}
	}

	TSharedRef<FJsonObject> J = MakeShared<FJsonObject>();
	J->SetStringField(TEXT("format"), TEXT("shaderixel-link"));
	J->SetNumberField(TEXT("version"), 1);
	J->SetStringField(TEXT("id"), S.Id);
	J->SetStringField(TEXT("kind"), S.Kind);
	J->SetStringField(TEXT("host"), FString::Printf(TEXT("Unreal Engine %d.%d"), ENGINE_MAJOR_VERSION, ENGINE_MINOR_VERSION));
	J->SetStringField(TEXT("asset"), Mesh->GetPathName());
	J->SetStringField(TEXT("name"), Name);
	J->SetStringField(TEXT("mesh"), TEXT("mesh.obj"));
	J->SetNumberField(TEXT("slot"), Slot);
	J->SetNumberField(TEXT("resolution"), Resolution > 0 ? Resolution : Settings->NewTextureResolution);
	if (bReuse)
		J->SetStringField(TEXT("project"), TEXT("project.sx3d"));
	J->SetArrayField(TEXT("textures"), Inputs);
	J->SetArrayField(TEXT("outputs"), Outputs);
	const FString SessionFile = FPaths::Combine(S.Dir, TEXT("session.json"));
	SaveJson(J, SessionFile);

	S.ResultStamp = FM.GetTimeStamp(*FPaths::Combine(S.Dir, TEXT("result.json")));
	Sessions.RemoveAll([&](const FShaderixelSession& O) { return O.Id == S.Id; });
	Sessions.Add(S);
	if (Launch(SessionFile))
		Notify(FString::Printf(TEXT("%s ouvert dans Shaderixel%s"), *Name, S.bNewTextures ? TEXT(" (nouvelles textures)") : TEXT("")));
}

void FShaderixelLink::EditTexture(UTexture2D* Texture)
{
	if (!Texture)
		return;
	FShaderixelSession S;
	S.Name = Sanitize(Texture->GetName());
	S.Id = FString::Printf(TEXT("%s_%08x"), *S.Name, FCrc::StrCrc32(*Texture->GetPathName()));
	S.Dir = FPaths::Combine(LinkDir(), TEXT("sessions"), S.Id);
	S.Kind = TEXT("texture");
	S.Texture = Texture;
	IFileManager& FM = IFileManager::Get();
	FM.MakeDirectory(*FPaths::Combine(S.Dir, TEXT("in")), true);
	FM.MakeDirectory(*FPaths::Combine(S.Dir, TEXT("out")), true);
	FString Error;
	const FString InFile = FPaths::Combine(S.Dir, TEXT("in"), S.Name + TEXT(".png"));
	if (!ExportTexturePng(Texture, InFile, Error))
	{
		Notify(FString::Printf(TEXT("%s : %s"), *Texture->GetName(), *Error), true);
		return;
	}
	const FString OutFile = FPaths::Combine(S.Dir, TEXT("out"), S.Name + TEXT(".png"));
	S.Outputs.Add({TEXT("Image"), OutFile, Texture->GetPathName()});
	TSharedRef<FJsonObject> J = MakeShared<FJsonObject>();
	J->SetStringField(TEXT("format"), TEXT("shaderixel-link"));
	J->SetNumberField(TEXT("version"), 1);
	J->SetStringField(TEXT("id"), S.Id);
	J->SetStringField(TEXT("kind"), S.Kind);
	J->SetStringField(TEXT("host"), FString::Printf(TEXT("Unreal Engine %d.%d"), ENGINE_MAJOR_VERSION, ENGINE_MINOR_VERSION));
	J->SetStringField(TEXT("asset"), Texture->GetPathName());
	J->SetStringField(TEXT("name"), S.Name);
	J->SetStringField(TEXT("image"), TEXT("in/") + S.Name + TEXT(".png"));
	J->SetStringField(TEXT("output"), OutFile);
	J->SetBoolField(TEXT("srgb"), Texture->SRGB);
	// The Shaderixel document of an earlier session keeps its layers (until the texture is replaced from elsewhere).
	if (FPaths::FileExists(FPaths::Combine(S.Dir, TEXT("image.sxl"))))
		J->SetStringField(TEXT("project"), TEXT("image.sxl"));
	const FString SessionFile = FPaths::Combine(S.Dir, TEXT("session.json"));
	SaveJson(J, SessionFile);
	S.ResultStamp = FM.GetTimeStamp(*FPaths::Combine(S.Dir, TEXT("result.json")));
	Sessions.RemoveAll([&](const FShaderixelSession& O) { return O.Id == S.Id; });
	Sessions.Add(S);
	if (Launch(SessionFile))
		Notify(FString::Printf(TEXT("%s ouverte dans Shaderixel"), *Texture->GetName()));
}

bool FShaderixelLink::Tick(float)
{
	IFileManager& FM = IFileManager::Get();
	for (FShaderixelSession& S : Sessions)
	{
		const FDateTime Stamp = FM.GetTimeStamp(*FPaths::Combine(S.Dir, TEXT("result.json")));
		if (Stamp == FDateTime::MinValue() || Stamp <= S.ResultStamp)
			continue;
		S.ResultStamp = Stamp;
		ApplyResult(S);
	}
	return true;
}

UTexture* FShaderixelLink::ImportOrReimport(const FShaderixelOutput& Out, bool& bCreated)
{
	bCreated = false;
	if (UTexture* Existing = LoadObject<UTexture>(nullptr, *Out.AssetPath, nullptr, LOAD_NoWarn | LOAD_Quiet))
	{
		// Same asset, new pixels (its settings stay: compression, sRGB, LOD group...).
		if (FReimportManager::Instance()->Reimport(Existing, false, false, Out.File, nullptr, INDEX_NONE, false, true))
			return Existing;
		return nullptr;
	}
	const FString PackageName = FPackageName::ObjectPathToPackageName(Out.AssetPath);
	const FString AssetName = FPackageName::GetShortName(PackageName);
	// The texture factory directly (same path on every engine version, no import pipeline UI).
	UPackage* Package = CreatePackage(*PackageName);
	if (!Package)
		return nullptr;
	UTextureFactory* Factory = NewObject<UTextureFactory>();
	Factory->AddToRoot();
	UTextureFactory::SuppressImportOverwriteDialog();
	bool bCancelled = false;
	UObject* Created = Factory->FactoryCreateFile(UTexture2D::StaticClass(), Package, FName(*AssetName), RF_Public | RF_Standalone | RF_Transactional, Out.File,
		nullptr, GWarn, bCancelled);
	Factory->RemoveFromRoot();
	UTexture* Tex = Cast<UTexture>(Created);
	if (!Tex)
		return nullptr;
	FAssetRegistryModule::AssetCreated(Tex);
	Package->MarkPackageDirty();
	Tex->Modify();
	if (Out.Role == TEXT("Normal"))
	{
		Tex->SRGB = false;
		Tex->CompressionSettings = TC_Normalmap;
	}
	else if (Out.Role == TEXT("ORM"))
	{
		Tex->SRGB = false;
		Tex->CompressionSettings = TC_Masks;
	}
	else if (IsDataRole(Out.Role))
	{
		Tex->SRGB = false;
		Tex->CompressionSettings = TC_Grayscale;
	}
	Tex->PostEditChange();
	Tex->MarkPackageDirty();
	bCreated = true;
	return Tex;
}

UMaterialInterface* FShaderixelLink::EnsureMaterialInstance(const FString& PackagePath, const FString& Name, const TMap<FString, UTexture*>& Textures)
{
	IAssetTools& Tools = FModuleManager::LoadModuleChecked<FAssetToolsModule>(TEXT("AssetTools")).Get();
	UMaterial* Master = LoadObject<UMaterial>(nullptr, MasterMaterialPath, nullptr, LOAD_NoWarn | LOAD_Quiet);
	if (!Master)
	{
		// Master material: BaseColor, Normal and ORM (R occlusion, G roughness, B metallic) parameters.
		UMaterialFactoryNew* Factory = NewObject<UMaterialFactoryNew>();
		Master = Cast<UMaterial>(Tools.CreateAsset(TEXT("M_ShaderixelPBR"), TEXT("/Game/Shaderixel"), UMaterial::StaticClass(), Factory));
		if (!Master)
			return nullptr;
		auto Param = [&](const TCHAR* ParamName, int32 Y, EMaterialSamplerType Sampler) -> UMaterialExpressionTextureSampleParameter2D*
		{
			UTexture* const* Tex = Textures.Find(ParamName);
			if (!Tex || !*Tex)
				return nullptr;
			auto* E = Cast<UMaterialExpressionTextureSampleParameter2D>(
				UMaterialEditingLibrary::CreateMaterialExpression(Master, UMaterialExpressionTextureSampleParameter2D::StaticClass(), -420, Y));
			if (!E)
				return nullptr;
			E->ParameterName = ParamName;
			E->Texture = *Tex;
			E->SamplerType = Sampler;
			return E;
		};
		if (auto* E = Param(TEXT("BaseColor"), -200, SAMPLERTYPE_Color))
			UMaterialEditingLibrary::ConnectMaterialProperty(E, TEXT("RGB"), MP_BaseColor);
		if (auto* E = Param(TEXT("Normal"), 100, SAMPLERTYPE_Normal))
			UMaterialEditingLibrary::ConnectMaterialProperty(E, TEXT("RGB"), MP_Normal);
		if (auto* E = Param(TEXT("ORM"), 400, SAMPLERTYPE_Masks))
		{
			UMaterialEditingLibrary::ConnectMaterialProperty(E, TEXT("R"), MP_AmbientOcclusion);
			UMaterialEditingLibrary::ConnectMaterialProperty(E, TEXT("G"), MP_Roughness);
			UMaterialEditingLibrary::ConnectMaterialProperty(E, TEXT("B"), MP_Metallic);
		}
		UMaterialEditingLibrary::RecompileMaterial(Master);
		Master->MarkPackageDirty();
	}
	const FString Path = PackagePath / Name + TEXT(".") + Name;
	UMaterialInstanceConstant* Instance = LoadObject<UMaterialInstanceConstant>(nullptr, *Path, nullptr, LOAD_NoWarn | LOAD_Quiet);
	if (!Instance)
	{
		UMaterialInstanceConstantFactoryNew* Factory = NewObject<UMaterialInstanceConstantFactoryNew>();
		Factory->InitialParent = Master;
		Instance = Cast<UMaterialInstanceConstant>(Tools.CreateAsset(Name, PackagePath, UMaterialInstanceConstant::StaticClass(), Factory));
		if (!Instance)
			return nullptr;
	}
	for (const auto& Pair : Textures)
		if (Pair.Value)
			UMaterialEditingLibrary::SetMaterialInstanceTextureParameterValue(Instance, FName(*Pair.Key), Pair.Value);
	UMaterialEditingLibrary::UpdateMaterialInstance(Instance);
	Instance->MarkPackageDirty();
	return Instance;
}

void FShaderixelLink::ApplyResult(FShaderixelSession& S)
{
	TSharedPtr<FJsonObject> R = LoadJson(FPaths::Combine(S.Dir, TEXT("result.json")));
	if (!R.IsValid() || R->GetStringField(TEXT("format")) != TEXT("shaderixel-link-result"))
		return;
	const int32 Revision = (int32)R->GetNumberField(TEXT("revision"));
	S.LastRevision = Revision;
	TMap<FString, UTexture*> ByRole;
	int32 Updated = 0, Created = 0, Failed = 0;
	const TArray<TSharedPtr<FJsonValue>>* Outs = nullptr;
	if (R->TryGetArrayField(TEXT("outputs"), Outs))
		for (const TSharedPtr<FJsonValue>& V : *Outs)
		{
			const TSharedPtr<FJsonObject> O = V->AsObject();
			if (!O.IsValid())
				continue;
			FShaderixelOutput Out{O->GetStringField(TEXT("role")), O->GetStringField(TEXT("file")), O->GetStringField(TEXT("asset"))};
			if (Out.AssetPath.IsEmpty() || !FPaths::FileExists(Out.File))
				continue;
			bool bCreated = false;
			if (UTexture* T = ImportOrReimport(Out, bCreated))
			{
				ByRole.Add(Out.Role, T);
				(bCreated ? Created : Updated)++;
			}
			else
				++Failed;
		}
	UStaticMesh* Mesh = S.Mesh.Get();
	if (S.Kind == TEXT("mesh") && S.bNewTextures && Mesh && GetDefault<UShaderixelLinkSettings>()->bAssignMaterial && ByRole.Num())
	{
		const FString Folder = FPackageName::GetLongPackagePath(Mesh->GetOutermost()->GetName()) / GetDefault<UShaderixelLinkSettings>()->NewAssetsFolder;
		if (UMaterialInterface* Instance = EnsureMaterialInstance(Folder, TEXT("MI_") + S.Name, ByRole))
			if (Mesh->GetMaterial(S.Slot) != Instance)
			{
				Mesh->Modify();
				Mesh->SetMaterial(S.Slot, Instance);
				Mesh->PostEditChange();
				Mesh->MarkPackageDirty();
			}
	}
	FString Msg = FString::Printf(TEXT("Shaderixel : %s, révision %d (%d mise%s à jour, %d créée%s)"), *S.Name, Revision, Updated, Updated > 1 ? TEXT("s") : TEXT(""),
								  Created, Created > 1 ? TEXT("s") : TEXT(""));
	if (Failed)
		Msg += FString::Printf(TEXT(", %d échec%s"), Failed, Failed > 1 ? TEXT("s") : TEXT(""));
	Notify(Msg, Failed > 0);
}

void FShaderixelLink::ImportShaderixelExport(const FString& Manifest, const FString& PackagePath)
{
	TSharedPtr<FJsonObject> M = LoadJson(Manifest);
	if (!M.IsValid() || M->GetStringField(TEXT("format")) != TEXT("shaderixel-textures"))
	{
		Notify(TEXT("Fichier d'export Shaderixel illisible : ") + Manifest, true);
		return;
	}
	const FString Name = Sanitize(M->GetStringField(TEXT("name")));
	const FString Dir = FPaths::GetPath(Manifest);
	TMap<FString, UTexture*> ByRole;
	const TArray<TSharedPtr<FJsonValue>>* Texs = nullptr;
	if (M->TryGetArrayField(TEXT("textures"), Texs))
		for (const TSharedPtr<FJsonValue>& V : *Texs)
		{
			const TSharedPtr<FJsonObject> O = V->AsObject();
			if (!O.IsValid())
				continue;
			FShaderixelOutput Out;
			Out.Role = O->GetStringField(TEXT("role"));
			Out.File = FPaths::Combine(Dir, O->GetStringField(TEXT("file")));
			const FString AssetName = TEXT("T_") + Name + TEXT("_") + Out.Role;
			Out.AssetPath = PackagePath / AssetName + TEXT(".") + AssetName;
			bool bCreated = false;
			if (UTexture* T = ImportOrReimport(Out, bCreated))
				ByRole.Add(Out.Role, T);
		}
	if (ByRole.Num() && EnsureMaterialInstance(PackagePath, TEXT("MI_") + Name, ByRole))
		Notify(FString::Printf(TEXT("Shaderixel : %d textures et MI_%s importés dans %s"), ByRole.Num(), *Name, *PackagePath));
	else
		Notify(TEXT("Aucune texture importée depuis ") + Manifest, true);
}

void FShaderixelLink::OpenSessionsFolder()
{
	const FString Dir = FPaths::Combine(LinkDir(), TEXT("sessions"));
	IFileManager::Get().MakeDirectory(*Dir, true);
	FPlatformProcess::ExploreFolder(*Dir);
}

#undef LOCTEXT_NAMESPACE
