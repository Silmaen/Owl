"""
Lexical "include what you use" audit for the C++ standard library.

A file that names `std::mutex`, `uint32_t`, `std::vector`, … must include the standard header that
declares it, never count on another header pulling it in. libstdc++ keeps shrinking its transitive
includes (MSYS2 MinGW broke on `<cstdint>` / `<mutex>`), and the precompiled header hides the problem on
every other toolchain, so the compiler alone cannot be trusted to catch it.

The audit is deliberately lexical (comments and string literals stripped) and conservative: only the
symbols listed in `SYMBOLS` are tracked, each mapped to the headers that are guaranteed to declare it.
A `.cpp` file also benefits from what its own header (same stem, same folder) and `owlpch.h` include.
"""

from __future__ import annotations

import re
from dataclasses import dataclass
from pathlib import Path

_CONTAINERS: tuple[str, ...] = (
    "array", "deque", "forward_list", "list", "map", "regex", "set", "span", "string", "string_view",
    "unordered_map", "unordered_set", "vector", "flat_map", "flat_set",
)
"""Headers that must declare the range access functions (`std::begin`, `std::size`, …) and `initializer_list`."""

_RANGE_ACCESS: tuple[str, ...] = ("iterator", *_CONTAINERS)
_STREAMS: tuple[str, ...] = ("ios", "iostream", "istream", "ostream", "sstream", "fstream", "iomanip", "spanstream",
                             "syncstream")
_SIZE_T: tuple[str, ...] = ("cstddef", "cstdio", "cstdlib", "cstring", "ctime", "cwchar", "cuchar", "stddef.h",
                            "stdio.h", "stdlib.h", "string.h", "time.h")
_INT_T: tuple[str, ...] = ("cstdint", "stdint.h", "cinttypes", "inttypes.h")


def _group(header: str, *names: str) -> dict[str, tuple[str, ...]]:
    return {name: (header,) for name in names}


