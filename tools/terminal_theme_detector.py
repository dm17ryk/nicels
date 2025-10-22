#!/usr/bin/env python3
# -*- coding: utf-8 -*-

"""
Terminal theme detector (Linux + Windows).

Strategy (best → fallback):
1) Query the terminal for its actual default background via OSC 11 (xterm/VT "dynamic colors").
2) Infer from COLORFGBG if present.
3) Terminal-specific hints:
    - Windows Terminal: read the active profile's color scheme from settings.json (if WT_PROFILE_ID is set).
    - Classic Windows Console: read DefaultBackground/ColorTableXX from HKCU/Console.
    - GNOME Terminal: read the active profile colors from gsettings/dconf.
    - (Konsole, kitty, etc. typically answer OSC 10/11; no special handling is required in most cases.)
4) OS preference:
    - Linux: xdg-desktop-portal Settings "org.freedesktop.appearance color-scheme" or GNOME's color-scheme.
    - Windows: AppsUseLightTheme registry value.

Returns "dark" or "light".
"""


from __future__ import annotations

import contextlib
import json
import os
import pathlib
import re
import subprocess
import sys
import tty
from dataclasses import dataclass
from typing import List, Optional, Tuple

# ---------------------------------------------------------------------------
# Helpers
# ---------------------------------------------------------------------------


def _is_windows() -> bool:
    return os.name == "nt"


def _clamp01(x: float) -> float:
    return max(0.0, min(1.0, x))


def _luma(rgb: Tuple[int, int, int]) -> float:
    """Compute relative luma (Rec. 709)."""
    r, g, b = rgb
    return 0.2126 * (r / 255.0) + 0.7152 * (g / 255.0) + 0.0722 * (b / 255.0)


def _rgb_from_hexlike(s: str) -> Optional[Tuple[int, int, int]]:
    """Parse 'rgb:aa/bb/cc' or '#aabbcc' forms into (r,g,b)."""
    s = s.strip()
    if "rgb:" in s:
        try:
            part = s.split("rgb:", 1)[1]
            # trim trailing ST/BEL or other control bytes
            part = re.split(r"[\x07\x1b]", part)[0]
            a, b, c, *_ = part.split("/")
            return int(a, 16), int(b, 16), int(c, 16)
        except Exception:
            return None
    if "#" in s:
        try:
            part = s.split("#", 1)[1]
            part = re.split(r"[\x07\x1b]", part)[0]
            part = part[:6]
            return int(part[:2], 16), int(part[2:4], 16), int(part[4:6], 16)
        except Exception:
            return None
    return None


def _mode_from_rgb(rgb: Tuple[int, int, int]) -> str:
    """Map an RGB background to 'dark' or 'light' using luma threshold."""
    return "dark" if _luma(rgb) < 0.5 else "light"


# ---------------------------------------------------------------------------
# OSC (xterm) dynamic-colors query: ask terminal for default background
# ---------------------------------------------------------------------------
def _osc_query_background_rgb(
    timeout: float = 1.08,
) -> Optional[Tuple[int, int, int]]:
    """
    Queries the controlling terminal for its background color using OSC 11.

    This function safely places the terminal in cbreak mode to read the
    response directly, and includes a timeout to prevent hanging.

    Args:
        timeout: Time in seconds to wait for a response.

    Returns:
        A tuple of (r, g, b) integers (0-255) if successful, otherwise None.
    """

    if _is_windows():
        return None

    import select
    import termios

    # /dev/tty is the special device file for the controlling terminal.
    # It must be used instead of stdin in case stdin is redirected.
    try:
        fd = os.open("/dev/tty", os.O_RDWR | os.O_NOCTTY)
    except OSError:
        # This can happen in environments without a controlling tty, like cron jobs.
        return None

    # Save the original terminal attributes to restore them later.
    # This is a critical step to ensure the terminal is not left in a broken state.
    old_settings = termios.tcgetattr(fd)

    try:
        # Place the terminal into cbreak mode. This disables canonical mode
        # (line buffering) and echo, allowing us to read the response
        # byte-by-byte as it arrives. Signal handling (Ctrl-C) remains enabled.
        tty.setcbreak(fd)

        # The OSC 11 query sequence for background color.
        # \x1b] is OSC, 11 is the parameter for background color,? requests the value.
        # \x1b\\ is the String Terminator (ST).
        query_sequence = b"\x1b]11;?\x1b\\"
        os.write(fd, query_sequence)

        # Ensure the query is sent before we start waiting for a response.
        termios.tcdrain(fd)

        # Use select() to wait for the file descriptor to become readable,
        # with a timeout. This is crucial to prevent the program from hanging
        # if the terminal does not respond.
        readable, _, _ = select.select([fd], [], [], timeout)

        if not readable:
            return None  # Timeout occurred

        response = b""
        # Read the response byte by byte until we find a terminator.
        # We look for ST (\x1b\\) but also handle BEL (\x07) as some terminals use it.
        while True:
            char = os.read(fd, 1)
            if not char:
                # Should not happen if select() indicated readability, but good practice.
                break
            response += char
            # Check for String Terminator
            if response.endswith(b"\x1b\\"):
                break
            # Check for Bell terminator
            if response.endswith(b"\x07"):
                break
            # Failsafe to prevent infinite loop on malformed response
            if len(response) > 100:
                return None

        # Parse the response string. Expected format: \x1b]11;rgb:RRRR/GGGG/BBBB\x1b\\
        match = re.search(
            rb"rgb:([0-9a-fA-F]{2,4})/([0-9a-fA-F]{2,4})/([0-9a-fA-F]{2,4})", response
        )
        if not match:
            return None

        # The values are 16-bit (4 hex digits). Scale them to 8-bit (0-255).
        # A 4-digit hex value 'hhhh' is converted by taking the most significant byte 'hh'.
        # A 2-digit hex value 'hh' is used as is.
        r_hex, g_hex, b_hex = match.groups()
        r = int(r_hex[:2], 16)
        g = int(g_hex[:2], 16)
        b = int(b_hex[:2], 16)

        return (r, g, b)

    finally:
        # CRITICAL: Restore the original terminal settings. This happens
        # even if an exception occurs in the try block.
        termios.tcsetattr(fd, termios.TCSADRAIN, old_settings)
        os.close(fd)


