#!/usr/bin/env python3
"""virtual-gamepad.py: a virtual Xbox 360 pad for StreamFlex's Linux hands-on check.

PURPOSE
    The qa-harness Ubuntu guest has no gamepad. This script makes one through the
    kernel's uinput module, so the hands-on check can press StreamFlex's gamepad
    controls (ButtonStart opens settings, ButtonA selects, ButtonB goes back, the
    D-pad moves). The device copies a wired Xbox 360 pad as the kernel's xpad driver
    presents it (USB, 045e:028e, the same button, stick, trigger and hat codes), so
    SDL's game-controller layer recognizes it: through SDL's mapping database entry
    for that pad, and through SDL's own Linux mapping for a pad with BTN_A and friends.

USAGE (as root)
    virtual-gamepad.py list
    virtual-gamepad.py press ButtonStart [--hold MS]
    virtual-gamepad.py sequence "ButtonDPadDown ButtonDPadDown ButtonA" [--gap MS] [--hold MS]
    virtual-gamepad.py axis LStickX -32768 [--hold MS] [--stay]
    virtual-gamepad.py serve --keep-open SECONDS [--control PATH]
    virtual-gamepad.py send "press ButtonStart --hold 2000" [--control PATH] [--no-wait]
    virtual-gamepad.py selftest

    Every command that makes a device also takes --keep-open SECONDS (keep the device
    that long after the action), --settle MS (wait after creating it, before the
    first event, so udev and SDL have seen it; default 1000) and --name TEXT.

    Names are StreamFlex's own labels (docs/configuration.md, "Gamepad Controls"):
      buttons  ButtonA ButtonB ButtonX ButtonY ButtonBack ButtonGuide ButtonStart
               ButtonLeftStick ButtonRightStick ButtonLeftShoulder ButtonRightShoulder
               ButtonDPadUp ButtonDPadDown ButtonDPadLeft ButtonDPadRight
      axes     LStickX LStickY RStickX RStickY (-32768..32767), LTrigger RTrigger (0..255)
    "press" also takes a stick direction or a trigger: LStickX- LStickX+ LStickY- LStickY+
    (and the RStick ones), LTrigger, RTrigger. Minus is left or up, as in StreamFlex.

    SDL may only look for pads at startup, and every run of press/sequence/axis makes a
    NEW device that disappears when the run ends. For a whole check, start one device
    that outlives StreamFlex and drive it with "send":
        virtual-gamepad.py serve --keep-open 1800 &      # before StreamFlex starts
        virtual-gamepad.py send "press ButtonStart"      # waits until the press is done
        virtual-gamepad.py send "quit"                   # or SIGTERM the serve process
    "serve" reads commands from a FIFO (default /run/virtual-gamepad.fifo) and appends
    "done <id>" to PATH.done after each one, which is what "send" waits for.

    The D-pad is the hat ABS_HAT0X/ABS_HAT0Y, as xpad reports it. A press shorter than a
    frame can be missed, because StreamFlex samples the pad once a frame, so the default
    hold is 150 ms. StreamFlex repeats a held control after 500 ms: use --hold 2000 to
    test a held button.

    The device is destroyed (UI_DEV_DESTROY) on exit, on SIGTERM, SIGINT and SIGHUP.
    Exit codes: 0 success, 1 a failed command, 2 /dev/uinput unusable, 3 no server.

REQUIREMENTS
    Linux with uinput (kernel 4.5 or later, for UI_DEV_SETUP): "modprobe uinput".
    Root, or write access to /dev/uinput. python3 standard library only.
    The ioctl encoding is the generic one (x86, arm, arm64, riscv); powerpc, mips,
    sparc and alpha use their own encoding, which is also handled.
"""

import argparse
import errno
import fcntl
import os
import select
import shlex
import signal
import stat
import struct
import sys
import time

# ---------------------------------------------------------------------------
# Kernel ABI: linux/input.h, linux/input-event-codes.h, linux/uinput.h
# ---------------------------------------------------------------------------

EV_SYN, EV_KEY, EV_ABS = 0x00, 0x01, 0x03
SYN_REPORT = 0
BUS_USB = 0x03

