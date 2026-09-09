# SPDX-License-Identifier: Apache-2.0
"""Explicit whole-command counter collection; never invent per-kernel counts."""
import platform
import re
import shutil


def validate_options(args, check_tools=True):
    if args.perf and args.likwid_group:
        raise ValueError("select either perf or LIKWID for one run")
    if bool(args.likwid_group) != bool(args.likwid_cpus):
        raise ValueError("LIKWID requires both --likwid-group and --likwid-cpus")
    if args.likwid_group:
        if args.cpu_node is not None:
            raise ValueError("LIKWID CPU pinning cannot be combined with --cpu-node")
        if not re.fullmatch(r"[0-9]+(?:-[0-9]+)?(?:,[0-9]+(?:-[0-9]+)?)*", args.likwid_cpus):
            raise ValueError("likwid-cpus must be a numeric CPU list, such as 0,2-5")
        seen = set()
        for part in args.likwid_cpus.split(","):
            limits = part.split("-")
            first, last = int(limits[0]), int(limits[-1])
            if first > last or last > 65535:
                raise ValueError("invalid LIKWID CPU range")
            selected = set(range(first, last + 1))
            if selected & seen:
                raise ValueError("duplicate LIKWID CPUs")
            seen.update(selected)
    tool = "perf" if args.perf else "likwid-perfctr" if args.likwid_group else None
    if check_tools and tool and (platform.system() != "Linux" or not shutil.which(tool)):
        raise RuntimeError(f"{tool} collection requires the tool on Linux")


def counter_prefix(args, output):
    if args.perf:
        return ["perf", "stat", "-x", ";", "-e", args.perf_events,
                "-o", str(output), "--"]
    if args.likwid_group:
        return ["likwid-perfctr", "-C", args.likwid_cpus,
                "-g", args.likwid_group, "-O", "-o", str(output)]
    return []


def metadata(args):
    if args.likwid_group:
        return dict(tool="likwid-perfctr", group=args.likwid_group,
                    cpus=args.likwid_cpus,
                    scope="selected CPUs during the whole command, including initialization")
    if args.perf:
        return dict(tool="perf", events=args.perf_events.split(","),
                    scope="whole command, including initialization")
    return None
