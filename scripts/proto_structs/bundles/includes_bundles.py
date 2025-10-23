"""Sets of includes that are in C++ bundles."""

import re
import typing
from typing import Callable
from typing import Set

import library.python.resource

_INCLUDE_CONTENTS_REGEX = re.compile(r'#include <([^>]+)>')


def _extract_includes(resource_id: str) -> Set[str]:
    find_resource = typing.cast(Callable[[str], bytes], library.python.resource.find)
    file_contents: bytes = find_resource(resource_id)
    return {match.group(1) for match in _INCLUDE_CONTENTS_REGEX.finditer(file_contents.decode())}


BUNDLE_HPP: Set[str] = _extract_includes('userver/proto-structs/impl/bundles/structs_hpp.hpp')
assert 'userver/proto-structs/io/std/string.hpp' in BUNDLE_HPP, BUNDLE_HPP  # Sanity check.

BUNDLE_CPP: Set[str] = _extract_includes('userver/proto-structs/impl/bundles/structs_cpp.hpp')
assert 'userver/proto-structs/io/std/string_conv.hpp' in BUNDLE_CPP, BUNDLE_CPP  # Sanity check.
