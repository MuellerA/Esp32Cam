////////////////////////////////////////////////////////////////////////////////
// ota.cpp
////////////////////////////////////////////////////////////////////////////////

#include "esp32-cam.hpp"

////////////////////////////////////////////////////////////////////////////////

// find pattern memory
const uint8_t* otaMemmem(const uint8_t *buff, size_t size, const uint8_t *pattern, size_t patternSize)
{
  if (!patternSize || !buff || (patternSize > size))
    return nullptr ;
  uint8_t first = pattern[0] ;
  for (const uint8_t *b = buff, *e = buff+size-patternSize ; b < e ; ++b)
  {
    if ((*b == first) &&
        !memcmp(b, pattern, patternSize))
      return b ;
  }
  return nullptr ;
}

static bool otaFirmware(const uint8_t *boundary, size_t boundarySize, const uint8_t *data, size_t dataSize, const uint8_t **firmware, size_t *firmwareSize)
{
  const uint8_t nl[] = { 13, 10 } ;
  const size_t nlSize = sizeof(nl) ;
  const uint8_t *cd = (uint8_t*) "Content-Disposition:" ;
  const size_t cdSize = strlen((char*)cd) ;
  const uint8_t *fd = (uint8_t*) "form-data" ;
  const size_t fdSize = strlen((char*)fd) ;
  const uint8_t *n  = (uint8_t*) "name=\"firmware.bin\"" ;
  const size_t nSize = strlen((char*)n) ;
  
  while (dataSize > boundarySize)
  {
    const uint8_t *b = otaMemmem(data, dataSize, boundary, boundarySize) ;
    if (!b)
      return false ;

    b += boundarySize ;
    size_t s = b - data ;
    dataSize -= s ;
    data = b ;

    if ((dataSize < 2) || memcmp(data, nl, 2))
      return false ;
    dataSize -= 2 ;
    data += 2 ;

    // first line Content-Disposition
    
    if ((dataSize < cdSize) || memcmp(data, cd, cdSize))
      continue ;
    b = otaMemmem(data, dataSize, nl, nlSize) ;
    if (!b)
      continue ;
    s = b - data ;
    if (!otaMemmem(data, s, fd, fdSize) || !otaMemmem(data, s, n, nSize))
      continue ;
    b += 2 ;
    s = b - data ;
    
    dataSize -= s ;
    data = b ;

    // skip lines until empty line

    while (memcmp(data, nl, nlSize))
    {
      b = otaMemmem(data, dataSize, nl, nlSize) ;
      if (!b)
        return false ;
      b += nlSize ;
      s = b - data ;

      dataSize -= s ;
      data = b ;        
    }

    dataSize -= nlSize ; // skip empty line
    data += nlSize ;

    b = otaMemmem(data, dataSize, boundary, boundarySize) ;
    if (!b)
      return false ;
    s = b - data ;
    
    *firmware = data ;
    *firmwareSize = s ;
    return true ;    
  }

  return false ;
}

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

  char boundary[64] ;
  // Content-Type: multipart/form-data; boundary=---------------------------XXXXXX
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
  if (boundarySize >= sizeof(boundary))
  {
    free(content) ;
    return httpd_resp_sendstr(req, "HTTP header \"Content-Type\": \"boundary\" too big") ;
  }

  memcpy(boundary, boundaryBegin, boundarySize) ;
  boundary[boundarySize] = 0 ;

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

  const uint8_t *firmware ;
  size_t firmwareSize ;
  if (!otaFirmware((uint8_t*)boundary, boundarySize, content, contentSize, &firmware, &firmwareSize))
  {
    free(content) ;
    return httpd_resp_sendstr(req, "Content-Disposition: \"form-data; name=firmware.bin\" not found") ;
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
  err = esp_ota_begin(part, firmwareSize, &ota) ;
  if (err != ESP_OK)
  {
    free(content) ;
    sprintf(msg, "esp_ota_begin failed (%s)", esp_err_to_name(err)) ;
    return httpd_resp_sendstr(req, msg) ;
  }

  err = esp_ota_write(ota, firmware, firmwareSize) ;
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
