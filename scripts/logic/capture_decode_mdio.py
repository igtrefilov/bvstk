#!/usr/bin/env python3
"""Capture and decode Clause 22 MDIO traffic from Saleae Logic raw CSV."""

from __future__ import annotations

import argparse
import csv
import json
import statistics
import sys
import time
from dataclasses import asdict, dataclass
from pathlib import Path
from typing import Iterable


DEFAULT_APP = Path("/home/ilya/utils/Logic-2.4.22-linux-x64.AppImage")
DEFAULT_VENV_PYTHON = Path("/home/ilya/.venvs/logic2-automation/bin/python")
DEFAULT_OUTPUT_ROOT = Path("/tmp/logic2-mdio-captures")


@dataclass
class Frame:
    bus: str
    frame_index: int
    start_edge: int
    time_s: float
    bits_before: str
    raw_bits: str
    bits_after: str
    preamble_ones: int
    op_bits: str
    op: str
    phy: int
    reg: int
    ta: str
    ta_ok: bool
    ta0_time_s: float | None
    ta1_time_s: float | None
    data: int
    mdc_hz: float | None


def parse_args() -> argparse.Namespace:
    parser = argparse.ArgumentParser(
        description="Capture Logic 2 raw digital data and decode MDIO Clause 22 frames."
    )
    parser.add_argument("--decode-only", type=Path, help="Decode an existing Saleae raw digital.csv")
    parser.add_argument("--connect-only", action="store_true", help="Only check that Logic 2 can see Saleae")
    parser.add_argument("--duration", type=float, default=3.0, help="Timed capture duration in seconds")
    parser.add_argument("--sample-rate", type=int, default=24_000_000, help="Digital sample rate")
    parser.add_argument("--output-root", type=Path, default=DEFAULT_OUTPUT_ROOT)
    parser.add_argument("--output-dir", type=Path, help="Exact output directory")
    parser.add_argument("--logic-wrapper", type=Path, default=Path(__file__).with_name("logic2-nosandbox-wrapper.sh"))
    parser.add_argument("--connect-timeout", type=float, default=20.0)
    parser.add_argument("--slave-mdc", type=int, default=0)
    parser.add_argument("--slave-mdio", type=int, default=1)
    parser.add_argument("--master-mdc", type=int, default=2)
    parser.add_argument("--master-mdio", type=int, default=3)
    parser.add_argument("--min-preamble", type=int, default=8)
    parser.add_argument("--min-frame-spacing", type=int, default=40)
    parser.add_argument("--fail-on-ta", action="store_true", help="Exit non-zero if any read/write TA is bad")
    parser.add_argument("--show-ta-errors", type=int, default=8, help="Print up to N TA errors")
    return parser.parse_args()


def bit_value(bits: Iterable[int]) -> int:
    value = 0
    for bit in bits:
        value = (value << 1) | int(bit)
    return value


def read_raw_csv(path: Path) -> tuple[list[float], list[list[int]]]:
    with path.open(newline="") as f:
        reader = csv.DictReader(f)
        if not reader.fieldnames or "Time [s]" not in reader.fieldnames:
            raise ValueError(f"{path} is not a Saleae raw digital CSV")
        channel_names = [name for name in reader.fieldnames if name.startswith("Channel ")]
        if not channel_names:
            raise ValueError(f"{path} has no digital channel columns")

        times: list[float] = []
        values: list[list[int]] = []
        for row in reader:
            times.append(float(row["Time [s]"]))
            values.append([int(row[name]) for name in channel_names])
    return times, values


def collect_rising_edges(
    times: list[float],
    values: list[list[int]],
    mdc_channel: int,
    mdio_channel: int,
) -> tuple[list[float], list[int]]:
    if not values:
        return [], []
    width = len(values[0])
    if mdc_channel >= width or mdio_channel >= width:
        raise ValueError(f"CSV has {width} channels, cannot use MDC={mdc_channel}, MDIO={mdio_channel}")

    edge_times: list[float] = []
    edge_bits: list[int] = []
    prev_mdc = values[0][mdc_channel]
    for time_s, sample in zip(times[1:], values[1:]):
        mdc = sample[mdc_channel]
        if prev_mdc == 0 and mdc == 1:
            edge_times.append(time_s)
            edge_bits.append(sample[mdio_channel])
        prev_mdc = mdc
    return edge_times, edge_bits


def count_preamble_ones(bits: list[int], start: int) -> int:
    count = 0
    pos = start - 1
    while pos >= 0 and bits[pos] == 1:
        count += 1
        pos -= 1
    return count


def estimate_mdc_hz(edge_times: list[float], start: int, frame_len: int = 32) -> float | None:
    end = min(len(edge_times), start + frame_len)
    if end - start < 3:
        return None
    periods = [
        edge_times[i + 1] - edge_times[i]
        for i in range(start, end - 1)
        if edge_times[i + 1] > edge_times[i]
    ]
    if not periods:
        return None
    median_period = statistics.median(periods)
    if median_period <= 0:
        return None
    return 1.0 / median_period


