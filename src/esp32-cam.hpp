////////////////////////////////////////////////////////////////////////////////
// esp32-cam.hpp
////////////////////////////////////////////////////////////////////////////////

#include <esp_log.h>
#include <esp_https_server.h>
#include <esp_camera.h>
#include <esp_spiffs.h>
#include <esp_ota_ops.h>
#include <esp_heap_caps.h>
#include <esp_wifi.h>
#include <mbedtls/md.h>
#include <freertos/timers.h>

#include <string>
#include <vector>
#include <map>
#include <functional>

////////////////////////////////////////////////////////////////////////////////

using Data = std::vector<uint8_t> ;
#include "settings.hpp"

////////////////////////////////////////////////////////////////////////////////

class SpiFs
{
public:
  SpiFs() ;
  bool init() ;
  bool terminate() ;
  
  bool read(const std::string &name, Data &data) ;
  bool write(const std::string &name, const Data &data) ;

  bool read(const std::string &name, std::string &str) ;
  bool write(const std::string &name, const std::string &str) ;

  bool df(size_t &total, size_t &used) ;
  
private:
  static std::string    _root ;
  esp_vfs_spiffs_conf_t _conf ;
} ;

extern SpiFs spifs ;

////////////////////////////////////////////////////////////////////////////////

class Camera
{
public:
  class Light
  {
    friend class Camera ;
  public:
    enum class Mode { off, capture, on } ;

    bool brightness(uint8_t b) ;
    bool mode(Mode m) ;
    uint8_t brightness() const ;
    Mode mode() const ;
    
    void capture(bool) ;

  private:
    bool setDuty(uint32_t d) ;
    bool updateDuty() ;
    
    Mode _mode{Mode::off} ;
    uint8_t _brightness{128} ;
    int _pin{-1} ;
  } ;

  Camera() ;
  bool init() ;
  bool terminate() ;

  bool capture(Data &data) ;
  
  const sensor_t& sensor() const ;
  sensor_t& sensor() ;

  const Light& light() const ;
  Light& light() ;

private:
  camera_config_t _config ;
  sensor_t    *_sensor{nullptr} ;
  Light _light ;
  SemaphoreHandle_t _inUse ;
} ;

extern Camera camera ;

////////////////////////////////////////////////////////////////////////////////

class HTTPD
{
  struct FileInfo
  {
    const char *_url ;
    const char *_type ;
    const char *_file ;
  } ;

public:
  enum class Mode { none, setup, running } ;
  
  ~HTTPD() ;
  bool start() ;
  bool mode(Mode) ;
  bool stop() ;

  static esp_err_t getFile(httpd_req_t *req) ;
  static esp_err_t redirect(httpd_req_t *req, const char *location) ;
  
private:
  httpd_handle_t    _httpd{nullptr} ;
  Mode              _mode{Mode::none} ;
  std::string       _certPem ;
  std::string       _keyPem ;
  static const FileInfo _staticUriCommon[] ;      // static content for modes setup & running
  static const FileInfo _staticUriSetup[] ;       // static content for mode setup
  static const FileInfo _staticUriRunning[] ;     // static content for mode running 
  static const httpd_uri_t _dynamicUriCommon[] ;  // dynamic content for modes setup & running
  static const httpd_uri_t _dynamicUriRunning[] ; // dynamic content for mode running
} ;

extern HTTPD httpd ;

////////////////////////////////////////////////////////////////////////////////


class MultiPart
{
public:
  struct Part
  {
    Part() ;
    Part(const uint8_t *hb, const uint8_t *he, const uint8_t *bb, const uint8_t *be) ;

    uint32_t headSize() const ;
    uint32_t bodySize() const ;
    
    const uint8_t *_headBegin ;
    const uint8_t *_headEnd ;
    const uint8_t *_bodyBegin ;
    const uint8_t *_bodyEnd ;
  } ;

public:
  MultiPart(httpd_req_t *req) ;
  ~MultiPart() ;
  
  bool parse() ;
  bool get(const std::string &name, Part &part) ;
  
private:
  using PartByName = std::map<std::string, Part> ;

  bool parseBody(const uint8_t *boundary, size_t boundarySize, const uint8_t *data, size_t dataSize) ;
  bool parseName(const uint8_t *head, size_t headSize, std::string &name) ;            
  
  httpd_req_t *_req ;
  uint8_t     *_content ;
  PartByName   _parts ;
} ;

extern const uint8_t* memmem(const uint8_t *buff, size_t size, const uint8_t *pattern, size_t patternSize) ;

////////////////////////////////////////////////////////////////////////////////

class Crypto
{
public:
  bool init() ;
  bool terminate() ;

  Data sha256(const Data &data) ;
  bool pwdCheck(const Data &pwd) ;
  
private:
  mbedtls_md_context_t _mdCtx ;
} ;

extern Crypto crypto ;

////////////////////////////////////////////////////////////////////////////////

class Terminator
{
public:
  Terminator() ;
  void hastaLaVistaBaby() ;

private:
  TimerHandle_t _timer ;
} ;

extern Terminator terminator ;

////////////////////////////////////////////////////////////////////////////////

extern esp_err_t ota(httpd_req_t *req) ;
extern esp_err_t wifiSetup(httpd_req_t *req) ;

void wifi_event_handler(void* arg, esp_event_base_t event_base, int32_t event_id, void* event_data) ;
void ip_event_handler(void* arg, esp_event_base_t event_base, int32_t event_id, void* event_data) ;

////////////////////////////////////////////////////////////////////////////////

template<class T>
std::string to_s(T i) ;

template<class T>
bool to_i(const std::string &s, T &i) ;

std::string jsonStr(const std::string &key, const std::string &val) ;
std::string jsonInt(const std::string &key, const std::string &val) ;
std::string jsonInt(const std::string &key, int32_t val) ;
std::string jsonArr(const std::string &key, const std::vector<std::string> &arr) ;

////////////////////////////////////////////////////////////////////////////////

std::string mac_to_s(uint8_t *mac) ;
std::string ip_to_s(uint8_t *ip) ;
void setupSta(const std::string &ssid, const std::string &pwd) ;

////////////////////////////////////////////////////////////////////////////////
// EOF
////////////////////////////////////////////////////////////////////////////////