# struct input_event { struct timeval time; __u16 type; __u16 code; __s32 value; }
# The time is two C longs (__kernel_ulong_t in 32-bit time64 builds, the same size).
INPUT_EVENT = struct.Struct("@llHHi")
# struct input_id { __u16 bustype, vendor, product, version; }
# struct uinput_setup { struct input_id id; char name[UINPUT_MAX_NAME_SIZE]; __u32 ff_effects_max; }
UINPUT_MAX_NAME_SIZE = 80
UINPUT_SETUP = struct.Struct("@HHHH%dsI" % UINPUT_MAX_NAME_SIZE)
# struct input_absinfo { __s32 value, minimum, maximum, fuzz, flat, resolution; }
# struct uinput_abs_setup { __u16 code; /* 2 bytes of padding */ struct input_absinfo absinfo; }
UINPUT_ABS_SETUP = struct.Struct("@Hiiiiii")

_MACHINE = os.uname().machine if hasattr(os, "uname") else ""
if _MACHINE.startswith(("ppc", "powerpc", "mips", "sparc", "alpha")):
    _IOC_NONE, _IOC_WRITE, _IOC_READ, _IOC_SIZEBITS = 1, 4, 2, 13
else:
    _IOC_NONE, _IOC_WRITE, _IOC_READ, _IOC_SIZEBITS = 0, 1, 2, 14
_IOC_DIRSHIFT = 16 + _IOC_SIZEBITS


def _ioc(direction, kind, nr, size):
    return (direction << _IOC_DIRSHIFT) | (size << 16) | (ord(kind) << 8) | nr


UI_DEV_CREATE = _ioc(_IOC_NONE, "U", 1, 0)
UI_DEV_DESTROY = _ioc(_IOC_NONE, "U", 2, 0)
UI_DEV_SETUP = _ioc(_IOC_WRITE, "U", 3, UINPUT_SETUP.size)
UI_ABS_SETUP = _ioc(_IOC_WRITE, "U", 4, UINPUT_ABS_SETUP.size)
UI_SET_EVBIT = _ioc(_IOC_WRITE, "U", 100, 4)
UI_SET_KEYBIT = _ioc(_IOC_WRITE, "U", 101, 4)
UI_SET_ABSBIT = _ioc(_IOC_WRITE, "U", 103, 4)


def UI_GET_SYSNAME(length):
    return _ioc(_IOC_READ, "U", 44, length)


# ---------------------------------------------------------------------------
# The pad: a wired Xbox 360 controller as xpad presents it
# ---------------------------------------------------------------------------

VENDOR, PRODUCT, VERSION = 0x045E, 0x028E, 0x0114
DEVICE_NAME = "Microsoft X-Box 360 pad"

BTN_A, BTN_B, BTN_X, BTN_Y = 0x130, 0x131, 0x133, 0x134
BTN_TL, BTN_TR = 0x136, 0x137
BTN_SELECT, BTN_START, BTN_MODE = 0x13A, 0x13B, 0x13C
BTN_THUMBL, BTN_THUMBR = 0x13D, 0x13E

ABS_X, ABS_Y, ABS_Z, ABS_RX, ABS_RY, ABS_RZ = 0x00, 0x01, 0x02, 0x03, 0x04, 0x05
ABS_HAT0X, ABS_HAT0Y = 0x10, 0x11

# StreamFlex label -> key code
BUTTONS = {
    "ButtonA": BTN_A,
    "ButtonB": BTN_B,
    "ButtonX": BTN_X,
    "ButtonY": BTN_Y,
    "ButtonBack": BTN_SELECT,
    "ButtonGuide": BTN_MODE,
    "ButtonStart": BTN_START,
    "ButtonLeftStick": BTN_THUMBL,
    "ButtonRightStick": BTN_THUMBR,
    "ButtonLeftShoulder": BTN_TL,
    "ButtonRightShoulder": BTN_TR,
}

# StreamFlex label -> (hat axis, value while pressed)
DPAD = {
    "ButtonDPadUp": (ABS_HAT0Y, -1),
    "ButtonDPadDown": (ABS_HAT0Y, 1),
    "ButtonDPadLeft": (ABS_HAT0X, -1),
    "ButtonDPadRight": (ABS_HAT0X, 1),
}

