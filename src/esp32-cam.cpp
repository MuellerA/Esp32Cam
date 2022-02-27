////////////////////////////////////////////////////////////////////////////////
// esp32-cam.cpp
////////////////////////////////////////////////////////////////////////////////

#include <nvs_flash.h>
#include <driver/ledc.h>
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

std::string jsonArr(const std::string &key, const std::vector<std::string> &arr)
{
  std::string json = "\"" + key + "\": [ " ;
  bool first{true} ;
  for (const std::string &ele : arr)
  {
    if (first)
      first = false ;
    else
      json += ", " ;
    json += "\"" + ele + "\"" ;
  }
  json += " ]" ;
  return json ;
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
  ESP_LOGD("SpiFs", "init()") ;
  
  if (esp_vfs_spiffs_register(&_conf) != ESP_OK)
  {
    ESP_LOGE("SpiFs", "esp_vfs_spiffs_register() failed") ;
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
  // AI Thinker
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
  _config.fb_count     = 1 ;          /*!< Number of frame buffers to be allocated. If more than one, then each frame will be acquired (double speed)  */
  _config.fb_location  = CAMERA_FB_IN_PSRAM ;     /*!< The location where the frame buffer will be allocated */
  _config.grab_mode    = CAMERA_GRAB_LATEST ;     /*!< When buffers should be filled */

  _light._pin = 4 ;
}

bool Camera::init()
{
  ESP_LOGD("Camera", "init()") ;

  if (esp_camera_init(&_config) != ESP_OK)
  {
    ESP_LOGE("Camera", "esp_camera_init() failed") ;
    return false ;
  }

  _sensor = esp_camera_sensor_get() ;
  if (!_sensor)
  {
    ESP_LOGE("Settings", "esp_camera_sensor_get() failed") ;
    return false ;
  }

  _inUse = xSemaphoreCreateMutex() ;
  
  if (_light._pin >= 0)
  {
    // Prepare and then apply the LEDC PWM timer configuration
    ledc_timer_config_t ledc_timer = {
                                      .speed_mode       = LEDC_LOW_SPEED_MODE,
                                      .duty_resolution  = LEDC_TIMER_13_BIT,
                                      .timer_num        = LEDC_TIMER_1,
                                      .freq_hz          = 5000,
                                      .clk_cfg          = LEDC_AUTO_CLK
    };
    if (ledc_timer_config(&ledc_timer) != ESP_OK)
    {
      ESP_LOGE("Camera", "ledc_timer_config() failed") ;
      return false ;
    }
    
    // Prepare and then apply the LEDC PWM channel configuration
    ledc_channel_config_t ledc_channel = {
                                          .gpio_num       = _light._pin,
                                          .speed_mode     = LEDC_LOW_SPEED_MODE,
                                          .channel        = LEDC_CHANNEL_1,
                                          .intr_type      = LEDC_INTR_DISABLE,
                                          .timer_sel      = LEDC_TIMER_1,
                                          .duty           = 0,
                                          .hpoint         = 0
    };
    if (ledc_channel_config(&ledc_channel) != ESP_OK)
    {
      ESP_LOGE("Camera", "ledc_channel_config() failed") ;
      return false ;
    }
  }
  return true ;
}

const sensor_t& Camera::sensor() const { return *_sensor ; }
sensor_t& Camera::sensor() { return *_sensor ; }
const Camera::Light& Camera::light() const { return _light ; }
Camera::Light& Camera::light() { return _light ; }

bool Camera::terminate()
{
  vSemaphoreDelete(_inUse) ;

  if (esp_camera_deinit() != ESP_OK)
    return false ;
  
  return true ;
}

bool Camera::capture(Data &data)
{
  if (!xSemaphoreTake(_inUse, 0))
    return false ;

  _light.capture(true) ;
  
  camera_fb_t* fb0 = esp_camera_fb_get() ;
  esp_camera_fb_return(fb0) ;
  
  camera_fb_t* fb = esp_camera_fb_get() ;
  if (fb)
  {
    data.assign(fb->buf, fb->buf + fb->len) ;
    esp_camera_fb_return(fb) ;
  }

  _light.capture(false) ;
  
  xSemaphoreGive(_inUse) ;
  return fb != nullptr ;
}

bool Camera::Light::brightness(uint8_t b)
{
  if (_pin < 0)
    return true ;
  
  _brightness = b ;
  if (_mode == Mode::on)
  {
    uint32_t duty = ((uint32_t)_brightness + 1) * 32 - 1 ;

    if (!setDuty(duty) || !updateDuty())
      return false ;
  }
  return true ;
}

bool Camera::Light::mode(Camera::Light::Mode m)
{
  if (_pin < 0)
    return true ;

  _mode = m ;
  uint32_t duty = (m == Mode::on) ? (((uint32_t)_brightness + 1) * 32 - 1) : 0 ;
  
  if (!setDuty(duty) || !updateDuty())
    return false ;
  return true ;
}

uint8_t Camera::Light::brightness() const { return _brightness ; }
Camera::Light::Mode Camera::Light::mode() const { return _mode ; }

void Camera::Light::capture(bool on)
{
  if (_pin < 0)
    return ;
  
  if (_mode != Camera::Light::Mode::capture)
    return ;

  uint32_t duty = on ? (((uint32_t)_brightness + 1) * 32 - 1) : 0 ;
  if (!setDuty(duty) || !updateDuty())
    return ;

  //if (on)
  //  vTaskDelay(50 / portTICK_PERIOD_MS) ;
  
  return ;
}

bool Camera::Light::setDuty(uint32_t duty)
{
  if (_pin < 0)
    return true ;
  
  if (ledc_set_duty(LEDC_LOW_SPEED_MODE, LEDC_CHANNEL_1, duty) != ESP_OK)
  {
    ESP_LOGE("Camera", "ledc_set_duty() failed") ;
    return false ;
  }
  return true ;
}

bool Camera::Light::updateDuty()
{
  if (_pin < 0)
    return true ;
  
  if (ledc_update_duty(LEDC_LOW_SPEED_MODE, LEDC_CHANNEL_1) != ESP_OK)
  {
    ESP_LOGE("Camera", "ledc_update_duty() failed") ;
    return false ;
  }
  return true ;
}

////////////////////////////////////////////////////////////////////////////////

static void terminate(void*)
{
  ESP_LOGD("Esp32Cam", "terminate()");

  httpd.stop() ;
  crypto.terminate() ;
  publicSettings.terminate() ;
  privateSettings.terminate() ;
  camera.terminate() ;
  spifs.terminate() ;

  esp_event_loop_delete_default() ;
  esp_netif_deinit() ;
  nvs_flash_deinit() ;
  
  esp_restart() ;
}

Terminator::Terminator()
{
  _timer = xTimerCreate("Terminator", 1000 / portTICK_PERIOD_MS, pdFALSE, 0, terminate) ;
}

void Terminator::hastaLaVistaBaby()
{
  if (xTimerStart(_timer, 0) != pdPASS)
  {
    ESP_LOGE("Esp32Cam", "start termination timer failed") ;
  }
}

Terminator terminator ;

////////////////////////////////////////////////////////////////////////////////

void setupSta(const std::string &ssid, const std::string &pwd)
{
  wifi_config_t wifi_config{} ;

  strcpy((char*)wifi_config.sta.ssid, ssid.c_str()) ;
  strcpy((char*)wifi_config.sta.password, pwd.c_str()) ;
    
  wifi_config.sta.threshold.authmode = WIFI_AUTH_WPA2_PSK ;

  wifi_config.sta.pmf_cfg.capable = true ;
  wifi_config.sta.pmf_cfg.required = false ;
    
  esp_wifi_set_config(WIFI_IF_STA, &wifi_config);
}

void setup()
{
  ESP_LOGD("Esp32Cam", "setup()");

  if (!spifs.init() ||
      !camera.init() ||
      !privateSettings.init() ||
      !publicSettings.init() ||
      !crypto.init())
    esp_restart() ;

  esp_ota_mark_app_valid_cancel_rollback() ;
  
  ESP_LOGD("Setup", "complete") ;
  
  std::string apSsid, apCountry, stSsid, stPwd, espName ;
  if (!privateSettings.get("wifi.ap-ssid", apSsid) ||
      !privateSettings.get("wifi.ap-country", apCountry) ||
      !privateSettings.get("wifi.st-ssid", stSsid) ||
      !privateSettings.get("wifi.st-pwd", stPwd) ||
      !publicSettings.get("esp.name", espName))
    esp_restart() ;

  {
    esp_netif_init() ;
    esp_event_loop_create_default() ;
    esp_netif_create_default_wifi_sta() ;
    wifi_init_config_t cfg = WIFI_INIT_CONFIG_DEFAULT();
    esp_wifi_init(&cfg) ;
    esp_wifi_set_storage(WIFI_STORAGE_RAM) ;

    if (esp_wifi_set_country_code(apCountry.c_str(), true) != ESP_OK)
    {
      ESP_LOGE("Esp32CAM", "esp_wifi_set_country_code() failed") ;
      esp_restart() ;
    }
    
    // ap
    {
      wifi_config_t wifi_config{} ;

      strcpy((char*)wifi_config.ap.ssid, apSsid.c_str()) ;
      wifi_config.ap.ssid_len = 0 ;
      wifi_config.ap.authmode = WIFI_AUTH_OPEN ;
      wifi_config.ap.pairwise_cipher = WIFI_CIPHER_TYPE_CCMP ;

      esp_wifi_set_config(WIFI_IF_AP, &wifi_config) ;
    }
    
    // sta
    if (stSsid.size())
    {
      esp_wifi_set_mode(WIFI_MODE_APSTA);
      setupSta(stSsid, stPwd) ;
    }
    else
      esp_wifi_set_mode(WIFI_MODE_AP);
    
    ESP_ERROR_CHECK(esp_event_handler_instance_register(WIFI_EVENT,
                                                        ESP_EVENT_ANY_ID,
                                                        &wifi_event_handler,
                                                        nullptr,
                                                        nullptr));
    ESP_ERROR_CHECK(esp_event_handler_instance_register(IP_EVENT,
                                                        IP_EVENT_STA_GOT_IP,
                                                        &ip_event_handler,
                                                        nullptr,
                                                        nullptr));

    esp_wifi_start();
  }

  if (!httpd.start())
    esp_restart() ;
}

////////////////////////////////////////////////////////////////////////////////

extern "C"
{
  void app_main()
  {
    ESP_LOGD("Esp32Cam", "appmain()");

    //esp_log_level_set("*", ESP_LOG_VERBOSE) ;
    
    // nvs -- not used by esp32-cam, but might from used library
    esp_err_t ret = nvs_flash_init();
    if (ret == ESP_ERR_NVS_NO_FREE_PAGES || ret == ESP_ERR_NVS_NEW_VERSION_FOUND) {
      nvs_flash_erase();
      ret = nvs_flash_init();
    }

    setup() ;
  }
}

////////////////////////////////////////////////////////////////////////////////
// EOF
////////////////////////////////////////////////////////////////////////////////