SYMBOLS: dict[str, tuple[str, ...]] = {
    **_group("algorithm", "sort", "stable_sort", "find", "find_if", "find_if_not", "min", "max", "clamp", "copy",
             "copy_n", "copy_if", "copy_backward", "fill", "fill_n", "remove_if", "transform", "count", "count_if",
             "any_of", "all_of", "none_of", "reverse", "unique", "lower_bound", "upper_bound", "minmax",
             "min_element", "max_element", "minmax_element", "equal", "for_each", "replace", "replace_if", "rotate",
             "binary_search", "mismatch", "lexicographical_compare", "partial_sort", "nth_element", "shuffle",
             "partition", "generate", "iter_swap", "move_backward", "is_sorted", "adjacent_find", "search"),
    **_group("any", "any", "any_cast"),
    **_group("array", "array", "to_array"),
    **_group("atomic", "atomic", "atomic_bool", "atomic_int", "atomic_flag", "atomic_uint32_t", "atomic_size_t",
             "memory_order", "memory_order_relaxed", "memory_order_acquire", "memory_order_release",
             "memory_order_seq_cst", "memory_order_acq_rel", "atomic_thread_fence"),
    **_group("bit", "bit_cast", "popcount", "countl_zero", "countr_zero", "countl_one", "countr_one", "bit_width",
             "has_single_bit", "bit_ceil", "bit_floor", "rotl", "rotr", "endian", "byteswap"),
    **_group("bitset", "bitset"),
    **_group("cctype", "tolower", "toupper", "isspace", "isdigit", "isalpha", "isalnum", "isupper", "islower",
             "isxdigit", "isprint", "ispunct"),
    **_group("charconv", "from_chars", "to_chars", "chars_format", "from_chars_result", "to_chars_result"),
    **_group("chrono", "chrono"),
    **_group("cmath", "sqrt", "sin", "cos", "tan", "atan2", "atan", "asin", "acos", "pow", "floor", "ceil",
             "round", "lround", "llround", "fabs", "fmod", "exp", "exp2", "log", "log2", "log10", "isnan", "isinf",
             "isfinite", "trunc", "hypot", "fmin", "fmax", "copysign", "lerp", "signbit", "modf", "cbrt",
             "nextafter", "remainder", "fma", "sinh", "cosh", "tanh", "frexp", "ldexp", "nearbyint", "rint",
             "lrint"),
    **_group("compare", "strong_ordering", "weak_ordering", "partial_ordering", "compare_three_way"),
    **_group("concepts", "integral", "floating_point", "same_as", "convertible_to", "derived_from", "invocable",
             "default_initializable", "signed_integral", "unsigned_integral", "copyable", "regular_invocable",
             "predicate", "totally_ordered", "equality_comparable", "movable", "constructible_from"),
    **_group("condition_variable", "condition_variable", "condition_variable_any"),
    **_group("cstdio", "snprintf", "printf", "fprintf", "fopen", "fclose", "fread", "fwrite", "sprintf", "puts",
             "fflush", "fputs", "sscanf", "fseek", "ftell", "rewind", "fgets", "FILE"),
    **_group("cstdlib", "getenv", "exit", "abort", "malloc", "free", "strtol", "strtoul", "strtoull", "strtoll",
             "strtof", "strtod", "atoi", "atof", "rand", "srand", "system", "realloc", "calloc", "aligned_alloc",
             "quick_exit", "setenv"),
    **_group("cstring", "memcpy", "memset", "memcmp", "memmove", "memchr", "strlen", "strcmp", "strncmp", "strcpy",
             "strncpy", "strchr", "strrchr", "strstr", "strerror", "strtok"),
    **_group("ctime", "time_t", "tm", "localtime", "strftime", "gmtime", "mktime", "localtime_r", "gmtime_r"),
    **_group("deque", "deque"),
    **_group("exception", "exception_ptr", "current_exception", "rethrow_exception", "terminate",
             "uncaught_exceptions", "make_exception_ptr"),
    **_group("expected", "expected", "unexpected", "unexpect"),
    **_group("filesystem", "filesystem"),
    **_group("format", "format", "format_to", "format_to_n", "formatter", "vformat", "make_format_args",
             "format_string", "format_context", "formatted_size", "basic_format_string", "format_parse_context",
             "runtime_format", "format_error"),
    **_group("forward_list", "forward_list"),
    **_group("fstream", "ifstream", "ofstream", "fstream", "basic_ifstream", "basic_ofstream", "filebuf"),
    **_group("functional", "function", "reference_wrapper", "ref", "cref", "invoke", "bind", "bind_front",
             "less", "greater", "less_equal", "greater_equal", "equal_to", "not_equal_to", "identity", "plus",
             "minus", "multiplies", "move_only_function", "mem_fn", "not_fn", "placeholders"),
    **_group("future", "future", "promise", "async", "shared_future", "packaged_task", "launch", "future_status"),
    **_group("iomanip", "setw", "setprecision", "setfill", "put_time", "quoted"),
    **_group("iostream", "cout", "cerr", "cin", "clog"),
    **_group("iterator", "back_inserter", "front_inserter", "inserter", "distance", "advance", "next", "prev",
             "istreambuf_iterator", "ostreambuf_iterator", "reverse_iterator", "iterator_traits",
             "forward_iterator_tag", "random_access_iterator_tag", "bidirectional_iterator_tag",
             "input_iterator_tag", "output_iterator_tag", "contiguous_iterator_tag", "make_move_iterator",
             "back_insert_iterator", "default_sentinel_t", "default_sentinel", "istream_iterator",
             "ostream_iterator", "forward_iterator", "random_access_iterator", "input_iterator"),
    **_group("latch", "latch"),
    **_group("limits", "numeric_limits"),
    **_group("list", "list"),
    **_group("map", "map", "multimap"),
    **_group("memory", "shared_ptr", "unique_ptr", "weak_ptr", "make_shared", "make_unique", "allocate_shared",
             "enable_shared_from_this", "static_pointer_cast", "dynamic_pointer_cast", "reinterpret_pointer_cast",
             "addressof", "allocator", "default_delete", "construct_at", "destroy_at", "uninitialized_copy",
             "pointer_traits", "to_address", "assume_aligned", "allocator_traits"),
    **_group("mutex", "mutex", "lock_guard", "unique_lock", "scoped_lock", "recursive_mutex", "once_flag",
             "call_once", "timed_mutex", "recursive_timed_mutex", "adopt_lock", "defer_lock", "try_to_lock"),
    **_group("new", "bad_alloc", "nothrow", "launder", "align_val_t", "nothrow_t",
             "hardware_destructive_interference_size"),
    **_group("numbers", "numbers"),
    **_group("numeric", "accumulate", "iota", "reduce", "inner_product", "gcd", "lcm", "partial_sum", "midpoint",
             "transform_reduce", "exclusive_scan", "inclusive_scan", "adjacent_difference"),
    **_group("optional", "optional", "nullopt", "nullopt_t", "make_optional", "bad_optional_access"),
    **_group("print", "print", "println"),
    **_group("queue", "queue", "priority_queue"),
    **_group("random", "random_device", "mt19937", "mt19937_64", "uniform_int_distribution",
             "uniform_real_distribution", "normal_distribution", "default_random_engine",
             "bernoulli_distribution", "minstd_rand", "discrete_distribution"),
    **_group("ranges", "views"),
    **_group("regex", "regex", "regex_match", "regex_search", "regex_replace", "smatch", "cmatch",
             "sregex_iterator"),
    **_group("semaphore", "counting_semaphore", "binary_semaphore"),
    **_group("set", "set", "multiset"),
    **_group("shared_mutex", "shared_mutex", "shared_lock", "shared_timed_mutex"),
    **_group("source_location", "source_location"),
    **_group("span", "span", "dynamic_extent", "as_bytes", "as_writable_bytes"),
    **_group("sstream", "stringstream", "istringstream", "ostringstream", "stringbuf"),
    **_group("stack", "stack"),
    **_group("stacktrace", "stacktrace", "stacktrace_entry"),
    **_group("stdexcept", "runtime_error", "logic_error", "out_of_range", "invalid_argument", "length_error",
             "range_error", "overflow_error", "domain_error", "underflow_error"),
    **_group("stop_token", "stop_token", "stop_source", "stop_callback"),
    **_group("streambuf", "streambuf", "basic_streambuf"),
    **_group("string", "string", "basic_string", "wstring", "u8string", "u16string", "u32string", "to_string",
             "to_wstring", "stoi", "stol", "stoll", "stoul", "stoull", "stof", "stod", "stold", "getline",
             "char_traits"),
    **_group("string_view", "string_view", "basic_string_view", "wstring_view", "u8string_view"),
    **_group("thread", "thread", "jthread", "this_thread"),
    **_group("tuple", "tuple", "make_tuple", "tie", "apply", "forward_as_tuple", "tuple_cat", "make_from_tuple"),
    **_group("type_traits", "is_same", "is_same_v", "enable_if", "enable_if_t", "decay", "decay_t",
             "remove_cvref", "remove_cvref_t", "is_integral", "is_integral_v", "is_floating_point",
             "is_floating_point_v", "conditional", "conditional_t", "underlying_type", "underlying_type_t",
             "is_enum", "is_enum_v", "is_trivially_copyable", "is_trivially_copyable_v", "integral_constant",
             "bool_constant", "true_type", "false_type", "is_arithmetic", "is_arithmetic_v", "is_base_of",
             "is_base_of_v", "is_constant_evaluated", "remove_reference", "remove_reference_t",
             "is_convertible", "is_convertible_v", "is_signed", "is_signed_v", "is_unsigned", "is_unsigned_v",
             "make_unsigned", "make_unsigned_t", "make_signed", "make_signed_t", "is_pointer", "is_pointer_v",
             "remove_const", "remove_const_t", "is_invocable", "is_invocable_v", "is_invocable_r_v",
             "invoke_result", "invoke_result_t", "common_type", "common_type_t", "is_trivially_destructible_v",
             "type_identity", "type_identity_t", "is_default_constructible_v", "add_const_t", "is_class_v",
             "is_empty_v", "is_standard_layout_v", "is_void_v", "is_lvalue_reference_v", "is_const_v",
             "remove_pointer_t", "is_constructible_v", "is_nothrow_move_constructible_v", "is_abstract_v",
             "is_polymorphic_v", "is_function_v", "is_member_pointer_v", "is_reference_v", "is_aggregate_v",
             "is_scoped_enum_v", "is_trivial_v", "is_copy_constructible_v", "is_move_constructible_v",
             "is_assignable_v", "is_bounded_array_v", "is_array_v", "extent_v", "rank_v", "remove_extent_t",
             "add_pointer_t", "add_lvalue_reference_t", "void_t", "conjunction_v", "disjunction_v", "negation_v",
             "is_nothrow_constructible_v", "is_trivially_constructible_v", "is_trivially_default_constructible_v",
             "has_unique_object_representations_v", "aligned_storage_t", "alignment_of_v"),
    **_group("typeindex", "type_index"),
    **_group("typeinfo", "type_info", "bad_cast", "bad_typeid"),
    **_group("unordered_map", "unordered_map", "unordered_multimap"),
    **_group("unordered_set", "unordered_set", "unordered_multiset"),
    **_group("utility", "forward", "exchange", "declval", "index_sequence", "make_index_sequence",
             "integer_sequence", "make_integer_sequence", "index_sequence_for", "as_const", "to_underlying",
             "unreachable", "in_place", "in_place_t", "in_place_type", "in_place_index", "cmp_less", "cmp_equal",
             "cmp_greater", "cmp_less_equal", "cmp_greater_equal", "cmp_not_equal", "in_range", "piecewise_construct"),
    **_group("variant", "variant", "visit", "get_if", "holds_alternative", "monostate", "variant_alternative_t",
             "variant_size_v", "bad_variant_access"),
    **_group("vector", "vector"),
    # Names declared by several headers on purpose.
    "move": ("utility", "algorithm"),
    "swap": ("utility", "algorithm", *_CONTAINERS, "tuple", "optional", "variant", "memory", "functional", "any"),
    "pair": ("utility", "map", "unordered_map"),
    "make_pair": ("utility", "map", "unordered_map"),
    "ignore": ("tuple", "utility"),
    "tuple_size": ("tuple", "utility", "array"),
    "tuple_element": ("tuple", "utility", "array"),
    "remove": ("algorithm", "cstdio"),
    "abs": ("cmath", "cstdlib", "numeric", "complex"),
    "hash": ("functional", "string", "string_view", "memory", "optional", "variant", "typeindex", "thread",
             "bitset", "filesystem", "system_error", "vector", "coroutine", "stacktrace"),
    "initializer_list": ("initializer_list", *_CONTAINERS, "algorithm", "utility", "random", "valarray"),
    "begin": _RANGE_ACCESS, "end": _RANGE_ACCESS, "cbegin": _RANGE_ACCESS, "cend": _RANGE_ACCESS,
    "rbegin": _RANGE_ACCESS, "rend": _RANGE_ACCESS, "size": _RANGE_ACCESS, "ssize": _RANGE_ACCESS,
    "data": _RANGE_ACCESS, "empty": _RANGE_ACCESS,
    "get": ("tuple", "utility", "array", "variant", "ranges"),
    "exception": ("exception", "stdexcept"),
    "error_code": ("system_error", "filesystem"),
    "errc": ("system_error", "charconv"),
    "system_error": ("system_error",),
    "error_category": ("system_error",),
    "generic_category": ("system_error",),
    "system_category": ("system_error",),
    "ios": _STREAMS, "ios_base": _STREAMS, "streamsize": _STREAMS, "streamoff": _STREAMS, "hex": _STREAMS,
    "dec": _STREAMS, "fixed": _STREAMS, "boolalpha": _STREAMS, "scientific": _STREAMS,
    "ostream": ("ostream", "iostream", "sstream", "fstream", "spanstream", "syncstream"),
    "istream": ("istream", "iostream", "sstream", "fstream", "spanstream"),
    "endl": ("ostream", "iostream", "sstream", "fstream"),
    "flush": ("ostream", "iostream", "sstream", "fstream"),
    "size_t": _SIZE_T,
    "ptrdiff_t": ("cstddef", "stddef.h"),
    "nullptr_t": ("cstddef", "stddef.h"),
    "max_align_t": ("cstddef", "stddef.h"),
    "byte": ("cstddef",),
    "to_integer": ("cstddef",),
    **{name: _INT_T for name in (
        "int8_t", "int16_t", "int32_t", "int64_t", "uint8_t", "uint16_t", "uint32_t", "uint64_t", "intptr_t",
        "uintptr_t", "intmax_t", "uintmax_t", "int_fast8_t", "int_fast16_t", "int_fast32_t", "int_fast64_t",
        "uint_fast8_t", "uint_fast16_t", "uint_fast32_t", "uint_fast64_t", "int_least8_t", "int_least16_t",
        "int_least32_t", "int_least64_t", "uint_least8_t", "uint_least16_t", "uint_least32_t",
        "uint_least64_t")},
}
"""`std` name → the standard headers any one of which declares it (the first is the one to add)."""

