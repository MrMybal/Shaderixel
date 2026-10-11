// Shaderixel Link module: Content Browser and Tools menu entries, result polling.
#include "ShaderixelLink.h"

#include "ContentBrowserModule.h"
#include "Containers/Ticker.h"
#include "DesktopPlatformModule.h"
#include "Editor.h"
#include "Engine/Selection.h"
#include "Engine/StaticMesh.h"
#include "Engine/StaticMeshActor.h"
#include "Components/StaticMeshComponent.h"
#include "Engine/Texture2D.h"
#include "Framework/Application/SlateApplication.h"
#include "Framework/MultiBox/MultiBoxBuilder.h"
#include "IContentBrowserSingleton.h"
#include "Modules/ModuleManager.h"
#include "HAL/IConsoleManager.h"
#include "ToolMenus.h"
#include "Subsystems/ImportSubsystem.h"
#include "Factories/Factory.h"

#define LOCTEXT_NAMESPACE "ShaderixelLink"

namespace
{
#if ENGINE_MAJOR_VERSION > 5 || (ENGINE_MAJOR_VERSION == 5 && ENGINE_MINOR_VERSION >= 1)
FName AssetClassName(const FAssetData& A) { return A.AssetClassPath.GetAssetName(); }
#else
FName AssetClassName(const FAssetData& A) { return A.AssetClass; }
#endif

void PaintMenu(FMenuBuilder& Menu, UStaticMesh* Mesh)
{
	const TArray<FStaticMaterial>& Materials = SX_STATIC_MATERIALS(Mesh);
	for (int32 i = 0; i < Materials.Num(); ++i)
	{
		const FString Label = FString::Printf(TEXT("%d : %s"), i, *(Materials[i].MaterialSlotName.IsNone() ? FString(TEXT("Slot")) : Materials[i].MaterialSlotName.ToString()));
		TWeakObjectPtr<UStaticMesh> Weak = Mesh;
		Menu.AddMenuEntry(FText::FromString(Label), FText::FromString(TEXT("Peindre cet emplacement de matériau (ses textures) dans Shaderixel")), FSlateIcon(),
			FUIAction(FExecuteAction::CreateLambda([Weak, i]() {
				if (UStaticMesh* M = Weak.Get())
					FShaderixelLink::Get().PaintMesh(M, i);
			})));
	}
}

TSharedRef<FExtender> AssetMenuExtender(const TArray<FAssetData>& Assets)
{
	TSharedRef<FExtender> Extender = MakeShared<FExtender>();
	TArray<FAssetData> Meshes, Textures;
	for (const FAssetData& A : Assets)
	{
		const FName Class = AssetClassName(A);
		if (Class == TEXT("StaticMesh"))
			Meshes.Add(A);
		else if (Class == TEXT("Texture2D"))
			Textures.Add(A);
	}
	if (Meshes.Num() == 0 && Textures.Num() == 0)
		return Extender;
	Extender->AddMenuExtension(TEXT("GetAssetActions"), EExtensionHook::After, nullptr,
		FMenuExtensionDelegate::CreateLambda([Meshes, Textures](FMenuBuilder& Menu) {
			Menu.BeginSection(TEXT("Shaderixel"), LOCTEXT("Section", "Shaderixel"));
			if (Meshes.Num() == 1)
			{
				FAssetData MeshAsset = Meshes[0];
				Menu.AddSubMenu(LOCTEXT("Paint", "Peindre dans Shaderixel"),
					LOCTEXT("PaintTip", "Ouvre le maillage dans un projet Paint 3D de Shaderixel avec les textures de son matériau ; Ctrl+S dans Shaderixel les réimporte ici."),
					FNewMenuDelegate::CreateLambda([MeshAsset](FMenuBuilder& Sub) {
						if (UStaticMesh* Mesh = Cast<UStaticMesh>(MeshAsset.GetAsset()))
							PaintMenu(Sub, Mesh);
					}));
			}
			for (const FAssetData& T : Textures)
			{
				FAssetData TexAsset = T;
				Menu.AddMenuEntry(FText::Format(LOCTEXT("EditTex", "Éditer {0} dans Shaderixel"), FText::FromName(T.AssetName)),
					LOCTEXT("EditTexTip", "Ouvre la texture dans Shaderixel ; Ctrl+S dans Shaderixel la réimporte (mêmes réglages)."), FSlateIcon(),
					FUIAction(FExecuteAction::CreateLambda([TexAsset]() {
						if (UTexture2D* Tex = Cast<UTexture2D>(TexAsset.GetAsset()))
							FShaderixelLink::Get().EditTexture(Tex);
					})));
			}
			Menu.EndSection();
		}));
	return Extender;
}

void PaintSelectedActor()
{
	if (!GEditor)
		return;
	for (FSelectionIterator It(GEditor->GetSelectedActorIterator()); It; ++It)
		if (AActor* Actor = Cast<AActor>(*It))
		{
			TArray<UStaticMeshComponent*> Components;
			Actor->GetComponents<UStaticMeshComponent>(Components);
			for (UStaticMeshComponent* C : Components)
				if (C && C->GetStaticMesh())
				{
					FShaderixelLink::Get().PaintMesh(C->GetStaticMesh(), 0);
					return;
				}
		}
}

void ImportExport()
{
	IDesktopPlatform* Platform = FDesktopPlatformModule::Get();
	if (!Platform)
		return;
	TArray<FString> Files;
	const void* Parent = FSlateApplication::Get().FindBestParentWindowHandleForDialogs(nullptr);
	if (!Platform->OpenFileDialog(Parent, TEXT("Modèle ou textures Shaderixel"), FString(), FString(),
			TEXT("Shaderixel (*.sx3d;*.sxmat.json;*.sxue.json)|*.sx3d;*.sxmat.json;*.sxue.json|Projet Paint 3D (*.sx3d)|*.sx3d|Modèle + matériau (*.sxmat.json)|*.sxmat.json|Textures (*.sxue.json)|*.sxue.json"),
			EFileDialogFlags::None, Files) ||
		Files.Num() == 0)
		return;
	FContentBrowserModule& CB = FModuleManager::LoadModuleChecked<FContentBrowserModule>(TEXT("ContentBrowser"));
	FString Path = CB.Get().GetCurrentPath()
#if ENGINE_MAJOR_VERSION >= 5
		.GetInternalPathString()
#endif
		;
	if (Path.IsEmpty() || !Path.StartsWith(TEXT("/")))
		Path = TEXT("/Game/Shaderixel");
	FShaderixelLink::Get().ImportAny(Files[0], Path);
}
}  // namespace