# StreamFlex label -> axis code; ranges, fuzz and flat are xpad's
AXES = {
    "LStickX": ABS_X,
    "LStickY": ABS_Y,
    "LTrigger": ABS_Z,
    "RStickX": ABS_RX,
    "RStickY": ABS_RY,
    "RTrigger": ABS_RZ,
}
STICK_RANGE = (-32768, 32767, 16, 128)   # minimum, maximum, fuzz, flat
TRIGGER_RANGE = (0, 255, 0, 0)
HAT_RANGE = (-1, 1, 0, 0)
ABS_SETUP = [
    (ABS_X, STICK_RANGE), (ABS_Y, STICK_RANGE), (ABS_Z, TRIGGER_RANGE),
    (ABS_RX, STICK_RANGE), (ABS_RY, STICK_RANGE), (ABS_RZ, TRIGGER_RANGE),
    (ABS_HAT0X, HAT_RANGE), (ABS_HAT0Y, HAT_RANGE),
]
ABS_RANGES = {code: rng for code, rng in ABS_SETUP}

# SDL's mapping database entry for this pad on Linux (SDL_gamecontrollerdb.h). The
# self-test checks that SDL's evdev button and axis numbering lands on these indices.
SDL_DB_MAPPING = ("030000005e0400008e02000014010000,Xbox 360 Controller,a:b0,b:b1,back:b6,"
                  "dpdown:h0.4,dpleft:h0.8,dpright:h0.2,dpup:h0.1,guide:b8,leftshoulder:b4,"
                  "leftstick:b9,lefttrigger:a2,leftx:a0,lefty:a1,rightshoulder:b5,rightstick:b10,"
                  "righttrigger:a5,rightx:a3,righty:a4,start:b7,x:b2,y:b3,platform:Linux")
SDL_NAMES = {
    "ButtonA": "a", "ButtonB": "b", "ButtonX": "x", "ButtonY": "y", "ButtonBack": "back",
    "ButtonGuide": "guide", "ButtonStart": "start", "ButtonLeftStick": "leftstick",
    "ButtonRightStick": "rightstick", "ButtonLeftShoulder": "leftshoulder",
    "ButtonRightShoulder": "rightshoulder",
    "LStickX": "leftx", "LStickY": "lefty", "RStickX": "rightx", "RStickY": "righty",
    "LTrigger": "lefttrigger", "RTrigger": "righttrigger",
}

DEFAULT_HOLD_MS = 150
DEFAULT_GAP_MS = 500
DEFAULT_SETTLE_MS = 1000
DEFAULT_CONTROL = "/run/virtual-gamepad.fifo"


class UinputError(Exception):
    """/dev/uinput is missing, unwritable or refused the device."""


class Terminated(Exception):
    """A signal asked us to stop."""


class NoServer(Exception):
    """No serve process is reading the control FIFO."""


def log(message):
    sys.stdout.write("virtual-gamepad: %s\n" % message)
    sys.stdout.flush()


def pack_event(ev_type, code, value, sec=0, usec=0):
    return INPUT_EVENT.pack(sec, usec, ev_type, code, value)


def pack_setup(name, bustype=BUS_USB, vendor=VENDOR, product=PRODUCT, version=VERSION):
    raw = name.encode("utf-8")[:UINPUT_MAX_NAME_SIZE - 1]   # leave room for the NUL
    return UINPUT_SETUP.pack(bustype, vendor, product, version, raw, 0)


def pack_abs_setup(code, minimum, maximum, fuzz, flat, value=0, resolution=0):
    return UINPUT_ABS_SETUP.pack(code, value, minimum, maximum, fuzz, flat, resolution)


# ---------------------------------------------------------------------------
# The device
# ---------------------------------------------------------------------------

