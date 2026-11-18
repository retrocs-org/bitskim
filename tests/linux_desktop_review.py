"""Exercise native GTK incoming decisions through the actual keyboard UI."""
import argparse
import json
import os
from pathlib import Path
import shlex
import select
import socket
import subprocess
import tempfile
import time


def free_port():
    with socket.socket() as connection:
        connection.bind(('127.0.0.1', 0))
        return connection.getsockname()[1]


def review(directory, screenshot=None):
    import gi
    gi.require_version('Atspi', '2.0')
    from gi.repository import Atspi
    Atspi.init()
    def accessible(predicate, parent=None, timeout=8):
        deadline = time.monotonic() + timeout
        while time.monotonic() < deadline:
            pending = [parent or Atspi.get_desktop(0)]
            inspected = 0
            while pending and inspected < 3000:
                node = pending.pop()
                inspected += 1
                try:
                    if predicate(node):
                        return node
                    pending.extend(node.get_child_at_index(i) for i in range(node.get_child_count()))
                except Exception:
                    continue  # Transient windows may disappear during an accessibility query.
            time.sleep(.1)
        raise RuntimeError('Expected accessible GTK control did not appear')
    def named(name, parent=None):
        return accessible(lambda node: node.get_name() == name and node.get_state_set().contains(Atspi.StateType.SHOWING), parent)
    def editable(parent):
        return accessible(lambda node: node.get_state_set().contains(Atspi.StateType.EDITABLE)
                          and node.get_state_set().contains(Atspi.StateType.SHOWING), parent)
    runner = shlex.split(os.environ.get('BITSKIM_LINUX_TEST_RUNNER', ''))
    processes = []
    with tempfile.TemporaryDirectory(prefix='bitskim-gtk-') as temporary:
        root = Path(temporary)
        received = root / 'received'
        received.mkdir()
        port = free_port()
        environment = dict(os.environ, GSETTINGS_BACKEND='memory', XDG_CONFIG_HOME=str(root/'session-config'), XDG_DATA_HOME=str(root/'session-data'))
        def command(binary, profile, listen, *arguments):
            return runner + [str(directory / binary), '--profile', str(root / profile),
                             '--port', str(listen), '--bind', '127.0.0.1',
                             '--no-discovery', *arguments]
        def wait_window(name):
            deadline = time.monotonic() + 12
            while time.monotonic() < deadline:
                result = subprocess.run(['xdotool', 'search', '--onlyvisible', '--name', name],
                                        capture_output=True, text=True)
                if result.returncode == 0 and result.stdout.strip():
                    return result.stdout.splitlines()[-1]
                if desktop.poll() is not None:
                    raise RuntimeError('Desktop exited: ' + desktop.stderr.read())
                time.sleep(.1)
            raise RuntimeError('Missing GTK window: ' + name)
        try:
            desktop = subprocess.Popen(command('bitskim', 'desktop', port, '--directory', str(received)),
                                       stdout=subprocess.PIPE, stderr=subprocess.PIPE, text=True, env=environment)
            processes.append(desktop)
            main = wait_window('^BitSkim 0.2$')
            for index, kind in enumerate(('file', 'folder', 'decline')):
                source = root / ('Möte.txt' if kind == 'file' else 'Tree' if kind == 'folder' else 'Declined.txt')
                if kind == 'folder':
                    (source / 'Empty').mkdir(parents=True)
                    (source / 'Nested').mkdir()
                    (source / 'Nested' / 'payload').write_bytes(b'native GTK folder')
                else:
                    source.write_bytes(b'native GTK receive approval\n' * 1000)
                sender = subprocess.Popen(command('bitskim-cli', 'sender-' + str(index), free_port(),
                                                  'send', '127.0.0.1:' + str(port), str(source),
                                                  '--security', 'require', '--json', '--timeout', '15'),
                                          stdout=subprocess.PIPE, stderr=subprocess.PIPE, text=True, env=environment)
                processes.append(sender)
                offer = wait_window('Incoming transfer')
                subprocess.run(['xdotool', 'windowfocus', '--sync', offer], check=True)
                time.sleep(.2)
                subprocess.run(['xdotool', 'key', '--clearmodifiers', 'alt+d' if kind == 'decline' else 'alt+a'], check=True)
                out, err = sender.communicate(timeout=20)
                if kind == 'decline':
                    assert sender.returncode == 1, out + err
                    assert not (received / source.name).exists()
                else:
                    assert sender.returncode == 0, out + err
                    if kind == 'file':
                        assert (received / source.name).read_bytes() == source.read_bytes()
                        result = json.loads(out.splitlines()[-1])
                        assert result['encrypted'] and result['verified'], out
                    else:
                        assert (received / 'Tree' / 'Empty').is_dir()
                        assert (received / 'Tree' / 'Nested' / 'payload').read_bytes() == b'native GTK folder'
            # Exercise the real file picker and explicit Send, through accessible native controls.
            outgoing = root / 'Desktop-send.txt'
            outgoing.write_bytes(b'GTK chosen outgoing payload' * 1000)
            cli_destination = root / 'cli-received'
            cli_destination.mkdir()
            cli_port = free_port()
            receiver = subprocess.Popen(command('bitskim-cli', 'outgoing-receiver', cli_port,
                                                'receive', '--accept', '--directory', str(cli_destination),
                                                '--once', '--timeout', '20', '--json'),
                                        stdout=subprocess.PIPE, stderr=subprocess.PIPE, text=True, env=environment)
            processes.append(receiver)
            ready = ''
            deadline = time.monotonic() + 10
            while 'Listening on TCP' not in ready and time.monotonic() < deadline:
                if select.select([receiver.stderr], [], [], .2)[0]:
                    ready += os.read(receiver.stderr.fileno(), 4096).decode()
            assert 'Listening on TCP' in ready, ready
            main_accessible = named('BitSkim 0.2')
            address = editable(main_accessible)
            assert address.set_text_contents('127.0.0.1:' + str(cli_port))
            assert named('Choose files…', main_accessible).do_action(0)
            chooser = wait_window('Choose files to send')
            chooser_accessible = named('Choose files to send')
            # Focus the dialog before requesting its standard Location entry.
            subprocess.run(['xdotool', 'windowfocus', '--sync', chooser], check=True)
            time.sleep(.3)
            subprocess.run(['xdotool', 'key', '--clearmodifiers', 'ctrl+l'], check=True)
            location = editable(chooser_accessible)
            assert location.set_text_contents(str(outgoing))
            assert named('Open', chooser_accessible).do_action(0)
            deadline = time.monotonic() + 3
            while time.monotonic() < deadline:
                visible = subprocess.run(['xdotool', 'search', '--onlyvisible', '--name', 'Choose files to send'], capture_output=True, text=True)
                if visible.returncode != 0:
                    break
                time.sleep(.1)
            if visible.returncode == 0:
                assert named('Open', chooser_accessible).do_action(0)
            send = named('Send selected items', main_accessible)
            accessible(lambda node: node == send and node.get_state_set().contains(Atspi.StateType.SENSITIVE), main_accessible)
            assert send.do_action(0)
            if screenshot:
                subprocess.run(['import', '-window', main, str(screenshot)], check=True)
            out, err = receiver.communicate(timeout=25)
            assert receiver.returncode == 0, out + err
            assert (cli_destination / outgoing.name).read_bytes() == outgoing.read_bytes()
            result = json.loads(out.splitlines()[-1])
            assert result['encrypted'] and result['verified'], out
            if screenshot:
                subprocess.run(['import', '-window', main, str(screenshot)], check=True)
            assert desktop.poll() is None, desktop.stderr.read()
            print('PASS GTK TLS file/folder acceptance, empty directory, safe decline, native picker/send and persistent window')
        finally:
            for process in processes:
                if process.poll() is None:
                    process.terminate()
            for process in processes:
                try:
                    process.communicate(timeout=3)
                except subprocess.TimeoutExpired:
                    process.kill()
                    process.communicate()
            if 'desktop' in locals() and desktop.stderr and not desktop.stderr.closed:
                error = desktop.stderr.read()
                assert 'CRITICAL' not in error, error


if __name__ == '__main__':
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('directory', type=Path)
    parser.add_argument('--screenshot', type=Path)
    arguments = parser.parse_args()
    review(arguments.directory, arguments.screenshot)
