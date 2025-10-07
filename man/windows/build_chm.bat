@echo off
setlocal

rem Adjust this path if HTML Help Workshop is installed elsewhere:
set HHC="C:\Program Files (x86)\HTML Help Workshop\hhc.exe"

if not exist %HHC% (
  echo ERROR: HTML Help Workshop not found at:
  echo   %HHC%
  echo Install from https://learn.microsoft.com/en-us/previous-versions/windows/desktop/htmlhelp/microsoft-html-help-downloads
  exit /b 1
)

pushd "%~dp0"
%HHC% nls.hhp
if errorlevel 1 (
  echo.
  echo Build failed. Check .log output in this folder.
  popd & exit /b 1
)
popd

echo.
echo Build completed. Output:
echo   C:\Users\dmitr\src\nls\man\nls.chm
echo If Windows shows 'Cannot open the file', right-click the CHM -> Properties -> Unblock.
