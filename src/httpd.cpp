////////////////////////////////////////////////////////////////////////////////
// httpd.cpp
////////////////////////////////////////////////////////////////////////////////

#include "esp32-cam.hpp"

////////////////////////////////////////////////////////////////////////////////

HTTPD  httpd ;

////////////////////////////////////////////////////////////////////////////////

std::string infoJson()
{
  std::string json ;
  std::string str ;

  json += "{ " ;
  
  publicSettings.get("esp.name", str) ;
  json += jsonStr("name", str) + ", " ;

  json += jsonInt("total internal heap", heap_caps_get_total_size(MALLOC_CAP_INTERNAL)) + ", " ;
  json += jsonInt("free internal heap", heap_caps_get_free_size(MALLOC_CAP_INTERNAL)) + ", " ;

  json += jsonInt("total external heap", heap_caps_get_total_size(MALLOC_CAP_SPIRAM)) + ", " ;
  json += jsonInt("free external heap", heap_caps_get_free_size(MALLOC_CAP_SPIRAM)) + ", " ;

  {
    size_t total, used ;
    if (spifs.df(total, used))
    {
      json += jsonInt("total spifs", (uint32_t)total) + ", " ;
      json += jsonInt("free spifs", (uint32_t)(total - used)) + ", " ;
    }
    else
    {
      json += jsonStr("spifs", "n/a") + ", " ;
    }
  }
  
  {
    uint8_t mac[6] ;
    if (esp_wifi_get_mac(WIFI_IF_AP, mac) == ESP_OK)
    {
      json += jsonStr("ap mac", mac_to_s(mac)) + ", " ;
      tcpip_adapter_ip_info_t ipInfo;
      tcpip_adapter_get_ip_info(TCPIP_ADAPTER_IF_AP, &ipInfo);
      json += jsonStr("ap ip", ip_to_s((uint8_t*)&ipInfo.ip.addr)) + ", " ;
    }
    else
      json += jsonStr("ap", "n/a") + ", " ;
  }

  {
    uint8_t mac[6] ;
    if (esp_wifi_get_mac(WIFI_IF_STA, mac) == ESP_OK)
    {
      json += jsonStr("sta mac", mac_to_s(mac)) + ", " ;
      tcpip_adapter_ip_info_t ipInfo;
      tcpip_adapter_get_ip_info(TCPIP_ADAPTER_IF_STA, &ipInfo);
      json += jsonStr("sta ip", ip_to_s((uint8_t*)&ipInfo.ip.addr)) + ", " ;
    }
    else
      json += jsonStr("sta", "n/a") + ", " ;
  }

  // more info?
  // ssid, bssid, rssi, gw, netmask
  // freq, uptime, spifs
  
  json += jsonStr("eot", "eot") ;

  json += " }" ;

  return json ;
}

const HTTPD::FileInfo HTTPD::_staticUriCommon[]
  {
   { "/favicon.ico"         , "image/x-icon"             , "camera-40.ico"       },
   { "/camera.svg"          , "image/svg+xml"            , "camera.svg"          },
   { "/camera-16.png"       , "image/png"                , "camera-16.png"       },
   { "/camera-32.png"       , "image/png"                , "camera-32.png"       },
   { "/camera-64.png"       , "image/png"                , "camera-64.png"       },
   { "/esp32-cam-ota.html"  , "text/html"                , "esp32-cam-ota.html"  },
   { "/esp32-cam-info.html" , "text/html"                , "esp32-cam-info.html" },
   { "/esp32-cam.css"       , "text/css"                 , "esp32-cam.css"       },
   { "/esp32-cam.js"        , "text/javascript"          , "esp32-cam.js"        },
   { "/settings.txt"        , "text/plain;charset=utf-8" , "settings.txt"        },
  } ;

const HTTPD::FileInfo HTTPD::_staticUriSetup[]
  {
   { "/esp32-cam.html"      , "text/html"                , "esp32-cam-setup.html"},
  } ;

const HTTPD::FileInfo HTTPD::_staticUriRunning[]
  {
   { "/esp32-cam.html"      , "text/html"                , "esp32-cam.html"      },
   { "/esp32-cam-setup.html", "text/html"                , "esp32-cam-setup.html"},
  } ;

const httpd_uri_t HTTPD::_dynamicUriCommon[] =
  {
   {
    "/",
    HTTP_GET,
    [](httpd_req_t *req){ return HTTPD::redirect(req, "esp32-cam.html") ; },
    nullptr
   },
   {
    "/index.html",
    HTTP_GET,
    [](httpd_req_t *req){ return HTTPD::redirect(req, "esp32-cam.html") ; },
    nullptr
   },
   {
    "/ota",
    HTTP_POST,
    ota,
    nullptr
   },
   {
    "/setup",
    HTTP_POST,
    wifiSetup,
    nullptr
   },
   {
    "/info.json",
    HTTP_GET,
    [](httpd_req_t *req)
    {
      httpd_resp_set_type(req, "application/json") ;
      std::string json = infoJson() ;
      return httpd_resp_send(req, json.data(), json.size()) ;
    },
    nullptr
   }
  } ;

