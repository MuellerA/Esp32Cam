////////////////////////////////////////////////////////////////////////////////
// esp32-cam.cpp
////////////////////////////////////////////////////////////////////////////////

#include "esp32-cam.hpp"

////////////////////////////////////////////////////////////////////////////////

SpiFs spifs ;
Camera camera ;

////////////////////////////////////////////////////////////////////////////////

template<class T>
std::string to_s(T val)
{
  if (!val)
    return "0" ;
  
  bool neg = val < 0 ;
  if (neg)
    val = - val ;
  
  static uint8_t digits[] = { '0', '1', '2', '3', '4', '5', '6', '7', '8', '9' } ;
  char buff[20] ;

  size_t i = sizeof(buff) ;
  while (val)
  {
    buff[--i] = digits[val % 10] ;
    val = val / 10 ;
  }

  if (neg)
    buff[--i] = '-' ;

  return std::string(buff+i, sizeof(buff)-i) ;
}

// explicit template instantiation
template std::string to_s<uint8_t>(uint8_t) ;
template std::string to_s<int16_t>(int16_t) ;
template std::string to_s<int32_t>(int32_t) ;

template<class T>
bool to_i(const std::string &str, T &val)
{
  if (str.size() == 0)
    return false ;
  size_t iStr{0} ;
  size_t eStr{str.size()} ;
  bool neg{false} ;
  if ((str[0] == '-') || (str[0] == '+'))
  {
    iStr = 1 ;
    neg = str[0] == '-' ;
  }
  if (eStr > (4 + iStr))
    return false ;

  val = 0 ;
  while (iStr < eStr)
  {
    char ch = str[iStr++] ;
    if ((ch < '0') || ('9' < ch))
      return false ;
    val = val*10 + ch - '0' ;
  }
  if (neg)
    val = -val ;

  return true ;
}

// explicit template instantiation
template bool to_i<uint8_t>(const std::string&, uint8_t&) ;
template bool to_i<int16_t>(const std::string&, int16_t&) ;

////////////////////////////////////////////////////////////////////////////////

std::string mac_to_s(uint8_t *mac)
{
  std::string str ;
  char buff[8] ;
  bool first{true} ;
  for (size_t i = 0 ; i < 6 ; ++i)
  {
    if (first)
      first = false ;
    else
      str += ":" ;
    sprintf(buff, "%02X", mac[i]) ;
    str += buff ;
  }
  return str ;
}

std::string ip_to_s(uint8_t *ip)
{
  std::string str ;
  char buff[8] ;
  bool first{true} ;
  for (size_t i = 0 ; i < 4 ; ++i)
  {
    if (first)
      first = false ;
    else
      str += "." ;
    sprintf(buff, "%u", ip[i]) ;
    str += buff ;
  }
  return str ;
}

////////////////////////////////////////////////////////////////////////////////

std::string jsonStr(const std::string &key, const std::string &val)
{
  return "\"" + key + "\": \"" + val + "\"" ;
}

std::string jsonInt(const std::string &key, const std::string &val)
{
  return "\"" + key + "\": " + val ;
}

std::string jsonInt(const std::string &key, int32_t val)
{
  return jsonInt(key, to_s(val)) ;
}

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
  data.clear() ;
  while ((size = fread(buff, 1, sizeof(buff), file)))
    data.insert(data.end(), buff, buff+size) ;

  fclose(file) ;

  return true ;
}

bool SpiFs::write(const std::string &name, const Data &data)
{
  FILE *file = fopen((_root + name).c_str(), "wb") ;
  if (!file)
    return false ;
  
  bool res = fwrite(data.data(), 1, data.size(), file) == data.size() ;

  fclose(file) ;
  return res ;
}

bool SpiFs::read(const std::string &name, std::string &str)
{
  FILE *file = fopen((_root + name).c_str(), "rb") ;
  if (!file)
    return false ;
  
  char buff[1024] ;
  size_t size ;
  str.clear() ;
  while ((size = fread(buff, 1, sizeof(buff), file)))
    str.append(buff, size) ;

  fclose(file) ;

  return true ;
}

bool SpiFs::write(const std::string &name, const std::string &str)
{
  FILE *file = fopen((_root + name).c_str(), "wb") ;
  if (!file)
    return false ;
  
  bool res = fwrite(str.data(), 1, str.size(), file) == str.size() ;

  fclose(file) ;
  return res ;
}

bool SpiFs::df(size_t &total, size_t &used)
{
  return esp_spiffs_info(_conf.partition_label, &total, &used) == ESP_OK ;
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

void Terminator::hastaLaVistaBaby()
{
  _time = esp_timer_get_time() + 1000000 ;
}

bool Terminator::operator()()
{
  return
    (_time != 0) &&
    (_time < esp_timer_get_time()) ;
}

Terminator terminator ;

////////////////////////////////////////////////////////////////////////////////

void setup()
{
  Serial.begin(115200) ;
  Serial.println("Setup") ;

  if (!spifs.init() ||
      !camera.init() ||
      !privateSettings.init() ||
      !publicSettings.init() ||
      !crypt.init())
    ESP.restart() ;

  esp_ota_mark_app_valid_cancel_rollback() ;
  
  Serial.println("Setup complete") ;
  
  std::string apSsid, apCountry, stSsid, stPwd, espName ;
  if (!privateSettings.get("wifi.ap-ssid", apSsid) ||
      !privateSettings.get("wifi.ap-country", apCountry) ||
      !privateSettings.get("wifi.st-ssid", stSsid) ||
      !privateSettings.get("wifi.st-pwd", stPwd) ||
      !publicSettings.get("esp.name", espName))
    ESP.restart() ;

  esp_wifi_set_storage(WIFI_STORAGE_RAM) ;
  WiFi.mode(WIFI_MODE_APSTA) ;
  WiFi.onEvent(onWiFiStGotIp, SYSTEM_EVENT_STA_GOT_IP) ;
  WiFi.onEvent(onWifiStLostIp, SYSTEM_EVENT_STA_LOST_IP) ;
  WiFi.onEvent(onWifiApConnect, SYSTEM_EVENT_AP_STACONNECTED) ;
  WiFi.onEvent(onWifiApDisconnect, SYSTEM_EVENT_AP_STADISCONNECTED) ;
  WiFi.softAP(apSsid.c_str()) ;
  if (stSsid.size())
    WiFi.begin(stSsid.c_str(), stPwd.c_str()) ;

  if (!httpd.start())
    ESP.restart() ;
}

void terminate()
{
  httpd.stop() ;
  crypt.terminate() ;
  publicSettings.terminate() ;
  privateSettings.terminate() ;
  camera.terminate() ;
  spifs.terminate() ;

  ESP.restart() ;
}

////////////////////////////////////////////////////////////////////////////////

void loop()
{
  if (terminator())
  {
    terminate() ;
  }
}

////////////////////////////////////////////////////////////////////////////////
// EOF
////////////////////////////////////////////////////////////////////////////////
