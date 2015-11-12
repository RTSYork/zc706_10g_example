/******************************************************************************
*
* Copyright (C) 2009 - 2014 Xilinx, Inc.  All rights reserved.
*
* Permission is hereby granted, free of charge, to any person obtaining a copy
* of this software and associated documentation files (the "Software"), to deal
* in the Software without restriction, including without limitation the rights
* to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
* copies of the Software, and to permit persons to whom the Software is
* furnished to do so, subject to the following conditions:
*
* The above copyright notice and this permission notice shall be included in
* all copies or substantial portions of the Software.
*
* Use of the Software is limited solely to applications:
* (a) running on a Xilinx device, or
* (b) that interact with a Xilinx device through a bus or interconnect.
*
* THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
* IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
* FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL
* XILINX  BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER LIABILITY,
* WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM, OUT OF
* OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE
* SOFTWARE.
*
* Except as contained in this notice, the name of the Xilinx shall not be used
* in advertising or otherwise to promote the sale, use or other dealings in
* this Software without prior written authorization from Xilinx.
*
******************************************************************************/

/*
 * helloworld.c: simple test application
 *
 * This application configures UART 16550 to baud rate 9600.
 * PS7 UART (Zynq) is not initialized by this application, since
 * bootrom/bsp configures it to baud rate 115200
 *
 * ------------------------------------------------
 * | UART TYPE   BAUD RATE                        |
 * ------------------------------------------------
 *   uartns550   9600
 *   uartlite    Configurable only in HW design
 *   ps7_uart    115200 (configured by bootrom/bsp)
 */

#include <stdio.h>
#include <stdint.h>
#include <stdlib.h>
#include <malloc.h>
#include "platform.h"
#include "xiicps.h"
#include "xaxidma.h"
#include "xaxidma_hw.h"
#include "xaxidma_bd.h"
#include "xparameters.h"

#define I2C_DEV 0
#define I2C_CLK 100000

#define I2C_BUS_SWITCH 0x74
#define I2C_SI5324 0x68
#define I2C_BUS_SWITCH_DIR_SI5324 (1 << 4)

#define DMA_BLR_WIDTH 14
#define DMA_BUF_MAX ((1 << DMA_BLR_WIDTH) - 1)

// MTU is, of course, 1500
// 1500 isn't a clean multiple of eight bytes (i.e. the 64-bit datapath)
// though, so for testing, round down to 1496
#define TEST_PKT_SIZE 1496

#define DMA_BASE XPAR_AXI_DMA_0_BASEADDR // Shorten the symbol name...
#define TENGIG_BASE XPAR_AXI_10G_ETHERNET_0_BASEADDR

#define DMA_GET_REG(offset) (*((volatile uint32_t*)(DMA_BASE + (offset))))
#define DMA_SET_REG(offset, val) (*((volatile uint32_t*)(DMA_BASE + (offset))) = (val))

#define TENGIG_GET_REG(offset) (*((volatile uint32_t*)(TENGIG_BASE + (offset))))

int i2c_write(XIicPs* i2c_dev, uint8_t dev_id, uint8_t* buffer, int size);
int i2c_write_await(XIicPs* i2c_dev, uint8_t dev_id, uint8_t* buffer, int size);
int i2c_recv(XIicPs* i2c_dev, uint8_t dev_id, uint8_t* buffer, int buf_max);
int i2c_write_single_reg(XIicPs* i2c_dev, uint8_t dev_id, uint8_t reg_address, uint8_t reg_value);
int i2c_init_clk();

int i2c_write(XIicPs* i2c_dev, uint8_t dev_id, uint8_t* buffer, int size)
{
	while(XIicPs_BusIsBusy(i2c_dev));
	return XIicPs_MasterSendPolled(i2c_dev, buffer, size, dev_id);
}

int i2c_write_await(XIicPs* i2c_dev, uint8_t dev_id, uint8_t* buffer, int size)
{
	int rv = 0;
	rv = i2c_write(i2c_dev, dev_id, buffer, size);
	if(rv != XST_SUCCESS)
		return rv;
	while(XIicPs_BusIsBusy(i2c_dev));
	return XST_SUCCESS;
}

int i2c_recv(XIicPs* i2c_dev, uint8_t dev_id, uint8_t* buffer, int buf_max)
{
	return XIicPs_MasterRecvPolled(i2c_dev, buffer, buf_max, dev_id);
}

