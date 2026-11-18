"""Exercise native GTK incoming decisions through the actual keyboard UI."""
import argparse
import json
import os
from pathlib import Path
import shlex
import select
import socket
import subprocess
import sys
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
        environment = dict(os.environ, GSETTINGS_BACKEND='memory', GIO_USE_NOTIFICATION_BACKEND='freedesktop', XDG_CONFIG_HOME=str(root/'session-config'), XDG_DATA_HOME=str(root/'session-data'))
        applications = root/'session-data/applications'
        applications.mkdir(parents=True)
        desktop_source = Path(__file__).resolve().parents[1]/'clients/linux/org.bitskim.BitSkim.desktop'
        if desktop_source.is_file(): (applications/'org.bitskim.BitSkim.desktop').write_bytes(desktop_source.read_bytes())
        handler = root/'open-folder.py'
        handler.write_text('from pathlib import Path\nimport sys\nPath(sys.argv[1]).write_text(sys.argv[2])\n')
        (applications/'bitskim-folder-check.desktop').write_text('[Desktop Entry]\nType=Application\nName=Folder test handler\nExec=/usr/bin/python3 '+str(handler)+' '+str(root/'opened-folder')+' %u\nMimeType=inode/directory;\nNoDisplay=true\n')
        configuration = root/'session-config'; configuration.mkdir()
        (configuration/'mimeapps.list').write_text('[Default Applications]\ninode/directory=bitskim-folder-check.desktop;\n')
        server = subprocess.Popen([sys.executable, str(Path(__file__).with_name('linux_notification_server.py')), str(root)], stdout=subprocess.PIPE, stderr=subprocess.PIPE, text=True)
        processes.append(server)
        deadline = time.monotonic()+5
        while not (root/'notifications-ready').exists() and time.monotonic()<deadline: time.sleep(.05)
        assert (root/'notifications-ready').exists(), 'Private notification service did not start'
        notification_count = 0
        def notification(title):
            nonlocal notification_count
            deadline = time.monotonic()+6
            while time.monotonic()<deadline:
                try: records=json.loads((root/'notifications.json').read_text())
                except (FileNotFoundError,json.JSONDecodeError): records=[]
                if len(records)>notification_count:
                    assert len(records)==notification_count+1, records
                    notification_count=len(records)
                    assert records[-1]['title']==title, records[-1]
                    return records[-1]
                time.sleep(.05)
            raise RuntimeError('Missing notification: '+title)

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
        def choose_outgoing(path):
            assert named('Choose files…', main_accessible).do_action(0)
            chooser = wait_window('Choose files to send')
            chooser_accessible = named('Choose files to send')
            # Focus the dialog before requesting its standard Location entry.
            subprocess.run(['xdotool', 'windowfocus', '--sync', chooser], check=True)
            time.sleep(.3)
            subprocess.run(['xdotool', 'key', '--clearmodifiers', 'ctrl+l'], check=True)
            location = editable(chooser_accessible)
            assert location.set_text_contents(str(path))
            opening=named('Open',chooser_accessible)
            accessible(lambda node: node==opening and node.get_state_set().contains(Atspi.StateType.SENSITIVE),chooser_accessible)
            assert opening.do_action(0)
            deadline = time.monotonic() + 3
            while time.monotonic() < deadline:
                visible = subprocess.run(['xdotool', 'search', '--onlyvisible', '--name', 'Choose files to send'], capture_output=True, text=True)
                if visible.returncode != 0:
                    break
                time.sleep(.1)
            if visible.returncode == 0:
                assert named('Open', chooser_accessible).do_action(0)
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
                decision=named('Decline' if kind=='decline' else 'Accept collection' if kind=='folder' else 'Accept',named('Incoming transfer — BitSkim'))
                assert decision.do_action(0)
                out, err = sender.communicate(timeout=20)
                if kind == 'decline':
                    assert sender.returncode == 1, out + err
                    assert not (received / source.name).exists()
                else:
                    assert sender.returncode == 0, kind+' '+out+err+(root/'desktop/config/settings.ini').read_text()
                    if kind == 'file':
                        assert (received / source.name).read_bytes() == source.read_bytes()
                        result = json.loads(out.splitlines()[-1])
                        assert result['encrypted'] and result['verified'], out
                    else:
                        assert (received / 'Tree' / 'Empty').is_dir()
                        assert (received / 'Tree' / 'Nested' / 'payload').read_bytes() == b'native GTK folder'
                notice = notification('File received' if kind=='file' else 'Transfer complete' if kind=='folder' else 'Transfer declined')
                assert str(root) not in notice['body'], 'Private path leaked into result notification'
                if kind=='file':
                    actions=notice['actions']; index=actions.index('Open folder'); action=actions[index-1]
                    subprocess.run(['gdbus','call','--session','--dest','org.freedesktop.Notifications','--object-path','/org/freedesktop/Notifications','--method','org.bitskim.NotificationChecks.Invoke',str(notice['id']),action],check=True,capture_output=True)
                    deadline=time.monotonic()+6
                    while not (root/'opened-folder').exists() and time.monotonic()<deadline:time.sleep(.05)
                    opened=(root/'opened-folder').read_text(); assert opened in (received.as_uri(),str(received)), 'Unexpected folder action target: '+repr(opened)
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
            choose_outgoing(outgoing)
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
            notification('File sent')
            assert address.set_text_contents('127.0.0.1:'+str(free_port()))
            choose_outgoing(outgoing)
            send=named('Send selected items',main_accessible)
            accessible(lambda node: node==send and node.get_state_set().contains(Atspi.StateType.SENSITIVE),main_accessible)
            assert send.do_action(0)
            notice=notification('Transfer failed')
            assert '1 failed' in notice['body'] and 'written files verified' not in notice['body'], notice
            assert (root/'desktop/state/transfers.dat').is_file()
            # Restart with this profile's saved opt-out; no old or new result is replayed.
            desktop.terminate(); desktop.communicate(timeout=5)
            saved=root/'desktop/config/settings.ini'
            saved.write_text(saved.read_text().replace('Notifications=1','Notifications=0'))
            desktop=subprocess.Popen(command('bitskim','desktop',port,'--directory',str(received)),stdout=subprocess.PIPE,stderr=subprocess.PIPE,text=True,env=environment)
            processes.append(desktop); main=wait_window('^BitSkim 0.2$')
            muted=root/'Muted.txt';muted.write_bytes(b'saved opt-out')
            sender=subprocess.Popen(command('bitskim-cli','muted',free_port(),'send','127.0.0.1:'+str(port),str(muted),'--security','require','--timeout','15'),stdout=subprocess.PIPE,stderr=subprocess.PIPE,text=True,env=environment)
            processes.append(sender);wait_window('Incoming transfer')
            assert named('Accept',named('Incoming transfer — BitSkim')).do_action(0)
            out,err=sender.communicate(timeout=20);assert sender.returncode==0,out+err
            assert (received/muted.name).read_bytes()==muted.read_bytes()
            time.sleep(1)
            assert len(json.loads((root/'notifications.json').read_text()))==notification_count, 'Saved opt-out still emitted a notification'
            assert 'Notifications=0' in saved.read_text()


            if screenshot:
                subprocess.run(['import', '-window', main, str(screenshot)], check=True)
            assert desktop.poll() is None, desktop.stderr.read()
            print('PASS GTK TLS file/folder acceptance, empty directory, safe decline, native picker/send, grouped notifications, saved opt-out, failure alert and Open folder action')
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
