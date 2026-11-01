"""Compare the public API of pycdfpp.cdflib.<module> with the real cdflib.<module>."""
import dataclasses
import importlib
import inspect

OWN_PREFIXES = ("cdflib", "pycdfpp.cdflib")


def module_pair(name):
    return importlib.import_module("pycdfpp.cdflib." + name), importlib.import_module("cdflib." + name)


def _is_public(name):
    return not name.startswith("_")


def _defined_by_cdflib(obj):
    owner = obj.__name__ if inspect.ismodule(obj) else getattr(obj, "__module__", None)
    return owner is None or owner.startswith(OWN_PREFIXES)


def _parameters(callable_):
    try:
        signature = inspect.signature(callable_)
    except (TypeError, ValueError):
        return None
    return [(p.name, p.kind.name, repr(p.default)) for p in signature.parameters.values()]


def _class_members(cls):
    own_classes = [c for c in cls.__mro__ if c.__module__.startswith(OWN_PREFIXES)]
    return {
        name: value
        for c in reversed(own_classes)
        for name, value in vars(c).items()
        if _is_public(name) or name == "__init__"
    }


def _describe_class(cls):
    members = {name: _describe(value) for name, value in _class_members(cls).items()}
    fields = [f.name for f in dataclasses.fields(cls)] if dataclasses.is_dataclass(cls) else []
    return ("class", members, fields)


def _describe(obj):
    if isinstance(obj, (staticmethod, classmethod)):
        return ("function", _parameters(obj.__func__))
    if isinstance(obj, property):
        return ("property",)
    if inspect.ismodule(obj):
        return ("module",)
    if inspect.isclass(obj):
        return _describe_class(obj)
    if callable(obj) and inspect.isroutine(obj):
        return ("function", _parameters(obj))
    return ("const", repr(obj))


def public_api(module):
    """Public names defined by cdflib (not re-imported third-party names), with signatures."""
    return {
        name: _describe(value)
        for name, value in vars(module).items()
        if _is_public(name) and _defined_by_cdflib(value)
    }


def instance_attributes(instance):
    return sorted(name for name in vars(instance) if _is_public(name))


def assert_same_api(testcase, ours, theirs):
    testcase.assertEqual(public_api(ours), public_api(theirs))


def assert_same_instance_attributes(testcase, ours, theirs):
    testcase.assertEqual(instance_attributes(ours), instance_attributes(theirs))