int i2c_write_single_reg(XIicPs* i2c_dev, uint8_t dev_id, uint8_t reg_address, uint8_t reg_value)
{
	uint8_t buf[2] = {reg_address, reg_value};

	return i2c_write_await(i2c_dev, dev_id, buf, 2);
}

int i2c_init_clk()
{
	XIicPs i2c_device;
	volatile int x;

	// We only need the I2C device for the clock setup, so just move it all here instead
	memset(&i2c_device, 0, sizeof(XIicPs));
	int rv = 0;

	XIicPs_Config* cfg = XIicPs_LookupConfig(I2C_DEV);
	if(cfg == NULL)
	{
		printf("Could not lookup config for device %d\r\n", I2C_DEV);
		return -1;
	}

	rv = XIicPs_CfgInitialize(&i2c_device, cfg, cfg->BaseAddress);
	//printf("TEST 0x%x\r\n", cfg->BaseAddress);
	//printf("TEST 0x%x\r\n", cfg->BaseAddress);
	if(rv != XST_SUCCESS)
	{
		printf("Could not init I2C device\r\n");
		return -1;
	}

	rv = XIicPs_SelfTest(&i2c_device);
	if(rv != XST_SUCCESS)
	{
		printf("I2C Self-test failed\r\n");
		return -1;
	}

	XIicPs_SetSClk(&i2c_device, I2C_CLK);

	// Ok, try and read from the bus switch
	uint8_t val = 0;
	rv = i2c_recv(&i2c_device, I2C_BUS_SWITCH, &val, 1);
	if(rv != XST_SUCCESS)
	{
		printf("i2c_recv failed with code %d\r\n", rv);
		return -1;
	}
	printf("1: Bus switch says 0x%x\r\n", val);

	val = I2C_BUS_SWITCH_DIR_SI5324;
	rv = i2c_write_await(&i2c_device, I2C_BUS_SWITCH, &val, 1);
	if(rv != XST_SUCCESS)
	{
		printf("i2c_send failed with code %d\r\n", rv);
		return -1;
	}

	printf("Written 0x%x to bus switch\r\n", val);

	rv = i2c_recv(&i2c_device, I2C_BUS_SWITCH, &val, 1);
	if(rv != XST_SUCCESS)
	{
		printf("i2c_read failed with code %d\r\n", rv);
		return -1;
	}

	printf("Bus switch says 0x%x\r\n", val);

	i2c_write_single_reg(&i2c_device, I2C_SI5324, 0x0,   0x54);   // FREE_RUN
	i2c_write_single_reg(&i2c_device, I2C_SI5324, 0x1,   0xE4);   // CK_PRIOR2,CK_PRIOR1
	i2c_write_single_reg(&i2c_device, I2C_SI5324, 0x2,   0x12);   // BWSEL was 32
	i2c_write_single_reg(&i2c_device, I2C_SI5324, 0x3,   0x15);   // CKSEL_REG
	i2c_write_single_reg(&i2c_device, I2C_SI5324, 0x4,   0x92);   // AUTOSEL_REG
	i2c_write_single_reg(&i2c_device, I2C_SI5324, 0xa,   0x08);   // DSBL2_REG
	i2c_write_single_reg(&i2c_device, I2C_SI5324, 0xb,   0x40);   // PD_CK2
	i2c_write_single_reg(&i2c_device, I2C_SI5324, 0x19,  0xA0);   // N1_HS
	i2c_write_single_reg(&i2c_device, I2C_SI5324, 0x1f,  0x00);   // NC1_LS[19:16]
	i2c_write_single_reg(&i2c_device, I2C_SI5324, 0x20,  0x00);   // NC1_LS[15:8]
	i2c_write_single_reg(&i2c_device, I2C_SI5324, 0x21,  0x03);   // NC1_LS[7:0]
	i2c_write_single_reg(&i2c_device, I2C_SI5324, 0x28,  0xC2);   // N2_HS, N2_LS[19:16]
	i2c_write_single_reg(&i2c_device, I2C_SI5324, 0x29,  0x49);   // N2_LS[15:8]
	i2c_write_single_reg(&i2c_device, I2C_SI5324, 0x2a,  0xEF);   // N2_LS[7:0]
	i2c_write_single_reg(&i2c_device, I2C_SI5324, 0x2b,  0x00);   // N31[18:16]
	i2c_write_single_reg(&i2c_device, I2C_SI5324, 0x2c,  0x77);   // N31[15:8]
	i2c_write_single_reg(&i2c_device, I2C_SI5324, 0x2d,  0x0B);   // N31[7:0]
	i2c_write_single_reg(&i2c_device, I2C_SI5324, 0x2e,  0x00);   // N32[18:16]
	i2c_write_single_reg(&i2c_device, I2C_SI5324, 0x2f,  0x77);   // N32[15:8]
	i2c_write_single_reg(&i2c_device, I2C_SI5324, 0x30,  0x0B);   // N32[7:0]
	i2c_write_single_reg(&i2c_device, I2C_SI5324, 0x88,  0x40);   // RST_REG,ICAL

	return XST_SUCCESS;
}

