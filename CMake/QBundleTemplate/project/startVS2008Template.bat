@set CONFIG_TYPE=debug
REM @set CONFIG_TYPE=release

@call "%VS90COMNTOOLS%vsvars32.bat"
@set CL=/D_CRT_SECURE_NO_DEPRECATE /D_CRT_NONSTDC_NO_DEPRECATE
@set LINK=/LARGEADDRESSAWARE

PATH=@BATCH_FILE_VS_PATH@;%PATH%
"@BATCH_FILE_VS_EXEC_CMD@"

