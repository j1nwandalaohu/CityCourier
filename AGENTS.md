# Repository Guidelines

## Project Structure & Module Organization

CityCourier is a Windows single-player delivery game built with Unreal Engine 5.6.1.

- `Source/CityCourier/`: C++ gameplay module, organized into `Player`, `Delivery`, `Vehicles`, `Buildings`, `Ambient`, `Profile`, `UI`, and `World`.
- `Source/CityCourier/Tests/`: Unreal automation tests and shared fixtures; `Validation/` contains packaged-game checks.
- `Content/CityCourier/`: runtime maps, Blueprints, input mappings, and imported art. `Content/Python/` contains editor generation, import, and capture scripts.
- `AssetsSource/CityCourier/` and `tools/art/`: editable art, exports, provenance manifests, and Blender authoring scripts.
- `Config/`, `scripts/`, and `docs/`: engine settings, PowerShell workflows, and design/validation records.

Generated `Binaries/`, `Intermediate/`, `Saved/`, `DerivedDataCache/`, and `Artifacts/` are ignored.

## Build, Test, and Development Commands

Run PowerShell commands from the repository root. Install UE 5.6 and the Windows C++ toolchain; configure your engine location:

```powershell
$env:CITYCOURIER_UE_ROOT = 'D:/sdk/UE_5.6'
./scripts/Build.ps1 -Target Editor
./scripts/Build.ps1 -Target Game
./scripts/Test.ps1 -Filter CityCourier.CharacterActions
./scripts/Test.ps1 -Filter CityCourier
./scripts/Package.ps1 -Configuration Shipping
```

These build Development editor/game targets, run focused/full automation suites, and compile/cook/archive Shipping. Open `CityCourier.uproject` for editor work. Run `Artifacts/Windows/Shipping/CityCourier.exe` after packaging; retain its entire directory. Build before running tests. Run UE builds, editor scripts, tests, and packaging sequentially.

## Coding Style & Naming Conventions

Use four-space indentation and match neighboring code. Follow Unreal prefixes: `A` actors, `U` objects, `F` structs, `E` enums, and `b` booleans. Match class/header filenames and keep features in their subsystem directories. Assets use patterns such as `BP_`, `ABP_`, `IA_`, `IMC_`, `A_`, and `L_`; Python scripts use snake_case. Respect `.gitattributes` line endings. No repository-wide formatter or linter configuration is present.

## Testing Guidelines

Use Unreal Automation (`IMPLEMENT_SIMPLE_AUTOMATION_TEST`) with hierarchical names such as `CityCourier.CharacterActions.Posture.RenderedCrawl`. Reuse existing fixtures and test observable gameplay, collisions, and rendered poses. No numeric coverage threshold is configured. Run affected suites; retain reports under `Saved/Validation/`. Include PIE screenshots/state traces for animation changes and use isolated `-UserDir` saves for packaged validation.

## Commit & Pull Request Guidelines

History uses concise imperative subjects with `feat:`, `fix:`, and scoped forms such as `feat(art):`. Stage related files explicitly. Include required imported assets, editable sources, generators, and provenance; exclude generated packages and unrelated work. PRs should describe behavior, link relevant issues/design notes, list validation commands/results, and attach screenshots for visual changes. Record external art licensing in `docs/art/asset-manifest.csv`.
