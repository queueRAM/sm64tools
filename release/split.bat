:: change to batch file's directory so tools and config file can be referenced
pushd %~dp0
:: split input ROM using verbose mode and let it autodetect the config file
tools\n64split.exe -v %1
popd
:: wait for user
pause
