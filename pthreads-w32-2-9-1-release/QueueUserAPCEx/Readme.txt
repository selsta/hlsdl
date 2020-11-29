TEST PLATFORM:
Windows XP Home, Microsoft Visual C++ 6.0, Drivers Development Kit
(Windows 2000 DDK).

PREBUILT DRIVER AND DLL SUPPLIED
Exec/AlertDrv.sys
User/quserex.dll

DIRECTORIES:
driver: Source code of the device driver
user: Source code of QueueUserAPCEx
testapp: Demo of QueueUserAPCEx
execs: Binary version of the device driver

INSTALLING THE DRIVER
1. Copy the driver alertdrv.sys into the WINNT\system32\drivers directory.
2. Execute "regedit alertdrv.reg"
3. Reboot the system

INSTALLING THE DLL
Put User/QuserEx.dll somewhere appropriate where the system will find
it when your application is run.

LOADING AND UNLOADING THE DRIVER
After rebooting, the driver has been installed on the system.
Use "net start alertdrv" to load (start) the driver, 
or "net stop alertdrv" to unload (stop) the driver.

BUILDING THE DRIVER FROM SOURCE
Click the Free Build Environment or Checked Build Environment 
icon under your Developement Kits program group to set basic 
environment variables needed by the build utility.

Change to the directory containing the device source code. 
Run build -ceZ. The driver alertdrv.sys will be placed in
the platform specific subdirectory driver\i386.

BUILDING QUSEREX.DLL
Use either Makefile (MSVC nmake) or GNUmakefile (MinGW MSys make)
supplied on the User folder, or follow your nose if using an IDE.

BUILDING THE TEST APPLICATION
1. Open the testapp.dsp file with Microsoft Visual C++.
2. Do not forget to load (start) the driver.
