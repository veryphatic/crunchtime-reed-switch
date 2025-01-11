# CrunchTime

CrunchTime is a small device that can be hidden inside objects. It can detect
RFID tags and will send an OSC message to the specified IP address and port.

The OSC messages are compatible with qLab.

More details at: [http://reprage.com/post/crunchtime](http://reprage.com/post/crunchtime)

## Build notes

How to build.

1. Open VSCode to the Crunchtime application
2. Edit the `main.cpp` file to change the sensor and hostname ID and save
3. Click the Alien icon, and under `d1_mini` -> `General` run `Build`
4. Make sure the device is plugged in
5. Under `d1_mini` -> `General` run `Upload and monitor`
6. If succesful you should see `WiFi Connected!` and `Firmware Version: 0x92 = v2.0`

### Windows 11 Support

- Don't use Windows 11 Silicon labs CP210x Windows drivers v6.7.6 with the D1 Mini; it won't work.

### .pio/libdeps/d1_mini/OSC/SLIPEncodedBluetoothSerial.h:11:10: fatal error: BluetoothSerial.h: No such file or directory

The CMAT OSC library contains additional headers that are missing references to BluetoothSerial.h. 
Since this application does not require BluetoothSerial, the two affected files can be removed
- `.pio/libdeps/d1_mini/OSC/SLIPEncodedBluetoothSerial.h`
- `.pio/libdeps/d1_mini/OSC/SLIPEncodedBluetoothSerial.cpp`

### Lonely Binary board support

As of January 2025 new replacement boards are using the Lonely Binary D1 Mini ESP8266EX boards. Additional confirmations have been added to `platform.io` to support this board.

### Apple Silicon

You'll need to have Rosetta installed to support the C++ libraries. By default it's not. You can install this by opening the application info of Terminal app, and ticking the 'Open with Rosetta' option. Once the app starts Rosettta will install


## Changelog

_11/01/2025_

- Updated readme
- Updated `platformio.ini` with `d1_mini` environment
- Added hostname to sensor boards


## License

Permission is hereby granted, free of charge, to any person obtaining a copy of
this software and associated documentation files (the "Software"), to deal in
the Software without restriction, including without limitation the rights to
use, copy, modify, merge, publish, distribute, sublicense, and/or sell copies of
the Software, and to permit persons to whom the Software is furnished to do so,
subject to the following conditions:

The above copyright notice and this permission notice shall be included in all
copies or substantial portions of the Software.

THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY, FITNESS
FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE AUTHORS OR
COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER LIABILITY, WHETHER
IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM, OUT OF OR IN
CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE SOFTWARE.
