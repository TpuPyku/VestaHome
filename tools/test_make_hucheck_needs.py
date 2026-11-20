"""Tests of make_hucheck_needs.py: the vendor functions our code calls, by their DLL."""
import os
import sys
import unittest

sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))
import make_hucheck_needs as m  # noqa: E402


class TestNames(unittest.TestCase):
    def test_groups(self):
        """Only quoted names, each once, sorted, by the DLL's prefix; others left out."""
        texts = ['McuSend0("SPApi_SendResetEvent"); f("SPApi_LoadRadioEngine"); // SPApi_NotQuoted',
                 'P("Flc_Bluetooth_Init") P("Flc_Bluetooth_Init") P("BTRF_SendData") P("XCBM_GetConnections")',
                 'G("BTDDB_AddDevice") G("SV_AudioSetRoute") G("Other_Thing") G("SPApi_")']
        g = m.names(texts)
        self.assertEqual(g['kNeedSpm'], ['SPApi_LoadRadioEngine', 'SPApi_SendResetEvent'])
        self.assertEqual(g['kNeedFlc'], ['Flc_Bluetooth_Init'])
        self.assertEqual(g['kNeedBtapi'], ['BTDDB_AddDevice', 'BTRF_SendData', 'XCBM_GetConnections'])
        self.assertEqual(g['kNeedSv'], ['SV_AudioSetRoute'])

    def test_include(self):
        """The C arrays; an empty group still compiles."""
        text = m.include({'kNeedSpm': ['SPApi_A'], 'kNeedFlc': [], 'kNeedBtapi': [], 'kNeedSv': []})
        self.assertIn('static const char* const kNeedSpm[] = {\n    "SPApi_A",\n};', text)
        self.assertIn('static const char* const kNeedFlc[] = {\n    "",\n};', text)

    def test_repo(self):
        """Our sources as they are: the known calls there."""
        g = m.names(m.sources(m.REPO))
        self.assertIn('SPApi_SetSkelonton', g['kNeedSpm'])
        self.assertIn('Flc_Bluetooth_Init', g['kNeedFlc'])
        self.assertIn('BTRF_OpenClientChannelEx', g['kNeedBtapi'])
        self.assertIn('SV_AudioSetRoute', g['kNeedSv'])


if __name__ == '__main__':
    unittest.main()
