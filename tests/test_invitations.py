"""Isolated processes exercise certificate pins without sharing TLS identities."""
import subprocess,sys,unittest
from pathlib import Path
ROOT=Path(__file__).resolve().parents[1]
class Invitations(unittest.TestCase):
    def test_invitation_security(self):
        for name in ['test_replacement_revokes_previous_invitation', 'test_altered_expiration_is_rejected_by_receiver', 'test_fresh_invitation_encrypted_send', 'test_wrong_certificate_rejected_before_offer', 'test_wrong_device_id_rejected_before_offer', 'test_expired_and_noncanonical_addresses_rejected', 'test_plaintext_cannot_be_used_for_invitation', 'test_shared_key_cannot_create_invitation']:
            with self.subTest(name=name):
                result=subprocess.run([sys.executable,'tests/invitation_worker.py','Invitations.'+name],cwd=ROOT,capture_output=True,text=True)
                self.assertEqual(result.returncode,0,result.stdout+result.stderr)
