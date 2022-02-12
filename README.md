# <img src="icon/camera.svg" alt="ESP32 CAM logo" width="42px"/> ESP32 CAM 

**Work in progress**

## Install

### Erase ESP flash (once)
```
pio run -t erase
```

### Configure ESP IDF Framework (if neccessary)
```
pio run -t menuconfig
```

### Build & Upload Firmware
```
pio run -t upload
```

### Prepare & Upload File System

* Create cert.der and key.der (RSA or ECC) for HTTPS
  * RSA: ```openssl req -x509 -subj "/CN=ESP32-CAM" -nodes -days 9999 -keyout data/key.pem -out data/cert.pem -newkey rsa:2048```
  * ECC: ```openssl req -x509 -subj "/CN=ESP32-CAM" -nodes -days 9999 -keyout data/key.pem -out data/cert.pem -newkey ec:<(openssl ecparam -name prime256v1)```
  * Convert to DER:
    * ```openssl x509 -in data/cert.pem -outform DER -out data/cert.der```
    * ```openssl ec   -in data/key.pem  -outform DER -out data/key.der```

* Copy data/settings.template.txt to data/settings.txt and edit
  * esp.name: display name
  * camera.*: camera settings
* Copy data/secret.template.txt to data/secret.txt and edit
  * esp.salt: random value (eg ```dd if=/dev/random bs=50 count=1 | base64```)
  * esp.pwdHash: sha256 hash of concatination of salt and user password (```(echo -n ${esp_salt} ; echo -n 'mypassword') | sha256sum```)
  * wlan.ap-*: settings for wifi soft access point
  * wlan.st-*: settings for wifi station mode
  
```
pio run -t uploadfs
```

## TODO

* more settings
* record to sdcard
* spiffs upload