class FShaderixelLinkModule : public IModuleInterface
{
public:
	virtual void StartupModule() override
	{
		FContentBrowserModule& CB = FModuleManager::LoadModuleChecked<FContentBrowserModule>(TEXT("ContentBrowser"));
		CB.GetAllAssetViewContextMenuExtenders().Add(FContentBrowserMenuExtender_SelectedAssets::CreateStatic(&AssetMenuExtender));
		ExtenderHandle = CB.GetAllAssetViewContextMenuExtenders().Last().GetHandle();
		UToolMenus::RegisterStartupCallback(FSimpleMulticastDelegate::FDelegate::CreateRaw(this, &FShaderixelLinkModule::RegisterMenus));
		// A model imported the usual way with a <name>.sxmat.json next to it gets its Shaderixel material.
		if (GEditor)
			if (UImportSubsystem* Import = GEditor->GetEditorSubsystem<UImportSubsystem>())
				PostImportHandle = Import->OnAssetPostImport.AddLambda([](UFactory*, UObject* Created) {
					if (UStaticMesh* Mesh = Cast<UStaticMesh>(Created))
						FShaderixelLink::Get().ApplySidecarMaterial(Mesh);
				});
#if SX_UE5
		TickHandle = FTSTicker::GetCoreTicker().AddTicker(FTickerDelegate::CreateLambda([](float Dt) { return FShaderixelLink::Get().Tick(Dt); }), 0.5f);
#else
		TickHandle = FTicker::GetCoreTicker().AddTicker(FTickerDelegate::CreateLambda([](float Dt) { return FShaderixelLink::Get().Tick(Dt); }), 0.5f);
#endif
	}

