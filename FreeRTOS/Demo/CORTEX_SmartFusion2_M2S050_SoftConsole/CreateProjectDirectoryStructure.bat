REM FreeRTOS Kernel V10.3.0
REM Copyright (C) 2020 Amazon.com, Inc. or its affiliates.  All Rights Reserved.
REM
REM Permission is hereby granted, free of charge, to any person obtaining a copy of
REM this software and associated documentation files (the "Software"), to deal in
REM the Software without restriction, including without limitation the rights to
REM use, copy, modify, merge, publish, distribute, sublicense, and/or sell copies of
REM the Software, and to permit persons to whom the Software is furnished to do so,
REM subject to the following conditions:
REM
REM The above copyright notice and this permission notice shall be included in all
REM copies or substantial portions of the Software.
REM
REM THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
REM IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY, FITNESS
REM FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE AUTHORS OR
REM COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER LIABILITY, WHETHER
REM IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM, OUT OF OR IN
REM CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE SOFTWARE.
REM
REM http://www.FreeRTOS.org
REM http://aws.amazon.com/freertos
REM
REM This file should be executed from the command line prior to the first
REM build.  It will be necessary to refresh the Eclipse project once the
REM .bat file has been executed (normally just press F5 to refresh).

REM Copies all the required files from their location within the standard
REM FreeRTOS directory structure to under the Eclipse project directory.
REM This permits the Eclipse project to be used in 'managed' mode and without
REM having to setup any linked resources.

REM Standard paths
SET FREERTOS_SOURCE=..\..\Source
SET COMMON_SOURCE=..\Common\minimal
SET COMMON_INCLUDE=..\Common\include
SET CLI_SOURCE=..\..\..\FreeRTOS-Plus\Source\FreeRTOS-Plus-CLI
SET FAT_SOURCE=..\..\..\FreeRTOS-Plus\Source\FreeRTOS-Plus-FAT-SL

REM Have the files already been copied?
IF EXIST RTOSDemo\FreeRTOS-Source Goto END

    REM Create the required directory structure.
    MD RTOSDemo\FreeRTOS-Source
    MD RTOSDemo\FreeRTOS-Source\include
    MD RTOSDemo\FreeRTOS-Source\portable
    MD RTOSDemo\FreeRTOS-Source\portable\GCC
    MD RTOSDemo\FreeRTOS-Source\portable\GCC\ARM_CM3
    MD RTOSDemo\FreeRTOS-Source\portable\MemMang

    REM Copy the core kernel files into the project directory
    copy %FREERTOS_SOURCE%\tasks.c RTOSDemo\FreeRTOS-Source
    copy %FREERTOS_SOURCE%\queue.c RTOSDemo\FreeRTOS-Source
    copy %FREERTOS_SOURCE%\list.c RTOSDemo\FreeRTOS-Source
    copy %FREERTOS_SOURCE%\timers.c RTOSDemo\FreeRTOS-Source

    REM Copy the common header files into the project directory
    copy %FREERTOS_SOURCE%\include\*.* RTOSDemo\FreeRTOS-Source\include

    REM Copy the portable layer files into the project directory
    copy %FREERTOS_SOURCE%\portable\GCC\ARM_CM3\*.* RTOSDemo\FreeRTOS-Source\portable\GCC\ARM_CM3

    REM Copy the memory allocation files into the project directory
    copy %FREERTOS_SOURCE%\portable\MemMang\heap_4.c RTOSDemo\FreeRTOS-Source\portable\MemMang

    REM Copy the files that define the common demo tasks.
    copy %COMMON_SOURCE%\dynamic.c         RTOSDemo\Full-Demo\Common-Demo-Source
    copy %COMMON_SOURCE%\BlockQ.c          RTOSDemo\Full-Demo\Common-Demo-Source
    copy %COMMON_SOURCE%\flash_timer.c     RTOSDemo\Full-Demo\Common-Demo-Source
    copy %COMMON_SOURCE%\death.c           RTOSDemo\Full-Demo\Common-Demo-Source
    copy %COMMON_SOURCE%\blocktim.c        RTOSDemo\Full-Demo\Common-Demo-Source
    copy %COMMON_SOURCE%\semtest.c         RTOSDemo\Full-Demo\Common-Demo-Source
    copy %COMMON_SOURCE%\PollQ.c           RTOSDemo\Full-Demo\Common-Demo-Source
    copy %COMMON_SOURCE%\GenQTest.c        RTOSDemo\Full-Demo\Common-Demo-Source
    copy %COMMON_SOURCE%\recmutex.c        RTOSDemo\Full-Demo\Common-Demo-Source
    copy %COMMON_SOURCE%\countsem.c        RTOSDemo\Full-Demo\Common-Demo-Source
    copy %COMMON_SOURCE%\integer.c         RTOSDemo\Full-Demo\Common-Demo-Source

    REM Copy the common demo file headers.
    copy %COMMON_INCLUDE%\*.h              RTOSDemo\Full-Demo\Common-Demo-Source\include

	REM Copy the FreeRTOS+CLI source.
	copy %CLI_SOURCE%\*.*                  RTOSDemo\Full-Demo\FreeRTOS-Plus-CLI-Source

	REM Copy the FreeRTOS+FAT SL source.
	xcopy %FAT_SOURCE%\*.*                 RTOSDemo\Full-Demo\FreeRTOS-Plus-FAT-SL-Source /S

: END
