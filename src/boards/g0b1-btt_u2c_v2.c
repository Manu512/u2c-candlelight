/*
 * The MIT License (MIT)
 *
 * Copyright (c) 2026 Emmanuel Waldner
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
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
 * IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
 * FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
 * AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
 * LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
 * OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN
 * THE SOFTWARE.
 *
 */

#include "board.h"
#include "config.h"
#include "device.h"
#include "gpio.h"
#include "usbd_gs_can.h"

/*
 * BIGTREETECH U2C V2.x (STM32G0B1, single CAN channel)
 *
 * BTT publishes no schematic for this board. The pin usage below is
 * taken from the firmware BTT ships on it (bigtreetech/candleLight_fw,
 * branch stm32g0_support, commit b1ecdd26), and the electrical states
 * are kept identical to what that firmware drives:
 *
 * LED_RX   PB4  active high
 * LED_TX   PB3  active high
 * USB_N    PA11
 * USB_P    PA12
 * CAN      PB5, PB6  FDCAN2 (AF3)
 * PA0      held high: the vendor firmware writes it high at init, on
 *          channel start and on channel stop (its standby handling
 *          never drives it low)
 * PA1      held low: vendor "termination" output, never enabled. The
 *          board terminates the bus with physical 120R jumpers, so no
 *          software termination control is advertised to the host.
 */

#define LEDRX_GPIO_Port	  GPIOB
#define LEDRX_Pin		  GPIO_PIN_4
#define LEDRX_Mode		  GPIO_MODE_OUTPUT_PP
#define LEDRX_Active_High 1

#define LEDTX_GPIO_Port	  GPIOB
#define LEDTX_Pin		  GPIO_PIN_3
#define LEDTX_Mode		  GPIO_MODE_OUTPUT_PP
#define LEDTX_Active_High 1

void board_setup(USBD_GS_CAN_HandleTypeDef __maybe_unused *hcan)
{
	GPIO_InitTypeDef GPIO_InitStruct;

	__HAL_RCC_GPIOA_CLK_ENABLE();
	__HAL_RCC_GPIOB_CLK_ENABLE();

	/* LEDs */
	HAL_GPIO_WritePin(LEDRX_GPIO_Port, LEDRX_Pin, GPIO_INIT_STATE(LEDRX_Active_High));
	GPIO_InitStruct.Pin = LEDRX_Pin;
	GPIO_InitStruct.Mode = LEDRX_Mode;
	GPIO_InitStruct.Pull = GPIO_NOPULL;
	GPIO_InitStruct.Speed = GPIO_SPEED_FREQ_LOW;
	HAL_GPIO_Init(LEDRX_GPIO_Port, &GPIO_InitStruct);

	HAL_GPIO_WritePin(LEDTX_GPIO_Port, LEDTX_Pin, GPIO_INIT_STATE(LEDTX_Active_High));
	GPIO_InitStruct.Pin = LEDTX_Pin;
	GPIO_InitStruct.Mode = LEDTX_Mode;
	GPIO_InitStruct.Pull = GPIO_NOPULL;
	GPIO_InitStruct.Speed = GPIO_SPEED_FREQ_LOW;
	HAL_GPIO_Init(LEDTX_GPIO_Port, &GPIO_InitStruct);

	/* PA0 high, PA1 low: same levels as the vendor firmware */
	HAL_GPIO_WritePin(GPIOA, GPIO_PIN_0, GPIO_PIN_SET);
	HAL_GPIO_WritePin(GPIOA, GPIO_PIN_1, GPIO_PIN_RESET);
	GPIO_InitStruct.Pin = GPIO_PIN_0 | GPIO_PIN_1;
	GPIO_InitStruct.Mode = GPIO_MODE_OUTPUT_PP;
	GPIO_InitStruct.Pull = GPIO_NOPULL;
	GPIO_InitStruct.Speed = GPIO_SPEED_FREQ_LOW;
	HAL_GPIO_Init(GPIOA, &GPIO_InitStruct);

	/*
	 * Crystal-less USB: let the CRS trim HSI48 on the USB start of
	 * frame, as device_f0.c does for the F0 boards without HSE.
	 */
	__HAL_RCC_CRS_CLK_ENABLE();
	RCC_CRSInitTypeDef RCC_CRSInitStruct = {
		.Prescaler = RCC_CRS_SYNC_DIV1,
		.Source = RCC_CRS_SYNC_SOURCE_USB,
		.Polarity = RCC_CRS_SYNC_POLARITY_RISING,
		.ReloadValue = __HAL_RCC_CRS_RELOADVALUE_CALCULATE(48000000, 1000),
		.ErrorLimitValue = RCC_CRS_ERRORLIMIT_DEFAULT,
		.HSI48CalibrationValue = RCC_CRS_HSI48CALIBRATION_DEFAULT,
	};
	HAL_RCCEx_CRSConfig(&RCC_CRSInitStruct);

	/* FDCAN */

	RCC_PeriphCLKInitTypeDef PeriphClkInit = {
		.PeriphClockSelection = RCC_PERIPHCLK_FDCAN,
		.FdcanClockSelection = RCC_FDCANCLKSOURCE_PLL,
	};

	HAL_RCCEx_PeriphCLKConfig(&PeriphClkInit);
	__HAL_RCC_FDCAN_CLK_ENABLE();

	/* FDCAN2_RX, FDCAN2_TX */
	GPIO_InitStruct.Pin = GPIO_PIN_5 | GPIO_PIN_6;
	GPIO_InitStruct.Mode = GPIO_MODE_AF_PP;
	GPIO_InitStruct.Pull = GPIO_NOPULL;
	GPIO_InitStruct.Speed = GPIO_SPEED_FREQ_LOW;
	GPIO_InitStruct.Alternate = GPIO_AF3_FDCAN2;
	HAL_GPIO_Init(GPIOB, &GPIO_InitStruct);
}

const struct board_config config = {
	.channel[0] = {
		.interface = FDCAN2,
		.leds = {
			[LED_RX] = {
				.port = LEDRX_GPIO_Port,
				.pin = LEDRX_Pin,
				.active_high = LEDRX_Active_High,
			},
			[LED_TX] = {
				.port = LEDTX_GPIO_Port,
				.pin = LEDTX_Pin,
				.active_high = LEDTX_Active_High,
			},
		},
	},
};
