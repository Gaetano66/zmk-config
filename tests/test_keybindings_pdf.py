import importlib.util
from pathlib import Path
import re
import shutil
import subprocess
import sys
from tempfile import TemporaryDirectory
import unittest

ROOT = Path(__file__).resolve().parents[1]
spec = importlib.util.spec_from_file_location('keybindings_pdf', ROOT / 'scripts/keybindings_pdf.py')
assert spec is not None and spec.loader is not None
pdf = importlib.util.module_from_spec(spec)
spec.loader.exec_module(pdf)


class KeybindingsPDF(unittest.TestCase):
    def setUp(self):
        self.source = (ROOT / pdf.KEYMAP).read_text()
        self.commands = (ROOT / pdf.COMMANDS).read_text()

    def test_layout_and_combos(self):
        layers, combos, conditions = pdf.parse(self.source, self.commands)
        self.assertEqual([len(keys) for _, keys in layers], [42] * 4)
        self.assertEqual(layers[0][1][36:], ['Win', 'Lower', 'Leer', 'Leer', 'Raise', 'AltGr'])
        self.assertEqual(layers[1][1][11], 'ü')
        self.assertEqual(layers[3][1][10], 'Pomo Neustart')
        self.assertIn(('P + Lösch links', 'Entf', 'Basis', 50), combos)
        self.assertEqual(conditions, [('Lower + Raise', 'Lighting')])

    def test_live_bindings_not_comments_and_reproducible(self):
        doc = pdf.render(pdf.parse(self.source, self.commands))
        changed_comment = self.source.replace('// Base', '// Something misleading')
        self.assertEqual(doc, pdf.render(pdf.parse(changed_comment, self.commands)))
        self.assertNotEqual(doc, pdf.render(pdf.parse(self.source.replace('&kp TAB', '&kp ESC', 1), self.commands)))
        commands = re.sub(r'(POMO_RESTART\s+)1', r'\g<1>4', self.commands)
        self.assertNotEqual(doc, pdf.render(pdf.parse(self.source, commands)))
        self.assertTrue(doc.startswith(b'%PDF-'))

    def test_layout_geometry_fails_closed(self):
        with self.assertRaisesRegex(ValueError, '42'):
            pdf.parse(self.source.replace('&kp TAB', '', 1), self.commands)

    def test_commented_duplicate_command_does_not_override_define(self):
        commands = self.commands + '\n/*\n#define POMO_RESTART 4\n*/\n'
        self.assertEqual(pdf.parse(self.source, commands), pdf.parse(self.source, self.commands))