# ---------------------------------------------------------------------------
# COLORFGBG heuristic
# ---------------------------------------------------------------------------


def _rgb_from_colorfgbg(env_val: str) -> Optional[Tuple[int, int, int]]:
    """
    Interpret COLORFGBG 'fg;bg' 16-color indexes.

    This is a rough heuristic: map bg index to a plausible RGB.
    """
    with contextlib.suppress(Exception):
        parts = [p for p in env_val.strip().split(";") if p != ""]
        if not parts:
            return None
        bg_idx = int(parts[-1], 10)
        # crude palette mapping (xterm-ish defaults)
        table = [
            (0, 0, 0),  # 0  black
            (205, 0, 0),  # 1  red
            (0, 205, 0),  # 2  green
            (205, 205, 0),  # 3  yellow
            (0, 0, 238),  # 4  blue
            (205, 0, 205),  # 5  magenta
            (0, 205, 205),  # 6  cyan
            (229, 229, 229),  # 7  light gray
            (127, 127, 127),  # 8  dark gray
            (255, 0, 0),  # 9  bright red
            (0, 255, 0),  # 10 bright green
            (255, 255, 0),  # 11 bright yellow
            (92, 92, 255),  # 12 bright blue
            (255, 0, 255),  # 13 bright magenta
            (0, 255, 255),  # 14 bright cyan
            (255, 255, 255),  # 15 white
        ]
        if 0 <= bg_idx < len(table):
            return table[bg_idx]
    return None


# ---------------------------------------------------------------------------
# Windows-specific readers
# ---------------------------------------------------------------------------


def _windows_terminal_settings_paths() -> List[pathlib.Path]:
    """
    Candidate paths for Windows Terminal settings.json (stable/preview/unpackaged/dev).
    """
    candidates: List[pathlib.Path] = []
    if local := os.environ.get("LOCALAPPDATA"):
        candidates.extend(
            [
                pathlib.Path(
                    local,
                    "Packages",
                    "Microsoft.WindowsTerminal_8wekyb3d8bbwe",
                    "LocalState",
                    "settings.json",
                ),
                pathlib.Path(
                    local,
                    "Packages",
                    "Microsoft.WindowsTerminalPreview_8wekyb3d8bbwe",
                    "LocalState",
                    "settings.json",
                ),
                pathlib.Path(
                    local,
                    "Packages",
                    "WindowsTerminalDev_6q6wn7rc29ae4",
                    "LocalState",
                    "settings.json",
                ),  # dev package
                pathlib.Path(
                    local, "Microsoft", "Windows Terminal", "settings.json"
                ),  # unpackaged (Scoop/Chocolatey)
            ]
        )
    return candidates


def _read_json(path: pathlib.Path) -> Optional[dict]:
    try:
        text = path.read_text(encoding="utf-8")
        return json.loads(text)
    except Exception:
        return None


def _winreg_get_dword(root, subkey: str, name: str) -> Optional[int]:
    try:
        import winreg

        with winreg.OpenKey(root, subkey) as k:
            val, typ = winreg.QueryValueEx(k, name)
            if typ == winreg.REG_DWORD:
                return int(val)
    except Exception:
        return None
    return None