class VirtualPad:
    def __init__(self, name=DEVICE_NAME, path="/dev/uinput"):
        self.fd = None
        self.created = False
        self.node = None
        try:
            self.fd = os.open(path, os.O_WRONLY | os.O_NONBLOCK | getattr(os, "O_CLOEXEC", 0))
        except OSError as e:
            if e.errno == errno.ENOENT:
                raise UinputError(
                    "%s does not exist. Load the uinput module with 'modprobe uinput' "
                    "(as root), then run this script as root." % path)
            if e.errno in (errno.EACCES, errno.EPERM):
                raise UinputError(
                    "cannot open %s for writing: %s. Run this script as root (sudo), and "
                    "make sure the module is loaded ('modprobe uinput')." % (path, e.strerror))
            raise UinputError("cannot open %s: %s" % (path, e.strerror))
        try:
            fcntl.ioctl(self.fd, UI_SET_EVBIT, EV_SYN)
            fcntl.ioctl(self.fd, UI_SET_EVBIT, EV_KEY)
            fcntl.ioctl(self.fd, UI_SET_EVBIT, EV_ABS)
            for code in sorted(BUTTONS.values()):
                fcntl.ioctl(self.fd, UI_SET_KEYBIT, code)
            for code, (minimum, maximum, fuzz, flat) in ABS_SETUP:
                fcntl.ioctl(self.fd, UI_SET_ABSBIT, code)
                fcntl.ioctl(self.fd, UI_ABS_SETUP, pack_abs_setup(code, minimum, maximum, fuzz, flat))
            fcntl.ioctl(self.fd, UI_DEV_SETUP, pack_setup(name))
            fcntl.ioctl(self.fd, UI_DEV_CREATE)
        except OSError as e:
            os.close(self.fd)
            self.fd = None
            raise UinputError("uinput refused the device (%s). UI_DEV_SETUP needs kernel 4.5 "
                              "or later." % e.strerror)
        self.created = True
        self.node = self._find_node()
        log("created '%s' (%04x:%04x)%s" % (name, VENDOR, PRODUCT,
                                             " at " + self.node if self.node else ""))

    def _find_node(self):
        """The /dev/input/eventN the kernel made for us, for the log (best effort)."""
        try:
            buf = bytearray(64)
            fcntl.ioctl(self.fd, UI_GET_SYSNAME(len(buf)), buf, True)
            sysname = bytes(buf).split(b"\0", 1)[0].decode()
            base = "/sys/devices/virtual/input/%s" % sysname
            for _ in range(20):   # the evdev handler may take a moment to attach
                events = sorted(e for e in os.listdir(base) if e.startswith("event"))
                if events:
                    return "/dev/input/%s (%s)" % (events[0], sysname)
                time.sleep(0.05)
            log("WARNING: %s has no /dev/input/eventN, so SDL cannot see it. Is the evdev "
                "module loaded? ('modprobe evdev')" % sysname)
            return sysname
        except OSError:
            return None

    def _write(self, events):
        data = b"".join(pack_event(t, c, v) for t, c, v in events)
        data += pack_event(EV_SYN, SYN_REPORT, 0)
        os.write(self.fd, data)

    def button(self, code, down):
        self._write([(EV_KEY, code, 1 if down else 0)])

    def axis(self, code, value):
        self._write([(EV_ABS, code, value)])

    def release_all(self):
        events = [(EV_KEY, code, 0) for code in sorted(BUTTONS.values())]
        events += [(EV_ABS, code, 0) for code, _ in ABS_SETUP]
        self._write(events)

    def close(self):
        if self.fd is None:
            return
        if self.created:
            try:
                self.release_all()
                time.sleep(0.05)
            except OSError:
                pass
            try:
                fcntl.ioctl(self.fd, UI_DEV_DESTROY)
                log("destroyed the device")
            except OSError as e:
                log("UI_DEV_DESTROY failed: %s" % e.strerror)
            self.created = False
        os.close(self.fd)
        self.fd = None


# ---------------------------------------------------------------------------
# Actions
# ---------------------------------------------------------------------------

def control_of(name):
    """Resolve a press name to ('button', code) or ('abs', code, pressed, rest)."""
    if name in BUTTONS:
        return ("button", BUTTONS[name])
    if name in DPAD:
        code, value = DPAD[name]
        return ("abs", code, value, 0)
    if name in ("LTrigger", "RTrigger"):
        return ("abs", AXES[name], 255, 0)
    if name[:-1] in AXES and name[-1] in "+-" and "Trigger" not in name:
        code = AXES[name[:-1]]
        return ("abs", code, 32767 if name[-1] == "+" else -32768, 0)
    raise ValueError("unknown control '%s' (run 'list' for the names)" % name)


