////////////////////////////////////////////////////////////////////////////////
// esp32-cam.cpp
////////////////////////////////////////////////////////////////////////////////

#include <WiFi.h>
#include <WiFiManager.h>
#include <esp_https_server.h>
#include <esp_camera.h>
#include <esp_spiffs.h>

#include "esp32-cam.hpp"

////////////////////////////////////////////////////////////////////////////////

SpiFs spifs ;
Camera camera ;
HTTPD  httpd ;

////////////////////////////////////////////////////////////////////////////////

std::string SpiFs::_root{"/spiffs/"} ;

SpiFs::SpiFs() : _conf{ .base_path = "/spiffs",
                        .partition_label = nullptr,
                        .max_files = 3,
                        .format_if_mount_failed = false }
{}

bool SpiFs::init()
{
  Serial.println("SpiFs::init()") ;
  
  if (esp_vfs_spiffs_register(&_conf) != ESP_OK)
  {
    Serial.println("esp_vfs_spiffs_register() failed") ;
    return false ;
  }
  return true ;
}

bool SpiFs::terminate()
{
  esp_vfs_spiffs_unregister(_conf.partition_label);
  return true ;
}

bool SpiFs::read(const std::string &name, Data &data)
{
  FILE *file = fopen((_root + name).c_str(), "rb") ;
  if (!file)
    return false ;
  
  uint8_t buff[1024] ;
  size_t size ;
  while ((size = fread(buff, 1, sizeof(buff), file)))
    data.insert(data.end(), buff, buff+size) ;

  fclose(file) ;

  return true ;
}
  
////////////////////////////////////////////////////////////////////////////////

Camera::Camera()
{
  _config.pin_pwdn     = 32 ;         /*!< GPIO pin for camera power down line */
  _config.pin_reset    = -1 ;         /*!< GPIO pin for camera reset line */
  _config.pin_xclk     =  0 ;         /*!< GPIO pin for camera XCLK line */
  _config.pin_sscb_sda = 26 ;         /*!< GPIO pin for camera SDA line */
  _config.pin_sscb_scl = 27 ;         /*!< GPIO pin for camera SCL line */
  _config.pin_d7       = 35 ;         /*!< GPIO pin for camera D7 line */
  _config.pin_d6       = 34 ;         /*!< GPIO pin for camera D6 line */
  _config.pin_d5       = 39 ;         /*!< GPIO pin for camera D5 line */
  _config.pin_d4       = 36 ;         /*!< GPIO pin for camera D4 line */
  _config.pin_d3       = 21 ;         /*!< GPIO pin for camera D3 line */
  _config.pin_d2       = 19 ;         /*!< GPIO pin for camera D2 line */
  _config.pin_d1       = 18 ;         /*!< GPIO pin for camera D1 line */
  _config.pin_d0       =  5 ;         /*!< GPIO pin for camera D0 line */
  _config.pin_vsync    = 25 ;         /*!< GPIO pin for camera VSYNC line */
  _config.pin_href     = 23 ;         /*!< GPIO pin for camera HREF line */
  _config.pin_pclk     = 22 ;         /*!< GPIO pin for camera PCLK line */
  
  _config.xclk_freq_hz = 16500000 ;   /*!< Frequency of XCLK signal, in Hz. EXPERIMENTAL: Set to 16MHz on ESP32-S2 or ESP32-S3 to enable EDMA mode */

  _config.ledc_timer   = LEDC_TIMER_0 ;   /*!< LEDC timer to be used for generating XCLK  */
  _config.ledc_channel = LEDC_CHANNEL_0 ; /*!< LEDC channel to be used for generating XCLK  */

  _config.pixel_format = PIXFORMAT_JPEG ; /*!< Format of the pixel data: PIXFORMAT_ + YUV422|GRAYSCALE|RGB565|JPEG  */
  _config.frame_size   = FRAMESIZE_SVGA ; /*!< Size of the output image: FRAMESIZE_ + QVGA|CIF|VGA|SVGA|XGA|SXGA|UXGA  */

  _config.jpeg_quality = 5 ;          /*!< Quality of JPEG output. 0-63 lower means higher quality  */
  _config.fb_count     = 2 ;          /*!< Number of frame buffers to be allocated. If more than one, then each frame will be acquired (double speed)  */
  //_config.fb_location  = CAMERA_FB_IN_PSRAM ;     /*!< The location where the frame buffer will be allocated */
  //_config.grab_mode    = CAMERA_GRAB_WHEN_EMPTY ; /*!< When buffers should be filled */
}

