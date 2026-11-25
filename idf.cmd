@echo off
rem Wrapper so "idf build", "idf flash monitor" etc. work from cmd or PowerShell in this folder.
powershell -NoProfile -ExecutionPolicy Bypass -File "%~dp0idf.ps1" %*