def sleep_ms(ms):
    time.sleep(max(ms, 0) / 1000.0)


def do_press(pad, name, hold_ms):
    control = control_of(name)
    if control[0] == "button":
        pad.button(control[1], True)
        sleep_ms(hold_ms)
        pad.button(control[1], False)
    else:
        _, code, pressed, rest = control
        pad.axis(code, pressed)
        sleep_ms(hold_ms)
        pad.axis(code, rest)
    log("pressed %s for %d ms" % (name, hold_ms))


def do_sequence(pad, names, hold_ms, gap_ms):
    for i, name in enumerate(names):
        if i:
            sleep_ms(gap_ms)
        do_press(pad, name, hold_ms)


def do_axis(pad, name, value, hold_ms, stay):
    if name not in AXES:
        raise ValueError("unknown axis '%s' (axes: %s)" % (name, " ".join(AXES)))
    code = AXES[name]
    minimum, maximum = ABS_RANGES[code][:2]
    if not minimum <= value <= maximum:
        raise ValueError("%s takes %d..%d, not %d" % (name, minimum, maximum, value))
    pad.axis(code, value)
    if stay:
        log("set %s to %d (left there)" % (name, value))
        return
    sleep_ms(hold_ms)
    pad.axis(code, 0)
    log("set %s to %d for %d ms" % (name, value, hold_ms))


def run_action(pad, args):
    if args.command == "press":
        for name in args.control:
            control_of(name)          # validate every name before pressing anything
        do_sequence(pad, args.control, args.hold, args.gap)
    elif args.command == "sequence":
        names = args.controls.split()
        for name in names:
            control_of(name)
        do_sequence(pad, names, args.hold, args.gap)
    elif args.command == "axis":
        do_axis(pad, args.axis, args.value, args.hold, args.stay)


# ---------------------------------------------------------------------------
# serve / send
# ---------------------------------------------------------------------------

def serve(pad, control, deadline, parser):
    done_path = control + ".done"
    try:
        if os.path.exists(control) and not stat.S_ISFIFO(os.stat(control).st_mode):
            raise UinputError("%s exists and is not a FIFO" % control)
        if not os.path.exists(control):
            os.mkfifo(control, 0o600)
        with open(done_path, "w"):
            pass
        # O_RDWR keeps a writer open ourselves, so the FIFO never reads end-of-file
        # between "send" runs.
        fd = os.open(control, os.O_RDWR | os.O_NONBLOCK)
    except OSError as e:
        raise UinputError("cannot make the control FIFO %s: %s" % (control, e.strerror))
    log("serving on %s until %s" % (control, time.strftime("%H:%M:%S", time.localtime(deadline))))
    pending = b""
    try:
        while True:
            remaining = deadline - time.time()
            if remaining <= 0:
                log("--keep-open time is up")
                return 0
            ready, _, _ = select.select([fd], [], [], min(remaining, 1.0))
            if not ready:
                continue
            try:
                pending += os.read(fd, 4096)
            except BlockingIOError:
                continue
            while b"\n" in pending:
                raw, pending = pending.split(b"\n", 1)
                line = raw.decode("utf-8", "replace").strip()
                if not line:
                    continue
                request_id, _, text = line.partition(" ")
                status = "ok"
                if text.strip() == "quit":
                    append_done(done_path, request_id, status)
                    log("quit requested")
                    return 0
                try:
                    run_action(pad, parser.parse_args(shlex.split(text)))
                except (ValueError, SystemExit) as e:
                    status = "error"
                    log("bad command '%s': %s" % (text, e))
                append_done(done_path, request_id, status)
    finally:
        os.close(fd)
        for path in (control, done_path):
            try:
                os.unlink(path)
            except OSError:
                pass


def append_done(done_path, request_id, status):
    with open(done_path, "a") as f:
        f.write("done %s %s\n" % (request_id, status))


