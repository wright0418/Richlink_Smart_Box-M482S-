#!/usr/bin/env python3
"""
RL_SPORT Mole BLE self-test runner

Purpose:
- Reference docs/認知訓練/index.html packet format
- Connect to MOLE_* BLE devices
- Send all key command permutations (LED / BRIGHTNESS_CMD / HIT_CONFIG / invalid packets)
- Cross-validate by reading COM3 UART trace lines from firmware:
    [MOLE_TEST] RX ...
    [MOLE_TEST] TX HIT=0x01

Dependencies:
- bleak
- pyserial

Example:
  python tools/mole_ble_selftest.py --com COM3 --count 1 --manual-hit-seconds 6
"""

from __future__ import annotations

import argparse
import asyncio
import dataclasses
import re
import sys
import threading
import time
from collections import defaultdict
from typing import Callable, Dict, List, Optional, Tuple

try:
    import serial  # type: ignore
except Exception as exc:  # pragma: no cover
    print(f"[FATAL] pyserial import failed: {exc}")
    print("[HINT] install: pip install pyserial bleak")
    sys.exit(2)

try:
    from bleak import BleakClient, BleakScanner  # type: ignore
except Exception as exc:  # pragma: no cover
    print(f"[FATAL] bleak import failed: {exc}")
    print("[HINT] install: pip install bleak pyserial")
    sys.exit(2)


BLE_SERVICE_UUID = "524cacc0-3c17-d293-8e48-14fe2e4da212"
BLE_TX_UUID = "0000d002-0000-1000-8000-00805f9b34fb"  # write
BLE_RX_UUID = "0000d001-0000-1000-8000-00805f9b34fb"  # notify

HEADER = 0xAA
FOOTER = 0x55

COLOR_MAP = {
    "off": 0x00,
    "red": 0x01,
    "green": 0x02,
    "blue": 0x03,
    "yellow": 0x04,
    "purple": 0x05,
    "white": 0x06,
}

# 8x8 rows in same style as index.html
PATTERN_ROWS = {
    "CLEAR": [0x00] * 8,
    "CIRCLE": [0x3C, 0x42, 0x81, 0x81, 0x81, 0x81, 0x42, 0x3C],
    "CROSS": [0x81, 0x42, 0x24, 0x18, 0x18, 0x24, 0x42, 0x81],
    "CHECK": [0x00, 0x01, 0x02, 0x04, 0x88, 0x50, 0x20, 0x00],
    "SQUARE": [0x00, 0x7E, 0x42, 0x42, 0x42, 0x42, 0x7E, 0x00],
    "FULL": [0xFF] * 8,
}


@dataclasses.dataclass
class TestCaseResult:
    name: str
    status: str
    detail: str

    @property
    def passed(self) -> bool:
        return self.status == "PASS"


class SerialTraceReader:
    def __init__(self, com: str, baud: int) -> None:
        self.com = com
        self.baud = baud
        self._ser: Optional[serial.Serial] = None
        self._stop = threading.Event()
        self._thread: Optional[threading.Thread] = None
        self._lines: List[str] = []
        self._lock = threading.Lock()

    def start(self) -> None:
        self._ser = serial.Serial(self.com, self.baud, timeout=0.1)
        self._thread = threading.Thread(target=self._run, daemon=True)
        self._thread.start()
        print(f"[UART] Opened {self.com} @ {self.baud}")

    def stop(self) -> None:
        self._stop.set()
        if self._thread:
            self._thread.join(timeout=1.0)
        if self._ser and self._ser.is_open:
            self._ser.close()
        print("[UART] Closed")

    def snapshot(self) -> List[str]:
        with self._lock:
            return list(self._lines)

    def _append_line(self, line: str) -> None:
        with self._lock:
            self._lines.append(line)
            if len(self._lines) > 5000:
                self._lines = self._lines[-5000:]

    def _run(self) -> None:
        assert self._ser is not None
        while not self._stop.is_set():
            try:
                raw = self._ser.readline()
            except Exception:
                continue
            if not raw:
                continue
            try:
                line = raw.decode("utf-8", errors="replace").strip()
            except Exception:
                continue
            if line:
                self._append_line(line)
                print(f"[UART] {line}")