	virtual void ShutdownModule() override
	{
#if SX_UE5
		FTSTicker::GetCoreTicker().RemoveTicker(TickHandle);
#else
		FTicker::GetCoreTicker().RemoveTicker(TickHandle);
#endif
		if (FContentBrowserModule* CB = FModuleManager::GetModulePtr<FContentBrowserModule>(TEXT("ContentBrowser")))
			CB->GetAllAssetViewContextMenuExtenders().RemoveAll([this](const FContentBrowserMenuExtender_SelectedAssets& D) { return D.GetHandle() == ExtenderHandle; });
		UToolMenus::UnRegisterStartupCallback(this);
		UToolMenus::UnregisterOwner(this);
		if (GEditor)
			if (UImportSubsystem* Import = GEditor->GetEditorSubsystem<UImportSubsystem>())
				Import->OnAssetPostImport.Remove(PostImportHandle);
	}

private:
	void RegisterMenus()
	{
		FToolMenuOwnerScoped Owner(this);
		UToolMenu* Tools = UToolMenus::Get()->ExtendMenu(TEXT("LevelEditor.MainMenu.Tools"));
		FToolMenuSection& Section = Tools->AddSection(TEXT("Shaderixel"), LOCTEXT("ToolsSection", "Shaderixel"));
		Section.AddMenuEntry(TEXT("ShaderixelPaintActor"), LOCTEXT("PaintActor", "Peindre l'acteur sélectionné dans Shaderixel"),
			LOCTEXT("PaintActorTip", "Maillage statique de l'acteur sélectionné, premier emplacement de matériau."), FSlateIcon(),
			FUIAction(FExecuteAction::CreateStatic(&PaintSelectedActor)));
		Section.AddMenuEntry(TEXT("ShaderixelImport"), LOCTEXT("Import", "Importer un modèle Shaderixel..."),
			LOCTEXT("ImportTip", "Projet Paint 3D (*.sx3d) ou modèle exporté (*.sxmat.json) : maillage statique, textures et instance de matériau\nimportés dans le dossier courant du Content Browser. Aussi les textures seules (*.sxue.json)."),
			FSlateIcon(), FUIAction(FExecuteAction::CreateStatic(&ImportExport)));
		Section.AddMenuEntry(TEXT("ShaderixelSessions"), LOCTEXT("Sessions", "Dossier des sessions Shaderixel"), LOCTEXT("SessionsTip", "Projets et textures échangés avec Shaderixel."),
			FSlateIcon(), FUIAction(FExecuteAction::CreateLambda([]() { FShaderixelLink::Get().OpenSessionsFolder(); })));
	}

	FDelegateHandle ExtenderHandle;
	FDelegateHandle PostImportHandle;
#if SX_UE5
	FTSTicker::FDelegateHandle TickHandle;
#else
	FDelegateHandle TickHandle;
#endif
};

IMPLEMENT_MODULE(FShaderixelLinkModule, ShaderixelLink)

// Console commands (scripts and automated tests).
static FAutoConsoleCommand GShaderixelPaint(TEXT("Shaderixel.Paint"), TEXT("Shaderixel.Paint <static mesh path> [slot]: paint the mesh slot in Shaderixel"),
	FConsoleCommandWithArgsDelegate::CreateLambda([](const TArray<FString>& Args) {
		if (Args.Num() == 0)
			return;
		if (UStaticMesh* Mesh = LoadObject<UStaticMesh>(nullptr, *Args[0]))
			FShaderixelLink::Get().PaintMesh(Mesh, Args.Num() > 1 ? FCString::Atoi(*Args[1]) : 0);
		else
			UE_LOG(LogTemp, Warning, TEXT("Shaderixel.Paint: %s not found"), *Args[0]);
	}));
static FAutoConsoleCommand GShaderixelEditTexture(TEXT("Shaderixel.EditTexture"), TEXT("Shaderixel.EditTexture <texture path>: edit the texture in Shaderixel"),
	FConsoleCommandWithArgsDelegate::CreateLambda([](const TArray<FString>& Args) {
		if (Args.Num() == 0)
			return;
		if (UTexture2D* Tex = LoadObject<UTexture2D>(nullptr, *Args[0]))
			FShaderixelLink::Get().EditTexture(Tex);
		else
			UE_LOG(LogTemp, Warning, TEXT("Shaderixel.EditTexture: %s not found"), *Args[0]);
	}));
static FAutoConsoleCommand GShaderixelImport(TEXT("Shaderixel.Import"),
	TEXT("Shaderixel.Import <file .sx3d / .sxmat.json / .sxue.json> [content folder]: import a Shaderixel model (mesh, textures, material)"),
	FConsoleCommandWithArgsDelegate::CreateLambda([](const TArray<FString>& Args) {
		if (Args.Num() == 0)
			return;
		FString File = Args[0].TrimQuotes();
		FShaderixelLink::Get().ImportAny(File, Args.Num() > 1 ? Args[1] : FString());
	}));
static FAutoConsoleCommand GShaderixelPoll(TEXT("Shaderixel.Poll"), TEXT("Shaderixel.Poll: apply the results Shaderixel wrote (also done every half second)"),
	FConsoleCommandDelegate::CreateLambda([]() { FShaderixelLink::Get().Tick(0.0f); }));

#undef LOCTEXT_NAMESPACE
