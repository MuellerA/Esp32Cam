////////////////////////////////////////////////////////////////////////////////
// httpd.cpp
////////////////////////////////////////////////////////////////////////////////

#include "esp32-cam.hpp"

////////////////////////////////////////////////////////////////////////////////

HTTPD  httpd ;

////////////////////////////////////////////////////////////////////////////////

esp_err_t cmdInfo(httpd_req_t *req)
{
  // todo
  return ESP_OK ;
}

const HTTPD::FileInfo HTTPD::_fileInfos[]
  {
   { "/favicon.ico"   , "image/x-icon"    , "camera-40.ico"  },
   { "/camera.svg"    , "image/svg+xml"   , "camera.svg"     },
   { "/camera-16.png" , "image/png"       , "camera-16.png"  },
   { "/camera-32.png" , "image/png"       , "camera-32.png"  },
   { "/camera-64.png" , "image/png"       , "camera-64.png"  },
   { "/esp32-cam.html", "text/html"       , "esp32-cam.html" },
   { "/esp32-cam.css" , "text/css"        , "esp32-cam.css"  },
   { "/esp32-cam.js"  , "text/javascript" , "esp32-cam.js"   },
   { "/settings.txt"  , "text/plain"      , "settings.txt"   },
  } ;

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
    "/capture.jpg",
    HTTP_GET,
    [](httpd_req_t *req)
    {
      Data data ;
      if (!camera.capture(data))
      {
        Serial.println("camera caputure failed") ;
        httpd_resp_send_err(req, HTTPD_500_INTERNAL_SERVER_ERROR, "camera capture failed") ;
        return ESP_OK ;
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
      static std::string nl{"\r\n"} ;
      static std::string boundaryDef{"esp32camstreampart"} ;
      static std::string boundary{"--" + boundaryDef + nl} ;
      static std::string contentTypeDef{"multipart/x-mixed-replace; boundary=" + boundaryDef} ;
      static std::string contentType{"Content-Type: image/jpeg" + nl} ;

      esp_err_t res ;
      Serial.println(contentTypeDef.c_str()) ;
      if ((res = httpd_resp_set_type(req, contentTypeDef.c_str())) != ESP_OK)
        return res ;

      Serial.println(boundary.c_str()) ;
      if ((res = httpd_resp_send_chunk(req, boundary.data(), boundary.size())) != ESP_OK)
        return res ;

      while (true) // send images
      {
        Data data ;
        if (!camera.capture(data))
        {
          Serial.println("camera caputure failed") ;
          return ESP_FAIL ;
        }
            
        char length[32] ;
        snprintf(length, sizeof(length), "%zd", data.size()) ;
            
        std::string str ;
        str += contentType ;
        str += "Content-Length: " + std::string(length) + nl ;
        str += nl ;
        Serial.print(str.c_str()) ;
        if (((res = httpd_resp_send_chunk(req, str.data(), str.size())) != ESP_OK) ||
            ((res = httpd_resp_send_chunk(req, (const char*) data.data(), data.size())) != ESP_OK) ||
            ((res = httpd_resp_send_chunk(req, boundary.data(), boundary.size())) != ESP_OK))
          return res ;

        delay(1000) ;
      }
    }
   },
   {
    "/settings.json",
    HTTP_GET,
    [](httpd_req_t *req)
    {
      std::string json = settings.json() ;
      return httpd_resp_send(req, json.data(), json.size()) ;
    }
   },
   {  
    "/cmd",
    HTTP_GET,
    [](httpd_req_t *req)
    {
      char query[128] ;
      char cmd[20] ;
      if ((httpd_req_get_url_query_str(req, query, sizeof(query)) != ESP_OK) ||
          (httpd_query_key_value(query, "cmd", cmd, sizeof(cmd)) != ESP_OK))
      {
        httpd_resp_send_err(req, HTTPD_400_BAD_REQUEST, "file not found") ;
        return ESP_OK ;
      }

      if (!strcmp(cmd, "save"  ))
      {
        if (settings.save())
        {
          httpd_resp_set_type(req, "text/plain") ;
          httpd_resp_sendstr(req, "ok") ;
        }
        else
        {
          httpd_resp_send_err(req, HTTPD_400_BAD_REQUEST, "save failed") ;
        }

        return ESP_OK ;
      }
      if (!strcmp(cmd, "info"  )) return cmdInfo(req) ;
      if (!strcmp(cmd, "reset" ))
      {
        terminator.hastaLaVistaBaby() ;
        std::string msg("rebooting ...") ;

        httpd_resp_set_hdr(req, "Refresh", "8;URL=/") ;
        return httpd_resp_send(req, msg.c_str(), msg.size()) ;
      }
      
      httpd_resp_send_err(req, HTTPD_400_BAD_REQUEST, "file not found") ;
      return ESP_OK ;
    }
   },
   {
    "/set",
    HTTP_GET,
    [](httpd_req_t *req)
    {
      return ESP_OK ;
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

  cfg.httpd.max_uri_handlers = 20 ;
  
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

  // out-of-memory in 2nd https connection :-(
  cfg.transport_mode = HTTPD_SSL_TRANSPORT_INSECURE ; 
  
  if (httpd_ssl_start(&_httpd, &cfg) != ESP_OK)
  {
    Serial.println("httpd_ssl_start() failed") ;
    return false ;
  }

  for (const FileInfo &fi : _fileInfos)
  {
    httpd_uri_t uri ;
    uri.uri = fi._url ;
    uri.method = HTTP_GET ;
    uri.handler = HTTPD::getFile ;
    uri.user_ctx = (void*)&fi ;
    if (httpd_register_uri_handler(_httpd, &uri) != ESP_OK)
    {
      Serial.println("httpd_register_uri_handler() failed") ;
      return false ;
    }
  }
  
  for (const auto &uri : uris)
    if (httpd_register_uri_handler(_httpd, &uri) != ESP_OK)
    {
      Serial.println("httpd_register_uri_handler() failed") ;
      return false ;
    }

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

esp_err_t HTTPD::getFile(httpd_req_t *req)
{
  Data data ;

  const FileInfo &fi = *((const FileInfo*)req->user_ctx) ;
  
  if (!spifs.read(fi._file, data))
  {
    Serial.println("read file failed") ;
    httpd_resp_send_err(req, HTTPD_400_BAD_REQUEST, "file not found") ;
    return ESP_OK ;
  }

  httpd_resp_set_type(req, fi._type);
  httpd_resp_send(req, (const char*) data.data(), data.size()) ;

  return ESP_OK ;
}
       
esp_err_t HTTPD::redirect(httpd_req_t *req, const char *location)
{
  std::string msg ;
  msg += "HTTP/1.1 301 Moved Permanently\r\n" ;
  msg += "Location: " ;
  msg += location ;
  msg += "\r\n" ;
  msg += "Content-Length: 0\r\n\r\n" ;
  
  httpd_send(req, msg.c_str(), msg.size()) ;

  return ESP_OK ;
}

////////////////////////////////////////////////////////////////////////////////
// EOF
////////////////////////////////////////////////////////////////////////////////
