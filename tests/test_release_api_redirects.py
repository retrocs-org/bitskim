"""Exercise actual HTTP redirect handling without contacting a release service."""
import io
import json
import unittest
from unittest.mock import patch
from test_release_tool import release


class ReleaseRedirects(unittest.TestCase):
    host = 'gitlab.example.invalid'
    project = '/api/v4/projects/group%2Fapp'

    def request(self, routes, path=None, method='GET', github=False):
        calls = []
        hosts = []

        class Connection:
            def __init__(inner, host, timeout):
                hosts.append(host)
            def request(inner, verb, path, body=None, headers=None):
                calls.append((verb, path))
                status, location, payload = routes[(verb, path)]
                inner.status, inner.location = status, location
                inner.stream = io.BytesIO(json.dumps(payload).encode())
            def getresponse(inner):
                return inner
            def getheader(inner, name, default=None):
                return inner.location if name == 'Location' else default
            def read(inner, size=-1):
                return inner.stream.read(size)
            def close(inner):
                pass

        with patch.object(release.http.client, 'HTTPSConnection', Connection):
            try:
                return release.API(self.host, 'synthetic-test-value', github).request(
                    method, path or self.project + '/releases/permalink/latest')
            finally:
                self.assertTrue(all(host == self.host for host in hosts))
                self.calls = calls

    def routes(self, location):
        return {
            ('GET', self.project): (200, None, {'id': 50}),
            ('GET', self.project + '/releases/permalink/latest'): (302, location, {}),
            ('GET', '/api/v4/projects/50/releases/0.2'): (200, None, {'tag_name': '0.2'}),
            ('GET', self.project + '/releases/0.2'): (200, None, {'tag_name': '0.2'}),
        }

    def test_latest_numeric_project_redirect_is_verified_then_followed(self):
        self.assertEqual(self.request(self.routes('/api/v4/projects/50/releases/0.2')),
                         {'tag_name': '0.2'})
        self.assertEqual(self.calls, [
            ('GET', self.project + '/releases/permalink/latest'),
            ('GET', self.project), ('GET', '/api/v4/projects/50/releases/0.2')])

    def test_same_host_absolute_and_encoded_project_paths_are_supported(self):
        for path in ('/api/v4/projects/50/releases/0.2', self.project + '/releases/0.2'):
            with self.subTest(path=path):
                self.assertEqual(self.request(self.routes('https://' + self.host + path)),
                                 {'tag_name': '0.2'})

    def test_untrusted_origins_and_paths_never_receive_a_followup(self):
        for location in ('https://other.invalid/api/v4/projects/50/releases/0.2',
                         '//other.invalid/api/v4/projects/50/releases/0.2',
                         'http://' + self.host + '/api/v4/projects/50/releases/0.2',
                         'https://user@' + self.host + '/api/v4/projects/50/releases/0.2',
                         '/api/v4/projects/50/releases/0.2?token=anything',
                         '/api/v4/projects/50/releases/0.2#fragment', '/users/sign_in',
                         self.project + '/releases/permalink/latest', ''):
            with self.subTest(location=location), self.assertRaises(ValueError):
                self.request(self.routes(location))
            self.assertEqual(len(self.calls), 1)

    def test_another_project_is_not_followed(self):
        with self.assertRaisesRegex(ValueError, 'changed project'):
            self.request(self.routes('/api/v4/projects/51/releases/0.2'))
        self.assertEqual(len(self.calls), 2)

    def test_redirected_writes_and_other_reads_remain_refused(self):
        for method, path, github in (
                ('POST', self.project + '/releases', False),
                ('PUT', self.project + '/packages/generic/app/0.2/file', False),
                ('GET', self.project + '/releases/0.2', False),
                ('GET', self.project + '/releases/permalink/latest', True)):
            with self.subTest(method=method, path=path, github=github):
                with self.assertRaisesRegex(RuntimeError, 'HTTP 302'):
                    self.request({(method, path): (302, 'https://other.invalid/', {})},
                                 path, method, github)
                self.assertEqual(len(self.calls), 1)

    def test_a_second_redirect_is_refused(self):
        routes = self.routes('/api/v4/projects/50/releases/0.2')
        routes[('GET', '/api/v4/projects/50/releases/0.2')] = (302, '/users/sign_in', {})
        with self.assertRaisesRegex(RuntimeError, 'HTTP 302'):
            self.request(routes)
        self.assertEqual(len(self.calls), 3)