def decode_bus(
    bus: str,
    edge_times: list[float],
    bits: list[int],
    min_preamble: int,
    min_frame_spacing: int,
) -> list[Frame]:
    frames: list[Frame] = []
    last_start = -10_000
    i = 0
    while i <= len(bits) - 32:
        if i - last_start <= min_frame_spacing:
            i += 1
            continue
        if bits[i : i + 2] != [0, 1]:
            i += 1
            continue
        if bits[i + 2 : i + 4] not in ([1, 0], [0, 1]):
            i += 1
            continue

        preamble_ones = count_preamble_ones(bits, i)
        if preamble_ones < min_preamble:
            i += 1
            continue

        raw = bits[i : i + 32]
        bits_before = bits[max(0, i - 8) : i]
        bits_after = bits[i + 32 : min(len(bits), i + 40)]
        op_bits = "".join(str(bit) for bit in raw[2:4])
        op = "read" if op_bits == "10" else "write"
        ta = "".join(str(bit) for bit in raw[14:16])
        frame = Frame(
            bus=bus,
            frame_index=len(frames),
            start_edge=i,
            time_s=edge_times[i],
            bits_before="".join(str(bit) for bit in bits_before),
            raw_bits="".join(str(bit) for bit in raw),
            bits_after="".join(str(bit) for bit in bits_after),
            preamble_ones=preamble_ones,
            op_bits=op_bits,
            op=op,
            phy=bit_value(raw[4:9]),
            reg=bit_value(raw[9:14]),
            ta=ta,
            ta_ok=(ta == "10"),
            ta0_time_s=edge_times[i + 14] if i + 14 < len(edge_times) else None,
            ta1_time_s=edge_times[i + 15] if i + 15 < len(edge_times) else None,
            data=bit_value(raw[16:32]),
            mdc_hz=estimate_mdc_hz(edge_times, i),
        )
        frames.append(frame)
        last_start = i
        i += 32
    return frames


def summarize(frames: list[Frame]) -> dict:
    ta_errors = [frame for frame in frames if not frame.ta_ok]
    by_reg: dict[str, int] = {}
    for frame in ta_errors:
        key = f"phy{frame.phy}.reg{frame.reg}"
        by_reg[key] = by_reg.get(key, 0) + 1
    hz_values = [frame.mdc_hz for frame in frames if frame.mdc_hz is not None]
    return {
        "frames": len(frames),
        "reads": sum(1 for frame in frames if frame.op == "read"),
        "writes": sum(1 for frame in frames if frame.op == "write"),
        "ta_errors": len(ta_errors),
        "ta_errors_by_reg": by_reg,
        "mdc_hz_median": statistics.median(hz_values) if hz_values else None,
        "first_time_s": frames[0].time_s if frames else None,
        "last_time_s": frames[-1].time_s if frames else None,
    }


def write_outputs(output_dir: Path, frames_by_bus: dict[str, list[Frame]]) -> dict:
    output_dir.mkdir(parents=True, exist_ok=True)
    summary = {bus: summarize(frames) for bus, frames in frames_by_bus.items()}

    for bus, frames in frames_by_bus.items():
        csv_path = output_dir / f"{bus}_decoded_mdio.csv"
        with csv_path.open("w", newline="") as f:
            writer = csv.DictWriter(
                f,
                fieldnames=[
                    "bus",
                    "frame_index",
                    "start_edge",
                    "time_s",
                    "bits_before",
                    "raw_bits",
                    "bits_after",
                    "preamble_ones",
                    "op_bits",
                    "op",
                    "phy",
                    "reg",
                    "ta",
                    "ta_ok",
                    "ta0_time_s",
                    "ta1_time_s",
                    "data_hex",
                    "data",
                    "mdc_hz",
                ],
            )
            writer.writeheader()
            for frame in frames:
                row = asdict(frame)
                row["data_hex"] = f"0x{frame.data:04X}"
                writer.writerow(row)

    ta_error_path = output_dir / "ta_errors.csv"
    with ta_error_path.open("w", newline="") as f:
        writer = csv.DictWriter(
            f,
            fieldnames=[
                "bus",
                "frame_index",
                "time_s",
                "op",
                "phy",
                "reg",
                "ta",
                "data_hex",
                "mdc_hz",
                "ta0_time_s",
                "ta1_time_s",
                "bits_before",
                "raw_bits",
                "bits_after",
            ],
        )
        writer.writeheader()
        for frames in frames_by_bus.values():
            for frame in frames:
                if frame.ta_ok:
                    continue
                writer.writerow(
                    {
                        "bus": frame.bus,
                        "frame_index": frame.frame_index,
                        "time_s": frame.time_s,
                        "op": frame.op,
                        "phy": frame.phy,
                        "reg": frame.reg,
                        "ta": frame.ta,
                        "data_hex": f"0x{frame.data:04X}",
                        "mdc_hz": frame.mdc_hz,
                        "ta0_time_s": frame.ta0_time_s,
                        "ta1_time_s": frame.ta1_time_s,
                        "bits_before": frame.bits_before,
                        "raw_bits": frame.raw_bits,
                        "bits_after": frame.bits_after,
                    }
                )

    with (output_dir / "mdio_summary.json").open("w") as f:
        json.dump(summary, f, indent=2, sort_keys=True)
        f.write("\n")
    return summary


