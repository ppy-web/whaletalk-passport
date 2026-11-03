#!/usr/bin/env python3
"""Read-only host checks and bounded, non-resetting serial observation."""
import argparse
import getpass
import ipaddress
import json
import os
from pathlib import Path
import re
import time
import urllib.error
import urllib.parse
import urllib.request


def redact(text):
    text = re.sub(r'(?i)(Bearer\s+)\S+', r'\1[redacted]', text)
    return re.sub(r'(?i)(["\']?(?:accessToken|pairingCode|password|token|nonce)["\']?\s*[:=]\s*)("[^"\n]*"|\'[^\'\n]*\'|[^\s&,;]+)', r'\1[redacted]', text)


def analyze(text):
    playback = [dict(zip(('gaps', 'maxGapMs', 'maxWriteMs'), map(int, match))) for match in re.findall(
        r'playback stats gaps=(\d+) max_gap_ms=(\d+) max_write_ms=(\d+)', text)]
    drops = [int(value) for value in re.findall(r'capture_drops=(\d+)', text)]
    reasons = sorted(set(map(int, re.findall(r'wifi disconnected reason=(\d+)', text))))
    findings = []
    if 201 in reasons:
        findings.append('wifi-ap-not-found')
    if 'ESP_ERR_ESP_TLS_CONNECTION_TIMEOUT' in text:
        findings.append('tcp-connection-timeout')
    if any(re.search(rf'esp_ws_handshake_status_code={code}\b', text) for code in (401, 403)):
        findings.append('authentication-rejected')
    if '请检查 DSH 对话配置' in text or 'passport-session-unavailable' in text:
        findings.append('host-session-or-preset-unavailable')
    if 'playback queue empty' in text or any(item['gaps'] for item in playback):
        findings.append('playback-underflow-observed')
    if 'slow audio write' in text:
        findings.append('slow-speaker-write-observed')
    faults = [marker for marker in ('Guru Meditation', 'Stack canary', 'Task watchdog', 'panic') if marker.lower() in text.lower()]
    return {'findings': findings, 'wifiReasons': reasons, 'playback': playback, 'faultMarkers': faults,
            'captureDropCounters': drops, 'captureDropsAreUplink': True,
            'completePlaybackStatsObserved': bool(playback), 'shortPlaybackGapsMayBeUnreported': True}


def endpoint(base, suffix):
    value = urllib.parse.urlsplit(base)
    if value.scheme != 'http' or not value.hostname or value.username or value.password or value.query or value.fragment:
        raise ValueError('use an HTTP LAN base URL without credentials, query or fragment')
    if value.path not in ('', '/'):
        raise ValueError('use the base URL without an endpoint path')
    if value.hostname != 'localhost':
        address = ipaddress.ip_address(value.hostname)
        private = any(address in ipaddress.ip_network(net) for net in ('10.0.0.0/8', '172.16.0.0/12', '192.168.0.0/16', '127.0.0.0/8')) if address.version == 4 else address == ipaddress.ip_address('::1')
        if not private:
            raise ValueError('host checks are limited to localhost or private LAN addresses')
    return urllib.parse.urlunsplit((value.scheme, value.netloc, suffix, '', ''))


def request(url, code=None):
    headers = {'Authorization': f'Bearer {code}'} if code else {}
    # Local probes must not accidentally travel through a user/system HTTP proxy.
    opener = urllib.request.build_opener(urllib.request.ProxyHandler({}))
    try:
        with opener.open(urllib.request.Request(url, headers=headers), timeout=5) as response:
            payload = json.loads(response.read(65536))
            allowed = ('configured', 'listening', 'connected', 'servicesReady', 'recognitionReady', 'host', 'port', 'addresses', 'error', 'ok', 'adapter', 'protocol_version')
            return {'httpStatus': response.status, 'data': {key: payload[key] for key in allowed if key in payload}}
    except urllib.error.HTTPError as error:
        return {'httpStatus': error.code}
    except (OSError, ValueError, urllib.error.URLError):
        return {'httpStatus': 0, 'error': 'unreachable-or-invalid-response'}


def serial_ports():
    from serial.tools import list_ports
    return [{'port': port.device, 'description': port.description, 'vid': port.vid, 'pid': port.pid} for port in list_ports.comports()]


def capture(args):
    import serial
    if not any(port['port'].casefold() == args.port.casefold() for port in serial_ports()):
        raise ValueError('selected serial port was not found; enumerate ports first')
    output = Path(args.output)
    if output.exists():
        raise ValueError('output already exists; choose a new local log path')
    output.parent.mkdir(parents=True, exist_ok=True)
    with serial.Serial(port=None, baudrate=115200, timeout=0.2) as port:
        port.port = args.port
        port.dtr = False
        port.rts = False
        port.open()
        end = time.monotonic() + args.seconds
        pending = b''
        with output.open('x', encoding='utf-8') as log:
            while time.monotonic() < end:
                pending += port.read(4096)
                while b'\n' in pending:
                    line, pending = pending.split(b'\n', 1)
                    log.write(redact(line.decode('utf-8', errors='replace')) + '\n')
                log.flush()
            if pending:
                log.write(redact(pending.decode('utf-8', errors='replace')))
    return {'output': str(output), 'seconds': args.seconds, 'portClosed': True,
            'analysis': analyze(output.read_text(encoding='utf-8'))}


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    modes = parser.add_subparsers(dest='mode', required=True)
    modes.add_parser('ports')
    host = modes.add_parser('host')
    host.add_argument('--web-url', default='http://127.0.0.1:3080')
    host.add_argument('--bridge-url', default='http://127.0.0.1:3101')
    host.add_argument('--prompt-code', action='store_true')
    observe = modes.add_parser('serial')
    observe.add_argument('--port', required=True)
    observe.add_argument('--seconds', type=int, choices=range(5, 121), metavar='5..120', default=60)
    observe.add_argument('--output', required=True)
    logs = modes.add_parser('analyze')
    logs.add_argument('log')
    args = parser.parse_args()
    try:
        if args.mode == 'ports':
            result = {'ports': serial_ports()}
        elif args.mode == 'host':
            web_url = endpoint(args.web_url, '/plugins/xiaomi-mimo-tts/passport-status')
            bridge_url = endpoint(args.bridge_url, '/api/health')
            code = getpass.getpass('Current four-digit code (hidden): ') if args.prompt_code else os.environ.get('PASSPORT_PAIRING_CODE')
            if code and not re.fullmatch(r'\d{4}', code):
                raise ValueError('pairing code must contain exactly four digits')
            result = {'web': request(web_url), 'bridge': request(bridge_url, code) if code else {'skipped': 'no-pairing-code'},
                      'externalDeviceReachabilityVerified': False}
        elif args.mode == 'serial':
            result = capture(args)
        else:
            result = analyze(Path(args.log).read_text(encoding='utf-8', errors='replace'))
        print(json.dumps(result, ensure_ascii=False, indent=2))
        return 0
    except (OSError, ValueError, ImportError) as error:
        print(json.dumps({'error': redact(str(error)), 'type': type(error).__name__}, ensure_ascii=False))
        return 1


if __name__ == '__main__':
    raise SystemExit(main())
