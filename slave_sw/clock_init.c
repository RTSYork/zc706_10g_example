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
#include "platform.h"
#include "xiicps.h"

#define I2C_DEV 0
#define I2C_CLK 100000

#define I2C_BUS_SWITCH 0x74
#define I2C_SI5324 0x68
#define I2C_BUS_SWITCH_DIR_SI5324 (1 << 4)

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
	while(XIicPs_BusIsBusy(i2c_dev));
	return XIicPs_MasterRecvPolled(i2c_dev, buffer, buf_max, dev_id);
}

int i2c_write_single_reg(XIicPs* i2c_dev, uint8_t dev_id, uint8_t reg_address, uint8_t reg_value)
{
	uint8_t buf[2] = {reg_address, reg_value};

	return i2c_write_await(i2c_dev, dev_id, buf, 2);
}

int i2c_init_clk()
{
	// We only need the I2C device for the clock setup, so just move it all here instead
	XIicPs i2c_dev;
	int rv = 0;

	XIicPs_Config* cfg = XIicPs_LookupConfig(I2C_DEV);
	if(cfg == NULL)
	{
		printf("Could not lookup config for device %d\r\n", I2C_DEV);
		return -1;
	}

	rv = XIicPs_CfgInitialize(&i2c_dev, cfg, cfg->BaseAddress);
	if(rv != XST_SUCCESS)
	{
		printf("Could not init I2C device\r\n");
		return -1;
	}

	rv = XIicPs_SelfTest(&i2c_dev);
	if(rv != XST_SUCCESS)
	{
		printf("I2C Self-test failed\r\n");
		return -1;
	}

	XIicPs_SetSClk(&i2c_dev, I2C_CLK);

	// Ok, try and read from the bus switch
	uint8_t val = 0;
	rv = i2c_recv(&i2c_dev, I2C_BUS_SWITCH, &val, 1);
	if(rv != XST_SUCCESS)
	{
		printf("i2c_recv failed with code %d\r\n", rv);
		return -1;
	}

	val = I2C_BUS_SWITCH_DIR_SI5324;
	rv = i2c_write_await(&i2c_dev, I2C_BUS_SWITCH, &val, 1);
	if(rv != XST_SUCCESS)
	{
		printf("i2c_send failed with code %d\r\n", rv);
		return -1;
	}

	rv = i2c_recv(&i2c_dev, I2C_BUS_SWITCH, &val, 1);
	if(rv != XST_SUCCESS)
	{
		printf("i2c_read failed with code %d\r\n", rv);
		return -1;
	}

	i2c_write_single_reg(&i2c_dev, I2C_SI5324, 0x0,   0x54);   // FREE_RUN
	i2c_write_single_reg(&i2c_dev, I2C_SI5324, 0x1,   0xE4);   // CK_PRIOR2,CK_PRIOR1
	i2c_write_single_reg(&i2c_dev, I2C_SI5324, 0x2,   0x12);   // BWSEL was 32
	i2c_write_single_reg(&i2c_dev, I2C_SI5324, 0x3,   0x15);   // CKSEL_REG
	i2c_write_single_reg(&i2c_dev, I2C_SI5324, 0x4,   0x92);   // AUTOSEL_REG
	i2c_write_single_reg(&i2c_dev, I2C_SI5324, 0xa,   0x08);   // DSBL2_REG
	i2c_write_single_reg(&i2c_dev, I2C_SI5324, 0xb,   0x40);   // PD_CK2
	i2c_write_single_reg(&i2c_dev, I2C_SI5324, 0x19,  0xA0);   // N1_HS
	i2c_write_single_reg(&i2c_dev, I2C_SI5324, 0x1f,  0x00);   // NC1_LS[19:16]
	i2c_write_single_reg(&i2c_dev, I2C_SI5324, 0x20,  0x00);   // NC1_LS[15:8]
	i2c_write_single_reg(&i2c_dev, I2C_SI5324, 0x21,  0x03);   // NC1_LS[7:0]
	i2c_write_single_reg(&i2c_dev, I2C_SI5324, 0x28,  0xC2);   // N2_HS, N2_LS[19:16]
	i2c_write_single_reg(&i2c_dev, I2C_SI5324, 0x29,  0x49);   // N2_LS[15:8]
	i2c_write_single_reg(&i2c_dev, I2C_SI5324, 0x2a,  0xEF);   // N2_LS[7:0]
	i2c_write_single_reg(&i2c_dev, I2C_SI5324, 0x2b,  0x00);   // N31[18:16]
	i2c_write_single_reg(&i2c_dev, I2C_SI5324, 0x2c,  0x77);   // N31[15:8]
	i2c_write_single_reg(&i2c_dev, I2C_SI5324, 0x2d,  0x0B);   // N31[7:0]
	i2c_write_single_reg(&i2c_dev, I2C_SI5324, 0x2e,  0x00);   // N32[18:16]
	i2c_write_single_reg(&i2c_dev, I2C_SI5324, 0x2f,  0x77);   // N32[15:8]
	i2c_write_single_reg(&i2c_dev, I2C_SI5324, 0x30,  0x0B);   // N32[7:0]
	i2c_write_single_reg(&i2c_dev, I2C_SI5324, 0x88,  0x40);   // RST_REG,ICAL

	return XST_SUCCESS;
}

int main()
{	
	int bufId = 0;
    init_platform();

    printf("Hello World\n\r");
    if(i2c_init_clk() != XST_SUCCESS)
    	return -1;

    while(1);

    cleanup_platform();
    return 0;
}