def print_summary(summary: dict, output_dir: Path) -> None:
    print(f"output: {output_dir}")
    for bus, item in summary.items():
        mdc = item["mdc_hz_median"]
        mdc_text = "n/a" if mdc is None else f"{mdc:.1f} Hz"
        print(
            f"{bus}: frames={item['frames']} reads={item['reads']} writes={item['writes']} "
            f"ta_errors={item['ta_errors']} mdc_median={mdc_text}"
        )
        if item["ta_errors_by_reg"]:
            detail = ", ".join(f"{key}:{value}" for key, value in sorted(item["ta_errors_by_reg"].items()))
            print(f"{bus}: ta_errors_by_reg {detail}")


def print_ta_examples(frames_by_bus: dict[str, list[Frame]], limit: int) -> None:
    if limit <= 0:
        return
    printed = 0
    for frames in frames_by_bus.values():
        for frame in frames:
            if frame.ta_ok:
                continue
            print(
                f"TAERR {frame.bus}#{frame.frame_index} t={frame.time_s:.9f}s "
                f"{frame.op} phy={frame.phy} reg={frame.reg} ta={frame.ta} "
                f"data=0x{frame.data:04X} raw={frame.raw_bits}"
            )
            printed += 1
            if printed >= limit:
                return


def capture_raw_csv(args: argparse.Namespace, output_dir: Path) -> Path:
    try:
        from saleae.automation import (
            CaptureConfiguration,
            LogicDeviceConfiguration,
            Manager,
            TimedCaptureMode,
        )
    except ImportError as exc:
        raise RuntimeError(
            f"saleae.automation is not importable. Run with {DEFAULT_VENV_PYTHON}"
        ) from exc

    if not args.logic_wrapper.exists():
        raise FileNotFoundError(args.logic_wrapper)

    channels = sorted({args.slave_mdc, args.slave_mdio, args.master_mdc, args.master_mdio})
    with Manager.launch(
        str(args.logic_wrapper),
        connect_timeout_seconds=args.connect_timeout,
    ) as manager:
        devices = manager.get_devices()
        if args.connect_only:
            print(f"devices: {devices}")
            return Path()
        if not devices:
            raise RuntimeError("Saleae device not found")
        capture = manager.start_capture(
            device_id=devices[0].device_id,
            device_configuration=LogicDeviceConfiguration(
                enabled_digital_channels=channels,
                digital_sample_rate=args.sample_rate,
            ),
            capture_configuration=CaptureConfiguration(
                capture_mode=TimedCaptureMode(duration_seconds=args.duration)
            ),
        )
        capture.wait()
        capture.save_capture(str(output_dir / "capture.sal"))
        capture.export_raw_data_csv(str(output_dir), digital_channels=channels)
    return output_dir / "digital.csv"


def main() -> int:
    args = parse_args()
    timestamp = time.strftime("%Y%m%d-%H%M%S")
    output_dir = args.output_dir or (args.output_root / f"{timestamp}-mdio")

    if args.decode_only:
        raw_csv = args.decode_only
        output_dir.mkdir(parents=True, exist_ok=True)
    else:
        output_dir.mkdir(parents=True, exist_ok=True)
        raw_csv = capture_raw_csv(args, output_dir)
        if args.connect_only:
            return 0

    times, values = read_raw_csv(raw_csv)
    frames_by_bus: dict[str, list[Frame]] = {}
    for bus, mdc, mdio in (
        ("pl_slave", args.slave_mdc, args.slave_mdio),
        ("pl_master", args.master_mdc, args.master_mdio),
    ):
        edge_times, edge_bits = collect_rising_edges(times, values, mdc, mdio)
        frames_by_bus[bus] = decode_bus(
            bus,
            edge_times,
            edge_bits,
            min_preamble=args.min_preamble,
            min_frame_spacing=args.min_frame_spacing,
        )

    summary = write_outputs(output_dir, frames_by_bus)
    print_summary(summary, output_dir)
    print_ta_examples(frames_by_bus, args.show_ta_errors)
    if args.fail_on_ta and any(item["ta_errors"] for item in summary.values()):
        return 2
    return 0


if __name__ == "__main__":
    try:
        raise SystemExit(main())
    except Exception as exc:
        print(f"error: {exc}", file=sys.stderr)
        raise
