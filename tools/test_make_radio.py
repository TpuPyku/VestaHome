"""Tests of make_radio.py: the radio stations' table (a city and a frequency -> the name and the logo) for VestaHome."""
import os
import sys
import unittest

sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))
import make_radio  # noqa: E402

# GeoNames' cities1000 lines: id, name, ascii, alternates, lat, lon, ... country (8) ... population (14)
def geo(name, alt, lat, lon, cc, pop):
    f = [''] * 19
    f[1], f[2], f[3], f[4], f[5], f[8], f[14] = name, name, alt, str(lat), str(lon), cc, str(pop)
    return '\t'.join(f) + '\n'


GEO = [
    geo('Samara', 'Самара,Samara', 53.20767, 50.13553, 'RU', 1163399),
    geo('Kirovsk', 'Кировск', 67.61475, 33.67274, 'RU', 29605),
    geo('Kirovsk', 'Кировск', 59.87, 30.99, 'RU', 25000),          # the same name, fewer people: not taken
    geo('Kerch', 'Керчь', 45.3562, 36.4674, 'UA', 150000),          # Crimea under UA in GeoNames
    geo('Mogilev', 'Могилев,Могилёв', 53.9168, 30.3449, 'BY', 360000),
    geo('Paris', 'Париж', 48.85, 2.35, 'FR', 2000000),               # another country: never
    geo('Tolyatti', 'Ставрополь,Тольятти', 53.53, 49.35, 'RU', 702879),   # its old name: not Stavropol
    geo('Stavropol', 'Ставрополь', 45.04, 41.97, 'RU', 433931),
    geo('Mirnyy', 'Мирный', 62.54, 113.96, 'RU', 40308),             # ("Mirnyy" for "Мирный" - the same name)
]


class TestCities(unittest.TestCase):
    def setUp(self):
        self.index = make_radio.city_index(GEO)

    def test_by_its_name_the_largest(self):
        """A city by its Russian name in its country; of two of one name - the larger one."""
        self.assertEqual(make_radio.locate('Самара', 'RU', self.index), (53.20767, 50.13553))
        self.assertEqual(make_radio.locate('Кировск', 'RU', self.index), (67.61475, 33.67274))

    def test_own_name_before_an_old_one(self):
        """A city whose own name is the Russian one (its Latin one the same when spelt) before a larger one that only
        had it once (Тольятти was Ставрополь)."""
        self.assertEqual(make_radio.locate('Ставрополь', 'RU', self.index), (45.04, 41.97))
        self.assertEqual(make_radio.locate('Тольятти', 'RU', self.index), (53.53, 49.35))
        self.assertEqual(make_radio.locate('Мирный', 'RU', self.index), (62.54, 113.96))

    def test_yo_and_case(self):
        """ё as е, any case."""
        self.assertEqual(make_radio.locate('МОГИЛЁВ', 'BY', self.index), (53.9168, 30.3449))

    def test_other_country(self):
        """Not in its own country - the neighbours' (GeoNames keeps Crimea under UA); far ones never."""
        self.assertEqual(make_radio.locate('Керчь', 'RU', self.index), (45.3562, 36.4674))
        self.assertIsNone(make_radio.locate('Париж', 'RU', self.index))
        self.assertIsNone(make_radio.locate('Нигде', 'RU', self.index))


class TestTable(unittest.TestCase):
    def test_lines(self):
        """A station a line: lat;lon;frequency (x10 kHz);logo;name - by the city, then the frequency; the cities not
        found told apart, their stations left out."""
        index = make_radio.city_index(GEO)
        radio = [
            {'cityName': 'Самара', 'country': 'RU', 'freqNum': 10430, 'freqLogo': 'ru200', 'freqName': 'Самара-Максимум'},
            {'cityName': 'Самара', 'country': 'RU', 'freqNum': 8820, 'freqLogo': 'ru069', 'freqName': 'Радио Гордость'},
            {'cityName': 'Нигде', 'country': 'RU', 'freqNum': 9000, 'freqLogo': 'ru001', 'freqName': 'X'},
            {'cityName': 'Самара', 'country': 'RU', 'freqNum': 9000, 'freqLogo': 'ru002', 'freqName': 'a;b\tc '},
        ]
        lines, missed = make_radio.station_lines(radio, index)
        self.assertEqual(lines, ['53.208;50.136;8820;ru069;Радио Гордость',
                                 '53.208;50.136;9000;ru002;a b c',
                                 '53.208;50.136;10430;ru200;Самара-Максимум'])
        self.assertEqual(missed, ['Нигде (RU)'])


if __name__ == '__main__':
    unittest.main()
