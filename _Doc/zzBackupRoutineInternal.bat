set WD_WORKING_PATH=%1
set WD_BACKUP_PATH=%2

robocopy %WD_WORKING_PATH%\Wendy %WD_BACKUP_PATH%\Wendy *.* /E /MIR /XD %WD_WORKING_PATH%\Wendy\Binaries %WD_WORKING_PATH%\Wendy\DerivedDataCache %WD_WORKING_PATH%\Wendy\Intermediate %WD_WORKING_PATH%\Wendy\Saved

REM It might has Engine source modification.. but in such case not backing up all.
REM robocopy %WD_WORKING_PATH%\Engine\Source\Runtime %WD_BACKUP_PATH%\Engine\Source\Runtime *.* /E /MIR

robocopy %WD_WORKING_PATH%\_Cmd %WD_BACKUP_PATH%\_Cmd *.* /E /MIR
robocopy %WD_WORKING_PATH%\_Resource %WD_BACKUP_PATH%\_Resource *.* /E /MIR

REM No /MIR argument for _Doc folder, backup dest might has its own data.
robocopy %WD_WORKING_PATH%\_Doc %WD_BACKUP_PATH%\_Doc *.* /E