def send(control, text, wait, timeout_s):
    request_id = "%d.%d" % (os.getpid(), int(time.time() * 1000))
    try:
        fd = os.open(control, os.O_WRONLY | os.O_NONBLOCK)
    except OSError as e:
        if e.errno in (errno.ENXIO, errno.ENOENT):
            raise NoServer("no 'serve' process is reading %s. Start one first: "
                           "virtual-gamepad.py serve --keep-open SECONDS &" % control)
        raise NoServer("cannot open %s: %s" % (control, e.strerror))
    try:
        os.write(fd, ("%s %s\n" % (request_id, text)).encode("utf-8"))
    finally:
        os.close(fd)
    if not wait:
        return 0
    done_path = control + ".done"
    end = time.time() + timeout_s
    while time.time() < end:
        try:
            with open(done_path) as f:
                for line in f:
                    parts = line.split()
                    if len(parts) == 3 and parts[1] == request_id:
                        log("sent '%s': %s" % (text, parts[2]))
                        return 0 if parts[2] == "ok" else 1
        except OSError:
            pass
        # The server removes the FIFO and the .done file as it exits, so a vanished
        # FIFO means "quit" was carried out, or the server stopped before our line.
        if not os.path.exists(control):
            if text.strip() == "quit":
                log("sent 'quit': the server has stopped")
                return 0
            log("sent '%s' but the server stopped before confirming it" % text)
            return 1
        time.sleep(0.05)
    log("sent '%s' but saw no 'done' within %d s" % (text, timeout_s))
    return 1


# ---------------------------------------------------------------------------
# Self-test: the packed layouts against the kernel's definitions, no uinput needed
# ---------------------------------------------------------------------------

def sdl_evdev_numbering():
    """Button and axis indices as SDL's Linux joystick driver assigns them.

    SDL numbers keys from BTN_JOYSTICK (0x120) up to KEY_MAX, then BTN_MISC (0x100) up
    to BTN_JOYSTICK; absolute axes in code order, skipping the hats, which become hat 0.
    """
    keys = sorted(BUTTONS.values())
    order = [k for k in keys if k >= 0x120] + [k for k in keys if 0x100 <= k < 0x120]
    buttons = {code: i for i, code in enumerate(order)}
    axes = [code for code, _ in ABS_SETUP if not ABS_HAT0X <= code <= 0x17]
    return buttons, {code: i for i, code in enumerate(sorted(axes))}


