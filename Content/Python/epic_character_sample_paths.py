"""Locations and deliberately narrow source selection for Epic references."""
from pathlib import Path
import json

SOURCE_ROOTS = {
    "gasp": "D:/UnrealProjects/GameAnimationSample",
    "stack_o_bot": "D:/UnrealProjects/StackOBot",
    "content_examples": "D:/UnrealProjects/ContentExamples",
}
REPORT_ROOT = Path(__file__).resolve().parents[2] / "Saved/Validation/EpicSamples"
GENERATED_ROOT = "/Game/CityCourier/Animation/Epic"
GRAPH_VERSION = 11  # Must match UCourierAnimInstance::RequiredEpicGraphVersion.
TRACKED_REPORT_ROOT = Path(__file__).resolve().parents[2] / "docs/art/epic-character-samples"


def read_receipt(name, local_root=REPORT_ROOT, tracked_root=TRACKED_REPORT_ROOT):
    """Prefer local evidence, including rejecting malformed or mismatched local data.

    Consumers still verify asset hashes before accepting either receipt.
    """
    local = Path(local_root) / name
    path = local if local.exists() else Path(tracked_root) / name
    return json.loads(path.read_text(encoding="utf-8"))

REFERENCES = {
    "gasp": ["/Game/Blueprints/ABP_SandboxCharacter"],
    "stack_o_bot": [
        "/Game/StackOBot/Blueprints/Character/BP_Bot",
        "/Game/StackOBot/Characters/Bot/Rig/CR_Bot_Correction",
    ],
    "content_examples": [
        "/Game/ExampleContent/Physics/Blueprints/PhysicsAnimationBlendingHittReaction",
        "/Game/ExampleContent/Physics/Blueprints/PhysicsAnimationBlendingPhysicalAnimationComponent",
    ],
}


def package_file(content_root, package):
    """Only /Game packages can be copied into project Content."""
    if not package.startswith("/Game/") or ".." in package.split("/"):
        raise ValueError(f"Unsafe project package: {package}")
    return Path(content_root) / (package.removeprefix("/Game/") + ".uasset")
