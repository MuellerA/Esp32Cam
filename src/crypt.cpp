////////////////////////////////////////////////////////////////////////////////
// esp32-cam.cpp
////////////////////////////////////////////////////////////////////////////////

#include "esp32-cam.hpp"

Crypto crypto ;

bool Crypto::init()
{
  mbedtls_md_init(&_mdCtx);
  mbedtls_md_setup(&_mdCtx, mbedtls_md_info_from_type(MBEDTLS_MD_SHA256), 0);
  return true ;
}

bool Crypto::terminate()
{
  mbedtls_md_free(&_mdCtx);
  return true ;
}

Data Crypto::sha256(const Data &data)
{
  uint8_t sha[32] ;
  mbedtls_md_starts(&_mdCtx);
  mbedtls_md_update(&_mdCtx, data.data(), data.size()) ;
  mbedtls_md_finish(&_mdCtx, sha);
  return Data(sha, sha+32) ;
}

bool Crypto::pwdCheck(const Data &pwd)
{
  std::string salt ;
  std::string pwdHash ;
  if (!privateSettings.get("esp.salt", salt) ||
      !privateSettings.get("esp.pwdHash", pwdHash))
    return false ;

  Data data ;
  data.insert(data.end(), salt.begin(), salt.end()) ;
  data.insert(data.end(), pwd.begin(), pwd.end()) ;
  data = sha256(data) ;
  std::string pwdHash2 ;

  for (uint8_t i : data)
  {
    uint8_t j = i >> 4 ;
    pwdHash2 += (j < 10) ? (j + '0') : (j + 'a' - 10) ;
    j = i & 0x0f ;
    pwdHash2 += (j < 10) ? (j + '0') : (j + 'a' - 10) ;
  }

  return pwdHash == pwdHash2 ;
}

////////////////////////////////////////////////////////////////////////////////
// EOF
////////////////////////////////////////////////////////////////////////////////