class MoleBleSelfTester:
    def __init__(
        self,
        com: Optional[str],
        baud: int,
        count: int,
        name_prefix: str,
        scan_timeout: float,
        cmd_gap_ms: int,
        settle_ms: int,
        manual_hit_seconds: int,
    ) -> None:
        self.count = count
        self.name_prefix = name_prefix
        self.scan_timeout = scan_timeout
        self.cmd_gap_ms = cmd_gap_ms
        self.settle_ms = settle_ms
        self.manual_hit_seconds = manual_hit_seconds
        self.com = com
        self.baud = baud
        self.trace = SerialTraceReader(com, baud) if com else None
        self.clients: List[Tuple[str, BleakClient]] = []
        self.char_map: Dict[str, Tuple[List[str], str]] = {}
        self.notify_hits: Dict[str, int] = defaultdict(int)

    @staticmethod
    def _xor_checksum(data: List[int], start: int, end_inclusive: int) -> int:
        v = 0
        for i in range(start, end_inclusive + 1):
            v ^= data[i]
        return v & 0xFF

    def build_led_packet(self, rows: List[int], color_name: str, is_target: bool) -> bytes:
        if len(rows) != 8:
            raise ValueError("rows must be 8 bytes")
        color = COLOR_MAP[color_name]
        pkt = [HEADER, color, 0x01 if is_target else 0x00, *rows, 0x00, FOOTER]
        pkt[11] = self._xor_checksum(pkt, 1, 10)
        return bytes(pkt)

    def build_brightness_cmd(self, value: int, force_bad_checksum: bool = False) -> bytes:
        pkt = [HEADER, 0xC0, value & 0xFF, 0x00, FOOTER]
        pkt[3] = self._xor_checksum(pkt, 1, 2)
        if force_bad_checksum:
            pkt[3] ^= 0xFF
        return bytes(pkt)

    def build_hit_config_cmd(self, method_bits: int, force_bad_checksum: bool = False) -> bytes:
        pkt = [HEADER, 0xC1, method_bits & 0xFF, 0x00, FOOTER]
        pkt[3] = self._xor_checksum(pkt, 1, 2)
        if force_bad_checksum:
            pkt[3] ^= 0xFF
        return bytes(pkt)

    async def discover_and_connect(self) -> None:
        print(
            f"[BLE] Scanning {self.scan_timeout:.1f}s for prefix '{self.name_prefix}'...")
        devices = await BleakScanner.discover(timeout=self.scan_timeout)
        picks = [d for d in devices if d.name and d.name.startswith(
            self.name_prefix)]
        if not picks:
            raise RuntimeError(
                f"No BLE device found with prefix '{self.name_prefix}'")

        picks.sort(key=lambda d: d.name or "")
        picks = picks[: self.count]
        print(
            f"[BLE] Matched devices: {', '.join((d.name or d.address) for d in picks)}")

        for d in picks:
            client = BleakClient(d)
            await client.connect(timeout=10.0)
            if not client.is_connected:
                raise RuntimeError(f"connect failed: {d.name or d.address}")

            services = client.services
            service = services.get_service(BLE_SERVICE_UUID)

            writable_uuid: Optional[str] = None
            notify_uuid: Optional[str] = None
            writable_candidates: List[str] = []

            def _has_prop(ch, p: str) -> bool:
                props = {x.lower() for x in ch.properties}
                return p in props

            if service is not None:
                print(
                    f"[BLE] Service {BLE_SERVICE_UUID} characteristics for {d.name or d.address}:")
                for ch in service.characteristics:
                    print(f"       - {ch.uuid} props={list(ch.properties)}")

                # prefer fixed UUIDs first
                for ch in service.characteristics:
                    cu = str(ch.uuid).lower()
                    if _has_prop(ch, "write-without-response") or _has_prop(ch, "write"):
                        writable_candidates.append(str(ch.uuid))
                    if cu == BLE_TX_UUID.lower() and (_has_prop(ch, "write") or _has_prop(ch, "write-without-response")):
                        writable_uuid = str(ch.uuid)
                    if cu == BLE_RX_UUID.lower() and (_has_prop(ch, "notify") or _has_prop(ch, "indicate")):
                        notify_uuid = str(ch.uuid)

                # fallback: auto-pick by properties in same service
                if writable_uuid is None:
                    for ch in service.characteristics:
                        if _has_prop(ch, "write-without-response") or _has_prop(ch, "write"):
                            writable_uuid = str(ch.uuid)
                            break
                if notify_uuid is None:
                    for ch in service.characteristics:
                        if _has_prop(ch, "notify") or _has_prop(ch, "indicate"):
                            notify_uuid = str(ch.uuid)
                            break

            if writable_uuid is None or notify_uuid is None:
                raise RuntimeError(
                    f"cannot resolve BLE chars for {d.name or d.address} "
                    f"(service={BLE_SERVICE_UUID}, write={writable_uuid}, notify={notify_uuid})"
                )

            if writable_uuid not in writable_candidates:
                writable_candidates.insert(0, writable_uuid)

            # de-dup while preserving order
            seen = set()
            writable_candidates = [
                x for x in writable_candidates if not (x in seen or seen.add(x))]

            dev_name = d.name or d.address
            self.char_map[dev_name] = (writable_candidates, notify_uuid)

            def _mk_cb(dev_name: str) -> Callable[[int, bytearray], None]:
                def _cb(_: int, data: bytearray) -> None:
                    hit_count = sum(1 for b in data if b == 0x01)
                    if hit_count:
                        self.notify_hits[dev_name] += hit_count
                        print(f"[BLE RX] {dev_name} <- HIT x{hit_count}")
                return _cb

            await client.start_notify(notify_uuid, _mk_cb(dev_name))
            self.clients.append((dev_name, client))
            print(
                f"[BLE] Connected: {dev_name} (writes={writable_candidates}, notify={notify_uuid})")

    async def disconnect_all(self) -> None:
        for dev_name, client in self.clients:
            try:
                if client.is_connected:
                    notify_uuid = self.char_map.get(dev_name, ([], ""))[1]
                    if notify_uuid:
                        await client.stop_notify(notify_uuid)
                    await client.disconnect()
                    print(f"[BLE] Disconnected: {dev_name}")
            except Exception as exc:
                print(f"[BLE] Disconnect warning ({dev_name}): {exc}")

    async def send_all(self, payload: bytes, label: str) -> None:
        hexs = payload.hex(" ").upper()
        for dev_name, client in self.clients:
            writable_uuids = self.char_map.get(dev_name, ([], ""))[0]
            if not writable_uuids:
                raise RuntimeError(
                    f"missing write characteristic for {dev_name}")
            for w_uuid in writable_uuids:
                try:
                    await client.write_gatt_char(w_uuid, payload, response=True)
                except Exception:
                    await client.write_gatt_char(w_uuid, payload, response=False)
            print(
                f"[BLE TX] {dev_name} {label} via {len(writable_uuids)} char(s): {hexs}")
        await asyncio.sleep(self.cmd_gap_ms / 1000.0)

    def _trace_contains(self, pattern: str) -> bool:
        if self.trace is None:
            return False
        rx = re.compile(pattern)
        return any(rx.search(line) for line in self.trace.snapshot())

    def _trace_snapshot_len(self) -> int:
        if self.trace is None:
            return 0
        return len(self.trace.snapshot())

    def _trace_slice_from(self, start: int) -> List[str]:
        if self.trace is None:
            return []
        return self.trace.snapshot()[start:]

    @staticmethod
    def _result(name: str, passed: bool, detail: str) -> TestCaseResult:
        return TestCaseResult(name, "PASS" if passed else "FAIL", detail)

    @staticmethod
    def _skipped(name: str, detail: str) -> TestCaseResult:
        return TestCaseResult(name, "SKIP", detail)

    async def run(self) -> int:
        results: List[TestCaseResult] = []
        if self.trace is not None:
            self.trace.start()
            await asyncio.sleep(self.settle_ms / 1000.0)
        else:
            print("[UART] Trace disabled (--no-trace)")

        try:
            await self.discover_and_connect()

            # 1) hit config permutations
            cfg_cases = [
                ("HIT_CONFIG button", 0x01,
                 r"\[MOLE_TEST\] RX HIT_CONFIG method=0x01"),
                ("HIT_CONFIG gsensor", 0x02,
                 r"\[MOLE_TEST\] RX HIT_CONFIG method=0x02"),
                ("HIT_CONFIG both", 0x03,
                 r"\[MOLE_TEST\] RX HIT_CONFIG method=0x03"),
                ("HIT_CONFIG none", 0x00,
                 r"\[MOLE_TEST\] RX HIT_CONFIG method=0x00"),
            ]
            for name, bits, trace_pat in cfg_cases:
                await self.send_all(self.build_hit_config_cmd(bits), name)
                if self.trace is None:
                    results.append(self._skipped(name, "trace disabled"))
                else:
                    ok = self._trace_contains(trace_pat)
                    results.append(self._result(
                        name, ok, "trace matched" if ok else "trace not found"))

            # 2) LED packets for all colors (index-compatible)
            for color_name in ["off", "red", "green", "blue", "yellow", "purple", "white"]:
                pkt = self.build_led_packet(
                    PATTERN_ROWS["CIRCLE"], color_name, True)
                await self.send_all(pkt, f"LED {color_name}")
                if self.trace is None:
                    results.append(self._skipped(
                        f"LED color {color_name}", "trace disabled"))
                else:
                    color_code = COLOR_MAP[color_name]
                    pat = rf"\[MOLE_TEST\] RX LED color={color_code} target=1"
                    ok = self._trace_contains(pat)
                    results.append(self._result(
                        f"LED color {color_name}", ok, "trace matched" if ok else "trace not found"))

            # 3) Brightness command edge values
            for value in [0, 1, 20, 50, 99, 100]:
                await self.send_all(self.build_brightness_cmd(value), f"BRIGHTNESS {value}")
                if self.trace is None:
                    results.append(self._skipped(
                        f"BRIGHTNESS_CMD {value}", "trace disabled"))
                else:
                    pat = rf"\[MOLE_TEST\] RX BRIGHTNESS_CMD value={value}"
                    ok = self._trace_contains(pat)
                    results.append(self._result(
                        f"BRIGHTNESS_CMD {value}", ok, "trace matched" if ok else "trace not found"))

            # 4) Invalid packets should not generate accepted trace
            if self.trace is None:
                await self.send_all(self.build_brightness_cmd(101, force_bad_checksum=False), "INVALID brightness=101")
                await self.send_all(self.build_hit_config_cmd(0x03, force_bad_checksum=True), "INVALID HIT_CONFIG bad checksum")
                results.append(self._skipped(
                    "INVALID brightness >100 rejected", "trace disabled"))
                results.append(self._skipped(
                    "INVALID hit_config checksum rejected", "trace disabled"))
            else:
                before = self._trace_snapshot_len()
                await self.send_all(self.build_brightness_cmd(101, force_bad_checksum=False), "INVALID brightness=101")
                await asyncio.sleep(0.2)
                after_lines = self._trace_slice_from(before)
                rejected_ok = not any(
                    "RX BRIGHTNESS_CMD value=101" in line for line in after_lines)
                results.append(self._result("INVALID brightness >100 rejected", rejected_ok,
                               "no accept trace" if rejected_ok else "unexpected accept trace"))

                before = self._trace_snapshot_len()
                await self.send_all(self.build_hit_config_cmd(0x03, force_bad_checksum=True), "INVALID HIT_CONFIG bad checksum")
                await asyncio.sleep(0.2)
                after_lines = self._trace_slice_from(before)
                rejected_ok = not any(
                    "RX HIT_CONFIG method=0x03" in line for line in after_lines)
                results.append(self._result("INVALID hit_config checksum rejected", rejected_ok,
                               "no accept trace" if rejected_ok else "unexpected accept trace"))

            # 5) Optional manual hit test
            if self.manual_hit_seconds > 0:
                print(
                    f"[ACTION] Press device key within {self.manual_hit_seconds}s to verify HIT report (0x01).")
                start = time.time()
                baseline = dict(self.notify_hits)
                while time.time() - start < self.manual_hit_seconds:
                    await asyncio.sleep(0.1)
                any_hit = False
                for dev_name, _ in self.clients:
                    if self.notify_hits.get(dev_name, 0) > baseline.get(dev_name, 0):
                        any_hit = True
                        break
                if self.trace is None:
                    detail = f"ble_hit={any_hit}, trace=disabled"
                    results.append(self._skipped(
                        "MANUAL key hit report", detail))
                else:
                    trace_hit = self._trace_contains(
                        r"\[MOLE_TEST\] TX HIT=0x01")
                    ok = any_hit and trace_hit
                    detail = f"ble_hit={any_hit}, trace_hit={trace_hit}"
                    results.append(self._result(
                        "MANUAL key hit report", ok, detail))

            passed = sum(1 for r in results if r.status == "PASS")
            failed = sum(1 for r in results if r.status == "FAIL")
            skipped = sum(1 for r in results if r.status == "SKIP")

            print("\n========== SELF TEST SUMMARY ==========")
            for r in results:
                print(f"[{r.status}] {r.name} :: {r.detail}")
            print(
                f"TOTAL={len(results)} PASS={passed} FAIL={failed} SKIP={skipped}")

            return 0 if failed == 0 else 1

        finally:
            await self.disconnect_all()
            if self.trace is not None:
                self.trace.stop()


