////////////////////////////////////////////////////////////////////////////////
// esp32-cam.cpp
////////////////////////////////////////////////////////////////////////////////

#include "esp32-cam.hpp"

////////////////////////////////////////////////////////////////////////////////

void onWiFiStGotIp(WiFiEvent_t ev, WiFiEventInfo_t info)
{
  Serial.println("got ip") ;
  WiFi.mode(WIFI_MODE_STA) ;
  httpd.mode(HTTPD::Mode::full) ;

  tcpip_adapter_ip_info_t ipInfo;
  tcpip_adapter_get_ip_info(TCPIP_ADAPTER_IF_STA, &ipInfo);
  Serial.println(ip_to_s((uint8_t*)&ipInfo.ip.addr).c_str()) ;
}

void onWifiStLostIp(WiFiEvent_t ev, WiFiEventInfo_t info)
{
  Serial.println("lost ip") ;
  WiFi.mode(WIFI_MODE_APSTA) ;
  httpd.mode(HTTPD::Mode::wifi) ;
}

void onWifiApConnect(WiFiEvent_t ev, WiFiEventInfo_t info)
{
  Serial.println("client connect") ;
}

void onWifiApDisconnect(WiFiEvent_t ev, WiFiEventInfo_t info)
{
  Serial.println("client disconnect") ;
}

esp_err_t wifiSetup(httpd_req_t *req)
{
  Serial.print("\nWIFI POST Requested\n") ;

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
  
  if (!crypt.pwdCheck(std::vector<uint8_t>(espPwd._bodyBegin, espPwd._bodyEnd)))
    return httpd_resp_sendstr(req, "invalid password") ;

  ////////////////////////////////////////

  std::string ssid((const char*)wifiSsid._bodyBegin, wifiSsid.bodySize()) ;
  std::string pwd ((const char*)wifiPwd ._bodyBegin, wifiPwd .bodySize()) ;

  privateSettings.set("wifi.st-ssid", ssid) ;
  privateSettings.set("wifi.st-pwd", pwd) ;

  privateSettings.save() ;

  httpd_resp_sendstr(req, "Upload successful.") ;

  WiFi.disconnect() ;
  if (ssid.size())
    WiFi.begin(ssid.c_str(), pwd.c_str()) ;

  return ESP_OK ;
}


////////////////////////////////////////////////////////////////////////////////
// EOF
////////////////////////////////////////////////////////////////////////////////
