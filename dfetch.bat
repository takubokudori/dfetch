@echo off
sc create dfetch binPath="C:\path\to\dfetch.sys" type=kernel
sc start dfetch
sc delete dfetch
pause
