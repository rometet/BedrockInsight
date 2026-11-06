@echo off
setlocal

if "%BEDROCKINSIGHT_CLANG_ROOT%"=="" (
    echo Set BEDROCKINSIGHT_CLANG_ROOT to the clang toolchain bin directory.
    exit /b 2
)
if "%BEDROCKINSIGHT_SQLITE_ROOT%"=="" (
    echo Set BEDROCKINSIGHT_SQLITE_ROOT to the SQLite package root.
    exit /b 2
)
if not exist "%BEDROCKINSIGHT_CLANG_ROOT%\clang++.exe" (
    echo clang++ was not found under %BEDROCKINSIGHT_CLANG_ROOT%.
    exit /b 2
)
if not exist "%BEDROCKINSIGHT_SQLITE_ROOT%\include\sqlite3.h" (
    echo sqlite3.h was not found under %BEDROCKINSIGHT_SQLITE_ROOT%.
    exit /b 2
)

if not exist build\manual mkdir build\manual
if not exist build\manual\tmp mkdir build\manual\tmp
set "TEMP=%CD%\build\manual\tmp"
set "TMP=%CD%\build\manual\tmp"
set "PATH=%BEDROCKINSIGHT_CLANG_ROOT%;%PATH%"

"%BEDROCKINSIGHT_CLANG_ROOT%\clang++.exe" -std=c++20 -Isrc -I"%BEDROCKINSIGHT_SQLITE_ROOT%\include" -Wall -Wextra -Wpedantic -c src\bedrock_insight\core\analytics_query.cpp -o build\manual\analytics_query.o
if errorlevel 1 exit /b %errorlevel%
"%BEDROCKINSIGHT_CLANG_ROOT%\clang++.exe" -std=c++20 -Isrc -I"%BEDROCKINSIGHT_SQLITE_ROOT%\include" -Wall -Wextra -Wpedantic -c src\bedrock_insight\core\dashboard.cpp -o build\manual\dashboard.o
if errorlevel 1 exit /b %errorlevel%
"%BEDROCKINSIGHT_CLANG_ROOT%\clang++.exe" -std=c++20 -Isrc -I"%BEDROCKINSIGHT_SQLITE_ROOT%\include" -Wall -Wextra -Wpedantic -c src\bedrock_insight\core\api_payload.cpp -o build\manual\api_payload.o
if errorlevel 1 exit /b %errorlevel%
"%BEDROCKINSIGHT_CLANG_ROOT%\clang++.exe" -std=c++20 -Isrc -I"%BEDROCKINSIGHT_SQLITE_ROOT%\include" -Wall -Wextra -Wpedantic -c src\bedrock_insight\core\api_json.cpp -o build\manual\api_json.o
if errorlevel 1 exit /b %errorlevel%
"%BEDROCKINSIGHT_CLANG_ROOT%\clang++.exe" -std=c++20 -Isrc -I"%BEDROCKINSIGHT_SQLITE_ROOT%\include" -Wall -Wextra -Wpedantic -c src\bedrock_insight\core\admin_api.cpp -o build\manual\admin_api.o
if errorlevel 1 exit /b %errorlevel%
"%BEDROCKINSIGHT_CLANG_ROOT%\clang++.exe" -std=c++20 -Isrc -I"%BEDROCKINSIGHT_SQLITE_ROOT%\include" -Wall -Wextra -Wpedantic -c src\bedrock_insight\core\event.cpp -o build\manual\event.o
if errorlevel 1 exit /b %errorlevel%
"%BEDROCKINSIGHT_CLANG_ROOT%\clang++.exe" -std=c++20 -Isrc -I"%BEDROCKINSIGHT_SQLITE_ROOT%\include" -Wall -Wextra -Wpedantic -c src\bedrock_insight\core\http_transport.cpp -o build\manual\http_transport.o
if errorlevel 1 exit /b %errorlevel%
"%BEDROCKINSIGHT_CLANG_ROOT%\clang++.exe" -std=c++20 -Isrc -I"%BEDROCKINSIGHT_SQLITE_ROOT%\include" -Wall -Wextra -Wpedantic -c src\bedrock_insight\core\http_server.cpp -o build\manual\http_server.o
if errorlevel 1 exit /b %errorlevel%
"%BEDROCKINSIGHT_CLANG_ROOT%\clang++.exe" -std=c++20 -Isrc -I"%BEDROCKINSIGHT_SQLITE_ROOT%\include" -Wall -Wextra -Wpedantic -c src\bedrock_insight\core\json.cpp -o build\manual\json.o
if errorlevel 1 exit /b %errorlevel%
"%BEDROCKINSIGHT_CLANG_ROOT%\clang++.exe" -std=c++20 -Isrc -I"%BEDROCKINSIGHT_SQLITE_ROOT%\include" -Wall -Wextra -Wpedantic -c src\bedrock_insight\core\metrics.cpp -o build\manual\metrics.o
if errorlevel 1 exit /b %errorlevel%
"%BEDROCKINSIGHT_CLANG_ROOT%\clang++.exe" -std=c++20 -Isrc -I"%BEDROCKINSIGHT_SQLITE_ROOT%\include" -Wall -Wextra -Wpedantic -c src\bedrock_insight\core\rollback.cpp -o build\manual\rollback.o
if errorlevel 1 exit /b %errorlevel%
"%BEDROCKINSIGHT_CLANG_ROOT%\clang++.exe" -std=c++20 -Isrc -I"%BEDROCKINSIGHT_SQLITE_ROOT%\include" -Wall -Wextra -Wpedantic -c src\bedrock_insight\core\sqlite_api.cpp -o build\manual\sqlite_api.o
if errorlevel 1 exit /b %errorlevel%
"%BEDROCKINSIGHT_CLANG_ROOT%\clang++.exe" -std=c++20 -Isrc -I"%BEDROCKINSIGHT_SQLITE_ROOT%\include" -Wall -Wextra -Wpedantic -c src\bedrock_insight\core\storage.cpp -o build\manual\storage.o
if errorlevel 1 exit /b %errorlevel%
"%BEDROCKINSIGHT_CLANG_ROOT%\clang++.exe" -std=c++20 -Isrc -I"%BEDROCKINSIGHT_SQLITE_ROOT%\include" -Wall -Wextra -Wpedantic -c tests\core_tests.cpp -o build\manual\core_tests.o
if errorlevel 1 exit /b %errorlevel%
"%BEDROCKINSIGHT_CLANG_ROOT%\clang++.exe" -std=c++20 -Isrc -I"%BEDROCKINSIGHT_SQLITE_ROOT%\include" -Wall -Wextra -Wpedantic -c tests\clang_msvc_runtime_shim.cpp -o build\manual\shim.o
if errorlevel 1 exit /b %errorlevel%

"%BEDROCKINSIGHT_CLANG_ROOT%\clang++.exe" build\manual\api_payload.o build\manual\api_json.o build\manual\admin_api.o build\manual\analytics_query.o build\manual\dashboard.o build\manual\event.o build\manual\http_server.o build\manual\http_transport.o build\manual\json.o build\manual\metrics.o build\manual\rollback.o build\manual\sqlite_api.o build\manual\storage.o build\manual\core_tests.o build\manual\shim.o "%BEDROCKINSIGHT_SQLITE_ROOT%\lib\sqlite3.lib" -Wl,/nodefaultlib:oldnames -lws2_32 -o build\manual\BedrockInsightCoreTests.exe
if errorlevel 1 exit /b %errorlevel%

build\manual\BedrockInsightCoreTests.exe
set result=%errorlevel%
echo BedrockInsightCoreTests exit=%result%
exit /b %result%
