// country.h - the country of a point (the internet radio's "Рядом": its stations), by Natural Earth's 1:110m borders
// (countries.inc, tools/make_countries.py): about 0.1-0.5 degree off at a border. Pure: no GPS, no files.
#pragma once

// true: the point (degrees) is in a country - its ISO 3166-1 code ("RU") into code; false: the sea, or no data.
bool CountryAt(double lat, double lon, char code[3]);

// One ring (n points, x y each): the point inside it - the even-odd rule (CountryAt's, open for its test).
bool RingHas(const short (*p)[2], int n, int x, int y);
