"""Render the PHP entry points and check the shared templates without a server."""
import os
from pathlib import Path
import shutil
import subprocess
import unittest

ROOT = Path(__file__).resolve().parents[1]
PHP = os.environ.get('PHP', 'php')

@unittest.skipUnless(shutil.which(PHP), 'PHP CLI not configured')
class PHPSite(unittest.TestCase):
    def test_entry_points_render_shared_layout_and_current_links(self):
        for name in ('index.php', 'release-notes.php'):
            result = subprocess.run([PHP, str(ROOT / 'public-web' / name)], check=True,
                                    capture_output=True, text=True, env=dict(os.environ, BITSKIM_RELEASES_OFFLINE="1"))
            self.assertEqual(result.stderr, '')
            self.assertEqual(result.stdout.count('<html lang="en">'), 1)
            self.assertEqual(result.stdout.count('<footer'), 1)
            self.assertIn('https://github.com/retrocs-org/bitskim', result.stdout)
            self.assertIn('release-notes.php', result.stdout)
            self.assertNotIn('<?php', result.stdout)
            self.assertNotIn('release-notes.html', result.stdout)
        self.assertFalse((ROOT / 'public-web/index.html').exists())
        self.assertFalse((ROOT / 'public-web/release-notes.html').exists())
