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

* Create Certs for HTTPS (HTTPS is disabled, too many out-of-memory errors)

  * RSA: ```openssl req -newkey rsa:2048 -nodes -keyout data/key.pem -x509 -days 9999 -out data/cert.pem -subj "/CN=ESP32-CAM"```
  * ECC: ```openssl req -x509 -subj "/CN=ESP32-CAM" -nodes -days 9999 -newkey ec:<(openssl ecparam -name prime256v1) -keyout data/key.pem -out data/cert.pem```

* Modify Display Name in ```data/settings.txt```

```
pio run -t uploadfs
```

## TODO

* more settings
* https out of memory???
* remove WiFiManager
* remove Arduino?
* record to sdcard
* spiffs ota
