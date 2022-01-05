////////////////////////////////////////////////////////////////////////////////
// esp32-cam.cpp
////////////////////////////////////////////////////////////////////////////////

#include "esp32-cam.hpp"

////////////////////////////////////////////////////////////////////////////////

void wifi_event_handler(void* arg, esp_event_base_t event_base,
                        int32_t event_id, void* event_data)
{
  if (event_base != WIFI_EVENT)
  {
    ESP_LOGE("Wifi", "event handler invalid event_base") ;
    return ;
  }
  
  switch (event_id)
  {
  case WIFI_EVENT_STA_START:
    esp_wifi_connect();
    break ;
  case WIFI_EVENT_STA_DISCONNECTED:
    esp_wifi_connect();
    ESP_LOGI("Wifi", "retry to client connect") ;
    break ;
  case WIFI_EVENT_AP_STACONNECTED:
    ESP_LOGI("Wifi", "client connect") ;
    break ;
  case WIFI_EVENT_AP_STADISCONNECTED:
    ESP_LOGI("Wifi", "client disconnect") ;
    break ;
  }
}

void ip_event_handler(void* arg, esp_event_base_t event_base,
                      int32_t event_id, void* event_data)
{
  if (event_base != IP_EVENT)
  {
    ESP_LOGE("Wifi", "event handler invalid event_base") ;
    return ;
  }
  
  switch (event_id)
  {
  case IP_EVENT_STA_GOT_IP:
    {
      ip_event_got_ip_t* event = (ip_event_got_ip_t*) event_data;
      ESP_LOGI("Wifi", "got ip:" IPSTR, IP2STR(&event->ip_info.ip));
      esp_wifi_set_mode(WIFI_MODE_STA) ;
      httpd.mode(HTTPD::Mode::full) ;
    }
    break ;
  case IP_EVENT_STA_LOST_IP:
    {
      ESP_LOGI("Wifi", "lost ip") ;
      httpd.mode(HTTPD::Mode::wifi) ;
      esp_wifi_set_mode(WIFI_MODE_APSTA) ;
    }
  }
}

esp_err_t wifiSetup(httpd_req_t *req)
{
  ESP_LOGD("Wifi", "POST Requested") ;

  httpd_resp_set_type(req, "text/plain") ;

  MultiPart multiPart(req) ;
  if (!multiPart.parse())
    return ESP_OK ;

  MultiPart::Part wifiSsid ;
  MultiPart::Part wifiPwd ;
  MultiPart::Part espPwd ;
  if (!multiPart.get("wifi-ssid", wifiSsid))
    return httpd_resp_sendstr(req, "Content-Disposition: form-data; name=\"wifi-ssid\" not found") ;

  if (!multiPart.get("wifi-pwd", wifiPwd))
    return httpd_resp_sendstr(req, "Content-Disposition: form-data; name=\"wifi-pwd\" not found") ;
  if (!multiPart.get("esp-pwd", espPwd))
    return httpd_resp_sendstr(req, "Content-Disposition: form-data; name=\"esp-pwd\" not found") ;

  if (wifiSsid.bodySize() > 64)
    return httpd_resp_sendstr(req, "wifi ssid too big") ;
  if (wifiPwd.bodySize() > 64)
    return httpd_resp_sendstr(req, "wifi pwd too big") ;
  if (espPwd.bodySize() > 64)
    return httpd_resp_sendstr(req, "esp pwd too big") ;
  
  if (!crypto.pwdCheck(std::vector<uint8_t>(espPwd._bodyBegin, espPwd._bodyEnd)))
    return httpd_resp_sendstr(req, "invalid password") ;

  ////////////////////////////////////////

  std::string ssid((const char*)wifiSsid._bodyBegin, wifiSsid.bodySize()) ;
  std::string pwd ((const char*)wifiPwd ._bodyBegin, wifiPwd .bodySize()) ;

  privateSettings.set("wifi.st-ssid", ssid) ;
  privateSettings.set("wifi.st-pwd", pwd) ;

  privateSettings.save() ;

  httpd_resp_sendstr(req, "Upload successful.") ;

  esp_wifi_set_mode(WIFI_MODE_AP);
  esp_wifi_disconnect() ;
  httpd.mode(HTTPD::Mode::wifi) ;
  
  if (ssid.size())
  {
    esp_wifi_set_mode(WIFI_MODE_APSTA);
    setupSta(ssid, pwd) ;
    esp_wifi_connect() ;
  }
  
  return ESP_OK ;
}


////////////////////////////////////////////////////////////////////////////////
// EOF
////////////////////////////////////////////////////////////////////////////////
