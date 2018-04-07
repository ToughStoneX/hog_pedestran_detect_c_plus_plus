@echo off
:: 如果要文件名带上路径，则需要在dir这一句的%%~nxi上作改动
::                  code by FBY && RMW
if exist FileNameList.txt del FileNameList.txt /q
::for /f "delims=" %%i in ('dir *.jpg /b /a-d /s') do echo %%~nxi >>FileNameList.txt
for /f "delims=" %%i in ('dir *.jpg /b /a-d /s') do (
			echo %%~dpi%%~nxi >>FileNameList.txt
			echo -1 >>FileNameList.txt
			)
if not exist FileNameList.txt goto no_file
start FileNameList.txt
exit

:no_file
cls
echo       %cur_dir% 文件夹下没有单独的文件
pause 