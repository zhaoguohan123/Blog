@echo off
:: 该脚本和RegAccMan.exe放在同级路径
:: add    增加禁止访问的注册表路径 
:: remove 移除禁止访问的注册表路径
:: clear  移除所有访问的注册表路径
runas /user:Administrator "RegAccMan.exe  \REGISTRY\MACHINE\SYSTEM\ControlSet001\Services\wuauserv"