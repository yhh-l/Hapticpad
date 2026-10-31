"""Windows bridge for the keyboard's portable memory; standard library only."""
import argparse
import contextlib
import datetime
import hashlib
import json
import os
from pathlib import Path
import subprocess
import sys
import time
import uuid
import zlib

FILES = ('identity.json', 'AGENTS.md', 'memory/owner.md',
         'memory/long-term.md', 'memory/handoff.md')
MAX_BYTES = 262144
FORMAT = 'feiyu-memory-v1'
STATE = '.feiyu-sync.json'


class MemoryConflict(RuntimeError):
    pass


def encoded(value):
    return json.dumps(value, ensure_ascii=False, sort_keys=True,
                      separators=(',', ':')).encode('utf-8')


def fingerprint(files):
    return {name: hashlib.sha256(text.encode('utf-8')).hexdigest()
            for name, text in files.items()}


def local_path(root, name):
    root = Path(root).resolve()
    path = root / name
    if not path.resolve().is_relative_to(root):
        raise MemoryConflict('Memory path points outside the workspace')
    return path


def local_files(root):
    return {name: local_path(root, name).read_bytes().decode('utf-8') for name in FILES}


def validate_bundle(bundle):
    if not isinstance(bundle, dict) or bundle.get('format') != FORMAT:
        raise ValueError('Unsupported memory bundle')
    files = bundle.get('files')
    if not isinstance(files, dict) or set(files) != set(FILES):
        raise ValueError('Unexpected or missing memory paths')
    if any(not isinstance(text, str) or not text.strip() for text in files.values()):
        raise ValueError('Empty or invalid memory content')
    identity = json.loads(files['identity.json'])
    pet_id = str(uuid.UUID(identity['pet_id']))
    if (identity.get('schema_version') != 1 or
            identity.get('memory_format') != 'utf8-markdown-v1' or
            not identity.get('name') or bundle.get('pet_id') != pet_id):
        raise ValueError('Invalid or inconsistent pet identity')
    return bundle


def make_bundle(root):
    files = local_files(root)
    bundle = {'format': FORMAT, 'pet_id': json.loads(files['identity.json'])['pet_id'],
              'saved_at': datetime.datetime.now(datetime.timezone.utc).isoformat(),
              'files': files}
    validate_bundle(bundle)
    data = encoded(bundle)
    if len(data) > MAX_BYTES:
        raise ValueError('Memory exceeds the 256 KiB snapshot limit')
    return bundle, data


def atomic_json(path, value):
    path = Path(path)
    path.parent.mkdir(parents=True, exist_ok=True)
    temp = path.with_name(path.name + '.' + uuid.uuid4().hex + '.tmp')
    try:
        with temp.open('xb') as output:
            output.write(encoded(value))
            output.flush()
            os.fsync(output.fileno())
        os.replace(temp, path)
    finally:
        if temp.exists():
            temp.unlink()


def load_state(root):
    path = local_path(root, STATE)
    return json.loads(path.read_text(encoding='utf-8')) if path.exists() else None


def save_state(root, bundle, info):
    atomic_json(local_path(root, STATE), {
        'pet_id': bundle['pet_id'], 'generation': info['generation'],
        'crc32': info['crc32'], 'hashes': fingerprint(bundle['files'])})


def install_bundle(root, bundle, info):
    """Refuse to replace unsaved local files or a different pet."""
    validate_bundle(bundle)
    root = Path(root)
    state = load_state(root)
    existing = {}
    for name in FILES:
        path = local_path(root, name)
        if path.exists():
            existing[name] = path.read_bytes().decode('utf-8')
    if state:
        if state.get('pet_id') != bundle['pet_id']:
            raise MemoryConflict('Workspace belongs to a different pet')
        if set(existing) != set(FILES) or fingerprint(existing) != state.get('hashes'):
            raise MemoryConflict('Unsaved local memory: save it or use a new workspace')
    elif any(bundle['files'][name] != text for name, text in existing.items()):
        raise MemoryConflict('Existing workspace has different memory; use an empty folder')
    # Prepare all replacement files before replacing any existing document.
    staging = root / ('.feiyu-incoming-' + uuid.uuid4().hex)
    staging.mkdir(parents=True)
    try:
        for name, text in bundle['files'].items():
            path = staging / name
            path.parent.mkdir(parents=True, exist_ok=True)
            with path.open('wb') as output:
                output.write(text.encode('utf-8'))
                output.flush()
                os.fsync(output.fileno())
        # Recheck before replacing: DSH might have edited the workspace meanwhile.
        current = {name: local_path(root, name).read_bytes().decode('utf-8')
                   for name in FILES if local_path(root, name).exists()}
        if current != existing:
            raise MemoryConflict('Workspace changed during restore')
        for name in FILES:
            target = local_path(root, name)
            target.parent.mkdir(parents=True, exist_ok=True)
            os.replace(staging / name, target)
        save_state(root, bundle, info)
    finally:
        # Only files created by this restore are removed, never the workspace.
        for path in sorted(staging.rglob('*'), reverse=True):
            if path.is_file():
                path.unlink()
            elif path.is_dir():
                path.rmdir()
        staging.rmdir()