def parse_args() -> argparse.Namespace:
    p = argparse.ArgumentParser(
        description="RL_SPORT Mole BLE self-test (with COM trace cross-check)")
    p.add_argument("--com", default="COM3",
                   help="UART debug port (default: COM3)")
    p.add_argument("--baud", type=int, default=115200,
                   help="UART baud (default: 115200)")
    p.add_argument("--count", type=int, default=1,
                   help="number of MOLE devices to connect")
    p.add_argument("--name-prefix", default="MOLE_",
                   help="BLE name prefix filter")
    p.add_argument("--scan-timeout", type=float, default=10.0,
                   help="BLE scan timeout (seconds)")
    p.add_argument("--cmd-gap-ms", type=int, default=180,
                   help="delay between commands")
    p.add_argument("--settle-ms", type=int, default=800,
                   help="startup settle delay before testing")
    p.add_argument("--manual-hit-seconds", type=int, default=6,
                   help="seconds waiting for manual key hit; 0 to skip")
    p.add_argument("--no-trace", action="store_true",
                   help="disable internal COM trace reader (use external watcher instead)")
    return p.parse_args()


def main() -> int:
    args = parse_args()
    tester = MoleBleSelfTester(
        com=None if args.no_trace else args.com,
        baud=args.baud,
        count=args.count,
        name_prefix=args.name_prefix,
        scan_timeout=args.scan_timeout,
        cmd_gap_ms=args.cmd_gap_ms,
        settle_ms=args.settle_ms,
        manual_hit_seconds=args.manual_hit_seconds,
    )
    return asyncio.run(tester.run())


if __name__ == "__main__":
    raise SystemExit(main())
