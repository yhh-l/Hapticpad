import copy
import importlib.util
import json
from pathlib import Path
import tempfile
import unittest
from unittest.mock import Mock, patch
import zlib

spec = importlib.util.spec_from_file_location('feiyu_bridge', Path(__file__).with_name('feiyu_bridge.py'))
bridge = importlib.util.module_from_spec(spec)
spec.loader.exec_module(bridge)


class FakeLink:
    def __init__(self):
        self.data = b''
        self.generation = 0
        self.pending = None
        self.expected = None
        self.corrupt = False

    def command(self, command):
        parts = command.split()
        operation = parts[2]
        if operation == 'INFO':
            return 'HP MEM INFO ' + json.dumps({'protocol': 1, 'generation': self.generation,
                'size': len(self.data), 'crc32': zlib.crc32(self.data), 'max_bytes': bridge.MAX_BYTES,
                'save_request': 0, 'save_pending': False})
        if operation == 'BEGIN':
            if int(parts[5]) != self.generation:
                raise RuntimeError('HP MEM ERROR STALE')
            self.pending = bytearray()
            self.expected = (int(parts[3]), int(parts[4], 16))
        elif operation == 'WRITE':
            if int(parts[3]) != len(self.pending):
                raise RuntimeError('HP MEM ERROR OFFSET')
            self.pending.extend(bytes.fromhex(parts[4]))
        elif operation == 'COMMIT':
            if (len(self.pending), zlib.crc32(self.pending)) != self.expected:
                raise RuntimeError('HP MEM ERROR CRC')
            self.data = bytes(self.pending)
            self.generation += 1
        elif operation == 'READ':
            offset = int(parts[3])
            data = self.data[offset:offset+128]
            if self.corrupt and data:
                data = bytes([data[0] ^ 1]) + data[1:]
            return f'HP MEM DATA {offset} {data.hex()}'
        elif operation == 'ABORT':
            self.pending = None
        else:
            raise AssertionError(command)
        return 'HP MEM OK ' + operation


class BridgeTests(unittest.TestCase):
    def setUp(self):
        self.temp = tempfile.TemporaryDirectory()
        self.addCleanup(self.temp.cleanup)
        self.root = Path(self.temp.name)
        self.first = self.root / 'computer-a'
        self.second = self.root / 'different-path-computer-b'
        for name in bridge.FILES:
            dest = self.first / name
            dest.parent.mkdir(parents=True, exist_ok=True)
            if name == 'identity.json':
                text = json.dumps({'schema_version': 1, 'pet_id': '12345678-1234-4234-8234-123456789abc',
                                   'name': 'Test fish', 'memory_format': 'utf8-markdown-v1'})
            else:
                text = '# Test memory\n' + name + '\n'
            dest.write_bytes(text.encode('utf-8'))
        self.link = FakeLink()

    def test_cross_computer_roundtrip_and_identity(self):
        bridge.push(self.link, self.first, initialize=True)
        bridge.pull(self.link, self.second)
        self.assertEqual(bridge.local_files(self.first), bridge.local_files(self.second))
        changed = self.second / 'memory' / 'handoff.md'
        changed.write_text('新电脑上的记忆已更新。\n', encoding='utf-8')
        bridge.push(self.link, self.second)
        bridge.pull(self.link, self.first)
        self.assertEqual(changed.read_bytes(), (self.first / 'memory' / 'handoff.md').read_bytes())
        self.assertEqual(bridge.load_state(self.first)['generation'], 2)

    def test_stale_computer_cannot_overwrite_new_memory(self):
        bridge.push(self.link, self.first, initialize=True)
        bridge.pull(self.link, self.second)
        bridge.push(self.link, self.second)
        before = self.link.data
        with self.assertRaises(bridge.MemoryConflict):
            bridge.push(self.link, self.first)
        self.assertEqual(before, self.link.data)

    def test_restore_refuses_unsaved_local_edits(self):
        bridge.push(self.link, self.first, initialize=True)
        path = self.first / 'memory' / 'owner.md'
        path.write_text('尚未保存的新偏好。\n', encoding='utf-8')
        with self.assertRaises(bridge.MemoryConflict):
            bridge.pull(self.link, self.first)
        self.assertEqual(path.read_text(encoding='utf-8'), '尚未保存的新偏好。\n')

    def test_never_reinitialize_existing_memory(self):
        bridge.push(self.link, self.first, initialize=True)
        with self.assertRaises(bridge.MemoryConflict):
            bridge.push(self.link, self.first, initialize=True)
        self.assertEqual(self.link.generation, 1)

    def test_corrupt_readback_does_not_mark_saved(self):
        self.link.corrupt = True
        with self.assertRaises(ValueError):
            bridge.push(self.link, self.first, initialize=True)
        self.assertIsNone(bridge.load_state(self.first))

    def test_untrusted_paths_and_identity_rejected(self):
        bundle, _ = bridge.make_bundle(self.first)
        bad = copy.deepcopy(bundle)
        bad['files']['../../outside'] = 'bad'
        with self.assertRaises(ValueError):
            bridge.install_bundle(self.second, bad, {})
        bundle['pet_id'] = '11111111-1111-4111-8111-111111111111'
        with self.assertRaises(ValueError):
            bridge.validate_bundle(bundle)

    def test_watch_survives_transport_loss_and_only_acknowledges_verified_save(self):
        metadata = bridge.push(self.link, self.first, initialize=True)
        first_request = dict(metadata, save_pending=True, save_request=1)
        retry_request = dict(metadata, save_pending=True, save_request=2)
        transport = Mock()
        transport.command.side_effect = ['HP MEM OK FAIL', 'HP MEM OK ACK']
        with patch.object(bridge, 'info', side_effect=[metadata, OSError('disconnected'),
                                                    first_request, retry_request]), \
                patch.object(bridge, 'push', side_effect=[OSError('lost write reply'),
                                                         {'generation': 2}]), \
                patch.object(bridge.time, 'sleep', side_effect=[None, None, StopIteration]), \
                patch('builtins.print'):
            with self.assertRaises(StopIteration):
                bridge.watch(transport, self.first)
        self.assertEqual([call.args[0] for call in transport.command.call_args_list],
                         ['HP MEM FAIL 1', 'HP MEM ACK 2 2'])

    def test_watch_started_without_keyboard_waits_for_reconnection(self):
        metadata = bridge.push(self.link, self.first, initialize=True)
        with patch.object(bridge, 'info', side_effect=[OSError('not plugged in'),
                                                    metadata, dict(metadata, save_pending=False)]), \
                patch.object(bridge.time, 'sleep', side_effect=[None, StopIteration]), \
                patch('builtins.print'):
            with self.assertRaises(StopIteration):
                bridge.watch(self.link, self.first)
        self.assertEqual(bridge.load_state(self.first)['generation'], 1)


if __name__ == '__main__':
    unittest.main()