class SerialLink:
    def __init__(self, port='', serial='E46468C09732292D'):
        if sys.platform != 'win32':
            raise RuntimeError('This bridge currently supports Windows')
        environment = dict(os.environ, FEIYU_LINK_SCRIPT=str(
            Path(__file__).with_name('serial-link.ps1')),
            FEIYU_PORT=port, FEIYU_DEVICE_SERIAL=serial)
        command = ("& ([scriptblock]::Create([System.IO.File]::ReadAllText("
                   "$env:FEIYU_LINK_SCRIPT))) -Port $env:FEIYU_PORT "
                   "-DeviceSerial $env:FEIYU_DEVICE_SERIAL")
        self.process = subprocess.Popen(
            ['powershell.exe', '-NoProfile', '-Command', command],
            stdin=subprocess.PIPE, stdout=subprocess.PIPE, stderr=subprocess.DEVNULL,
            text=True, encoding='utf-8', env=environment,
            creationflags=subprocess.CREATE_NO_WINDOW)

    def command(self, command):
        self.process.stdin.write(json.dumps({'command': command}) + '\n')
        self.process.stdin.flush()
        line = self.process.stdout.readline()
        if not line:
            raise OSError('Keyboard serial service stopped')
        result = json.loads(line)
        if not result.get('ok'):
            operation = ' '.join(command.split()[:3])
            raise OSError(operation + ': ' + result.get('error', 'Serial request failed'))
        response = result['response']
        if response.startswith('HP MEM ERROR '):
            raise RuntimeError(response)
        return response

    def close(self):
        if self.process.poll() is None:
            with contextlib.suppress(OSError):
                self.process.stdin.write('{"command":"close"}\n')
                self.process.stdin.flush()
            try:
                self.process.wait(timeout=3)
            except subprocess.TimeoutExpired:
                self.process.terminate()
                self.process.wait(timeout=3)


def info(link):
    response = link.command('HP MEM INFO')
    if not response.startswith('HP MEM INFO '):
        raise RuntimeError('Firmware does not support Feiyu memory protocol')
    value = json.loads(response[12:])
    if value.get('protocol') != 1:
        raise RuntimeError('Unsupported memory protocol')
    return value


def remote_bundle(link, metadata=None):
    before = metadata if metadata is not None else info(link)
    if not before['generation']:
        raise RuntimeError('SD memory is empty; initialize it first')
    if not 0 < before['size'] <= MAX_BYTES:
        raise ValueError('Invalid memory size')
    data = bytearray()
    while len(data) < before['size']:
        response = link.command('HP MEM READ ' + str(len(data)))
        parts = response.split(' ', 4)
        if len(parts) != 5 or parts[:3] != ['HP', 'MEM', 'DATA'] or int(parts[3]) != len(data):
            raise ValueError('Unexpected memory response')
        chunk = bytes.fromhex(parts[4])
        if not chunk or len(chunk) > 128 or len(data) + len(chunk) > before['size']:
            raise ValueError('Invalid memory chunk')
        data.extend(chunk)
    after = info(link)
    if any(after[key] != before[key] for key in ('generation', 'size', 'crc32')):
        raise MemoryConflict('SD memory changed while reading')
    if zlib.crc32(data) != before['crc32']:
        raise ValueError('Memory checksum mismatch')
    return validate_bundle(json.loads(data)), before


