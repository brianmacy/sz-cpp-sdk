#!/usr/bin/env python3
"""Generate include/senzing/sdk/SzFlags.hpp from the installed szflags.json.

No flag values are hardcoded in C++ -- every named constant, its int64 value, and
its usage-group membership come from the Senzing-provided szflags.json (default
/opt/senzing/er/sdk/szflags.json). Names use the PascalCase `altSymbol` field so
they mirror the C# SDK character-for-character.

In addition to the named flag constants, this emits the C# flags *runtime API*:
the `SzFlagUsageGroup` enumeration, the per-group `*AllFlags` aggregate constants,
and the static `SzFlags` methods GetFlags/GetFlagsByName/GetNamesByFlag/GetGroups/
FlagsToString/FlagsToLong -- mirroring Senzing.Sdk.SzFlags / SzFlagUsageGroup.

Usage: gen_flags.py <szflags.json> <output SzFlags.hpp>
"""

import json
import re
import sys


# Aliases that the C# SDK exposes but that are not standalone entries in
# szflags.json (they are defined purely in terms of another flag).
DERIVED_ALIASES = {
    # C#: public const SzFlag SzRedoDefaultFlags = SzNoFlags;
    "SzRedoDefaultFlags": "SzNoFlags",
}


def popcount(value: int) -> int:
    return bin(value & 0xFFFFFFFFFFFFFFFF).count("1")


def is_individual(entry: dict) -> bool:
    """An individual flag (a member of the C# SzFlag enum), as opposed to an
    aggregate/default constant. Reproduces exactly the 45 C# SzFlag enum members:
    a single-bit value that belongs to at least one usage group and is not a
    "*DefaultFlags" constant (those are single-bit aliases but are aggregates)."""
    return (popcount(entry.get("value", 0)) == 1
            and len(entry.get("altGroups", [])) > 0
            and not entry.get("altSymbol", "").endswith("DefaultFlags"))


