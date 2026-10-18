"""Rebind the existing new kit without reimporting meshes or editing old art."""
import sys
from pathlib import Path
import unreal
sys.path.insert(0, str(Path(unreal.Paths.project_dir()).resolve()/'Content/Python'))
from import_enterable_assets import prepare_materials
prepare_materials()