def push(link, root, initialize=False):
    bundle, data = make_bundle(root)
    state = load_state(root)
    before = info(link)
    if initialize:
        if before['generation']:
            raise MemoryConflict('SD already has memory; pull it instead of initializing')
        expected = 0
    else:
        if not state or state.get('pet_id') != bundle['pet_id']:
            raise MemoryConflict('Workspace is not linked to this pet; pull memory first')
        expected = state['generation']
        if before['generation'] != expected or before['crc32'] != state['crc32']:
            raise MemoryConflict('SD has a newer or different checkpoint; keep both versions')
    crc = zlib.crc32(data)
    started = False
    try:
        answer = link.command(f'HP MEM BEGIN {len(data)} {crc:08x} {expected}')
        if answer != 'HP MEM OK BEGIN':
            raise RuntimeError('Memory upload did not start')
        started = True
        for position in range(0, len(data), 128):
            answer = link.command(f'HP MEM WRITE {position} {data[position:position+128].hex()}')
            if answer != 'HP MEM OK WRITE':
                raise RuntimeError('Memory chunk was not accepted')
        if link.command('HP MEM COMMIT') != 'HP MEM OK COMMIT':
            raise RuntimeError('Memory commit failed')
        started = False
        verified, after = remote_bundle(link)
        if (after['generation'] != expected + 1 or after['crc32'] != crc or
                verified != bundle):
            raise ValueError('Saved memory readback differs')
        save_state(root, bundle, after)
        return after
    finally:
        if started:
            with contextlib.suppress(Exception):
                link.command('HP MEM ABORT')


def pull(link, root):
    bundle, metadata = remote_bundle(link)
    install_bundle(root, bundle, metadata)
    return metadata


def watch(link, root, assistant=None):
    waiting = False
    while True:
        try:
            remote = info(link)
            break
        except OSError:
            if not waiting:
                print('Keyboard not connected; waiting for the matching device.', flush=True)
                waiting = True
            time.sleep(2)
    state = load_state(root)
    if not state or state['generation'] != remote['generation'] or state['crc32'] != remote['crc32']:
        pull(link, root)
    else:
        # Validate identity and files without overwriting unsaved local edits.
        bundle, _ = make_bundle(root)
        if bundle['pet_id'] != state['pet_id']:
            raise MemoryConflict('Workspace identity changed')
    print('Ready: open this workspace in DSH. Hold 1+2+3 for one second to save.', flush=True)
    while True:
        try:
            status = info(link)
        except OSError as error:
            if assistant:
                assistant.disconnected()
            print('Keyboard connection interrupted; reconnecting:', error, flush=True)
            time.sleep(2)
            continue
        if assistant:
            local = load_state(root)
            dirty = local is None or fingerprint(local_files(root)) != local.get('hashes')
            assistant.tick(link, status, dirty)
        if status['save_pending']:
            request = status['save_request']
            try:
                if assistant and not assistant.prepare_save(request):
                    time.sleep(0.5)
                    continue
                saved = push(link, root)
                answer = link.command(f"HP MEM ACK {request} {saved['generation']}")
                if answer != 'HP MEM OK ACK':
                    raise RuntimeError('Save acknowledgement rejected')
                print('Saved and read back generation', saved['generation'], flush=True)
                if assistant:
                    assistant.saved()
            except OSError as error:
                with contextlib.suppress(Exception):
                    link.command(f'HP MEM FAIL {request}')
                print('Save not confirmed; release keys and hold 1+2+3 again:', error, flush=True)
            except Exception as error:
                with contextlib.suppress(Exception):
                    link.command(f'HP MEM FAIL {request}')
                if assistant:
                    assistant.failed(error)
                    print('Save not confirmed:', error, flush=True)
                else:
                    raise
        time.sleep(0.5)


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('operation', choices=('status', 'init', 'pull', 'push', 'watch', 'request-save'))
    parser.add_argument('--workspace', type=Path, default=Path(__file__).resolve().parents[1] / 'workspace')
    parser.add_argument('--port', default='')
    parser.add_argument('--device-serial', default='E46468C09732292D')
    args = parser.parse_args()
    link = SerialLink(args.port, args.device_serial)
    try:
        if args.operation == 'status':
            print(json.dumps(info(link), ensure_ascii=False, indent=2))
        elif args.operation == 'request-save':
            print(link.command('HP MEM SAVE'))
        elif args.operation == 'watch':
            from feiyu_assistant import Assistant
            watch(link, args.workspace, Assistant(args.workspace))
        else:
            result = pull(link, args.workspace) if args.operation == 'pull' else push(
                link, args.workspace, initialize=args.operation == 'init')
            print(json.dumps(result, ensure_ascii=False, indent=2))
    except (OSError, RuntimeError, ValueError, KeyError) as error:
        print('Memory operation failed:', error, file=sys.stderr)
        return 1
    except KeyboardInterrupt:
        return 0
    finally:
        link.close()
    return 0


if __name__ == '__main__':
    sys.exit(main())
