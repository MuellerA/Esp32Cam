////////////////////////////////////////////////////////////////////////////////
// multi-part.cpp
////////////////////////////////////////////////////////////////////////////////

#include "esp32-cam.hpp"

////////////////////////////////////////////////////////////////////////////////

MultiPart::Part::Part() :
  _headBegin{nullptr}, _headEnd{nullptr}, _bodyBegin{nullptr}, _bodyEnd{nullptr}
{}
MultiPart::Part::Part(const uint8_t *hb, const uint8_t *he, const uint8_t *bb, const uint8_t *be) :
  _headBegin{hb}, _headEnd{he}, _bodyBegin{bb}, _bodyEnd{be}
{}

uint32_t MultiPart::Part::headSize() const
{
  return _headEnd - _headBegin ;
}
uint32_t MultiPart::Part::bodySize() const
{
  return _bodyEnd - _bodyBegin ;
}

const uint8_t* memmem(const uint8_t *buff, size_t size, const uint8_t *pattern, size_t patternSize)
{
  if (!patternSize || !buff || (patternSize > size))
    return nullptr ;
  uint8_t first = pattern[0] ;
  for (const uint8_t *b = buff, *e = buff+size-patternSize ; b <= e ; ++b)
  {
    if ((*b == first) &&
        !memcmp(b, pattern, patternSize))
      return b ;
  }
  return nullptr ;
}

MultiPart::MultiPart(httpd_req_t *req) : _req{req}, _content{nullptr}
{
}

MultiPart::~MultiPart()
{
  if (_content)
    free(_content) ;
}


bool MultiPart::parseName(const uint8_t *head, size_t headSize, std::string &name)
{
  uint8_t nl[2] = { 13, 10 } ;

  while (headSize)
  {
    const uint8_t *contentDisposition = (uint8_t*) "Content-Disposition:" ;
    size_t contentDispositionSize = strlen((const char*)contentDisposition) ;
    const uint8_t *name1 = (uint8_t*) "name=\"" ;
    size_t name1Size = strlen((const char*)name1) ;
    const uint8_t *name2 = (uint8_t*) "\"" ;
    size_t name2Size = strlen((const char*)name2) ;
                             
    const uint8_t *eol = (const uint8_t*) memmem(head, headSize, nl, 2) ;
    if (!eol)
      return false ;
    size_t lSize = eol - head ;

    if (lSize < contentDispositionSize)
      return false ;
    if (!memcmp(head, contentDisposition, contentDispositionSize))
    {
      const uint8_t *value = head + contentDispositionSize ;
      size_t valueSize = headSize - contentDispositionSize ;

      uint8_t *n1 = (uint8_t*) memmem(value, valueSize, name1, name1Size) ;
      if (!n1)
        return false ;
      n1 += name1Size ;
      size_t n1Size = valueSize - (n1 - value) ;
      uint8_t *n2 = (uint8_t*) memmem(n1, n1Size, name2, name2Size) ;
      if (!n2)
        return false ;
      
      name = std::string((const char*)n1, n2 - n1) ;
    }

    head += lSize + 2 ;
    headSize -= lSize + 2 ;
  }
  return true ;
}

bool MultiPart::parseBody(const uint8_t *boundary, size_t boundarySize,
                          const uint8_t *data, size_t dataSize)
{
  uint8_t nl[4] = { 13, 10, 13, 10 } ;
  
  if (dataSize < boundarySize)
    return false ;
  if (memcmp(data, boundary+2, boundarySize-2))
    return false ;

  data += boundarySize - 2 ;
  dataSize -= boundarySize - 2 ;
  
  while (true)
  {
    if (dataSize < 2)
      return false ;
    if (!memcmp(data, "--", 2)) // end of multipart
      return true ;
    if (memcmp(data, nl, 2)) // end of boundary
      return false ;

    data += 2 ;
    dataSize -= 2 ;
    
    uint8_t *eot = (uint8_t*) memmem(data, dataSize, boundary, boundarySize) ;
    if (!eot)
      return false ;
    size_t tSize = eot - data ;
    
    uint8_t *eoh = (uint8_t*) memmem(data, tSize, nl, 4) ;
    if (!eoh)
      return false ;

    std::string name ;
    if (!parseName(data, eoh+2 - data, name))
      return false ;

    _parts.insert(std::pair<std::string, Part>(name, Part(data, eoh+2, eoh+4, eot))) ;

    data += tSize + boundarySize ;
    dataSize -= tSize + boundarySize ;
  }
}