bool Camera::init()
{
  Serial.println("Camera::init()") ;
  if (esp_camera_init(&_config) != ESP_OK)
  {
    Serial.println("esp_camera_init() failed") ;
    return false ;
  }
  return true ;
}

bool Camera::terminate()
{
  if (esp_camera_deinit() != ESP_OK)
    return false ;
  
  return true ;
}

bool Camera::capture(Data &data)
{
  camera_fb_t* fb = esp_camera_fb_get() ;
  if (!fb)
    return false ;
  data.assign(fb->buf, fb->buf + fb->len) ;
  esp_camera_fb_return(fb) ;

  return true ;
}


////////////////////////////////////////////////////////////////////////////////

static const httpd_uri_t uris[] =
  {
   {
    "/",
    HTTP_GET,
    [](httpd_req_t *req){ return HTTPD::redirect(req, "/esp32-cam.html") ; }
   },
   {
    "/index.html",
    HTTP_GET,
    [](httpd_req_t *req){ return HTTPD::redirect(req, "esp32-cam.html") ; }
   },
   {
    "/favicon.ico",
    HTTP_GET,
    [](httpd_req_t *req){ return HTTPD::getFile(req, "image/x-icon", "camera-40.ico") ; }
   },
   {
    "/camera-16.png",
    HTTP_GET,
    [](httpd_req_t *req){ return HTTPD::getFile(req, "image/png", "camera-16.png") ; }
   },
   {
    "/camera-32.png",
    HTTP_GET,
    [](httpd_req_t *req){ return HTTPD::getFile(req, "image/png", "camera-32.png") ; }
   },
   {
    "/camera-64.png",
    HTTP_GET,
    [](httpd_req_t *req){ return HTTPD::getFile(req, "image/png", "camera-64.png") ; }
   },
   {
    "/esp32-cam.html",
    HTTP_GET,
    [](httpd_req_t *req){ return HTTPD::getFile(req, "text/html", "esp32-cam.html") ; }
   },
   {
    "/esp32-cam.css",
    HTTP_GET,
    [](httpd_req_t *req){ return HTTPD::getFile(req, "text/css", "esp32-cam.css") ; }
   },
   {
    "/esp32-cam.js",
    HTTP_GET,
    [](httpd_req_t *req){ return HTTPD::getFile(req, "text/javascript", "esp32-cam.js") ; }
   },
   {
    "/capture.jpg",
    HTTP_GET,
    [](httpd_req_t *req)
    {
      Data data ;
      if (!camera.capture(data))
      {
        httpd_resp_send_err(req, HTTPD_500_INTERNAL_SERVER_ERROR, "camera capture failed") ;
        return ESP_FAIL ;
      }
         
      httpd_resp_set_type(req, "image/jpeg") ;
      httpd_resp_send(req, (const char*) data.data(), data.size()) ;

      return ESP_OK ;
    }
   },
   {  
    "/stream",
    HTTP_GET,
    [](httpd_req_t *req)
    {
      static std::string boundary{"esp32camstreampart"} ;
      static std::string nl{"\r\n"} ;

      esp_err_t res ;
      std::string str ;
      str  = "multipart/x-mixed-replace; boundary=" ;
      str += boundary ;
      Serial.print(str.c_str()) ;
      if ((res = httpd_resp_set_type(req, str.c_str())) != ESP_OK)
        return res ;

      while (true) // send images
      {
        delay(2000) ;
        Data data ;
        if (!camera.capture(data))
        {
          return ESP_FAIL ;
        }
            
        char length[32] ;
        snprintf(length, sizeof(length), "%zd", data.size()) ;
            
        str  = "--" + boundary + nl ;
        str += "Content-Type: image/jpeg" + nl ;
        str += "Content-Length: " + std::string(length) + nl ;
        str += nl ;
        Serial.print(str.c_str()) ;
        if (((res = httpd_resp_send_chunk(req, str.data(), str.size())) != ESP_OK) ||
            ((res = httpd_resp_send_chunk(req, (const char*) data.data(), data.size())) != ESP_OK))
          return res ;
      }
    }
   }
  } ;

