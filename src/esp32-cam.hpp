////////////////////////////////////////////////////////////////////////////////
// esp32-cam.hpp
////////////////////////////////////////////////////////////////////////////////

using Data = std::vector<uint8_t> ;

////////////////////////////////////////////////////////////////////////////////

class SpiFs
{
public:
  SpiFs() ;
  bool init() ;
  bool terminate() ;
  
  bool read(const std::string &name, Data &data) ;
  
private:
  static std::string    _root ;
  esp_vfs_spiffs_conf_t _conf ;
} ;

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

////////////////////////////////////////////////////////////////////////////////

class HTTPD
{
public:
  ~HTTPD() ;
  bool start() ;
  bool stop() ;

  static esp_err_t getFile(httpd_req_t *req, const char *type, const char *filename) ;
  static esp_err_t redirect(httpd_req_t *req, const char *location) ;
  
private:
  httpd_handle_t _httpd{nullptr} ;
  Data           _cert ;
  Data           _key ;
} ;

////////////////////////////////////////////////////////////////////////////////
// EOF
////////////////////////////////////////////////////////////////////////////////