def selftest():
    failures = []

    def check(what, got, want):
        ok = got == want
        print("%-4s %-48s got %-12s want %s" % ("ok" if ok else "FAIL", what, got, want))
        if not ok:
            failures.append(what)

    long_size = struct.calcsize("@l")
    # linux/input.h: two longs, then u16, u16, s32
    check("sizeof(struct input_event)", INPUT_EVENT.size, 2 * long_size + 8)
    check("sizeof(struct uinput_setup)", UINPUT_SETUP.size, 8 + 80 + 4)
    check("sizeof(struct uinput_abs_setup)", UINPUT_ABS_SETUP.size, 2 + 2 + 24)
    check("offsetof(uinput_abs_setup, absinfo)", struct.calcsize("@Hi") - 4, 4)

    ev = pack_event(EV_KEY, BTN_START, 1, sec=7, usec=9)
    check("input_event type at 2*sizeof(long)",
          struct.unpack_from("@H", ev, 2 * long_size)[0], EV_KEY)
    check("input_event code", struct.unpack_from("@H", ev, 2 * long_size + 2)[0], BTN_START)
    check("input_event value", struct.unpack_from("@i", ev, 2 * long_size + 4)[0], 1)
    check("input_event negative value", INPUT_EVENT.unpack(pack_event(EV_ABS, ABS_X, -32768))[4],
          -32768)

    setup = pack_setup("x" * 200)
    check("uinput_setup id", struct.unpack_from("@HHHH", setup, 0),
          (BUS_USB, VENDOR, PRODUCT, VERSION))
    check("uinput_setup name is NUL-terminated", setup[8 + 79], 0)
    check("uinput_setup ff_effects_max at 88", struct.unpack_from("@I", setup, 88)[0], 0)
    check("uinput_setup name", pack_setup(DEVICE_NAME)[8:8 + len(DEVICE_NAME)],
          DEVICE_NAME.encode())

    abs_setup = pack_abs_setup(ABS_RZ, 0, 255, 0, 0)
    check("uinput_abs_setup code", struct.unpack_from("@H", abs_setup, 0)[0], ABS_RZ)
    check("uinput_abs_setup absinfo.maximum at 12", struct.unpack_from("@i", abs_setup, 12)[0], 255)

    if _IOC_SIZEBITS == 14:
        # The values <linux/uinput.h> gives on x86, arm, arm64 and riscv.
        check("UI_DEV_CREATE", hex(UI_DEV_CREATE), "0x5501")
        check("UI_DEV_DESTROY", hex(UI_DEV_DESTROY), "0x5502")
        check("UI_DEV_SETUP", hex(UI_DEV_SETUP), "0x405c5503")
        check("UI_ABS_SETUP", hex(UI_ABS_SETUP), "0x401c5504")
        check("UI_SET_EVBIT", hex(UI_SET_EVBIT), "0x40045564")
        check("UI_SET_KEYBIT", hex(UI_SET_KEYBIT), "0x40045565")
        check("UI_SET_ABSBIT", hex(UI_SET_ABSBIT), "0x40045567")
        check("UI_GET_SYSNAME(64)", hex(UI_GET_SYSNAME(64)), "0x8040552c")

    buttons, axes = sdl_evdev_numbering()
    fields = dict(f.split(":", 1) for f in SDL_DB_MAPPING.split(",")[2:] if ":" in f)
    for label, code in BUTTONS.items():
        check("SDL db %s" % label, "b%d" % buttons[code], fields[SDL_NAMES[label]])
    for label, code in AXES.items():
        check("SDL db %s" % label, "a%d" % axes[code], fields[SDL_NAMES[label]])
    for label, (code, value) in DPAD.items():
        bit = {(-1, ABS_HAT0Y): 1, (1, ABS_HAT0X): 2, (1, ABS_HAT0Y): 4, (-1, ABS_HAT0X): 8}
        want = fields["dp" + label[len("ButtonDPad"):].lower()]
        check("SDL db %s" % label, "h0.%d" % bit[(value, code)], want)

    for name in list(BUTTONS) + list(DPAD) + ["LStickX-", "LStickY+", "RStickX+", "LTrigger"]:
        control_of(name)
    for bad in ("ButtonC", "LTrigger+", "LStick", "Start"):
        try:
            control_of(bad)
            failures.append("accepted bad name %s" % bad)
            print("FAIL accepted bad name %s" % bad)
        except ValueError:
            print("ok   refuses bad name %s" % bad)

    print("selftest: %s" % ("PASS" if not failures else "FAIL (%d)" % len(failures)))
    return 0 if not failures else 1


# ---------------------------------------------------------------------------
# Command line
# ---------------------------------------------------------------------------