class KeybindingsPDFCLI(unittest.TestCase):
    def setUp(self):
        self.repo = Path(self.enterContext(TemporaryDirectory()))
        self.output = self.repo / pdf.OUTPUT
        self.script = 'scripts/keybindings_pdf.py'
        self.git('init', '-q')
        for relative in (pdf.KEYMAP, pdf.COMMANDS, self.script):
            target = self.repo / relative
            target.parent.mkdir(parents=True, exist_ok=True)
            shutil.copyfile(ROOT / relative, target)
        self.git('add', '--', pdf.KEYMAP, pdf.COMMANDS)

    def git(self, *args):
        return subprocess.run(['git', '-C', str(self.repo), *args], check=True, capture_output=True)

    def generate(self, *args):
        return subprocess.run([sys.executable, '-B', self.script, *args], cwd=self.repo,
                              capture_output=True, text=True)

    def test_staged_generation_preserves_untracked_pdf(self):
        self.output.parent.mkdir()
        self.output.write_bytes(b'untracked PDF')
        result = self.generate('--staged')
        self.assertNotEqual(result.returncode, 0, result.stderr)
        self.assertIn(pdf.OUTPUT, result.stderr)
        self.assertEqual(self.output.read_bytes(), b'untracked PDF')

    def test_staged_generation_preserves_replacement_after_index_delete(self):
        self.output.parent.mkdir()
        self.output.write_bytes(b'original PDF')
        self.git('add', '--', pdf.OUTPUT)
        self.git('-c', 'user.name=PDF Test', '-c', 'user.email=pdf@example.invalid',
                 'commit', '--no-verify', '-qm', 'Initial PDF')
        self.git('rm', '-q', '--', pdf.OUTPUT)
        self.output.parent.mkdir(exist_ok=True)
        self.output.write_bytes(b'replacement PDF')
        self.assertEqual(self.git('diff', '--cached', '--name-status').stdout,
                         f'D\t{pdf.OUTPUT}\n'.encode())
        result = self.generate('--staged')
        self.assertNotEqual(result.returncode, 0, result.stderr)
        self.assertIn(pdf.OUTPUT, result.stderr)
        self.assertEqual(self.output.read_bytes(), b'replacement PDF')

    def test_staged_generation_creates_missing_pdf(self):
        result = self.generate('--staged')
        self.assertEqual(result.returncode, 0, result.stderr)
        self.assertTrue(self.output.read_bytes().startswith(b'%PDF-'))

    def test_staged_generation_updates_indexed_pdf(self):
        self.output.parent.mkdir()
        self.output.write_bytes(b'old PDF')
        self.git('add', '--', pdf.OUTPUT)
        result = self.generate('--staged')
        self.assertEqual(result.returncode, 0, result.stderr)
        self.assertTrue(self.output.read_bytes().startswith(b'%PDF-'))

    def assert_symlink_rejected(self, *args):
        result = self.generate(*args)
        self.assertNotEqual(result.returncode, 0, result.stderr)
        self.assertIn('Symlink', result.stderr)

    def test_symlink_output_is_rejected(self):
        target = self.repo / 'target.pdf'
        result = self.generate('--staged')
        self.assertEqual(result.returncode, 0, result.stderr)
        expected = self.output.read_bytes()
        self.git('add', '--', pdf.OUTPUT)
        self.output.rename(target)
        self.output.symlink_to(target)
        for opts in ((), ('--check',), ('--staged',), ('--staged', '--check')):
            with self.subTest(opts=opts):
                self.assert_symlink_rejected(*opts)
        self.assertEqual(self.output.readlink(), target)
        self.assertEqual(target.read_bytes(), expected)

    def test_symlink_parent_is_rejected(self):
        result = self.generate('--staged')
        self.assertEqual(result.returncode, 0, result.stderr)
        self.git('add', '--', pdf.OUTPUT)
        original = self.output.read_bytes()
        target = self.repo / 'redirected'
        self.output.parent.rename(target)
        self.output.parent.symlink_to(target, target_is_directory=True)
        for opts in ((), ('--check',), ('--staged',), ('--staged', '--check')):
            with self.subTest(opts=opts):
                self.assert_symlink_rejected(*opts)
        self.assertEqual(self.output.parent.readlink(), target)
        self.assertEqual((target / self.output.name).read_bytes(), original)

    def test_dangling_symlink_output_is_rejected(self):
        self.output.parent.mkdir()
        target = self.repo / 'missing.pdf'
        self.output.symlink_to(target)
        self.assert_symlink_rejected('--staged')
        self.assertFalse(target.exists())

    def test_dangling_symlink_parent_is_rejected(self):
        target = self.repo / 'missing-directory'
        self.output.parent.symlink_to(target, target_is_directory=True)
        self.assert_symlink_rejected('--staged')
        self.assertFalse(target.exists())

    def test_index_symlink_is_rejected_even_if_absent_from_worktree(self):
        self.output.parent.mkdir()
        self.output.symlink_to('missing.pdf')
        self.git('add', '--', pdf.OUTPUT)
        self.output.unlink()
        for opts in (('--staged',), ('--staged', '--check')):
            with self.subTest(opts=opts):
                self.assert_symlink_rejected(*opts)
        self.assertFalse(self.output.exists())

    def test_updated_pdf_replaces_file_instead_of_truncating_it(self):
        self.output.parent.mkdir()
        self.output.write_bytes(b'old PDF')
        self.git('add', '--', pdf.OUTPUT)
        other = self.repo / 'same-inode.pdf'
        other.hardlink_to(self.output)
        result = self.generate('--staged')
        self.assertEqual(result.returncode, 0, result.stderr)
        self.assertEqual(other.read_bytes(), b'old PDF')
        self.git('add', '--', pdf.OUTPUT)
        self.assertTrue(self.git('ls-files', '--stage', '--', pdf.OUTPUT).stdout.startswith(b'100644 '))


if __name__ == '__main__':
    unittest.main()
