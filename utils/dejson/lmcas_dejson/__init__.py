# SPDX-FileCopyrightText: 2022-2024 Smart Information Flow Technologies
#
# SPDX-License-Identifier: Apache-2.0 OR MIT

import dataclasses
import json
from multimethod import DispatchError, overload
from pathlib import Path
from types import GenericAlias
import typing
from typing import Any, Callable, Optional, Type, TypeVar, TYPE_CHECKING, cast

if TYPE_CHECKING:
    from _typeshed import SupportsRead

"""
Deserialization of JSON-like objects (i.e. those returned by json.load) into
classes, using type annotations to actually produce instances of dataclasses,
validate types, etc.
"""

# TODO: mypy really doesn't like @multimethod.overload... Would it be
# reasonable to just give up on typechecking this package and provide stubs
# instead?


T = TypeVar("T")


def eq(x: type) -> Callable[[type], bool]:
    return lambda y: x == y


def find_from_json_func(cls: type, value: Any) -> Optional[Callable[[Type[T], Any], T]]:
    for sig, func in reversed(from_json.items()):
        param = sig.parameters["cls"]
        if param.annotation is param.empty:
            try:
                if isinstance(value, cls):
                    return func  # type: ignore
            except TypeError as exc:
                continue
        elif param.annotation(cls):
            return func  # type: ignore
    return None


def is_applied(origin: Any) -> Callable[[type], bool]:
    return lambda cls: isinstance(cls, GenericAlias) and cls.__origin__ is origin


def is_literal(cls: type) -> bool:
    return isinstance(cls, typing._LiteralGenericAlias)  # type:ignore


def is_optional(cls: type) -> bool:
    return (
        isinstance(cls, typing._UnionGenericAlias)  # type: ignore
        and cls.__origin__ is typing.Union
        and len(cls.__args__) == 2
        and cls.__args__[1] == type(None)
    )


def is_union(cls: type) -> bool:
    return (
        isinstance(cls, typing._UnionGenericAlias)  # type: ignore
        and cls.__origin__ is typing.Union
    )


@overload
def from_json(cls, value):  # type: ignore
    # Check that cls is valid.
    try:
        isinstance(value, cls)
    except TypeError as exc:
        raise TypeError(f"{cls} cannot be used with isinstance", exc)

    if isinstance(value, cls):
        return value
    else:
        raise TypeError(
            f"{value!r} is not a valid {cls} (do you need to make a from_json overload?)"
        )


@from_json.register
def from_json_dataclass(cls: dataclasses.is_dataclass, value):  # type: ignore
    assert isinstance(value, dict)
    fields = {}
    for field in dataclasses.fields(cls):
        name = field.name
        if name in value:
            fields[name] = from_json(field.type, value[name])
    return cls(**fields)  # type: ignore


@from_json.register
def from_json_dict(cls: is_applied(dict), value):  # type: ignore
    assert isinstance(value, dict)
    k_ty, v_ty = cls.__args__
    return {from_json(k_ty, k): from_json(v_ty, v) for k, v in value.items()}


@from_json.register
def from_json_list(cls: is_applied(list), value):  # type: ignore
    assert isinstance(value, list) or isinstance(value, tuple)
    return [from_json(cls.__args__[0], item) for item in value]


@from_json.register
def from_json_tuple(cls: is_applied(tuple), value):  # type: ignore
    assert isinstance(value, list) or isinstance(value, tuple)
    return tuple(from_json(cls.__args__[0], item) for item in value)


@from_json.register
def from_json_literal(cls: is_literal, value):  # type: ignore
    for arg in cls.__args__:  # type: ignore
        if value == arg:
            return arg
    raise TypeError(f"{value!r} is not one of {cls.__args__!r}")  # type: ignore


@from_json.register
def from_json_union(cls: is_union, value):  # type: ignore
    for cls in cls.__args__:  # type: ignore
        func = find_from_json_func(cls, value)
        if func is not None:
            return func(cls, value)
    raise DispatchError("No matching functions found")


@from_json.register
def from_json_optional(cls: is_optional, value):  # type: ignore
    if value is None:
        return None
    return from_json(cls.__args__[0], value)  # type: ignore


@from_json.register
def from_json_any(cls: eq(Any), value):  # type: ignore
    return value


@from_json.register
def from_json_path(cls: eq(Path), value):  # type: ignore
    assert isinstance(value, str)
    return Path(value)


def load_json(cls: Type[T], fp: "SupportsRead[str | bytes]") -> T:
    return cast(T, from_json(cls, json.load(fp)))
