@echo off
rem 构建 KanDu C++ 版（Qt 6.11.2 + MinGW，绿色分发）
rem 步骤：编译 → windeployqt → 精简复制到 dist\（绿色版）→ 可选打包 zip
setlocal
set "QT=D:\Qt\6.11.2\mingw_64"
set "MINGW=D:\Qt\Tools\mingw1310_64"
set "CMAKE=D:\Qt\Tools\CMake_64"
set "PATH=%QT%\bin;%MINGW%\bin;%CMAKE%\bin;%PATH%"

cd /d "%~dp0"

echo [1/4] CMake 配置 ...
cmake -S . -B build -G "MinGW Makefiles" -DCMAKE_BUILD_TYPE=Release || goto :err

echo [2/4] 编译主程序与自测（约 1-2 分钟）...
cmake --build build -j 8 || goto :err

echo [3/4] 部署 Qt 运行库 ...
windeployqt --release --no-translations --no-system-d3d-compiler build\KanDu.exe || goto :err

echo [4/4] 精简复制到 dist\（绿色版）...
if exist dist rmdir /s /q dist
mkdir dist\platforms dist\imageformats dist\styles
copy /y build\KanDu.exe dist\ >nul
copy /y build\Qt6Core.dll dist\ >nul
copy /y build\Qt6Gui.dll dist\ >nul
copy /y build\Qt6Widgets.dll dist\ >nul
copy /y build\Qt6Svg.dll dist\ >nul
copy /y build\libgcc_s_seh-1.dll dist\ >nul
copy /y build\libstdc++-6.dll dist\ >nul
copy /y build\libwinpthread-1.dll dist\ >nul
copy /y build\platforms\qwindows.dll dist\platforms\ >nul
copy /y build\imageformats\qgif.dll dist\imageformats\ >nul
copy /y build\imageformats\qico.dll dist\imageformats\ >nul
copy /y build\imageformats\qjpeg.dll dist\imageformats\ >nul
copy /y build\imageformats\qsvg.dll dist\imageformats\ >nul
copy /y build\styles\qmodernwindowsstyle.dll dist\styles\ >nul
copy /y icon.ico dist\ >nul

rem WebP/TIFF 由系统 WIC 解码兜底（Win10/11 自带），无需 Qt 插件。
echo.
echo 构建完成：
echo   - 开发版：build\KanDu.exe
echo   - 绿色版：dist\KanDu.exe（双击即可运行，可整体拷贝分发）
echo.
echo 打包 zip？(Y/N)
choice /c YN /n
if errorlevel 2 goto :done
powershell -NoProfile -Command "Compress-Archive -Path dist\* -DestinationPath KanDu-绿色版.zip -Force"
echo 已生成 KanDu-绿色版.zip
:done
pause
exit /b 0

:err
echo.
echo 构建失败，请检查上方错误信息。
pause
exit /b 1
