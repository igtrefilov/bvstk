#!/usr/bin/env python3
"""Exercise the real CMS verifier against generated certificates and CRLs."""
import os
from pathlib import Path
import subprocess
import tempfile
import unittest

ROOT = Path(__file__).resolve().parents[2]


class VerifyUID(unittest.TestCase):
    @classmethod
    def setUpClass(cls):
        cls.temp = tempfile.TemporaryDirectory()
        cls.work = Path(cls.temp.name)
        cls.binary = cls.work / 'verify'
        subprocess.run(['cc', '-std=c11', '-Wall', '-Wextra', '-Werror',
                        '-DBVSTK_UID_VERIFY_TIMEOUT_SECONDS=2',
                        str(ROOT / 'src/apps/neutrino/security/uid_verify.c'),
                        '-lcrypto', '-o', str(cls.binary)], check=True)
        cls.security = cls.work / 'security'
        for p in ('trust/anchors', 'trust/crl', 'signatures/uid'):
            (cls.security / p).mkdir(parents=True)
        cls.uid = cls.work / 'uid'
        cls.uid.write_bytes(b'ABCD1234EFGH5678')
        (cls.work / 'index').touch()
        (cls.work / 'serial').write_text('01\n')
        (cls.work / 'crlnumber').write_text('01\n')
        (cls.work / 'newcerts').mkdir()
        cls.config = cls.work / 'ca.cnf'
        cls.config.write_text(f'''[ca]
 default_ca=issuer
[issuer]
 database={cls.work}/index
 serial={cls.work}/serial
 crlnumber={cls.work}/crlnumber
 new_certs_dir={cls.work}/newcerts
 certificate={cls.work}/root.pem
 private_key={cls.work}/root.key
 default_md=sha256
 default_days=1
 default_crl_days=1
 policy=policy
[policy]
 commonName=supplied
[code]
 basicConstraints=critical,CA:FALSE
 keyUsage=critical,digitalSignature
 extendedKeyUsage=codeSigning
[email]
 basicConstraints=critical,CA:FALSE
 keyUsage=critical,digitalSignature
 extendedKeyUsage=emailProtection
''')
        cls.openssl('req', '-x509', '-newkey', 'rsa:2048', '-nodes', '-days', '2',
                    '-subj', '/CN=Test root', '-keyout', str(cls.work/'root.key'),
                    '-out', str(cls.work/'root.pem'), '-addext', 'basicConstraints=critical,CA:TRUE')
        (cls.security/'trust/anchors/root.pem').write_bytes((cls.work/'root.pem').read_bytes())
        for kind in ('code', 'email'):
            cls.openssl('req', '-new', '-newkey', 'rsa:2048', '-nodes', '-subj', '/CN='+kind,
                        '-keyout', str(cls.work/(kind+'.key')), '-out', str(cls.work/(kind+'.csr')))
            cls.openssl('ca', '-batch', '-config', str(cls.config), '-extensions', kind,
                        '-in', str(cls.work/(kind+'.csr')), '-out', str(cls.work/(kind+'.pem')))
        cls.make_crl()

    @classmethod
    def tearDownClass(cls):
        cls.temp.cleanup()

    @classmethod
    def openssl(cls, *args):
        return subprocess.run(['openssl', *args], check=True, stdout=subprocess.PIPE,
                              stderr=subprocess.PIPE, env={**os.environ, 'OPENSSL_CONF': '/dev/null'})

    @classmethod
    def make_crl(cls, expired=False):
        args = ['ca', '-gencrl', '-config', str(cls.config), '-out', str(cls.work/'crl.pem')]
        if expired:
            args += ['-crl_lastupdate', '20200101000000Z', '-crl_nextupdate', '20200102000000Z']
        cls.openssl(*args)
        cls.openssl('crl', '-in', str(cls.work/'crl.pem'), '-outform', 'DER',
                    '-out', str(cls.security/'trust/crl/root.crl'))

    def sign(self, kind='code'):
        self.openssl('cms', '-sign', '-binary', '-in', str(self.uid),
                     '-signer', str(self.work/(kind+'.pem')), '-inkey', str(self.work/(kind+'.key')),
                     '-outform', 'DER', '-out', str(self.security/'signatures/uid/banana.uuid.sig'))

    def setUp(self):
        self.uid.write_bytes(b'ABCD1234EFGH5678')
        self.make_crl()
        self.sign()

    def verify(self, reason=None):
        result = subprocess.run([str(self.binary), '--files', str(self.security), str(self.uid)],
                                capture_output=True, text=True, timeout=5)
        self.assertEqual(result.returncode, 1 if reason else 0, result.stdout+result.stderr)
        self.assertIn('verify: false' if reason else 'verify: true', result.stdout)
        if reason:
            self.assertIn('verify_reason: '+reason, result.stdout)

    def test_valid(self):
        self.verify()

    def test_changed_uid(self):
        self.uid.write_bytes(b'ZBCD1234EFGH5678')
        self.verify('signature_or_certificate_invalid')

    def test_wrong_length(self):
        self.uid.write_bytes(b'short')
        self.verify('eeprom_read_failed')

    def test_blocked_reader_is_bounded(self):
        self.uid.unlink()
        os.mkfifo(self.uid)
        try:
            self.verify('verification_timeout')
        finally:
            self.uid.unlink()

    def test_wrong_purpose(self):
        self.sign('email')
        self.verify('signer_purpose_invalid')

    def test_expired_crl(self):
        self.make_crl(expired=True)
        self.verify('crl_expired')

    def test_missing_crl(self):
        (self.security/'trust/crl/root.crl').unlink()
        self.verify('security_files_invalid')

    def test_untrusted_signer(self):
        self.openssl('req', '-x509', '-newkey', 'rsa:2048', '-nodes', '-days', '1',
                     '-subj', '/CN=Other root', '-keyout', str(self.work/'other.key'),
                     '-out', str(self.security/'trust/anchors/root.pem'))
        try:
            self.verify('signature_or_certificate_invalid')
        finally:
            (self.security/'trust/anchors/root.pem').write_bytes((self.work/'root.pem').read_bytes())

    def test_attached_content_rejected(self):
        self.openssl('cms', '-sign', '-binary', '-nodetach', '-in', str(self.uid),
                     '-signer', str(self.work/'code.pem'), '-inkey', str(self.work/'code.key'),
                     '-outform', 'DER', '-out', str(self.security/'signatures/uid/banana.uuid.sig'))
        self.verify('security_files_invalid')


if __name__ == '__main__':
    unittest.main()
