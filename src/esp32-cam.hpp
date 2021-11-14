////////////////////////////////////////////////////////////////////////////////
// esp32-cam.hpp
////////////////////////////////////////////////////////////////////////////////

#include <WiFi.h>
#include <WiFiManager.h>
#include <esp_https_server.h>
#include <esp_camera.h>
#include <esp_spiffs.h>
#include <esp_task_wdt.h>

#include <string>
#include <vector>
#include <map>

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
  ~HTTPD() ;
  bool start() ;
  bool stop() ;

  static esp_err_t getFile(httpd_req_t *req) ;
  static esp_err_t redirect(httpd_req_t *req, const char *location) ;
  
private:
  httpd_handle_t    _httpd{nullptr} ;
  Data              _cert ;
  Data              _key ;
  static const FileInfo _fileInfos[] ;
} ;

extern HTTPD httpd ;

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
// EOF
////////////////////////////////////////////////////////////////////////////////
