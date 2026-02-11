from ci.actions.base.action import BaseAction


def get_actions() -> dict[str, BaseAction]:
    """Dynamically load all actions that inherit from BaseAction."""
    import inspect
    import pkgutil
    from pathlib import Path
    from importlib import import_module
    actions = {}
    current_package = __name__
    package_dir = Path(__file__).resolve().parent

    for _, module_name, _ in pkgutil.iter_modules([str(package_dir)]):
        module = import_module(f"{current_package}.{module_name}")
        for name, obj in inspect.getmembers(module, inspect.isclass):
            if issubclass(obj, BaseAction) and obj is not BaseAction:
                actions[name] = obj()
    return actions