GLOBAL_SYMBOLS: dict[str, tuple[str, ...]] = {
    **{name: _INT_T for name in (
        "int8_t", "int16_t", "int32_t", "int64_t", "uint8_t", "uint16_t", "uint32_t", "uint64_t", "intptr_t",
        "uintptr_t")},
    "size_t": _SIZE_T,
}
"""Unqualified names (the C typedefs the code uses without `std::`) → their headers."""

_STD_SUBNAMESPACE: dict[str, str] = {
    "chrono": "chrono", "filesystem": "filesystem", "this_thread": "thread", "numbers": "numbers",
    "views": "ranges", "placeholders": "functional", "literals": "",
}
"""`std::<sub>::…` namespaces whose whole content lives in one header ("" = not tracked)."""

_INCLUDE_RE = re.compile(r'^[ \t]*#[ \t]*include[ \t]*[<"]([^>"]+)[>"]', re.MULTILINE)
_STD_USE_RE = re.compile(r"(?<![\w:])(?:::)?std::(\w+)(?:::(\w+))?")
_GLOBAL_USE_RE = re.compile(r"(?<![\w:.>])(u?int(?:8|16|32|64)_t|u?intptr_t|size_t)\b")
_MEMBER_DECL_RE = re.compile(r"\b(?:using|typedef)\b")


