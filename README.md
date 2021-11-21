# ESP32-CAM

**Work in progress**

## Install

### Erase ESP flash (once)
```
pio run -t erase
```

### Build & Upload
```
pio run -t upload
```

### Prepare File System

* Create Certs for HTTPS (HTTPS is disabled, too many out-of-memory errors)

  * RSA: ```openssl req -newkey rsa:2048 -nodes -keyout data/key.pem -x509 -days 9999 -out data/cert.pem -subj "/CN=ESP32-CAM"```
  * ECC: ```openssl req -x509 -subj "/CN=ESP32-CAM" -nodes -days 9999 -newkey ec:<(openssl ecparam -name prime256v1) -keyout data/key.pem -out data/cert.pem```

* Modify Display Name in ```data/settings.txt```

### Upload File System
```
pio run -t uploadfs
```

## TODO

* settings
* https out of memory???
* remove WiFiManager
* remove Arduino?
* record to sdcard
* spiffs ota
