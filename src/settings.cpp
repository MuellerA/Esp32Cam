////////////////////////////////////////////////////////////////////////////////
// settings.cpp
////////////////////////////////////////////////////////////////////////////////

#include "esp32-cam.hpp"

////////////////////////////////////////////////////////////////////////////////

PrivateSettings privateSettings ;
PublicSettings  publicSettings ;

////////////////////////////////////////////////////////////////////////////////

Setting::Setting(const std::string &category, const std::string &name) :
  _category{category}, _name{name}
{
}
Setting::~Setting()
{
}

const std::string& Setting::category() const { return _category ; }
const std::string& Setting::name()     const { return _name     ; }
const std::string& Setting::value()    const { return _value    ; }

////////////////////////////////////////////////////////////////////////////////

SettingStr::SettingStr(const std::string &category, const std::string &name, IniFn iniFn, SetFn setFn) :
  Setting(category, name), _iniFn{iniFn}, _setFn{setFn}
{
}

void SettingStr::init(Settings &settings)
{
  if (_iniFn)
    _value = _iniFn(settings) ;
}

bool SettingStr::set(Settings &settings, const std::string &value)
{
  _value = value ;
  if (_setFn)
    _setFn(settings, _value) ;
  return true ;
}

std::string SettingStr::json()
{
  return "\"type\": \"str\", \"value\" : \"" + _value + "\"" ;
}

////////////////////////////////////////////////////////////////////////////////

std::string SettingInt::to_s(int16_t i)
{
  char buff[8] ;
  snprintf(buff, sizeof(buff), "%d", i) ;
  return buff ;  
}

bool SettingInt::to_i(const std::string &str, int16_t &i) const
{
  if (str.size() == 0)
    return false ;
  size_t iStr{0} ;
  size_t eStr{str.size()} ;
  bool neg{false} ;
  if ((str[0] == '-') || (str[0] == '+'))
  {
    iStr = 1 ;
    neg = str[0] == '-' ;
  }
  if (eStr > (4 + iStr))
    return false ;

  i = 0 ;
  while (iStr < eStr)
  {
    char ch = str[iStr++] ;
    if ((ch < '0') || ('9' < ch))
      return false ;
    i = i*10 + ch - '0' ;
  }
  if (neg)
    i = -i ;
  if ((i < _min) || (_max < i))
    return false ;
  
  return true ;
}

SettingInt::SettingInt(const std::string &category, const std::string &name, IniFn iniFn, SetFn setFn, int16_t min, int16_t max) :
  Setting(category, name), _iniFn{iniFn}, _setFn{setFn}, _min{min}, _max{max}
{
}

void SettingInt::init(Settings &settings)
{
  if (_iniFn)
    _value = to_s(_iniFn(settings)) ;
}

bool SettingInt::set(Settings &settings, const std::string &value)
{
  int16_t v ;
  if (!to_i(value, v))
    return false ;

  _value = value ;
  if (_setFn)
    _setFn(settings, v) ;
  return true ;
}

std::string SettingInt::json()
{
  return "\"type\": \"int\", \"min\": " + to_s(_min) + ", \"max\" :" + to_s(_max) + ", \"value\": " + _value ;
}

////////////////////////////////////////////////////////////////////////////////
// Settings
////////////////////////////////////////////////////////////////////////////////

Settings::Settings(const std::string &fileName, const std::vector<Setting*> &settings) :
  _fileName(fileName), _settings(settings)
{
  for (Setting *setting : _settings)
  {
    _settingByName[setting->category() + "." + setting->name()] = setting ;
  }
}

Settings::~Settings()
{
  for (Setting *setting : _settings)
    delete setting ;  
}

bool Settings::init()
{
  for (Setting *setting : _settings)
    setting->init(*this) ;
  
  load() ;

  return true ;
}

bool Settings::terminate()
{
  return true ;
}

bool Settings::load()
{
  std::string text ;
  if (!spifs.read(_fileName, text))
    return false ;

  const char *k{text.c_str()} ;
  const char *v{nullptr} ;
  const char *e{nullptr} ;
  while (*k)
  {
    v = strchr(k, '=') ;
    if (!v)
      break ;
    e = strchr(v, '\n') ;
    if (!e)
      e = text.c_str() + text.size() ;
    std::string key{k, (size_t)(v-k)} ;
    Serial.println((_fileName + " " + key + " " + std::string(v+1, e-(v+1))).c_str()) ;
    auto iSetting = _settingByName.find(key) ;
    if (iSetting != _settingByName.end())
      iSetting->second->set(*this, std::string(v+1, e-(v+1))) ;

    k = e+1 ;
  }
    
  return true ;
}