const httpd_uri_t HTTPD::_dynamicUriRunning[] =
  {
   {
    "/capture.jpg",
    HTTP_GET,
    [](httpd_req_t *req)
    {
      Data data ;
      if (!camera.capture(data))
      {
        ESP_LOGE("Camera", "caputure failed") ;
        httpd_resp_send_err(req, HTTPD_500_INTERNAL_SERVER_ERROR, "camera capture failed") ;
        return ESP_OK ;
      }
         
      httpd_resp_set_type(req, "image/jpeg") ;
      httpd_resp_send(req, (const char*) data.data(), data.size()) ;

      return ESP_OK ;
    },
    nullptr
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
      ESP_LOGD("Camera", "%s", contentTypeDef.c_str()) ;
      if ((res = httpd_resp_set_type(req, contentTypeDef.c_str())) != ESP_OK)
        return res ;

      ESP_LOGD("Camera", "%s", boundary.c_str()) ;
      if ((res = httpd_resp_send_chunk(req, boundary.data(), boundary.size())) != ESP_OK)
        return res ;

      while (true) // send images
      {
        Data data ;
        if (!camera.capture(data))
        {
          ESP_LOGE("Camera", "caputure failed") ;
          return ESP_FAIL ;
        }
            
        char length[32] ;
        snprintf(length, sizeof(length), "%zd", data.size()) ;
            
        std::string str ;
        str += contentType ;
        str += "Content-Length: " + std::string(length) + nl ;
        str += nl ;
        ESP_LOGD("Camera", "%s", str.c_str()) ;
        if (((res = httpd_resp_send_chunk(req, str.data(), str.size())) != ESP_OK) ||
            ((res = httpd_resp_send_chunk(req, (const char*) data.data(), data.size())) != ESP_OK) ||
            ((res = httpd_resp_send_chunk(req, boundary.data(), boundary.size())) != ESP_OK))
          return res ;

        vTaskDelay(1000 / portTICK_PERIOD_MS) ;
      }
    },
    nullptr
   },
   {
    "/settings.json",
    HTTP_GET,
    [](httpd_req_t *req)
    {
      httpd_resp_set_type(req, "application/json") ;
      std::string json = publicSettings.json() ;
      return httpd_resp_send(req, json.data(), json.size()) ;
    },
    nullptr
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
        if (publicSettings.save())
          httpd_resp_send(req, nullptr, 0) ;
        else
          httpd_resp_send_err(req, HTTPD_400_BAD_REQUEST, "save failed") ;

        return ESP_OK ;
      }
      if (!strcmp(cmd, "reboot"))
      {
        terminator.hastaLaVistaBaby() ;

        httpd_resp_set_hdr(req, "Refresh", "8;URL=/") ;
        return httpd_resp_sendstr(req, "rebooting ...") ;
      }
      
      httpd_resp_send_err(req, HTTPD_400_BAD_REQUEST, "file not found") ;
      return ESP_OK ;
    },
    nullptr
   },
   {
    "/set",
    HTTP_GET,
    [](httpd_req_t *req)
    {
      ESP_LOGD("Httpd", "/set") ;
      char buff[1024] ;
      size_t size = httpd_req_get_url_query_len(req) ;
      if (!size)
      {
        httpd_resp_send(req, nullptr, 0) ;
        return ESP_OK ;      
      }

      char *ch ;
      if (((size+1) > sizeof(buff)) ||
          (httpd_req_get_url_query_str(req, buff, size+1) != ESP_OK) ||
          ((ch = strchr(buff, '=')) == nullptr))
      {
        httpd_resp_send_err(req, HTTPD_400_BAD_REQUEST, "invalid parameters") ;
        return ESP_OK ;
      }
          
      std::string key(buff, (ch) - buff) ;
      std::string val(ch+1, (buff+size) - (ch+1)) ;

      ESP_LOGD("Httpd", "%s", ("- " + key + " " + val).c_str()) ;
      
      if (!publicSettings.set(key, val))
      {
        httpd_resp_send_err(req, HTTPD_400_BAD_REQUEST, "invalid parameters") ;
        return ESP_OK ;
      }
      
      httpd_resp_send(req, nullptr, 0) ;
      return ESP_OK ;      
    },
    nullptr
   },
  } ;

HTTPD::~HTTPD()
{
  stop() ;
}

