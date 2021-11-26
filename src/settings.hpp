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
  
  const std::string& category() const ;
  const std::string& name() const ;
  const std::string& value() const ;

  virtual void init(Settings &settings) = 0 ;
  virtual bool set(Settings &settings, const std::string &value) = 0 ;
  virtual std::string json() = 0 ;
protected:
  const std::string _category ;
  const std::string _name ;
  std::string _value ;
} ;

////////////////////////////////////////////////////////////////////////////////

class SettingStr : public Setting
{
public:
  using IniFn = std::function<std::string(Settings &settings)> ;
  using SetFn = std::function<void(Settings &settings, const std::string &value)> ;

  SettingStr(const std::string &category, const std::string &name, IniFn iniFn, SetFn setFn) ;
  virtual void init(Settings &settings) ;
  virtual bool set(Settings &settings, const std::string &value) ;
  virtual std::string json() ;
private:
  IniFn _iniFn ;
  SetFn _setFn ;
} ;

////////////////////////////////////////////////////////////////////////////////

class SettingInt : public Setting
{
public:
  using IniFn = std::function<int16_t(Settings &settings)> ;
  using SetFn = std::function<void(Settings &settings, const int16_t value)> ;

  static std::string to_s(int16_t i) ;
  bool to_i(const std::string &s, int16_t &i) const ;
  
  SettingInt(const std::string &category, const std::string &name, IniFn iniFn, SetFn setFn, int16_t min, int16_t max) ;
  virtual void init(Settings &settings) ;
  virtual bool set(Settings &settings, const std::string &value) ;
  virtual std::string json() ;
private:
  IniFn _iniFn ;
  SetFn _setFn ;
  int16_t _min ;
  int16_t _max ;
} ;

////////////////////////////////////////////////////////////////////////////////

class Settings
{
public:
  Settings(const std::string &fileName, const std::vector<Setting*> &settings) ;
  virtual ~Settings() ;
  
  virtual bool init() ;
  virtual bool terminate() ;

  bool load() ;
  bool save() const ;
  
  std::string json() const ;
  bool set(const std::string &key, const std::string &val) ;
  bool get(const std::string &key, std::string &val) ;

protected:
  const std::string _fileName ;
  
  std::vector<Setting*> _settings ;
  std::map<std::string, Setting*> _settingByName ;
} ;

class PublicSettings : public Settings
{
public:
  PublicSettings() ;
  
  virtual bool init() ;
  
  const sensor_t& sensor() const ;
  sensor_t& sensor() ;

private:
  sensor_t    *_sensor{nullptr} ;
} ;

class PrivateSettings : public Settings
{
public:
  PrivateSettings() ;
} ;

extern PublicSettings  publicSettings ;
extern PrivateSettings privateSettings ;

////////////////////////////////////////////////////////////////////////////////
// EOF
////////////////////////////////////////////////////////////////////////////////
