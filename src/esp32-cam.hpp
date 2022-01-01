////////////////////////////////////////////////////////////////////////////////
// esp32-cam.hpp
////////////////////////////////////////////////////////////////////////////////

#include <WiFi.h>

#include <esp_https_server.h>
#include <esp_camera.h>
#include <esp_spiffs.h>
#include <esp_ota_ops.h>
#include <esp_heap_caps.h>
#include <esp_wifi.h>
#include <mbedtls/md.h>

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
  Camera() ;
  bool init() ;
  bool terminate() ;

  bool capture(Data &data) ;
  
private:
  camera_config_t _config ;
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
  enum class Mode { none, wifi, full } ;
  
  ~HTTPD() ;
  bool start() ;
  bool mode(Mode) ;
  bool stop() ;

  static esp_err_t getFile(httpd_req_t *req) ;
  static esp_err_t redirect(httpd_req_t *req, const char *location) ;
  
private:
  httpd_handle_t    _httpd{nullptr} ;
  Mode              _mode{Mode::none} ;
  Data              _cert ;
  Data              _key ;
  static const FileInfo _staticUriAll[] ;
  static const FileInfo _staticUriWifi[] ;
  static const FileInfo _staticUriFull[] ;
  static const httpd_uri_t _dynamicUriAll[] ;
  static const httpd_uri_t _dynamicUriFull[] ;
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

class Crypt
{
public:
  bool init() ;
  bool terminate() ;

  Data sha256(const Data &data) ;
  bool pwdCheck(const Data &pwd) ;
  
private:
  mbedtls_md_context_t _mdCtx ;
} ;

extern Crypt crypt ;

////////////////////////////////////////////////////////////////////////////////

class Terminator
{
public:
  void hastaLaVistaBaby() ;
  bool operator()() ;

private:
  int64_t _time ;
} ;

extern Terminator terminator ;

////////////////////////////////////////////////////////////////////////////////

extern esp_err_t ota(httpd_req_t *req) ;
extern esp_err_t wifiSetup(httpd_req_t *req) ;

extern void onWiFiStGotIp(WiFiEvent_t ev, WiFiEventInfo_t info) ;
extern void onWifiStLostIp(WiFiEvent_t ev, WiFiEventInfo_t info) ;
extern void onWifiApConnect(WiFiEvent_t ev, WiFiEventInfo_t info) ;
extern void onWifiApDisconnect(WiFiEvent_t ev, WiFiEventInfo_t info) ;

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

////////////////////////////////////////////////////////////////////////////////
// EOF
////////////////////////////////////////////////////////////////////////////////