bool HTTPD::start()
{
  ESP_LOGD("Httpd", "start()") ;
  httpd_ssl_config_t cfg = HTTPD_SSL_CONFIG_DEFAULT() ;

  cfg.httpd.max_uri_handlers = 24 ;
  
  if (!spifs.read("cert.der", _certPem)) // pem does not work - use der
  {
    ESP_LOGE("Httpd", "read cert.der failed") ;
    return false ;
  }
  cfg.cacert_pem = (const uint8_t*) _certPem.data() ;
  cfg.cacert_len = _certPem.size() ;

  if (!spifs.read("key.der", _keyPem)) // pem does not work - use der
  {
    ESP_LOGE("Httpd", "read key.der failed") ;
    return false ;
  }
  cfg.prvtkey_pem = (const uint8_t*) _keyPem.data() ;
  cfg.prvtkey_len = _keyPem.size() ;

  //cfg.transport_mode = HTTPD_SSL_TRANSPORT_INSECURE ; 
  
  if (httpd_ssl_start(&_httpd, &cfg) != ESP_OK)
  {
    ESP_LOGE("Httpd", "httpd_ssl_start() failed") ;
    return false ;
  }

  for (const FileInfo &fi : _staticUriCommon)
  {
    httpd_uri_t uri ;
    uri.uri = fi._url ;
    uri.method = HTTP_GET ;
    uri.handler = HTTPD::getFile ;
    uri.user_ctx = (void*)&fi ;
    if (httpd_register_uri_handler(_httpd, &uri) != ESP_OK)
    {
      ESP_LOGW("Httpd", "httpd_register_uri_handler() failed %s", fi._url) ;
      return false ;
    }
  }

  for (const auto &uri : _dynamicUriCommon)
    if (httpd_register_uri_handler(_httpd, &uri) != ESP_OK)
    {
      ESP_LOGW("Httpd", "httpd_register_uri_handler() failed %s", uri.uri) ;
      return false ;
    }
  
  if (!mode(Mode::setup))
    return false ;

  return true ;
}

bool HTTPD::mode(HTTPD::Mode mode)
{
  if (mode == _mode)
    return true ;

  bool result{true} ;
  
  // unregister
  switch (_mode)
  {
  case Mode::none:
    break ;
    
  case Mode::setup:
    for (const FileInfo &fi : _staticUriSetup)
    {
      if (httpd_unregister_uri_handler(_httpd, fi._url, HTTP_GET) != ESP_OK)
      {
        ESP_LOGW("Httpd", "httpd_unregister_uri_handler() failed %s", fi._url) ;
        result = false ;
      }
    }
    break ;
    
  case Mode::running:
    for (const FileInfo &fi : _staticUriRunning)
    {
      if (httpd_unregister_uri_handler(_httpd, fi._url, HTTP_GET) != ESP_OK)
      {
        ESP_LOGW("Httpd", "httpd_unregister_uri_handler() failed %s", fi._url) ;
        result = false ;
      }
    }
 
    for (const auto &uri : _dynamicUriRunning)
      if (httpd_unregister_uri_handler(_httpd, uri.uri, uri.method) != ESP_OK)
      {
        ESP_LOGW("Httpd", "httpd_unregister_uri_handler() failed %s", uri.uri) ;
        result = false ;
      }
    break ;
  }

  _mode = mode ;

  // register
  switch (_mode)
  {
  case Mode::none:
    break ;

  case Mode::setup:
    for (const FileInfo &fi : _staticUriSetup)
    {
      httpd_uri_t uri ;
      uri.uri = fi._url ;
      uri.method = HTTP_GET ;
      uri.handler = HTTPD::getFile ;
      uri.user_ctx = (void*)&fi ;
      if (httpd_register_uri_handler(_httpd, &uri) != ESP_OK)
      {
        ESP_LOGW("Httpd", "httpd_register_uri_handler() failed %s", fi._url) ;
        result = false ;
      }
    }
    break ;

  case Mode::running:
    for (const FileInfo &fi : _staticUriRunning)
    {
      httpd_uri_t uri ;
      uri.uri = fi._url ;
      uri.method = HTTP_GET ;
      uri.handler = HTTPD::getFile ;
      uri.user_ctx = (void*)&fi ;
      if (httpd_register_uri_handler(_httpd, &uri) != ESP_OK)
      {
        ESP_LOGW("Httpd", "httpd_register_uri_handler() failed %s", fi._url) ;
        result = false ;
      }
    }
  
    for (const auto &uri : _dynamicUriRunning)
      if (httpd_register_uri_handler(_httpd, &uri) != ESP_OK)
      {
        ESP_LOGW("Httpd", "httpd_register_uri_handler() failed %s", uri.uri) ;
        result = false ;
      }
    break ;
  }

  return result ;
}

bool HTTPD::stop()
{
  if (_httpd)
  {
    httpd_ssl_stop(_httpd);
    _httpd = nullptr ;
  }
  _certPem.clear() ;
  _keyPem .clear() ;

  return true ;
}

esp_err_t HTTPD::getFile(httpd_req_t *req)
{
  Data data ;

  const FileInfo &fi = *((const FileInfo*)req->user_ctx) ;
  
  if (!spifs.read(fi._file, data))
  {
    ESP_LOGW("Httpd", "read file failed %s", fi._file) ;
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
  msg += "HTTP/1.1 308 Permanent Redirect\r\n" ;
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