HTTPD::~HTTPD()
{
  stop() ;
}

bool HTTPD::start()
{
  Serial.println("HTTPD::start()") ;
  httpd_ssl_config_t cfg = HTTPD_SSL_CONFIG_DEFAULT() ;

  cfg.httpd.max_uri_handlers = 16 ;
  
  if (!spifs.read("cert.pem", _cert))
  {
    Serial.println("read cert.pem failed") ;
    return false ;
  }
  cfg.cacert_pem = _cert.data() ;
  cfg.cacert_len = _cert.size() ;
  
  if (!spifs.read("key.pem", _key))
  {
    Serial.println("read key.pem failed") ;
    return false ;
  }
  cfg.prvtkey_pem = _key.data() ;
  cfg.prvtkey_len = _key.size() ;

  cfg.transport_mode = HTTPD_SSL_TRANSPORT_INSECURE ;
  
  if (httpd_ssl_start(&_httpd, &cfg) != ESP_OK)
  {
    Serial.println("httpd_ssl_start() failed") ;
    return false ;
  }

  for (const auto &uri : uris)
    httpd_register_uri_handler(_httpd, &uri);

  return true ;
}

bool HTTPD::stop()
{
  if (_httpd)
  {
    httpd_ssl_stop(_httpd);
    _httpd = nullptr ;
  }
  _cert.clear() ;
  _key .clear() ;

  return true ;
}

esp_err_t HTTPD::getFile(httpd_req_t *req, const char *type, const char *filename)
{
  Data data ;

  if (!spifs.read(filename, data))
  {
    Serial.println("read file failed") ;
    return ESP_ERR_NOT_FOUND ;
  }

  httpd_resp_set_type(req, type);
  httpd_resp_send(req, (const char*) data.data(), data.size()) ;

  return ESP_OK ;
}

esp_err_t HTTPD::redirect(httpd_req_t *req, const char *location)
{
  std::string msg ;
  msg += "HTTP/1.1 301 Moved Permanently\r\n" ;
  msg += "Location: " ;
  msg += location ;
  msg += "\r\n\r\n" ;
  
  httpd_send(req, msg.c_str(), msg.size()) ;

  return ESP_FAIL ; // close connection
}

////////////////////////////////////////////////////////////////////////////////

void setup()
{
  WiFiManager wm;

  WiFi.mode(WIFI_STA);
  Serial.begin(115200) ;
  Serial.println("Setup") ;
  
  if (!wm.autoConnect("ESP32-CAM", "ESP32-CAM"))
  {
    Serial.println("WiFiManager::autoConnect() failed") ;
    ESP.restart() ;
  }

  if (!spifs.init() ||
      !camera.init() ||
      !httpd.start())
    ESP.restart() ;

  Serial.println("Setup complete") ;
}

void terminate()
{
  httpd.stop() ;
  camera.terminate() ;
  spifs.terminate() ;
}

////////////////////////////////////////////////////////////////////////////////

void loop()
{
}

////////////////////////////////////////////////////////////////////////////////
// EOF
////////////////////////////////////////////////////////////////////////////////
