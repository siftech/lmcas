# SPDX-FileCopyrightText: 2022-2024 Smart Information Flow Technologies
#
# SPDX-License-Identifier: Apache-2.0 OR MIT

import functools
from typing import TypeVar, Optional, Callable

T = TypeVar("T")


def unwrap_or(x: Optional[T], if_none: T) -> T:
    if x is None:
        return if_none
    else:
        return x


def fallback_list(*fallbacks: Optional[T]) -> Optional[T]:
    """
    Suppose you want to chain a bunch of nested
    `unwrap_or`s:
    `unwrap_or(a,unwrap_or(b,unwrap_or(c,...)...))`
    How about instead we just offer a flat list of defaults?
    `fallback_list([a,b,c,d,e...])`
    Picks the first in the list that isn't `None`.
    (known as 'horizontal categorification')
    """

    return functools.reduce(
        lambda cur, nxt: nxt if cur is None else cur, fallbacks, None
    )


def option_map(opt: Optional[T], lam: Callable[[T], T]) -> Optional[T]:
    """
    Operate on a value assuming it isn't missing
    """
    if opt is None:
        return None
    else:
        return lam(opt)