def _windows_os_preference() -> Optional[str]:
    """
    Read AppsUseLightTheme (user) → 'light' if 1, 'dark' if 0.
    """
    if not _is_windows():
        return None
    try:
        import winreg

        val = _winreg_get_dword(
            winreg.HKEY_CURRENT_USER,
            r"Software\Microsoft\Windows\CurrentVersion\Themes\Personalize",
            "AppsUseLightTheme",
        )
        if val is None:
            return None
        return "light" if val != 0 else "dark"
    except Exception:
        return None


def _windows_console_default_bg_rgb() -> Optional[Tuple[int, int, int]]:
    """
    Try to infer the classic Console (conhost) default background from registry:
    HKCU/Console/DefaultBackground (index) + ColorTableXX (RGB).
    """
    if not _is_windows():
        return None
    with contextlib.suppress(Exception):
        import winreg

        # DefaultBackground is an index into ColorTableXX. If absent, assume 0.
        idx = _winreg_get_dword(
            winreg.HKEY_CURRENT_USER, r"Console", "DefaultBackground"
        )
        if idx is None:
            idx = 0
        if 0 <= idx <= 15:
            return _extracted_from__windows_console_default_bg_rgb_(idx, winreg)
    return None


# TODO Rename this here and in `_windows_console_default_bg_rgb`
def _extracted_from__windows_console_default_bg_rgb_(idx, winreg):
    name = f"ColorTable{idx:02d}"
    val = _winreg_get_dword(winreg.HKEY_CURRENT_USER, r"Console", name)
    if val is None:
        return None
    # DWORD is 0x00BBGGRR
    r = val & 0xFF
    g = (val >> 8) & 0xFF
    b = (val >> 16) & 0xFF
    return (r, g, b)


def _windows_terminal_profile_bg_rgb() -> Optional[Tuple[int, int, int]]:
    """
    If running inside Windows Terminal (WT_PROFILE_ID set), read settings.json,
    find the matching profile, and get its background color (inline or via scheme).
    """
    if not _is_windows():
        return None
    pid = os.environ.get("WT_PROFILE_ID")
    if not pid:
        return None

    for path in _windows_terminal_settings_paths():
        if not path.exists():
            continue
        data = _read_json(path)
        if not data:
            continue

        # Profiles can be in data["profiles"]["list"]; defaults in data["profiles"]["defaults"].
        profiles = []
        with contextlib.suppress(Exception):
            profiles = data.get("profiles", {}).get("list", []) or []
        target = next(
            (p for p in profiles if str(p.get("guid", "")).lower() == str(pid).lower()),
            None,
        )
        if target is None:
            # Fallback to defaults section if explicit profile isn't found
            target = data.get("profiles", {}).get("defaults", {}) or None
            if not target:
                continue

        # Inline overrides take precedence
        if "background" in target:
            bg = target["background"]
            if rgb := _rgb_from_hexlike(str(bg)):
                return rgb

        if scheme_name := target.get("colorScheme"):
            schemes = data.get("schemes", []) or []
            for sc in schemes:
                if sc.get("name") == scheme_name and "background" in sc:
                    if rgb := _rgb_from_hexlike(str(sc["background"])):
                        return rgb

    return None


# ---------------------------------------------------------------------------
# Linux desktop preference readers
# ---------------------------------------------------------------------------


def _linux_portal_preference() -> Optional[str]:
    """
    Query org.freedesktop.portal.Settings → ('dark'/'light') if available.
    """
    try:
        cmd = [
            "gdbus",
            "call",
            "--session",
            "--dest",
            "org.freedesktop.portal.Desktop",
            "--object-path",
            "/org/freedesktop/portal/desktop",
            "--method",
            "org.freedesktop.portal.Settings.Read",
            "org.freedesktop.appearance",
            "color-scheme",
        ]
        out = subprocess.check_output(
            cmd, stderr=subprocess.DEVNULL, text=True, timeout=0.25
        )
        # Typical reply: "(<'u', 1>,)"
        m = re.search(r"\b(\d)\b", out)
        if not m:
            return None
        code = int(m[1])
        if code == 1:
            return "dark"
        return "light" if code == 2 else None
    except Exception:
        return None


def _gnome_desktop_preference() -> Optional[str]:
    """
    gsettings get org.gnome.desktop.interface color-scheme → 'prefer-dark'/'default'/'prefer-light'
    """
    try:
        out = subprocess.check_output(
            ["gsettings", "get", "org.gnome.desktop.interface", "color-scheme"],
            stderr=subprocess.DEVNULL,
            text=True,
            timeout=0.2,
        ).strip()
        if "prefer-dark" in out:
            return "dark"
        return "light" if "prefer-light" in out else None
    except Exception:
        return None


