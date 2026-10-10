@echo off
chcp 65001 >nul
title Mouse Auto Clicker (Script Version)
start "" powershell -NoProfile -ExecutionPolicy Bypass -WindowStyle Hidden -File "%~dp0MouseClicker.ps1"
exit