@dataclass(frozen=True)
class MissingInclude:
    """One standard symbol used by a file whose header the file does not include."""

    path: Path
    line: int
    symbol: str
    header: str


def strip_code(text: str) -> str:
    """
    Blank comments, string and character literals (newlines kept, so offsets map to the same lines).

    :param text: C++ source text.
    :return: The text with every comment and literal replaced by spaces.
    """
    out: list[str] = []
    i = 0
    n = len(text)
    while i < n:
        c = text[i]
        if text.startswith("//", i):
            j = text.find("\n", i)
            j = n if j == -1 else j
            out.append(" " * (j - i))
            i = j
        elif text.startswith("/*", i):
            j = text.find("*/", i + 2)
            j = n if j == -1 else j + 2
            out.append(re.sub(r"[^\n]", " ", text[i:j]))
            i = j
        elif c == 'R' and text.startswith('R"', i) and (i == 0 or not (text[i - 1].isalnum() or text[i - 1] == "_")):
            m = re.match(r'R"([^(\s]*)\(', text[i:])
            if not m:
                out.append(c)
                i += 1
                continue
            end = text.find(")" + m.group(1) + '"', i)
            j = n if end == -1 else end + len(m.group(1)) + 2
            out.append(re.sub(r"[^\n]", " ", text[i:j]))
            i = j
        elif c in "\"'":
            if c == "'" and i > 0 and text[i - 1].isalnum():
                # digit separator (1'000) or a suffix like u8'x' handled loosely
                out.append(" ")
                i += 1
                continue
            j = i + 1
            while j < n and text[j] != c and text[j] != "\n":
                j += 2 if text[j] == "\\" else 1
            j = min(j + 1, n)
            out.append(" " * (j - i))
            i = j
        else:
            out.append(c)
            i += 1
    return "".join(out)