# ---------------------------------------------------------------------------
# GNOME Terminal profile colors (when not using theme colors)
# ---------------------------------------------------------------------------


def _gnome_terminal_bg_rgb() -> Optional[Tuple[int, int, int]]:
    """
    Read active GNOME Terminal profile and return background-color if set.
    """
    try:
        # Get default profile UUID
        out = subprocess.check_output(
            ["gsettings", "get", "org.gnome.Terminal.ProfilesList", "default"],
            stderr=subprocess.DEVNULL,
            text=True,
            timeout=0.2,
        ).strip()
        uuid = out.strip("'").strip()
        if not uuid or uuid == "":  # defensive
            return None

        base = f"org.gnome.Terminal.Legacy.Profile:/org/gnome/terminal/legacy/profiles:/:{uuid}/"

        # If 'use-theme-colors' is true, the terminal inherits desktop theme
        use_theme = subprocess.check_output(
            ["gsettings", "get", base, "use-theme-colors"],
            stderr=subprocess.DEVNULL,
            text=True,
            timeout=0.2,
        ).strip()
        if use_theme.lower() == "true":
            return None  # prefer OSC or desktop preference

        bg = subprocess.check_output(
            ["gsettings", "get", base, "background-color"],
            stderr=subprocess.DEVNULL,
            text=True,
            timeout=0.2,
        ).strip()
        # value looks like "'rgb(0,43,54)'" or '#002b36'
        return _rgb_from_hexlike(bg.strip("'"))
    except Exception:
        return None


# ---------------------------------------------------------------------------
# Main detector
# ---------------------------------------------------------------------------


@dataclass
class DetectionResult:
    mode: str  # "dark" or "light"
    source: str  # textual description of which method succeeded
    rgb: Optional[Tuple[int, int, int]] = None


class TerminalThemeDetector:
    """
    A small, dependency-free detector that chooses 'dark' or 'light'
    using several resilient methods, in this order:

      OSC → COLORFGBG → Terminal-specific (WT/Console/GNOME Terminal) → OS preference.
    """

    def __init__(self) -> None:
        self._cached: Optional[DetectionResult] = None

    # Public property as requested
    @property
    def terminal_theme(self) -> str:
        """
        Compute and cache the terminal theme: "dark" or "light".
        """
        if self._cached is None:
            self._cached = self._detect()
        return self._cached.mode

    # Optional: expose debugging info
    @property
    def details(self) -> DetectionResult:
        if self._cached is None:
            self._cached = self._detect()
        return self._cached

    # ---- detection pipeline ----

    def _detect(self) -> DetectionResult:
        if rgb := None if _is_windows() else _osc_query_background_rgb():
            return DetectionResult(_mode_from_rgb(rgb), "osc-11", rgb)

        if cfb := os.environ.get("COLORFGBG"):
            if rgb := _rgb_from_colorfgbg(cfb):
                return DetectionResult(_mode_from_rgb(rgb), "COLORFGBG", rgb)

        # 3) Terminal-specific sources
        if _is_windows():
            if rgb := _windows_terminal_profile_bg_rgb():
                return DetectionResult(
                    _mode_from_rgb(rgb), "windows-terminal-settings", rgb
                )

            if rgb := _windows_console_default_bg_rgb():
                return DetectionResult(_mode_from_rgb(rgb), "winconsole-registry", rgb)
        elif rgb := _gnome_terminal_bg_rgb():
            return DetectionResult(_mode_from_rgb(rgb), "gnome-terminal-profile", rgb)

        # 4) OS preference fallbacks
        if _is_windows():
            if pref := _windows_os_preference():
                return DetectionResult(pref, "windows-apps-theme", None)
        else:
            if pref := _linux_portal_preference():
                return DetectionResult(pref, "xdg-portal-color-scheme", None)
            if pref := _gnome_desktop_preference():
                return DetectionResult(pref, "gnome-color-scheme", None)

        # Final fallback: assume dark (safer for most palettes)
        return DetectionResult("dark", "default", None)


# ---------------------------------------------------------------------------
# CLI for quick manual testing
# ---------------------------------------------------------------------------


def main(argv: List[str]) -> int:
    det = TerminalThemeDetector()
    res = det.details
    rgb_str = f"#{res.rgb[0]:02x}{res.rgb[1]:02x}{res.rgb[2]:02x}" if res.rgb else "n/a"
    print(f"theme={res.mode} source={res.source} bg={rgb_str}")
    return 0


if __name__ == "__main__":
    raise SystemExit(main(sys.argv))