bool Settings::save() const
{
  std::string text ;
  for (Setting *setting : _settings)
  {
    std::string value ;
    text += setting->category() + "." + setting->name() + "=" + setting->value() + "\n" ;
  }
  
  return spifs.write(_fileName, text) ;
}

std::string Settings::json() const
{
  std::string text ;
  std::string category ;
  bool first{true} ;
  bool firstCategory{true} ; 
  
  text += "{ " ;
  for (Setting *setting : _settings)
  {
    if (category != setting->category())
    {
      category = setting->category() ;
      if (firstCategory)
        firstCategory = false ;
      else
        text += " }, " ;

      first = true ;
      text += "\"" + setting->category() + "\": { " ;
    }

    if (first)
      first = false ;
    else
      text += ", " ;

    text += "\"" + setting->name() + "\": { " + setting->json() + " }" ;
  } ;
  text += " } }" ;
  
  return text ;
}

bool Settings::set(const std::string &key, const std::string &val)
{
  auto iSetting = _settingByName.find(key) ;
  if (iSetting == _settingByName.end())
    return false ;

  return iSetting->second->set(*this, val) ;
}

bool Settings::get(const std::string &key, std::string &val)
{
  auto iSetting = _settingByName.find(key) ;
  if (iSetting == _settingByName.end())
    return false ;

  val = iSetting->second->value() ;
  return true ;
}

////////////////////////////////////////////////////////////////////////////////
// PublicSettings
////////////////////////////////////////////////////////////////////////////////

PublicSettings::PublicSettings() :
  Settings("settings.txt",
           std::vector<Setting*>
           {
             new SettingStr("global", "name",
                            [](Settings &settings) { return "ESP32 CAM" ; },
                            nullptr),
               new SettingInt("camera", "brightness",
                              [](Settings &settings) { return static_cast<PublicSettings&>(settings).sensor().status.brightness ; },
                              [](Settings &settings, const int16_t value)
                              {
                                sensor_t sensor = static_cast<PublicSettings&>(settings).sensor() ;
                                sensor.set_brightness(&sensor, value) ;
                              },
                              -2, 2 ),
               new SettingInt("camera", "contrast",
                              [](Settings &settings) { return static_cast<PublicSettings&>(settings).sensor().status.contrast ; },
                              [](Settings &settings, const int16_t value)
                              {
                                sensor_t sensor = static_cast<PublicSettings&>(settings).sensor() ;
                                sensor.set_contrast(&sensor, value) ;
                              },
                              -2, 2 ),
               new SettingInt("camera", "quality",
                              [](Settings &settings) { return static_cast<PublicSettings&>(settings).sensor().status.quality ; },
                              [](Settings &settings, const int16_t value)
                              {
                                sensor_t sensor = static_cast<PublicSettings&>(settings).sensor() ;
                                sensor.set_quality(&sensor, value) ;
                              },
                              0, 63 ),
               })
{
}

bool PublicSettings::init()
{
  _sensor = esp_camera_sensor_get() ;
  if (!_sensor)
  {
    Serial.println("esp_camera_sensor_get() failed") ;
    return false ;
  }

  return Settings::init() ;
}

const sensor_t& PublicSettings::sensor() const { return *_sensor ; }
sensor_t& PublicSettings::sensor() { return *_sensor ; }

////////////////////////////////////////////////////////////////////////////////
// PrivateSettings
////////////////////////////////////////////////////////////////////////////////

PrivateSettings::PrivateSettings() :
  Settings("secret.txt",
           std::vector<Setting*>
           {
             new SettingStr("esp", "salt",
                            [](Settings &settings) { return "ESP32 CAM" ; },
                            [](Settings &settings, const std::string &v) { settings.save() ; }),
               new SettingStr("esp", "pwdHash",
                              nullptr,
                              [](Settings &settings, const std::string &v) { settings.save() ; }),
               new SettingStr("wifi", "ssid",
                              nullptr,
                              [](Settings &settings, const std::string &v) { settings.save() ; }),
               new SettingStr("wifi", "pwd",
                              nullptr,
                              [](Settings &settings, const std::string &v) { settings.save() ; })
               })
{
}

////////////////////////////////////////////////////////////////////////////////
// EOF
////////////////////////////////////////////////////////////////////////////////
