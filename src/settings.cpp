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
  return
    jsonStr("type" , "str") + ", " +
    jsonStr("value", _value) ;
}

////////////////////////////////////////////////////////////////////////////////

bool SettingInt::inRange(int16_t &i) const
{
  return (_min <= i) && (i <= _max) ;
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
  if (!to_i(value, v) || !inRange(v))
    return false ;

  _value = value ;
  if (_setFn)
    _setFn(settings, v) ;
  return true ;
}

std::string SettingInt::json()
{
  return
    jsonStr("type" , "int") + ", " +
    jsonInt("min"  , _min ) + ", " +
    jsonInt("max"  , _max ) + ", " +
    jsonInt("value", _value) ;
}

////////////////////////////////////////////////////////////////////////////////

SettingEnum::SettingEnum(const std::string &category, const std::string &name, IniFn iniFn, SetFn setFn, const std::vector<std::string> &enums) :
  Setting(category, name), _iniFn{iniFn}, _setFn{setFn}, _enum{enums}
{
}

void SettingEnum::init(Settings &settings)
{
  if (_iniFn)
    _value = _iniFn(settings, _enum) ;
}

bool SettingEnum::set(Settings &settings, const std::string &value)
{
  _value = value ;
  if (_setFn)
    _setFn(settings, _enum, _value) ;
  return true ;
}

std::string SettingEnum::json()
{
  return
    jsonStr("type", "enum") + ", " +
    jsonArr("enum", _enum) + ", " +
    jsonStr("value", _value) ;
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
             new SettingStr("esp", "name",
                            [](Settings &settings) { return "ESP32 CAM" ; },
                            nullptr),
               new SettingEnum("camera", "framesize",
                               [](Settings &settings, const std::vector<std::string> &enums)
                               {
                                 size_t framesize = static_cast<PublicSettings&>(settings).sensor().status.framesize ;
                                 return (framesize < enums.size()) ? enums[framesize] : "" ;
                               },
                               [](Settings &settings, const std::vector<std::string> &enums, const std::string &value)
                               {
                                 for (size_t i = 0, e = enums.size() ; i < e ; ++i)
                                 {
                                   if (enums[i] == value)
                                   {
                                     sensor_t sensor = static_cast<PublicSettings&>(settings).sensor() ;
                                     sensor.set_framesize(&sensor, (framesize_t)i)  ;
                                     return ;
                                   }
                                 }
                               },
                               {
                                "96X96",           // 0
                                "QQVGA-160x120",   // 1
                                "QCIF-176x144",    // 2
                                "HQVGA-240x176",   // 3
                                "240x240",         // 4
                                "QVGA-320x240",    // 5
                                "CIF-400x296",     // 6
                                "HVGA-480x320",    // 7
                                "VGA-640x480",     // 8
                                "SVGA-800x600",    // 9
                                "XGA-1024x768",    // 10
                                "HD-1280x720",     // 11
                                "SXGA-1280x1024",  // 12
                                "UXGA-1600x1200",  // 13
                               }),
               new SettingInt("camera", "quality",
                              [](Settings &settings) { return static_cast<PublicSettings&>(settings).sensor().status.quality ; },
                              [](Settings &settings, const int16_t value)
                              {
                                sensor_t sensor = static_cast<PublicSettings&>(settings).sensor() ;
                                sensor.set_quality(&sensor, value) ;
                              },
                              0, 63 ),
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
               new SettingInt("camera", "saturation",
                              [](Settings &settings) { return static_cast<PublicSettings&>(settings).sensor().status.saturation ; },
                              [](Settings &settings, const int16_t value)
                              {
                                sensor_t sensor = static_cast<PublicSettings&>(settings).sensor() ;
                                sensor.set_saturation(&sensor, value) ;
                              },
                              -2, 2 ),
               new SettingInt("camera", "sharpness",
                              [](Settings &settings) { return static_cast<PublicSettings&>(settings).sensor().status.sharpness ; },
                              [](Settings &settings, const int16_t value)
                              {
                                sensor_t sensor = static_cast<PublicSettings&>(settings).sensor() ;
                                sensor.set_sharpness(&sensor, value) ;
                              },
                              -2, 2 ),
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
                            nullptr),
               new SettingStr("esp", "pwdHash",
                              nullptr,
                              nullptr),
               new SettingStr("wifi", "ap-ssid",
                              nullptr,
                              nullptr),
               new SettingStr("wifi", "ap-country",
                              nullptr,
                              nullptr),
               new SettingStr("wifi", "st-ssid",
                              nullptr,
                              nullptr),
               new SettingStr("wifi", "st-pwd",
                              nullptr,
                              nullptr),
               })
{
}

////////////////////////////////////////////////////////////////////////////////
// EOF
////////////////////////////////////////////////////////////////////////////////
