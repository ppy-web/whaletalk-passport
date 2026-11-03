import unittest
from diagnose import analyze, endpoint, redact


class DiagnosticsTest(unittest.TestCase):
    def test_secrets_are_removed_before_local_logging(self):
        raw = 'Authorization: Bearer private-secret\npassword="wifi secret" token=1234&next=ok\n{"accessToken":"secret"}'
        cleaned = redact(raw)
        for secret in ('private-secret', 'wifi secret', '1234', '"secret"'):
            self.assertNotIn(secret, cleaned)
        self.assertIn('next=ok', cleaned)

    def test_playback_and_capture_are_not_conflated(self):
        result = analyze('capture_drops=200\nplayback stats gaps=0 max_gap_ms=0 max_write_ms=21')
        self.assertNotIn('playback-underflow-observed', result['findings'])
        self.assertEqual(result['playback'][0]['maxWriteMs'], 21)
        self.assertTrue(result['shortPlaybackGapsMayBeUnreported'])
        self.assertFalse(analyze('ready=1')['completePlaybackStatsObserved'])

    def test_actual_transport_and_playback_failures_are_classified(self):
        result = analyze('wifi disconnected reason=201\nESP_ERR_ESP_TLS_CONNECTION_TIMEOUT\nesp_ws_handshake_status_code=401\nplayback queue empty ms=82\nGuru Meditation')
        for finding in ('wifi-ap-not-found', 'tcp-connection-timeout', 'authentication-rejected', 'playback-underflow-observed'):
            self.assertIn(finding, result['findings'])
        self.assertIn('Guru Meditation', result['faultMarkers'])

    def test_probes_do_not_send_credentials_to_public_urls(self):
        self.assertEqual(endpoint('http://192.168.137.1:3101', '/api/health'), 'http://192.168.137.1:3101/api/health')
        for url in ('http://8.8.8.8', 'http://user:secret@127.0.0.1', 'http://127.0.0.1?token=secret', 'http://127.0.0.1/path', 'https://example.com'):
            with self.assertRaises(ValueError):
                endpoint(url, '/api/health')


if __name__ == '__main__':
    unittest.main()
