/******************************************************************************
*  Filename:       sblAppEx.cpp
*  Revised:        $Date$
*  Revision:       $Revision$
*
*  Description:    Serial Bootloader Library application example.
*                  This example enumerates all COM devices and lets you
*                  select which port to connect to. The example assumes the
*                  connected device is a CC2538, CC2650 or CC2652 and programs 
*                  a blinky onto the device. After programming the blinky, 
*                  bootloader mode may be forced by 
*                  - holding SELECT button on 06EB (for CC2538 and CC26x0 EMKs), or
*                  - holding BTN-1 on the device LaunchPad (for CC26x2 LPs)
*                  when resetting the chip.
*
*  Copyright (C) 2013 - 2019 Texas Instruments Incorporated - http://www.ti.com/
*
*
*  Redistribution and use in source and binary forms, with or without
*  modification, are permitted provided that the following conditions
*  are met:
*
*    Redistributions of source code must retain the above copyright
*    notice, this list of conditions and the following disclaimer.
*
*    Redistributions in binary form must reproduce the above copyright
*    notice, this list of conditions and the following disclaimer in the
*    documentation and/or other materials provided with the distribution.
*
*    Neither the name of Texas Instruments Incorporated nor the names of
*    its contributors may be used to endorse or promote products derived
*    from this software without specific prior written permission.
*
*  THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS
*  "AS IS" AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT
*  LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR
*  A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT
*  OWNER OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL,
*  SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT
*  LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE,
*  DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY
*  THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
*  (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
*  OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
*
******************************************************************************/
#include "serialib.h"
#include "sbllib.h"
#include <vector>
#include <iostream>
#include <fstream>

//#include <time.h>

using namespace std;

// Calculate crc32 checksum the way CC2538 and CC2650 does it.
int calcCrcLikeChip(const unsigned char *pData, unsigned long ulByteCount)
{
    unsigned long d, ind;
    unsigned long acc = 0xFFFFFFFF;
    const unsigned long ulCrcRand32Lut[] =
    {
        0x00000000, 0x1DB71064, 0x3B6E20C8, 0x26D930AC, 
        0x76DC4190, 0x6B6B51F4, 0x4DB26158, 0x5005713C, 
        0xEDB88320, 0xF00F9344, 0xD6D6A3E8, 0xCB61B38C, 
        0x9B64C2B0, 0x86D3D2D4, 0xA00AE278, 0xBDBDF21C
    };

    while ( ulByteCount-- )
    {
        d = *pData++;
        ind = (acc & 0x0F) ^ (d & 0x0F);
        acc = (acc >> 4) ^ ulCrcRand32Lut[ind];
        ind = (acc & 0x0F) ^ (d >> 4);
        acc = (acc >> 4) ^ ulCrcRand32Lut[ind];
    }

    return (acc ^ 0xFFFFFFFF);
}

/// Application status function (used as SBL status callback)
void appStatus(char *pcText, bool bError)
{
    if(bError)
    {
        cerr << pcText;
    }
    else
    {
        cout << pcText;
    }
}


/// Application progress function (used as SBL progress callback)
static void appProgress(uint32_t progress)
{
    fprintf(stdout, "\r%d%% ", progress);
    fflush(stdout);
}

// Defines
//		Name							Hex				Dec
//		-------------------------------	---------------	---------------------------------
#define DEVICE_CC2538              		0x2538			// 9528
#define DEVICE_CC26X0                	0x2650			// 9808
#define DEVICE_CC2640R2              	0x2640			// 9792
#define DEVICE_CC26X2                	0x2652			// 9810
#define CC2538_FLASH_BASE            	0x00200000
#define CC26XX_FLASH_BASE            	0x00000000

