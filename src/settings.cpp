////////////////////////////////////////////////////////////////////////////////
// settings.cpp
////////////////////////////////////////////////////////////////////////////////

#include "esp32-cam.hpp"

////////////////////////////////////////////////////////////////////////////////

Settings settings ;

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

void Setting::init(Settings &settings)
{
}

bool Setting::set(Settings &settings, const std::string &value)
{
  _value = value ;
  return true ;
}

bool Setting::get(const Settings &settings, std::string &value) const
{
  value = _value ;
  return true ;
}

std::string Setting::json()
{
  return "\"" + _value + "\"" ;
}

////////////////////////////////////////////////////////////////////////////////

SettingName::SettingName() : Setting("global", "name")
{
}

void SettingName::init(Settings &settings)
{
  _value = "ESP32 CAM" ;
}

////////////////////////////////////////////////////////////////////////////////

Settings::Settings()
{
  _settings = std::vector<Setting*>
    {
     new SettingName()
    } ;

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
  _sensor = esp_camera_sensor_get() ;
  if (!_sensor)
  {
    Serial.println("esp_camera_sensor_get() failed") ;
    return false ;
  }

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
  if (!spifs.read("settings.txt", text))
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
    if (setting->get(*this, value))
      text += setting->category() + "." + setting->name() + "=" + value + "\n" ;
  }
  
  return spifs.write("settings.txt", text) ;
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

    text += "\"" + setting->name() + "\": " + setting->json() ;
  } ;
  text += " } }" ;
  
  return text ;
}

////////////////////////////////////////////////////////////////////////////////
// EOF
////////////////////////////////////////////////////////////////////////////////