def includes_of(text: str) -> set[str]:
    """
    :param text: C++ source text.
    :return: Every header named by an `#include` directive.
    """
    return set(_INCLUDE_RE.findall(text))


def _required(name: str, sub: str | None) -> tuple[str, ...] | None:
    if name in _STD_SUBNAMESPACE:
        header = _STD_SUBNAMESPACE[name]
        return (header,) if header else None
    if name == "ranges":
        if sub and sub in SYMBOLS and SYMBOLS[sub] == ("algorithm",):
            return ("algorithm",)
        return None
    if name.startswith("_"):
        return None
    return SYMBOLS.get(name)


def audit_file(path: Path, extra_includes: set[str] | None = None) -> list[MissingInclude]:
    """
    List the standard symbols a file uses without including their header.

    :param path: File to audit.
    :param extra_includes: Headers already provided to the file (its own header, the PCH, …).
    :return: One finding per missing header, at the first line that needs it.
    """
    text = path.read_text(errors="replace")
    have = includes_of(text) | (extra_includes or set())
    code = strip_code(text)
    first: dict[str, MissingInclude] = {}

    def need(offset: int, symbol: str, headers: tuple[str, ...]) -> None:
        if any(h in have for h in headers) or headers[0] in first:
            return
        line = code.count("\n", 0, offset) + 1
        first[headers[0]] = MissingInclude(path, line, symbol, headers[0])

    for m in _STD_USE_RE.finditer(code):
        headers = _required(m.group(1), m.group(2))
        if headers:
            need(m.start(), f"std::{m.group(1)}", headers)
    for m in _GLOBAL_USE_RE.finditer(code):
        need(m.start(), m.group(1), GLOBAL_SYMBOLS[m.group(1)])
    return sorted(first.values(), key=lambda f: (f.line, f.header))


def provided_to(path: Path, pch: Path | None = None) -> set[str]:
    """
    Headers a `.cpp` legitimately gets from elsewhere: its own header and, when it includes it, the PCH.

    :param path: The implementation file.
    :param pch: The precompiled header (`owlpch.h`), if any.
    :return: The set of header names provided.
    """
    if path.suffix not in (".cpp", ".cc", ".cxx"):
        return set()
    out: set[str] = set()
    own = path.with_suffix(".h")
    text = path.read_text(errors="replace")
    if own.exists():
        out |= includes_of(own.read_text(errors="replace"))
    if pch is not None and pch.exists() and pch.name in includes_of(text):
        out |= includes_of(pch.read_text(errors="replace"))
    return out
