# LCD Desk Counter

Very rough stub of a README. Will need to be expanded later.

ESP32 driven, 1602 LCD based desk counter for rendering stat values.

Requires an HTTPS endpoint configured to return a plaintext value that can be parsed and rendered on the display. Currently no JSON/XML extraction etc.
Endpoint may return one or multiple values, pipe delimited.
E.g:
* 12345
* 123|456|789
These values can be cycled through by short-pressing a button.
Labels may be provided to these values which will be rendered on the upper row of the LCD. See the secrets sample file for examples.

Variables might need to be adjusted for specific use cases. Most of these are located in the header file labelled globals_t93.h.
The secrets_t93.h.sample file will need to be cloned, updated and have the .sample suffix removed.

A Gerber containing the PCB design is included in this repo. Along with a schematic indicating resistor and capacitor values etc.
A parts list will be added... eventually.

A test API is added, will need Python, FastAPI and a few other dependancies.
