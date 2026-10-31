import unittest
from feiyu_assistant import pages_for, Assistant
from unittest.mock import Mock


class AssistantTests(unittest.TestCase):
    def test_chinese_pagination_is_bounded_and_preserves_content(self):
        text = '这是实际模型返回的中文回复。' * 40
        pages = pages_for(text)
        self.assertEqual(''.join(p.replace('\n','') for p in pages), text)
        self.assertTrue(all(len(p.split('\n')) <= 3 for p in pages))
        self.assertTrue(all(len(p.encode()) <= 125 for p in pages))

    def test_mixed_text_and_unrenderable_characters(self):
        pages = pages_for('ABC你好\n\n继续🐋\t123')
        self.assertIn('继续?  123', '\n'.join(pages))
        self.assertEqual(pages_for(''), ['等待任务'])

    def test_checkpoint_waits_and_rejects_model_failure(self):
        assistant = Assistant('.')
        assistant.api = Mock(side_effect=[{'id':'job'}, {'status':'running'},
            {'status':'failed','error':'没有实际写入交接文件'}])
        self.assertFalse(assistant.prepare_save(1))
        with self.assertRaisesRegex(RuntimeError, '没有实际写入'):
            assistant.prepare_save(1)
        self.assertEqual(assistant.api.call_args_list[0].args, ('/checkpoint', {}))

    def test_tick_handles_no_prior_bridge_and_acknowledges_once(self):
        assistant = Assistant('.')
        assistant.api = Mock(return_value={'phase':'done','text':'实际回复','bridge':None})
        link = Mock()
        def command(line):
            if line=='HP AI INFO':
                return 'HP AI INFO {"protocol":1,"action":"next","event_id":1,"page":0}'
            return 'HP AI OK '+line.split()[2]
        link.command.side_effect=command
        assistant.tick(link, {'generation':6}, False)
        self.assertFalse(assistant.error)
        self.assertIn('实际回复',assistant.content)
        self.assertIn('HP AI ACK 1',[c.args[0] for c in link.command.call_args_list])


if __name__=='__main__':
    unittest.main()
