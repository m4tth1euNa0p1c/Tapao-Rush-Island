"""Retire only this task's older runpy traversal callbacks after PIE is stopped."""
import gc
from pathlib import Path
import types
import unreal as ue


def cleanup():
    assert not ue.get_editor_subsystem(ue.LevelEditorSubsystem).is_in_play_in_editor()
    target=Path(__file__).with_name('fort_showcase_validate.py').resolve()
    handles=[]
    for obj in gc.get_objects():
        if not isinstance(obj,types.FunctionType) or obj.__name__!='tick':continue
        if Path(obj.__code__.co_filename).resolve()!=target:continue
        namespace=obj.__globals__
        state=namespace.get('STATE',{})
        if state.get('phase')=='finished':continue
        handle=namespace.get('HANDLE')
        if handle is not None and handle not in handles:
            ue.unregister_slate_post_tick_callback(handle);handles.append(handle)
        state['phase']='finished'
    return len(handles)
