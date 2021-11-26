////////////////////////////////////////////////////////////////////////////////

"use strict" ;

////////////////////////////////////////////////////////////////////////////////
// global vars
////////////////////////////////////////////////////////////////////////////////

var menuContent
var menuSettings
var settings

////////////////////////////////////////////////////////////////////////////////
// menu
////////////////////////////////////////////////////////////////////////////////

function menu()
{
    menuContent.style.display = (menuContent.style.display == 'none') ? 'block' : 'none'
}

function createHTML(parent, ele, attrs)
{
    ele = document.createElement(ele)
    for (let key in attrs)
        ele.setAttribute(key, attrs[key])
    if (parent)
        parent.appendChild(ele)
    return ele
}
function createText(parent, text)
{
    let ele = document.createTextNode(text)
    if (parent)
        parent.appendChild(ele)
    return ele
}

function buildMenu()
{
    while (menuSettings.firstChild)
        menuSettings.removeChild(menuSettings.firstChild)

    let table = createHTML(menuSettings, 'table')
    
    for (const [k, v] of Object.entries(settings.camera))
    {
        let tr = createHTML(table, 'tr')
        
        let tdLabel = createHTML(tr, 'td')
        let label = createHTML(tdLabel, 'label', { 'for': k })
        createText(label, k + ' ')
        
        switch (v.type)
        {
          case 'str':
            let tdText   = createHTML(tr, 'td', { 'colspan': 99 })
            createText(tdText, v.value)
            break

          case 'int':
            let tdMin   = createHTML(tr, 'td')
            createText(tdMin, v.min)
            
            let tdInput = createHTML(tr, 'td')
            let input = createHTML(tdInput, 'input', { 'type': 'range', 'min': v.min, 'max': v.max, 'value': v.value })
            input.onchange = function() { menuChange('camera.' + k, input.value) }
            
            let tdMax   = createHTML(tr, 'td')        
            createText(tdMax, v.max)
            break
        }
    }
}

function menuChange(key, value)
{
    let http = new XMLHttpRequest()
    http.open('GET', `/set?${key}=${value}`, true)
    http.send(null)    
}

function cmd(cmd)
{
    let http = new XMLHttpRequest()
    http.open('GET', `/cmd?cmd=${cmd}`, true)
    http.send(null)    
}

//window.onclick = function(event)
//{
//    if (!event.target.matches('.dropdown-button'))
//        menuContent.style.display = 'none'
//}

////////////////////////////////////////////////////////////////////////////////
// main() (esp32-cam.html)
////////////////////////////////////////////////////////////////////////////////

function main(initMenu)
{
    if (initMenu)
    {
        menuContent = document.getElementById("menu-content")
        menuSettings = document.getElementById("menu-settings")
    }
    
    fetch('settings.json', { method: 'get'})
        .then(response => response.json())
        .then(json =>
              {
                  settings = json
                  let name = settings.global.name
                  if (name)
                      for (let e of document.getElementsByClassName("name"))
                          e.innerText = e.innerText.replace('ESP32-CAM', name.value)
                  if (initMenu)
                      buildMenu()
              })
}

////////////////////////////////////////////////////////////////////////////////
// OTA (esp32-cam-ota.html)
////////////////////////////////////////////////////////////////////////////////

function uploadFirmware(inputFile)
{
    const msg = document.getElementById('ota-msg')
    const firmware = document.getElementById('firmware')
    const password = document.getElementById('esp-pwd')
    let formData = new FormData()

    msg.textContent = 'Uploading, please wait...'

    formData.set('esp-pwd', password.value)
    formData.set('firmware', firmware.files[0])
    fetch('/ota', { method: 'POST', body: formData })
        .then(response => response.text()) // should check if response is text/plain
        .then(text => 
              {
                  msg.textContent = text
                  if (text.includes('booting'))
                  {
                      new Promise(resolve => setTimeout(resolve, 6000))
                          .then(() => { window.location.href = '/esp32-cam.html' })
                  }
              })
}

////////////////////////////////////////////////////////////////////////////////
// Wifi (esp32-cam-wifi.html)
////////////////////////////////////////////////////////////////////////////////

function setupWifi(inputFile)
{
    const msg = document.getElementById('wifi-msg')
    const wifiSsid = document.getElementById('wifi-ssid')
    const wifiPwd = document.getElementById('wifi-pwd')
    const espPwd = document.getElementById('esp-pwd')
    let formData = new FormData()

    msg.textContent = 'Uploading, please wait...'

    formData.set('esp-pwd', espPwd.value)
    formData.set('wifi-ssid', wifiSsid.value)
    formData.set('wifi-pwd', wifiPwd.value)
    fetch('/wifi', { method: 'POST', body: formData })
        .then(response => response.text()) // should check if response is text/plain
        .then(text => 
              {
                  msg.textContent = text
              })
}


////////////////////////////////////////////////////////////////////////////////
// EOF
////////////////////////////////////////////////////////////////////////////////
