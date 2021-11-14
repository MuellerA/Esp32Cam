////////////////////////////////////////////////////////////////////////////////
// settings.hpp
////////////////////////////////////////////////////////////////////////////////

#pragma once

////////////////////////////////////////////////////////////////////////////////

class Settings ;

class Setting
{
public:
  Setting(const std::string &category, const std::string &name) ;
  virtual ~Setting() ;
  
  virtual const std::string& category() const ;
  virtual const std::string& name() const ;
  virtual void init(Settings &settings) ;
  virtual bool set(Settings &settings, const std::string &value) ;
  virtual bool get(const Settings &settings, std::string &value) const ;
  virtual std::string json() ;
protected:
  const std::string _category ;
  const std::string _name ;
  std::string _value ;
} ;

class SettingInt
{
public:
  SettingInt(const std::string &category, const std::string &name, int16_t min, int16_t max) ;
} ;

class SettingName : public Setting
{
public:
  SettingName() ;
  virtual void init(Settings &settings) ;
} ;


////////////////////////////////////////////////////////////////////////////////

class Settings
{
public:
  Settings() ;
  ~Settings() ;
  
  bool init() ;
  bool terminate() ;

  bool load() ;
  bool save() const ;
  
  std::string json() const ;

private:
  std::string _name ;
  sensor_t    *_sensor{nullptr} ;

  std::vector<Setting*> _settings ;
  std::map<std::string, Setting*> _settingByName ;
} ;

extern Settings settings ;

////////////////////////////////////////////////////////////////////////////////
// EOF
////////////////////////////////////////////////////////////////////////////////