def main() -> int:
    if len(sys.argv) != 3:
        sys.stderr.write(
            "usage: gen_flags.py <szflags.json> <output SzFlags.hpp>\n")
        return 2

    flags_json_path = sys.argv[1]
    output_path = sys.argv[2]

    with open(flags_json_path, "r", encoding="utf-8") as handle:
        entries = json.load(handle)

    if not isinstance(entries, list) or not entries:
        sys.stderr.write(
            f"error: {flags_json_path} did not contain a non-empty array\n")
        return 1

    # ---- Named flag constants (all entries) ----
    names_seen = set()
    const_lines = []
    for entry in entries:
        name = entry.get("altSymbol")
        value = entry.get("value")
        symbol = entry.get("symbol", "")
        if not name or value is None:
            sys.stderr.write(
                f"error: entry missing altSymbol/value: {entry!r}\n")
            return 1
        if name in names_seen:
            sys.stderr.write(f"error: duplicate altSymbol {name!r}\n")
            return 1
        names_seen.add(name)
        const_lines.append(
            f"/// {symbol}\n"
            f"inline constexpr SzFlags {name}{{static_cast<int64_t>"
            f"({value}LL)}};")

    alias_lines = []
    for alias, target in DERIVED_ALIASES.items():
        if alias in names_seen:
            continue
        if target not in names_seen:
            sys.stderr.write(
                f"error: alias {alias!r} targets unknown flag {target!r}\n")
            return 1
        alias_lines.append(
            f"/// Alias (C# SzFlags.cs): {alias} == {target}\n"
            f"inline constexpr SzFlags {alias}{{{target}}};")

    # ---- Usage groups (sorted by name for stable bit assignment) ----
    group_names = sorted({g for e in entries for g in e.get("altGroups", [])})
    group_bit = {g: i for i, g in enumerate(group_names)}

    enum_lines = ["    SzNoFlagUsageGroups = 0,"]
    for name in group_names:
        enum_lines.append(f"    {name} = (1LL << {group_bit[name]}),")

    # ---- Individual flags + their group masks (name-sorted so FlagsToString
    # renders ambiguous same-value names in deterministic alphabetical order) ----
    individuals = sorted((e for e in entries if is_individual(e)),
                         key=lambda e: e["altSymbol"])
    flag_def_lines = []
    for e in individuals:
        mask = 0
        for g in e["altGroups"]:
            mask |= (1 << group_bit[g])
        flag_def_lines.append(
            f'    {{"{e["altSymbol"]}", static_cast<int64_t>({e["value"]}LL), '
            f'static_cast<int64_t>({mask}LL)}},')

    group_def_lines = [
        f'    {{"{name}", SzFlagUsageGroup::{name}}},' for name in group_names]

    # ---- Per-group *AllFlags aggregate constants ----
    all_flags_lines = []
    for name in group_names:
        agg = 0
        for e in individuals:
            if name in e["altGroups"]:
                agg |= e["value"]
        # SzEntityFlags -> SzEntityAllFlags
        assert name.endswith("Flags")
        all_name = name[: -len("Flags")] + "AllFlags"
        if all_name in names_seen:
            continue
        names_seen.add(all_name)
        all_flags_lines.append(
            f"/// Aggregate of all flags in the {name} usage group.\n"
            f"inline constexpr SzFlags {all_name}"
            f"{{static_cast<int64_t>({agg}LL)}};")

    consts = "\n".join(const_lines)
    aliases = "\n".join(alias_lines)
    enum_body = "\n".join(enum_lines)
    flag_defs = "\n".join(flag_def_lines)
    group_defs = "\n".join(group_def_lines)
    all_flags = "\n".join(all_flags_lines)

    header = f"""// GENERATED FILE -- DO NOT EDIT.
// Generated by tools/gen_flags.py from {flags_json_path}
// Source of truth for all flag values/groups is the Senzing-provided szflags.json.
#ifndef SENZING_SDK_SZFLAGS_HPP
#define SENZING_SDK_SZFLAGS_HPP

#include <cstdint>
#include <map>
#include <stdexcept>
#include <string>
#include <vector>

namespace senzing::sdk {{

// Forward declaration so SzFlags static methods can take group arguments.
enum class SzFlagUsageGroup : int64_t;

/// Strongly-typed 64-bit bitmask mirroring the C# `SzFlag`/`SzFlags` surface.
/// Wraps an int64_t so flag combination is type-safe and explicit.
class SzFlags {{
public:
    constexpr SzFlags() noexcept = default;
    constexpr explicit SzFlags(int64_t value) noexcept : value_(value) {{}}

    /// The raw int64 bitmask value as consumed by the native engine.
    [[nodiscard]] constexpr int64_t Value() const noexcept {{ return value_; }}

    constexpr SzFlags operator|(SzFlags other) const noexcept {{
        return SzFlags{{value_ | other.value_}};
    }}
    constexpr SzFlags operator&(SzFlags other) const noexcept {{
        return SzFlags{{value_ & other.value_}};
    }}
    constexpr SzFlags operator^(SzFlags other) const noexcept {{
        return SzFlags{{value_ ^ other.value_}};
    }}
    constexpr SzFlags operator~() const noexcept {{
        return SzFlags{{~value_}};
    }}
    constexpr SzFlags& operator|=(SzFlags other) noexcept {{
        value_ |= other.value_;
        return *this;
    }}
    constexpr SzFlags& operator&=(SzFlags other) noexcept {{
        value_ &= other.value_;
        return *this;
    }}
    constexpr SzFlags& operator^=(SzFlags other) noexcept {{
        value_ ^= other.value_;
        return *this;
    }}

    constexpr bool operator==(SzFlags other) const noexcept {{
        return value_ == other.value_;
    }}
    constexpr bool operator!=(SzFlags other) const noexcept {{
        return value_ != other.value_;
    }}
    /// Ordering so SzFlags may be used as a std::map key.
    constexpr bool operator<(SzFlags other) const noexcept {{
        return value_ < other.value_;
    }}

    /// True if any bit is set.
    [[nodiscard]] constexpr explicit operator bool() const noexcept {{
        return value_ != 0;
    }}

    // ---- Flags runtime API (mirrors C# Senzing.Sdk.SzFlags static methods) ----

    /// The aggregate flags belonging to an individual usage group. Throws
    /// std::invalid_argument if `group` is zero or an aggregate of groups.
    static SzFlags GetFlags(SzFlagUsageGroup group);

    /// All individual flag symbolic names mapped to their values.
    static std::map<std::string, SzFlags> GetFlagsByName();

    /// Individual flag names mapped to values for one usage group. Throws
    /// std::invalid_argument if `group` is zero or an aggregate of groups.
    static std::map<std::string, SzFlags> GetFlagsByName(SzFlagUsageGroup group);

    /// Individual flag values mapped to names for one usage group. Throws
    /// std::invalid_argument if `group` is zero or an aggregate of groups.
    static std::map<SzFlags, std::string> GetNamesByFlag(SzFlagUsageGroup group);

    /// The aggregate usage groups associated with a symbolic flag name. Throws
    /// std::invalid_argument if the name is not a recognized individual flag.
    static SzFlagUsageGroup GetGroups(const std::string& symbolicFlagName);

    /// String form of a flag value, preferring symbolic names; ambiguous values
    /// (same bit, multiple names) render as "{{ a / b }}".
    static std::string FlagsToString(SzFlags flag);

    /// String form of a flag value resolved within a single usage group.
    static std::string FlagsToString(SzFlagUsageGroup group, SzFlags flag);

    /// The raw int64 value of a flag (mirrors C# FlagsToLong).
    static int64_t FlagsToLong(SzFlags flag) {{ return flag.value_; }}

    /// Formats a value as four space-separated groups of four lowercase hex
    /// digits (mirrors C# Utilities.HexFormat).
    static std::string HexFormat(int64_t value);

private:
    int64_t value_{{0}};
}};

/// Usage groups for Senzing flags (mirrors C# SzFlagUsageGroup). Each individual
/// group is a distinct bit; aggregates combine bits.
enum class SzFlagUsageGroup : int64_t {{
{enum_body}
}};

constexpr SzFlagUsageGroup operator|(SzFlagUsageGroup a, SzFlagUsageGroup b) noexcept {{
    return static_cast<SzFlagUsageGroup>(static_cast<int64_t>(a) | static_cast<int64_t>(b));
}}
constexpr SzFlagUsageGroup operator&(SzFlagUsageGroup a, SzFlagUsageGroup b) noexcept {{
    return static_cast<SzFlagUsageGroup>(static_cast<int64_t>(a) & static_cast<int64_t>(b));
}}
constexpr SzFlagUsageGroup operator~(SzFlagUsageGroup a) noexcept {{
    return static_cast<SzFlagUsageGroup>(~static_cast<int64_t>(a));
}}

/// Zero usage groups (mirrors C# SzFlags.SzNoFlagUsageGroups).
inline constexpr SzFlagUsageGroup SzNoFlagUsageGroups = SzFlagUsageGroup::SzNoFlagUsageGroups;

// ---- Named flag constants (PascalCase, mirroring the C# SDK) ----
{consts}

// ---- Derived aliases (defined by the C# SDK in terms of other flags) ----
{aliases}

// ---- Per-group aggregate "*AllFlags" constants (mirroring the C# SDK) ----
{all_flags}

// ---- Generated data backing the flags runtime API ----
namespace detail {{

struct SzFlagDef {{
    const char* name;
    int64_t value;
    int64_t groups;  // bitmask over SzFlagUsageGroup bits
}};

inline constexpr SzFlagDef kIndividualFlags[] = {{
{flag_defs}
}};

struct SzGroupDef {{
    const char* name;
    SzFlagUsageGroup group;
}};

inline constexpr SzGroupDef kGroups[] = {{
{group_defs}
}};

inline bool IsSingleGroup(SzFlagUsageGroup group) {{
    const auto value = static_cast<int64_t>(group);
    if (value == 0) {{
        return false;
    }}
    for (const SzGroupDef& g : kGroups) {{
        if (g.group == group) {{
            return true;
        }}
    }}
    return false;
}}

}}  // namespace detail

// ---- Flags runtime API definitions ----

inline std::string SzFlags::HexFormat(int64_t value) {{
    std::string out;
    const auto v = static_cast<uint64_t>(value);
    for (int group = 0; group < 4; ++group) {{
        if (group != 0) {{
            out.push_back(' ');
        }}
        const uint64_t chunk = (v >> ((3 - group) * 16)) & 0xFFFFu;
        for (int digit = 0; digit < 4; ++digit) {{
            const int nibble = static_cast<int>((chunk >> ((3 - digit) * 4)) & 0xF);
            out.push_back(nibble < 10 ? static_cast<char>('0' + nibble)
                                      : static_cast<char>('a' + nibble - 10));
        }}
    }}
    return out;
}}

inline SzFlags SzFlags::GetFlags(SzFlagUsageGroup group) {{
    if (!detail::IsSingleGroup(group)) {{
        throw std::invalid_argument(
            "A single, non-zero usage group must be specified.");
    }}
    int64_t aggregate = 0;
    const int64_t groupBit = static_cast<int64_t>(group);
    for (const detail::SzFlagDef& f : detail::kIndividualFlags) {{
        if ((f.groups & groupBit) != 0) {{
            aggregate |= f.value;
        }}
    }}
    return SzFlags{{aggregate}};
}}

inline std::map<std::string, SzFlags> SzFlags::GetFlagsByName() {{
    std::map<std::string, SzFlags> result;
    for (const detail::SzFlagDef& f : detail::kIndividualFlags) {{
        result.emplace(f.name, SzFlags{{f.value}});
    }}
    return result;
}}

inline std::map<std::string, SzFlags> SzFlags::GetFlagsByName(SzFlagUsageGroup group) {{
    if (!detail::IsSingleGroup(group)) {{
        throw std::invalid_argument(
            "A single, non-zero usage group must be specified.");
    }}
    std::map<std::string, SzFlags> result;
    const int64_t groupBit = static_cast<int64_t>(group);
    for (const detail::SzFlagDef& f : detail::kIndividualFlags) {{
        if ((f.groups & groupBit) != 0) {{
            result.emplace(f.name, SzFlags{{f.value}});
        }}
    }}
    return result;
}}

inline std::map<SzFlags, std::string> SzFlags::GetNamesByFlag(SzFlagUsageGroup group) {{
    if (!detail::IsSingleGroup(group)) {{
        throw std::invalid_argument(
            "A single, non-zero usage group must be specified.");
    }}
    std::map<SzFlags, std::string> result;
    const int64_t groupBit = static_cast<int64_t>(group);
    for (const detail::SzFlagDef& f : detail::kIndividualFlags) {{
        if ((f.groups & groupBit) != 0) {{
            result.emplace(SzFlags{{f.value}}, f.name);
        }}
    }}
    return result;
}}

inline SzFlagUsageGroup SzFlags::GetGroups(const std::string& symbolicFlagName) {{
    for (const detail::SzFlagDef& f : detail::kIndividualFlags) {{
        if (symbolicFlagName == f.name) {{
            return static_cast<SzFlagUsageGroup>(f.groups);
        }}
    }}
    throw std::invalid_argument(
        "Unrecognized symbolic SzFlag name: " + symbolicFlagName);
}}

inline std::string SzFlags::FlagsToString(SzFlags flag) {{
    // value -> sorted list of individual flag names sharing that value.
    static const std::map<int64_t, std::vector<std::string>> namesByValue = [] {{
        std::map<int64_t, std::vector<std::string>> m;
        for (const detail::SzFlagDef& f : detail::kIndividualFlags) {{
            m[f.value].push_back(f.name);  // kIndividualFlags is name-sorted upstream
        }}
        return m;
    }}();

    std::string out;
    std::string prefix;
    const int64_t value = flag.value_;
    auto appendNames = [&](const std::vector<std::string>& names) {{
        out += prefix;
        if (names.size() == 1) {{
            out += names[0];
        }} else {{
            out += "{{ ";
            std::string sep;
            for (const std::string& name : names) {{
                out += sep;
                out += name;
                sep = " / ";
            }}
            out += " }}";
        }}
        prefix = " | ";
    }};

    if (value == 0) {{
        out += "{{ NONE }}";
    }} else if (auto whole = namesByValue.find(value); whole != namesByValue.end()) {{
        appendNames(whole->second);
    }} else {{
        for (int index = 0; index < 64; ++index) {{
            const int64_t singleBit = (1LL << index);
            if ((value & singleBit) == 0) {{
                continue;
            }}
            auto it = namesByValue.find(singleBit);
            if (it == namesByValue.end()) {{
                out += prefix;
                out += HexFormat(singleBit);
                prefix = " | ";
            }} else {{
                appendNames(it->second);
            }}
        }}
    }}
    out += " [";
    out += HexFormat(value);
    out += "]";
    return out;
}}

inline std::string SzFlags::FlagsToString(SzFlagUsageGroup group, SzFlags flag) {{
    std::map<SzFlags, std::string> namesByFlag;
    if (detail::IsSingleGroup(group)) {{
        namesByFlag = GetNamesByFlag(group);
    }}
    std::string out;
    std::string prefix;
    const int64_t value = flag.value_;
    if (value == 0) {{
        out += "{{ NONE }}";
    }} else {{
        for (int index = 0; index < 64; ++index) {{
            const int64_t singleBit = (1LL << index);
            if ((value & singleBit) == 0) {{
                continue;
            }}
            auto it = namesByFlag.find(SzFlags{{singleBit}});
            out += prefix;
            if (it == namesByFlag.end()) {{
                out += HexFormat(singleBit);
            }} else {{
                out += it->second;
            }}
            prefix = " | ";
        }}
    }}
    out += " [";
    out += HexFormat(value);
    out += "]";
    return out;
}}

}}  // namespace senzing::sdk

#endif  // SENZING_SDK_SZFLAGS_HPP
"""

    with open(output_path, "w", encoding="utf-8") as handle:
        handle.write(header)

    return 0


if __name__ == "__main__":
    sys.exit(main())
