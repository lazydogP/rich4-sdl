"""Exercise the text audit against reintroduced, previously visible errors."""
import importlib.util
import tempfile
import unittest
from pathlib import Path
from unittest.mock import patch

ROOT = Path(__file__).resolve().parents[2]
SPEC = importlib.util.spec_from_file_location('text_audit', ROOT / 'tools/audit_original_text.py')
audit = importlib.util.module_from_spec(SPEC)
SPEC.loader.exec_module(audit)


class OriginalTextAuditTests(unittest.TestCase):
    def test_c_literals_concatenate_across_comments(self):
        with tempfile.TemporaryDirectory() as directory:
            path = Path(directory) / 'fixture.c'
            path.write_text(
                '/* "not a literal" */\n'
                'const char *p = "\\xa8\\xcf" /* join */ "\\xa5\\xce";\n'
                'const char q = \'"\'; // "not a literal"\n'
                'const char *r = "\\101\\x42" "C";\n', encoding='utf-8')
            rows = list(audit.source_literals(path))
            self.assertEqual([(line, raw) for line, _, raw in rows],
                             [(2, bytes.fromhex('a8cfa5ce')), (4, b'ABC')])

    @unittest.skipUnless((ROOT / 'assets/original/Rich4.exe').exists(),
                         'requires the locally owned original executable')
    def test_reintroduced_typos_are_not_accepted_as_reviewed_deviations(self):
        references = [ROOT / 'assets/original/Rich4.exe']
        base = ROOT / 'assets/original/SteamV2.06/Game/rich4.exe'
        if base.exists():
            references.append(base)
        replacements = {
            'game_loop_ai_settings.c': (bytes.fromhex('a8cfa5cea564a4f9'),
                                        bytes.fromhex('a8cfa5cea5cfa4f9')),
            'game_map_info.c': (bytes.fromhex('a55baa6fafb8'),
                               bytes.fromhex('a55ba6afb8')),
        }
        original_reader = audit.source_literals

        def corrupted_reader(path):
            for line, literal, raw in original_reader(path):
                replacement = replacements.get(path.name)
                if replacement and raw == replacement[0]:
                    raw = replacement[1]
                yield line, literal, raw

        with patch.object(audit, 'source_literals', corrupted_reader):
            result = audit.audit(references)
        bad = [r for r in result['literals'] if r['file'].endswith(tuple(replacements))
               and r['status'] not in ('original_bytes', 'utf8_metadata')
               and 'review' not in r]
        self.assertEqual(len(bad), 2)
        self.assertEqual({r['status'] for r in bad}, {'unmatched', 'invalid_big5'})
        self.assertGreaterEqual(result['unreviewed'], 2)


if __name__ == '__main__':
    unittest.main()
