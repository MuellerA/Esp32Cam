////////////////////////////////////////////////////////////////////////////////
// ota.cpp
////////////////////////////////////////////////////////////////////////////////

#include "esp32-cam.hpp"

////////////////////////////////////////////////////////////////////////////////

esp_err_t ota(httpd_req_t *req)
{
  Serial.print("\nOTA POST Requested\n") ;

  httpd_resp_set_type(req, "text/plain") ;
  
  char contentLength[32] ;
  size_t contentLengthSize{httpd_req_get_hdr_value_len(req, "Content-Length")} ;

  if (!contentLengthSize)
    return httpd_resp_sendstr(req, "HTTP header \"Content-Length\": not found") ;
  if (contentLengthSize > sizeof(contentLength))
    return httpd_resp_sendstr(req, "HTTP header \"Content-Length\": too big") ;
  httpd_req_get_hdr_value_str(req, "Content-Length", contentLength, sizeof(contentLength)) ;
  
  size_t contentSize = strtoul(contentLength, nullptr, 10) ;

  uint8_t *content = (uint8_t*) ps_malloc(contentSize) ;
  if (!content)
    return httpd_resp_sendstr(req, "HTTP header \"Content-Length\": too big") ;
  
  char contentType[256] ;
  size_t contentTypeSize{httpd_req_get_hdr_value_len(req, "Content-Type")} ;
  
  if (!contentTypeSize)
  {
    free(content) ;
    return httpd_resp_sendstr(req, "HTTP header \"Content-Type\": not found") ;
  }
  if (contentTypeSize > sizeof(contentType))
  {
    free(content) ;
    return httpd_resp_sendstr(req, "HTTP header \"Content-Type\": too big") ;
  }
  httpd_req_get_hdr_value_str(req, "Content-Type", contentType, sizeof(contentType)) ;

  // Content-Type: multipart/form-data; boundary=---------------------------XXXXXX
  char boundary[64] = { 13, 10, '-', '-' } ;
  if (!strstr(contentType, "multipart/form-data"))
  {
    free(content) ;
    return httpd_resp_sendstr(req, "HTTP header \"Content-Type\": \"multipart/form-data\" not found") ;
  }
  char *boundaryBegin = strstr(contentType, "boundary=") ;
  if (!boundaryBegin)
  {
    free(content) ;
    return httpd_resp_sendstr(req, "HTTP header \"Content-Type\": \"boundary\" not found") ;
  }
  boundaryBegin += strlen("boundary=") ;
  char *boundaryEnd = strchr(boundaryBegin, ';') ;
  if (!boundaryEnd)
    boundaryEnd = boundaryBegin + strlen(boundaryBegin) ;
  size_t boundarySize = boundaryEnd - boundaryBegin ;
  if (boundarySize >= (sizeof(boundary)-4))
  {
    free(content) ;
    return httpd_resp_sendstr(req, "HTTP header \"Content-Type\": \"boundary\" too big") ;
  }

  memcpy(boundary+4, boundaryBegin, boundarySize) ;
  boundary[boundarySize+4] = 0 ;

  size_t recvSize{0} ;
  while (recvSize != contentSize)
  {
    int n =  httpd_req_recv(req, (char*)content + recvSize, contentSize - recvSize);
    if (!n)
      return httpd_resp_sendstr(req, "Content: too small") ;
    if (n < 0)
      return httpd_resp_sendstr(req, "Content: error while receiving") ;
    recvSize += n ;
  }

  PartByName parts ;
  parseMultiPart((uint8_t*)boundary, boundarySize+4, content, contentSize, parts) ;

  auto iFirmware = parts.find("firmware") ;
  if (iFirmware == parts.end())
  {
    free(content) ;
    return httpd_resp_sendstr(req, "Content-Disposition: form-data; name=\"firmware\" not found") ;
  }
  auto iPassword = parts.find("esp-pwd") ;
  if (iPassword == parts.end())
  {
    free(content) ;
    return httpd_resp_sendstr(req, "Content-Disposition: form-data; name=\"esp-pwd\" not found") ;
  }

  if (!crypt.pwdCheck(std::vector<uint8_t>(iPassword->second._bodyBegin, iPassword->second._bodyEnd)))
  {
    free(content) ;
    return httpd_resp_sendstr(req, "invalid password") ;
  }
  
  ////////////////////////////////////////
  
  const esp_partition_t *part = esp_ota_get_running_partition() ;
  if (!part)
  {
    free(content) ;
    return httpd_resp_sendstr(req, "esp_ota_get_running_partition failed") ;
  }
  
  part = esp_ota_get_next_update_partition(part) ;  
  if (!part)
  {
    free(content) ;
    return httpd_resp_sendstr(req, "esp_ota_get_next_update_partition failed") ;
  }

  esp_err_t err ;
  esp_ota_handle_t ota ;
  char msg[64] ;
  err = esp_ota_begin(part, iFirmware->second._bodyEnd - iFirmware->second._bodyBegin, &ota) ;
  if (err != ESP_OK)
  {
    free(content) ;
    sprintf(msg, "esp_ota_begin failed (%s)", esp_err_to_name(err)) ;
    return httpd_resp_sendstr(req, msg) ;
  }

  err = esp_ota_write(ota, iFirmware->second._bodyBegin, iFirmware->second._bodyEnd - iFirmware->second._bodyBegin) ;
  if (err != ESP_OK)
  {
    free(content) ;
    sprintf(msg, "esp_ota_write failed (%s)", esp_err_to_name(err)) ;
    return httpd_resp_sendstr(req, msg) ;
  }

  err = esp_ota_end(ota) ;
  if (err != ESP_OK)
  {
    free(content) ;
    sprintf(msg, "esp_ota_end failed (%s)", esp_err_to_name(err)) ;
    return httpd_resp_sendstr(req, msg) ;
  }

  err = esp_ota_set_boot_partition(part) ;
  if (err != ESP_OK)
  {
    free(content) ;
    sprintf(msg, "esp_ota_set_boot_partition failed (%s)", esp_err_to_name(err)) ;
    return httpd_resp_sendstr(req, msg) ;
  }
  
  free(content) ;
  httpd_resp_sendstr(req, "Upload successful, booting new firmware ...") ;
  Serial.print("booting new firmware\n") ;

  terminator.hastaLaVistaBaby() ;

  return ESP_OK ;
}

////////////////////////////////////////////////////////////////////////////////
// EOF
////////////////////////////////////////////////////////////////////////////////
