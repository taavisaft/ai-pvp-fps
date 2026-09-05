"""Compatibility entry point for the current Blender soldier/Uzi asset pipeline.
Run: blender --background --python tools/player_model.py
Editable output: tools/soldier/soldier_uzi.blend
The former position-only generator is retained in Git history.
"""
import os
import runpy
runpy.run_path(os.path.join(os.path.dirname(os.path.abspath(__file__)),
                           'soldier', 'build.py'), run_name='__main__')
