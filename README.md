# Attentive: open source AT/GSM stack in pure C

*[NOTE: Open sourced internal company product. Work in progress. Not production/release ready yet.]*

## Features

* Full AT stack (command generation + response parsing).
* Modular design.
* Written in pure ANSI C99 with optional POSIX addons.
* Compliant with [ITU V.250](https://www.itu.int/rec/T-REC-V.250/en) and
  [3GPP TS 27.007](http://www.3gpp.org/DynaReport/27007.htm) for maximum interoperatibility.
* Tolerant to even the most misbehaving modems out there (if instructed so).
* Easily extendable to support new modems.

## Supported modem APIs

* TCP/IP support (sockets).
* FTP transfers.
* Date/time operations.
* BTS-based location.

## Supported modems

* Generic (Hayes AT, GSM, etc.)
* Telit modems (SELINT 2 compatibility level)
* Simcom SIM800 series
