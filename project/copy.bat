@echo off
if not exist "%2" (
	md "%2"
)
copy /Y "%1" "%2"
