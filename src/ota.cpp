////////////////////////////////////////////////////////////////////////////////
// ota.cpp
////////////////////////////////////////////////////////////////////////////////

#include "esp32-cam.hpp"

////////////////////////////////////////////////////////////////////////////////

esp_err_t ota(httpd_req_t *req)
{
  ESP_LOGD("Ota", "POST Requested") ;

  httpd_resp_set_type(req, "text/plain") ;

  MultiPart multiPart(req) ;
  if (!multiPart.parse())
    return ESP_OK ;

  MultiPart::Part firmware ;
  MultiPart::Part espPwd ;
  if (!multiPart.get("firmware", firmware))
    return httpd_resp_sendstr(req, "Content-Disposition: form-data; name=\"firmware\" not found") ;

  if (!multiPart.get("esp-pwd", espPwd))
    return httpd_resp_sendstr(req, "Content-Disposition: form-data; name=\"esp-pwd\" not found") ;

  if (!crypto.pwdCheck(std::vector<uint8_t>(espPwd._bodyBegin, espPwd._bodyEnd)))
    return httpd_resp_sendstr(req, "invalid password") ;
  
  ////////////////////////////////////////
  
  const esp_partition_t *part = esp_ota_get_running_partition() ;
  if (!part)
  {
    return httpd_resp_sendstr(req, "esp_ota_get_running_partition failed") ;
  }
  
  part = esp_ota_get_next_update_partition(part) ;  
  if (!part)
  {
    return httpd_resp_sendstr(req, "esp_ota_get_next_update_partition failed") ;
  }

  esp_err_t err ;
  esp_ota_handle_t ota ;
  char msg[64] ;
  err = esp_ota_begin(part, firmware._bodyEnd - firmware._bodyBegin, &ota) ;
  if (err != ESP_OK)
  {
    sprintf(msg, "esp_ota_begin failed (%s)", esp_err_to_name(err)) ;
    return httpd_resp_sendstr(req, msg) ;
  }

  err = esp_ota_write(ota, firmware._bodyBegin, firmware._bodyEnd - firmware._bodyBegin) ;
  if (err != ESP_OK)
  {
    sprintf(msg, "esp_ota_write failed (%s)", esp_err_to_name(err)) ;
    return httpd_resp_sendstr(req, msg) ;
  }

  err = esp_ota_end(ota) ;
  if (err != ESP_OK)
  {
    sprintf(msg, "esp_ota_end failed (%s)", esp_err_to_name(err)) ;
    return httpd_resp_sendstr(req, msg) ;
  }

  err = esp_ota_set_boot_partition(part) ;
  if (err != ESP_OK)
  {
    sprintf(msg, "esp_ota_set_boot_partition failed (%s)", esp_err_to_name(err)) ;
    return httpd_resp_sendstr(req, msg) ;
  }
  
  httpd_resp_sendstr(req, "Upload successful, booting new firmware ...") ;
  ESP_LOGW("Ota", "booting new firmware") ;

  terminator.hastaLaVistaBaby() ;

  return ESP_OK ;
}

////////////////////////////////////////////////////////////////////////////////
// EOF
////////////////////////////////////////////////////////////////////////////////
