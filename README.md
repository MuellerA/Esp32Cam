# <img src="icon/camera.svg" alt="ESP32 CAM logo" width="42px"/> ESP32 CAM 

**Work in progress**

## Install

### Erase ESP flash (once)
```
pio run -t erase
```

### Build & Upload Firmware
```
pio run -t upload
```

### Prepare & Upload File System

* Create cert and key (RSA or ECC) for HTTPS (HTTPS is disabled, too many out-of-memory errors)
  * RSA: ```openssl req -x509 -subj "/CN=ESP32-CAM" -nodes -days 9999 -keyout data/key.pem -out data/cert.pem -newkey rsa:2048```
  * ECC: ```openssl req -x509 -subj "/CN=ESP32-CAM" -nodes -days 9999 -keyout data/key.pem -out data/cert.pem -newkey ec:<(openssl ecparam -name prime256v1)```

* Modify data/settings.txt
  * global.name: display name
  * camera.*: camera settings
* Modify data/secret.txt
  * esp.salt: random value (eg ```dd if=/dev/random bs=50 count=1 | base64```)
  * esp.pwdHash: sha256 hash of concatination of salt and user password (```echo -n "saltpassword" | sha256sum```)
  * wlan.*: wifi network settings
  
```
pio run -t uploadfs
```

## TODO

* password rename setting
* wlan rename wifi
* gitignore settings.txt secret.txt

* more settings
* https out of memory???
* remove WiFiManager
* remove Arduino?
* record to sdcard
* spiffs ota
