# DalyBMS_linux

Это ответвление от проекта Smart BMS DALY + ESP8266 or Raspberry PI = MQTT https://www.youtube.com/watch?v=m-Umt_olgws

git clone https://github.com/immortalserg/DalyBMS_linux.git

cd DalyBMS_linux

cmake . && make

### Использование

отредактируйте файл bms.conf заменив на свой порт, в кронтаб добавьте строку

@reboot cd /ПУТЬ/ДО/DalyBMS_linux ./smart_bms

перезагрузите устройство