bool MultiPart::parse()
{
  char contentLength[32] ;
  size_t contentLengthSize{httpd_req_get_hdr_value_len(_req, "Content-Length")} ;

  if (!contentLengthSize)
  {
    httpd_resp_sendstr(_req, "HTTP header \"Content-Length\": not found") ;
    return false ;
  }
  if (contentLengthSize > sizeof(contentLength))
  {
    httpd_resp_sendstr(_req, "HTTP header \"Content-Length\": too big") ;
    return false ;
  }
  httpd_req_get_hdr_value_str(_req, "Content-Length", contentLength, sizeof(contentLength)) ;
  
  size_t contentSize = strtoul(contentLength, nullptr, 10) ;

  uint8_t *_content = (uint8_t*) malloc(contentSize) ;
  if (!_content)
  {
    httpd_resp_sendstr(_req, "HTTP header \"Content-Length\": too big") ;
    return false ;
  }
  
  char contentType[256] ;
  size_t contentTypeSize{httpd_req_get_hdr_value_len(_req, "Content-Type")} ;
  
  if (!contentTypeSize)
  {
    httpd_resp_sendstr(_req, "HTTP header \"Content-Type\": not found") ;
    return false ;
  }
  if (contentTypeSize > sizeof(contentType))
  {
    httpd_resp_sendstr(_req, "HTTP header \"Content-Type\": too big") ;
    return false ;
  }
  httpd_req_get_hdr_value_str(_req, "Content-Type", contentType, sizeof(contentType)) ;

  // Content-Type: multipart/form-data; boundary=---------------------------XXXXXX
  char boundary[64] = { 13, 10, '-', '-' } ;
  if (!strstr(contentType, "multipart/form-data"))
  {
    httpd_resp_sendstr(_req, "HTTP header \"Content-Type\": \"multipart/form-data\" not found") ;
    return false ;
  }
  char *boundaryBegin = strstr(contentType, "boundary=") ;
  if (!boundaryBegin)
  {
    httpd_resp_sendstr(_req, "HTTP header \"Content-Type\": \"boundary\" not found") ;
    return false ;
  }
  boundaryBegin += strlen("boundary=") ;
  char *boundaryEnd = strchr(boundaryBegin, ';') ;
  if (!boundaryEnd)
    boundaryEnd = boundaryBegin + strlen(boundaryBegin) ;
  size_t boundarySize = boundaryEnd - boundaryBegin ;
  if (boundarySize >= (sizeof(boundary)-4))
  {
    httpd_resp_sendstr(_req, "HTTP header \"Content-Type\": \"boundary\" too big") ;
    return false ;
  }

  memcpy(boundary+4, boundaryBegin, boundarySize) ;
  boundary[boundarySize+4] = 0 ;

  size_t recvSize{0} ;
  while (recvSize != contentSize)
  {
    int n =  httpd_req_recv(_req, (char*)_content + recvSize, contentSize - recvSize);
    if (!n)
    {
      httpd_resp_sendstr(_req, "Content: too small") ;
      return false ;
    }
    if (n < 0)
    {
      httpd_resp_sendstr(_req, "Content: error while receiving") ;
      return false ;
    }
    recvSize += n ;
  }

  if (!parseBody((uint8_t*)boundary, boundarySize+4, _content, contentSize))
  {
    httpd_resp_sendstr(_req, "Content: parse failed") ;
    return false ;
  }
  
  return true ;
}

bool MultiPart::get(const std::string &name, Part &part)
{
  auto iPart = _parts.find(name) ;
  if (iPart == _parts.end())
    return false ;

  part = iPart->second ;
  return true ;
}

////////////////////////////////////////////////////////////////////////////////
// EOF
////////////////////////////////////////////////////////////////////////////////
