@echo off
rem Android APK build script (new pipeline: aapt2 + d8 + zipalign + apksigner).
rem Requires: JAVA_HOME set, SDK at android-app/sdk
setlocal EnableDelayedExpansion

set APP=%~dp0
set SDK=%APP%sdk
set BT=%SDK%\build-tools\34.0.0
set PLATFORM=%SDK%\platforms\android-34\android.jar
set SRC=%APP%app\src
set ASSETS=%APP%app\assets
set OUT=%APP%build

set AAPT=%BT%\aapt.exe
set AAPT2=%BT%\aapt2.exe
set D8=%BT%\d8.bat
set ZALIGN=%BT%\zipalign.exe
set APKSIGNER=%BT%\apksigner.bat
set KEYSTORE=%OUT%\encrypt.keystore
set MIN_SDK=24
set TARGET_SDK=34
set VERSION_CODE=3
set VERSION_NAME=1.2

if not defined JAVA_HOME (
    echo ERROR: JAVA_HOME not set
    exit /b 1
)

if exist "%OUT%\classes" rd /s /q "%OUT%\classes"
if exist "%OUT%\dexout" rd /s /q "%OUT%\dexout"
mkdir "%OUT%\classes"
mkdir "%OUT%\dexout"

echo === Step 1: javac (--release 11, modern) ===
"%JAVA_HOME%\bin\javac.exe" --release 11 -classpath "%PLATFORM%" -d "%OUT%\classes" "%SRC%\com\example\encrypt\MainActivity.java" "%SRC%\com\example\encrypt\FileChooserClient.java" "%SRC%\com\example\encrypt\AppWebViewClient.java" "%SRC%\com\example\encrypt\JsBridge.java"
if errorlevel 1 goto fail

echo === Step 2: d8 (min-api %MIN_SDK%) ===
pushd "%OUT%\classes"
call "%D8%" --release --lib "%PLATFORM%" --min-api %MIN_SDK% --output "%OUT%\dexout" com\example\encrypt\*.class
if errorlevel 1 goto fail
popd

echo === Step 3: aapt package (min=%MIN_SDK% target=%TARGET_SDK% v%VERSION_NAME%) ===
"%AAPT%" package -f -M "%APP%app\AndroidManifest.xml" -S "%APP%app\res" -A "%ASSETS%" -I "%PLATFORM%" -F "%OUT%\base.apk" --min-sdk-version %MIN_SDK% --target-sdk-version %TARGET_SDK% --version-code %VERSION_CODE% --version-name %VERSION_NAME%
if errorlevel 1 goto fail

echo === Step 4: add dex ===
pushd "%OUT%\dexout"
"%AAPT%" add "%OUT%\base.apk" classes.dex
popd
if errorlevel 1 goto fail

echo === Step 5: zipalign ===
"%ZALIGN%" -f 4 "%OUT%\base.apk" "%OUT%\aligned.apk"
if errorlevel 1 goto fail

if not exist "%KEYSTORE%" (
    echo === Step 5.5: generate keystore ===
    call "%JAVA_HOME%\bin\keytool.exe" -genkeypair -keystore "%KEYSTORE%" -alias encrypt -keyalg RSA -keysize 2048 -validity 10000 -storepass encrypt123 -keypass encrypt123 -dname "CN=TextEncrypt,OU=Local,O=Local,C=CN"
)

echo === Step 6: sign ===
call "%APKSIGNER%" sign --ks "%KEYSTORE%" --ks-key-alias encrypt --ks-pass pass:encrypt123 --key-pass pass:encrypt123 --out "%OUT%\encrypt.apk" "%OUT%\aligned.apk"
if errorlevel 1 goto fail

echo === Step 7: verify ===
call "%APKSIGNER%" verify "%OUT%\encrypt.apk"
if errorlevel 1 goto fail

echo === APK info ===
"%AAPT%" dump badging "%OUT%\encrypt.apk" | findstr "sdkVersion targetSdkVersion versionCode versionName"
for %%F in ("%OUT%\encrypt.apk") do echo size: %%~zF bytes

echo BUILD OK
exit /b 0

:fail
echo BUILD FAILED
exit /b 1
