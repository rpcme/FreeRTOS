#! /bin/sh

# FreeRTOS Kernel V10.3.0
# Copyright (C) 2020 Amazon.com, Inc. or its affiliates.  All Rights Reserved.
#
# Permission is hereby granted, free of charge, to any person obtaining a copy of
# this software and associated documentation files (the "Software"), to deal in
# the Software without restriction, including without limitation the rights to
# use, copy, modify, merge, publish, distribute, sublicense, and/or sell copies of
# the Software, and to permit persons to whom the Software is furnished to do so,
# subject to the following conditions:
#
# The above copyright notice and this permission notice shall be included in all
# copies or substantial portions of the Software.
#
# THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
# IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY, FITNESS
# FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE AUTHORS OR
# COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER LIABILITY, WHETHER
# IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM, OUT OF OR IN
# CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE SOFTWARE.
#
# http://www.FreeRTOS.org
# http://aws.amazon.com/freertos
#
# This file should be executed from the command line prior to the first
# build.  It will be necessary to refresh the Eclipse project once the
# .bat file has been executed (normally just press F5 to refresh).

# Copies all the required files from their location within the standard
# FreeRTOS directory structure to under the Eclipse project directory.
# This permits the Eclipse project to be used in 'managed' mode and without
# having to setup any linked resources.

# Standard paths
BASEDIR=$(dirname $0)
FREERTOS_SOURCE=${BASEDIR}/../../Source
COMMON_SOURCE=${BASEDIR}/../Common/minimal
COMMON_INCLUDE=${BASEDIR}/../Common/include
CLI_SOURCE=${BASEDIR}/../../../FreeRTOS-Labs/Source/FreeRTOS-Plus-CLI
FAT_SOURCE=${BASEDIR}/../../../FreeRTOS-Labs/Source/FreeRTOS-Plus-FAT

# Have the files already been copied?
SRCDIR=${BASEDIR}/RTOSDemo/FreeRTOS-Source
FULLDEMO_COMMON=${BASEDIR}/RTOSDemo/Full-Demo/Common-Demo-Source
FULLDEMO_INC=${BASEDIR}/RTOSDemo/Full-Demo/Common-Demo-Source/include
FULLDEMO_CLI=${BASEDIR}/RTOSDemo/Full-Demo/FreeRTOS-Plus-CLI-Source
FULLDEMO_FAT=${BASEDIR}/RTOSDemo/Full-Demo/FreeRTOS-Plus-FAT-Source

if test -f ${SRCDIR}; then
    echo "ERROR: Target directory ${SRCDIR} already exists."
    exit 1
fi

# Create the required directory structure.
mkdir -p ${SRCDIR}
mkdir -p ${SRCDIR}/include
mkdir -p ${SRCDIR}/portable
mkdir -p ${SRCDIR}/portable/GCC
mkdir -p ${SRCDIR}/portable/GCC/ARM_CM3
mkdir -p ${SRCDIR}/portable/MemMang

# Copy the core kernel files into the project directory
cp ${FREERTOS_SOURCE}/tasks.c  ${SRCDIR}/
cp ${FREERTOS_SOURCE}/queue.c  ${SRCDIR}/
cp ${FREERTOS_SOURCE}/list.c   ${SRCDIR}/
cp ${FREERTOS_SOURCE}/timers.c ${SRCDIR}/

# Copy the common header files into the project directory
cp ${FREERTOS_SOURCE}/include/*.* ${SRCDIR}/include/

# Copy the portable layer files into the project directory
cp ${FREERTOS_SOURCE}/portable/GCC/ARM_CM3/*.* ${SRCDIR}/portable/GCC/ARM_CM3/

# Copy the memory allocation files into the project directory
cp ${FREERTOS_SOURCE}/portable/MemMang/heap_4.c ${SRCDIR}/portable/MemMang/

# Copy the files that define the common demo tasks.
cp ${COMMON_SOURCE}/dynamic.c         ${FULLDEMO_COMMON}/
cp ${COMMON_SOURCE}/BlockQ.c          ${FULLDEMO_COMMON}/
cp ${COMMON_SOURCE}/flash_timer.c     ${FULLDEMO_COMMON}/
cp ${COMMON_SOURCE}/death.c           ${FULLDEMO_COMMON}/
cp ${COMMON_SOURCE}/blocktim.c        ${FULLDEMO_COMMON}/
cp ${COMMON_SOURCE}/semtest.c         ${FULLDEMO_COMMON}/
cp ${COMMON_SOURCE}/PollQ.c           ${FULLDEMO_COMMON}/
cp ${COMMON_SOURCE}/GenQTest.c        ${FULLDEMO_COMMON}/
cp ${COMMON_SOURCE}/recmutex.c        ${FULLDEMO_COMMON}/
cp ${COMMON_SOURCE}/countsem.c        ${FULLDEMO_COMMON}/
cp ${COMMON_SOURCE}/integer.c         ${FULLDEMO_COMMON}/

# Copy the common demo file headers.
cp ${COMMON_INCLUDE}/*.h             ${FULLDEMO_INC}/

# Copy the FreeRTOS+CLI source.
cp ${CLI_SOURCE}\*                   ${FULLDEMO_CLI}/

# Copy the FreeRTOS+FAT source.
cp -r ${FAT_SOURCE}\*                ${FULLDEMO_FAT}/
