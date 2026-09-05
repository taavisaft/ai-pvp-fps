"""Real dedicated-server regression; isolated localhost port, no graphics."""
import os
import socket
import struct
import subprocess
import sys
import time

HELLO = struct.Struct('<BHI')
INPUT = struct.Struct('<BIBffIIBBIBBb')
VERSION, WORLD = 4, 0x20260723


def check(ok, message):
    if not ok:
        raise RuntimeError(message)


def main():
    with socket.socket(socket.AF_INET, socket.SOCK_DGRAM) as reserve:
        reserve.bind(('127.0.0.1', 0))
        port = reserve.getsockname()[1]
    server = subprocess.Popen([sys.argv[1]], env={**os.environ, 'FPS_PORT': str(port),
                                               'FPS_MAP': 'lobby'},
                              stdout=subprocess.DEVNULL, stderr=subprocess.PIPE)
    client = socket.socket(socket.AF_INET, socket.SOCK_DGRAM)
    client.bind(('127.0.0.1', 0))
    client.settimeout(.1)
    address = ('127.0.0.1', port)
    sequence = 0
    player = 0
    latest = None

    def read_state():
        nonlocal latest
        try:
            data = client.recv(1500)
        except socket.timeout:
            return None
        if data[0] != 4 or len(data) < 586:
            return None
        offset = 25 + 35 * player
        latest = {'epoch': struct.unpack_from('<I', data, 7)[0], 'mode': data[11],
                  'mag': data[offset + 21], 'reserve': data[offset + 22],
                  'reload': data[offset + 23], 'shots': data[offset + 26],
                  'weapon': data[offset + 28]}
        return latest

    def wait(predicate, timeout=2):
        end = time.monotonic() + timeout
        while time.monotonic() < end:
            state = read_state()
            if state is not None and predicate(state):
                return state
        raise RuntimeError(f'State timeout; latest={latest}')

    def send(shots, epoch, weapon=0, mode=0, flags=0, seq=None):
        nonlocal sequence
        sequence += 1
        packet = INPUT.pack(3, sequence if seq is None else seq, 0, 0., 80.,
                            shots, epoch, mode, flags, 0, 0, weapon, 0)
        client.sendto(packet, address)

    def unchanged(mag, shots, seconds=.45):
        end = time.monotonic() + seconds
        seen = 0
        while time.monotonic() < end:
            state = read_state()
            if state:
                check(state['mag'] == mag and state['shots'] == shots,
                      f'Unexpected firing: {state}')
                seen += 1
        check(seen > 0, 'No state traffic')

    try:
        deadline = time.monotonic() + 5
        while time.monotonic() < deadline:
            check(server.poll() is None, 'Test server failed to start')
            client.sendto(HELLO.pack(1, VERSION, WORLD), address)
            try:
                data = client.recv(1500)
            except socket.timeout:
                continue
            if data[0] == 2:
                player = data[1]
                check(player < 16, 'Handshake rejected')
                break
        else:
            raise RuntimeError('No handshake')
        initial = wait(lambda s: s['epoch'] != 0)
        epoch = initial['epoch']
        send(20, epoch)  # previously emptied 20 rounds in less than 450 ms
        unchanged(32, 0)
        send(21, epoch)
        wait(lambda s: s['shots'] == 1)
        send(21, epoch)  # cumulative count repeated in a newer input packet
        send(22, epoch, seq=1)  # old input packet with a different shot count
        unchanged(31, 1, .2)
        send(24, epoch)  # bounded recovery of three lost shot events
        wait(lambda s: s['shots'] == 4)
        unchanged(28, 4, .15)
        send(24, epoch, weapon=1)
        switched = wait(lambda s: s['weapon'] == 1 and s['epoch'] != epoch)
        send(25, epoch, weapon=1)  # old-state shot arrives after swap
        unchanged(15, 4, .2)
        epoch = switched['epoch']
        send(1, epoch, weapon=1)
        wait(lambda s: s['mag'] == 14 and s['shots'] == 5)
        send(1, epoch, weapon=1, flags=2)
        wait(lambda s: s['reload'] == 1)
        finished = wait(lambda s: s['reload'] == 0 and s['mag'] == 15)
        check(finished['reserve'] == 44, 'Reload did not conserve ammunition')
        send(2, epoch, weapon=1)  # delayed pre-reload request cannot fire now
        unchanged(15, 5, .2)
        send(1, finished['epoch'], weapon=1, mode=2)  # invalid Glock auto mode
        unchanged(15, 5, .2)
        send(1, finished['epoch'], weapon=1)
        wait(lambda s: s['shots'] == 6 and s['mag'] == 14)
        with socket.socket(socket.AF_INET, socket.SOCK_DGRAM) as old:
            old.settimeout(1)
            old.sendto(HELLO.pack(1, VERSION - 1, WORLD), address)
            rejected = old.recv(1500)
            check(rejected[0:2] == bytes([2, 255]), 'Old protocol was accepted')
        print('UDP regression passed: backlog rejection, bounded recovery, duplicates, '
              'reordering, swap/reload epochs, ammo, mode and v3 rejection')
    finally:
        client.close()
        server.terminate()
        try:
            server.wait(timeout=3)
        except subprocess.TimeoutExpired:
            server.kill()
            server.wait()
        if server.stderr:
            server.stderr.close()


if __name__ == '__main__':
    main()