// Application main function
// tisbl SerialDevNode baudRate deviceType firmware 
int main(int argc, char *argv[])
{
    // START: Program Configuration
    /* UART baud rate. Default: 230400 */
    uint32_t baudRate = 115200;
    uint32_t deviceType = DEVICE_CC26X2;
    SblDevice *pDevice = NULL;          // Pointer to SblDevice object
    char *SerialDevNode;
    int32_t devStatus = -1;             // Hold SBL status codes
    std::string fileName;               // File name to program
    uint32_t byteCount = 0;             // File size in bytes
    uint32_t fileCrc, devCrc;           // Variables to save CRC checksum
    uint32_t devFlashBase;              // Flash start address
    static std::vector<char> pvWrite(1);// Vector to application firmware in.
    static std::ifstream file;          // File stream

    // Set callback functions
    SblDevice::setCallBackStatusFunction(&appStatus);
    SblDevice::setCallBackProgressFunction(&appProgress);

    // Select device
	//	deviceType			FlashBase			File	
	//	DEVICE_CC2538		CC2538_FLASH_BASE	blinky_backdoor_select_btn2538.bin
	//	DEVICE_CC26X0		CC26XX_FLASH_BASE	blinky_backdoor_select_btn2650.bin
	//	DEVICE_CC2640R2		CC26XX_FLASH_BASE	blinky_backdoor_select_btn2640r2.bin	
	//	DEVICE_CC26X2		CC26XX_FLASH_BASE	blinky_backdoor_select_btn26x2.bin	
	if(argc < 5) {
		return -1;
	}	
	SerialDevNode = argv[1];
	baudRate = atoi(argv[2]);
	deviceType = strtol(argv[3], NULL, 16);
	fileName = argv[4];
	switch(deviceType)
	{
		case DEVICE_CC2538:
			devFlashBase = CC2538_FLASH_BASE;
			break;
		case DEVICE_CC26X0:
		case DEVICE_CC2640R2:
		case DEVICE_CC26X2:
			devFlashBase = CC26XX_FLASH_BASE;
			break;
	}
	printf("SerialDevNode=%s, baudRate=%d, deviceType=%04x, fileName=%s\n",SerialDevNode,baudRate,deviceType,fileName.c_str());

    // Should SBL try to enable XOSC? (Not possible for CC26xx)
    bool bEnableXosc = false;
    if(deviceType == DEVICE_CC2538)
    {
        char answer[64];
        cout << "Enable device CC2538 XOSC? (Y/N): ";
        cin >> answer;
        bEnableXosc = (answer[0] == 'Y' || answer[0] == 'y') ? true : false;
    }

    // Create SBL object
    pDevice = SblDevice::Create(deviceType);
    if(pDevice == NULL) 
    { 
        printf("No SBL device object.\n");
		cout << "\n\nAn error occurred: " << pDevice->getLastStatus();
		return -1;
    }

    // Connect to device
    printf("Connecting (%s @ %d baud) ...\n", SerialDevNode, baudRate);
    if(pDevice->connect(SerialDevNode, baudRate, bEnableXosc) != SBL_SUCCESS) 
    {
		cout << "\n\nAn error occurred: " << pDevice->getLastStatus();
		return -1;
    }
    
    // Read file
    file.open(fileName.c_str(), std::ios::binary);
    if(file.is_open())
    {
        // Get file size:
        file.seekg(0, std::ios::end);
        byteCount = (uint32_t)file.tellg();
        file.seekg(0, std::ios::beg);

        // Read data
        pvWrite.resize(byteCount);
        file.read((char*) &pvWrite[0], byteCount);
    }
    else   
    {
        cout << "Unable to open file " << fileName.c_str();
		cout << "\n\nAn error occurred: " << pDevice->getLastStatus();
		return -1;
    }

    // Calculate file CRC checksum
    fileCrc = calcCrcLikeChip((unsigned char *)&pvWrite[0], byteCount);
	
    if(pDevice->calculateCrc32(devFlashBase, byteCount, &devCrc) != SBL_SUCCESS)
    {
		cout << "\n\nAn error occurred: " << pDevice->getLastStatus();
		return -1;
    }
	printf ("pre-Comparing CRC: fileCrc=%x,devCrc=%x\n",fileCrc,devCrc);

    if(fileCrc == devCrc) {
		cout << "CRC is same, no need to upgrade\n";
		pDevice->reset();
		return 0;
	}	

    // Erasing as much flash needed to program firmware.
    cout << "Erasing flash ...\n";
    if(pDevice->eraseFlashRange(devFlashBase, byteCount) != SBL_SUCCESS)
    {
		cout << "\n\nAn error occurred: " << pDevice->getLastStatus();
		return -1;
    }

    // Writing file to device flash memory.
    cout << "Writing flash ...\n";
    if(pDevice->writeFlashRange(devFlashBase, byteCount, &pvWrite[0]) != SBL_SUCCESS)
    {
		cout << "\n\nAn error occurred: " << pDevice->getLastStatus();
		return -1;
    }

    // Calculate CRC checksum of flashed content.
    cout << "Calculating CRC on device ...\n";
    if(pDevice->calculateCrc32(devFlashBase, byteCount, &devCrc) != SBL_SUCCESS)
    {
		cout << "\n\nAn error occurred: " << pDevice->getLastStatus();
		return -1;
    }

    // Compare CRC checksums
	printf ("Comparing CRC: fileCrc=%x,devCrc=%x\n",fileCrc,devCrc);
    if(fileCrc == devCrc) printf("OK\n");
    else printf("Mismatch!\n");

    cout << "Resetting device ...\n";
	pDevice->reset();
    cout << "OK\n";

}