int main()
{	
	int bufId = 0;
    init_platform();

    printf("Hello World\n\r");
    if(i2c_init_clk() != XST_SUCCESS)
    	return -1;

    // Actual test
    // We'll allocate four packets on the heap
    uint8_t* buf[4];
    for(bufId = 0; bufId < 4; bufId++)
    {
    	//buf[bufId] = malloc(TEST_PKT_SIZE);
    	buf[bufId] = memalign(8, TEST_PKT_SIZE);

    	if(buf[bufId] == 0)
    		printf("ERROR: Buffer %d allocation failed\r\n", bufId);

    	// Sanity check the alignment
    	if((uint32_t)buf[bufId] & 0x7)
    	{
    		printf("ERROR: Buffer %d allocated at %p has invalid alignment\r\n", bufId, buf[bufId]);
    		return -1;
    	}
    }

    uint8_t destMac[6] = {0x00, 0x11, 0x22, 0x33, 0x01, 0x00};
    uint8_t srcMac[6] = {0x00, 0x11, 0x22, 0x33, 0x00, 0x00};

    // First, set up the actual contents of those packets
    for(bufId = 0; bufId < 4; bufId++)
    {
    	int i = 0;

    	// Fill in the MAC addresses
	    for(i = 0; i < 6; i++)
	    {
	    	buf[bufId][i]   = destMac[i];
	    	buf[bufId][i+6] = srcMac[i];
	    }

	    // Fill out the ethertype
    	buf[bufId][12] = 0x08;
    	buf[bufId][13] = 0x00;
    	memset(&buf[bufId][14], bufId+0xAA, TEST_PKT_SIZE - 14);
	}

    // Now fill in the bd descriptors
    // Alloc the BD descriptors
    XAxiDma_Bd* bd[4];
    for(bufId = 0; bufId < 4; bufId++)
    {
    	//bd[bufId] = malloc(sizeof(XAxiDma_Bd));
    	bd[bufId] = memalign(64, sizeof(XAxiDma_Bd));
    	if(bd[bufId] == 0)
    		printf("ERROR: Buffer BD descriptor %d allocation failed\r\n", bufId);

    	memset(bd[bufId], 0, sizeof(XAxiDma_Bd));

    	// Sanity check that they're aligned appropriately
    	// They need to be on a 16-word (i.e. 64-byte) boundary
    	if((uint32_t)bd[bufId] & 0x3F)
    	{
    		printf("ERROR: Buffer BD descriptor %d allocated at %p has invalid alignment\r\n", bufId, bd[bufId]);
    		return -1;
    	}

    	// Fill in the descriptor fields
    	//XAxiDma_BdSetBufAddr(bd[bufId], (uint32_t)buf[bufId]);
    	//XAxiDma_BdSetLength(bd[bufId], TEST_PKT_SIZE, DMA_BUF_MAX);

    	// Just do those manually - the driver appears to expect some metadata to be
    	// added by the ring manager, which we don't add ourselves.
    	XAxiDma_BdWrite(bd[bufId], XAXIDMA_BD_BUFA_OFFSET, (uint32_t)buf[bufId]);
    	XAxiDma_BdWrite(bd[bufId], XAXIDMA_BD_CTRL_LEN_OFFSET, TEST_PKT_SIZE);

    	// Need to set SOF and EOF bits too
    	// Do that manually - the API doesn't appear to expose it properly
    	// XAxiDma_Bd is actually an array behind the scenes, hence this syntax
    	uint32_t tmp = XAxiDma_BdRead(bd[bufId], XAXIDMA_BD_CTRL_LEN_OFFSET);
    	tmp |= (XAXIDMA_BD_CTRL_TXSOF_MASK | XAXIDMA_BD_CTRL_TXEOF_MASK);
    	XAxiDma_BdWrite(bd[bufId], XAXIDMA_BD_CTRL_LEN_OFFSET, tmp);
   	}

	for(bufId = 0; bufId < 4; bufId++)
	{
		// Set the "next" descriptor pointers now everything has been allocated properly.
    	XAxiDma_BdWrite(bd[bufId], XAXIDMA_BD_NDESC_OFFSET, (uint32_t)(bd[(bufId + 1) % 4]));

		// Debugging sanity check
		XAxiDma_DumpBd(bd[bufId]);
	}

	// Buffers should now be set up
	// Set up the DMA core and (hopefully) start!
	// Flush caches to ensure that everything is coherent
	for(bufId = 0; bufId < 4; bufId++)
	{
		XAXIDMA_CACHE_FLUSH(bd[bufId]);

		// Also flush packets
		// Syntax stolen from xaxidma_bd.h
		Xil_DCacheFlushRange((uint32_t)buf[bufId], TEST_PKT_SIZE);
	}

	// First, reset the DMA core
	uint32_t reg = 0;
	reg = DMA_GET_REG(XAXIDMA_CR_OFFSET);
	reg |= XAXIDMA_CR_RESET_MASK;
	DMA_SET_REG(XAXIDMA_CR_OFFSET, reg);
	while(DMA_GET_REG(XAXIDMA_CR_OFFSET) & XAXIDMA_CR_RESET_MASK);

	printf("DMA Reset\r\n");

	// Update the "next" pointer
	DMA_SET_REG(XAXIDMA_CDESC_OFFSET, (uint32_t)bd[0]);

	// The tail pointer should already be zero. Leave it.
	// Enable cyclic BD mode
	reg = DMA_GET_REG(XAXIDMA_CR_OFFSET);
	reg |= XAXIDMA_CR_CYCLIC_MASK;
	DMA_SET_REG(XAXIDMA_CR_OFFSET, reg);

	// Set run mode!
	reg = DMA_GET_REG(XAXIDMA_CR_OFFSET);
	reg |= XAXIDMA_CR_RUNSTOP_MASK;
	DMA_SET_REG(XAXIDMA_CR_OFFSET, reg);

	// Quickly check we can read the base address from the ethernet...
	printf("TEST: 0x%x\r\n", DMA_GET_REG(XAXIDMA_CR_OFFSET));

	// Cyclic mode seems to require that we write something to the tail descriptor
	DMA_SET_REG(XAXIDMA_TDESC_OFFSET, 0x0);

	while(1)
	{
		volatile int x = 0;
		uint32_t tmp = 0;
		uint32_t tmpHigh = 0;

		tmp = DMA_GET_REG(XAXIDMA_SR_OFFSET);
		printf("DMA STATUS: 0x%x\r\n", tmp);

		tmp = TENGIG_GET_REG(0x200); // LSW bytes recv
		tmpHigh = TENGIG_GET_REG(0x204); // MSW bytes recv
		printf("RX BYTES: 0x%x%x\r\n", tmpHigh, tmp);

		tmp = TENGIG_GET_REG(0x208); // LSW bytes tx
		tmpHigh = TENGIG_GET_REG(0x20C); // MSW bytes tx
		printf("TX BYTES: 0x%x%x\r\n", tmpHigh, tmp);

		tmp = TENGIG_GET_REG(0x2D8);
		tmpHigh = TENGIG_GET_REG(0x2DC);
		printf("TX OK: 0x%x%x\r\n", tmpHigh, tmp);

		tmp = TENGIG_GET_REG(0x2E0);
		tmpHigh = TENGIG_GET_REG(0x2E4);
		printf("TX BROADCAST: 0x%x%x\r\n", tmpHigh, tmp);

		tmp = TENGIG_GET_REG(0x2F0);
		tmpHigh = TENGIG_GET_REG(0x2F4);
		printf("TX UNDERRUN: 0x%x%x\r\n", tmpHigh, tmp);

		tmp = DMA_GET_REG(0x8);
		printf("CURDES: 0x%x\r\n", tmp);

		printf("\r\n");

		for(x = 0 ; x < 100000000; x++);
	}

    cleanup_platform();
    return 0;
}
