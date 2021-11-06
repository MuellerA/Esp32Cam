# ESP32-CAM


## Certs (RSA, ECC(NIST))

```
openssl req -newkey rsa:2048 -nodes -keyout prvtkey.pem -x509 -days 9999 -out cacert.pem -subj "/CN=ESP32-CAM"
openssl req -x509 -subj "/CN=ESP32-CAM" -nodes -days 9999 -newkey ec:<(openssl ecparam -name prime256v1) -keyout key.pem -out cert.pem
```

## Erase, Build, Upload FS (data dir)

```
~/.platformio/penv/bin/pio --no-ansi run -d ~/PlatformIO/esp32-cam -t erase
~/.platformio/penv/bin/pio --no-ansi run -d ~/PlatformIO/esp32-cam -t upload
~/.platformio/penv/bin/pio --no-ansi run -d ~/PlatformIO/esp32-cam -t uploadfs
```

## TODO

* http redirect
* http stream
* html,js options