def build_parser():
    common = argparse.ArgumentParser(add_help=False)
    common.add_argument("--keep-open", type=float, default=0, metavar="SECONDS",
                        help="keep the device this long after the action (serve: how long to serve)")
    common.add_argument("--settle", type=int, default=DEFAULT_SETTLE_MS, metavar="MS",
                        help="wait after creating the device, before the first event (default %(default)s)")
    common.add_argument("--name", default=DEVICE_NAME, help="device name (default: %(default)s)")
    common.add_argument("--device", default="/dev/uinput", help=argparse.SUPPRESS)
    timing = argparse.ArgumentParser(add_help=False)
    timing.add_argument("--hold", type=int, default=DEFAULT_HOLD_MS, metavar="MS",
                        help="how long each control is held (default %(default)s)")
    timing.add_argument("--gap", type=int, default=DEFAULT_GAP_MS, metavar="MS",
                        help="pause between controls (default %(default)s)")

    parser = argparse.ArgumentParser(
        description="A virtual Xbox 360 pad through uinput, for StreamFlex's hands-on check.")
    sub = parser.add_subparsers(dest="command", required=True)
    p = sub.add_parser("press", parents=[common, timing], help="press one or more controls")
    p.add_argument("control", nargs="+", help="e.g. ButtonStart, ButtonDPadDown, LStickX-")
    p = sub.add_parser("sequence", parents=[common, timing], help="press controls in order")
    p.add_argument("controls", help='one quoted string, e.g. "ButtonDPadDown ButtonA"')
    p = sub.add_parser("axis", parents=[common, timing], help="set a stick or trigger")
    p.add_argument("axis", help="LStickX, LStickY, RStickX, RStickY, LTrigger or RTrigger")
    p.add_argument("value", type=int)
    p.add_argument("--stay", action="store_true", help="leave the axis there (no return to rest)")
    p = sub.add_parser("serve", parents=[common], help="keep one device and take commands from a FIFO")
    p.add_argument("--control", default=DEFAULT_CONTROL, help="the FIFO (default %(default)s)")
    p = sub.add_parser("send", help="send one command to a running 'serve'")
    p.add_argument("text", help='e.g. "press ButtonStart --hold 2000", or "quit"')
    p.add_argument("--control", default=DEFAULT_CONTROL, help="the FIFO (default %(default)s)")
    p.add_argument("--no-wait", action="store_true", help="return without waiting for 'done'")
    p.add_argument("--timeout", type=float, default=60, help="seconds to wait for 'done'")
    sub.add_parser("list", help="print the control names")
    sub.add_parser("selftest", help="check the packed layouts; needs no uinput")

    # The parser "serve" uses for each FIFO line: the actions only, with timing options.
    line = argparse.ArgumentParser(prog="serve-command", add_help=False)
    line_sub = line.add_subparsers(dest="command", required=True)
    p = line_sub.add_parser("press", parents=[timing], add_help=False)
    p.add_argument("control", nargs="+")
    p = line_sub.add_parser("sequence", parents=[timing], add_help=False)
    p.add_argument("controls")
    p = line_sub.add_parser("axis", parents=[timing], add_help=False)
    p.add_argument("axis")
    p.add_argument("value", type=int)
    p.add_argument("--stay", action="store_true")
    return parser, line


def main(argv=None):
    parser, line_parser = build_parser()
    args = parser.parse_args(argv)

    if args.command == "selftest":
        return selftest()
    if args.command == "list":
        print("buttons:", " ".join(list(BUTTONS) + list(DPAD)))
        print("axes:   ", " ".join(AXES), "(sticks -32768..32767, triggers 0..255)")
        print("press also takes: LStickX- LStickX+ LStickY- LStickY+ RStickX- RStickX+ "
              "RStickY- RStickY+ LTrigger RTrigger")
        return 0
    if args.command == "send":
        try:
            return send(args.control, args.text, not args.no_wait, args.timeout)
        except NoServer as e:
            sys.stderr.write("virtual-gamepad: %s\n" % e)
            return 3

    if args.command in ("press", "sequence"):
        try:
            for name in (args.control if args.command == "press" else args.controls.split()):
                control_of(name)
        except ValueError as e:
            sys.stderr.write("virtual-gamepad: %s\n" % e)
            return 1

    def on_signal(signum, _frame):
        raise Terminated(signal.Signals(signum).name)

    for signum in (signal.SIGTERM, signal.SIGINT, signal.SIGHUP):
        signal.signal(signum, on_signal)

    pad = None
    try:
        pad = VirtualPad(args.name, args.device)
        sleep_ms(args.settle)
        if args.command == "serve":
            return serve(pad, args.control, time.time() + (args.keep_open or 600), line_parser)
        run_action(pad, args)
        if args.keep_open > 0:
            log("keeping the device for %g s" % args.keep_open)
            time.sleep(args.keep_open)
        return 0
    except UinputError as e:
        sys.stderr.write("virtual-gamepad: %s\n" % e)
        return 2
    except ValueError as e:
        sys.stderr.write("virtual-gamepad: %s\n" % e)
        return 1
    except Terminated as e:
        log("stopped by %s" % e)
        return 0
    finally:
        if pad is not None:
            # A second signal during teardown must not skip UI_DEV_DESTROY.
            for signum in (signal.SIGTERM, signal.SIGINT, signal.SIGHUP):
                signal.signal(signum, signal.SIG_IGN)
            pad.close()


if __name__ == "__main__":
    sys.exit(main())
