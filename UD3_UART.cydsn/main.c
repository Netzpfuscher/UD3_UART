/*******************************************************************************
* File Name: main.c
*
* Version: 2.0
*
* Description:
*   The component is enumerated as a Virtual Com port. Receives data from the 
*   hyper terminal, then sends back the received data.
*   For PSoC3/PSoC5LP, the LCD shows the line settings.
*
* Related Document:
*  Universal Serial Bus Specification Revision 2.0
*  Universal Serial Bus Class Definitions for Communications Devices
*  Revision 1.2
*
********************************************************************************
* Copyright 2015, Cypress Semiconductor Corporation. All rights reserved.
* This software is owned by Cypress Semiconductor Corporation and is protected
* by and subject to worldwide patent and copyright laws and treaties.
* Therefore, you may use this software only as provided in the license agreement
* accompanying the software package from which you obtained this software.
* CYPRESS AND ITS SUPPLIERS MAKE NO WARRANTY OF ANY KIND, EXPRESS OR IMPLIED,
* WITH REGARD TO THIS SOFTWARE, INCLUDING, BUT NOT LIMITED TO, NONINFRINGEMENT,
* IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE.
*******************************************************************************/

#include <project.h>
#include "stdio.h"
#include "math.h"

#if defined (__GNUC__)
    /* Add an explicit reference to the floating point printf library */
    /* to allow usage of the floating point conversion specifiers. */
    /* This is not linked in by default with the newlib-nano library. */
    asm (".global _printf_float");
#endif

#define USBFS_DEVICE    (0u)

/* The buffer size is equal to the maximum packet size of the IN and OUT bulk
* endpoints.
*/
#define USBUART_BUFFER_SIZE (64u)
#define LINE_STR_LENGTH     (20u)

char8* parity[] = {"None", "Odd", "Even", "Mark", "Space"};
char8* stop[]   = {"1", "1.5", "2"};

#define tsk_usb_BUFFER_LEN 64


void uart_baudrate(uint32_t baudrate){
    float divider = (float)(BCLK__BUS_CLK__HZ/8)/(float)baudrate;
    uint16_t divider_selected=1;
   
    uint32_t down_rate = (BCLK__BUS_CLK__HZ/8)/floor(divider);
    uint32_t up_rate = (BCLK__BUS_CLK__HZ/8)/ceil(divider);
   
    float down_rate_error = (down_rate/(float)baudrate)-1;
    float up_rate_error = (up_rate/(float)baudrate)-1;
    
    UART_2_Stop();
    if(fabs(down_rate_error) < fabs(up_rate_error)){
        //selected round down divider
        divider_selected = floor(divider);
    }else{
        //selected round up divider
        divider_selected = ceil(divider);
    }
    uint32_t uart_frequency = BCLK__BUS_CLK__HZ / divider_selected;
    uint32_t delay_tmr = ((BCLK__BUS_CLK__HZ / uart_frequency)*3)/4;

    Mantmr_WritePeriod(delay_tmr-3);
    Mantmr_Stop();
    Mantmr_Start();
    UART_CLK_SetDividerValue(divider_selected);
    
    UART_2_Start();    
  
}

/*******************************************************************************
* Function Name: main
********************************************************************************
*
* Summary:
*  The main function performs the following actions:
*   1. Waits until VBUS becomes valid and starts the USBFS component which is
*      enumerated as virtual Com port.
*   2. Waits until the device is enumerated by the host.
*   3. Waits for data coming from the hyper terminal and sends it back.
*   4. PSoC3/PSoC5LP: the LCD shows the line settings.
*
* Parameters:
*  None.
*
* Return:
*  None.
*
*******************************************************************************/


int main()
{
    uint16 count;
    uint8 buffer[USBUART_BUFFER_SIZE];
    
    UART_2_Start();
    Mantmr_Start();
  
    
#if (CY_PSOC3 || CY_PSOC5LP)
    uint8 state;
    char8 lineStr[LINE_STR_LENGTH];

#endif /* (CY_PSOC3 || CY_PSOC5LP) */
    
    CyGlobalIntEnable;

    /* Start USBFS operation with 5-V operation. */
    USBUART_Start(USBFS_DEVICE, USBUART_5V_OPERATION);
    
    for(;;)
    {
        if(bldr_Read()==0){
            Bootloadable_1_Load();   
        }
        
        /* Host can send double SET_INTERFACE request. */
        if (0u != USBUART_IsConfigurationChanged())
        {
            /* Initialize IN endpoints when device is configured. */
            if (0u != USBUART_GetConfiguration())
            {
                /* Enumeration is done, enable OUT endpoint to receive data 
                 * from host. */
                USBUART_CDC_Init();
            }
        }

        /* Service USB CDC when device is configured. */
        if (0u != USBUART_GetConfiguration())
        {
            /* Check for input data from host. */
            if (0u != USBUART_DataIsReady())
            {
                /* Read received data and re-enable OUT endpoint. */
                count = USBUART_GetAll(buffer);

                if (0u != count)
                {
                    /* Wait until component is ready to send data to host. */
                    while (0u == USBUART_CDCIsReady())
                    {
                    }
                    
                    UART_2_PutArray(buffer,count);
                    
                    /* Send data back to host. */
                    //USBUART_PutData(buffer, count);

                }
            }

            count = UART_2_GetRxBufferSize();
			count = (count > tsk_usb_BUFFER_LEN) ? tsk_usb_BUFFER_LEN : count;

			/* When component is ready to send more data to the PC */
			if ((USBUART_CDCIsReady() != 0u) && (count > 0)) {
				/*
    			 * Read the data from the transmit queue and buffer it
    			 * locally so that the data can be utilized.
    			 */

				for (uint8_t idx = 0; idx < count; ++idx) {
					buffer[idx] = UART_2_GetByte();
					
					//xQueueReceive( qUSB_tx,&buffer[idx],0);
				}

				/* Send data back to host */
				USBUART_PutData(buffer, count);

				/* If the last sent packet is exactly maximum packet size, 
            	 *  it shall be followed by a zero-length packet to assure the
             	 *  end of segment is properly identified by the terminal.
             	 */
				if (count == tsk_usb_BUFFER_LEN) {
					/* Wait till component is ready to send more data to the PC */
					while (USBUART_CDCIsReady() == 0u) {
						//vTaskDelay( 10 / portTICK_RATE_MS );
					}
					USBUART_PutData(NULL, 0u); /* Send zero-length packet to PC */
				}
			}
        

        #if (CY_PSOC3 || CY_PSOC5LP)
            /* Check for Line settings change. */
            state = USBUART_IsLineChanged();
            if (0u != state)
            {
                /* Output on LCD Line Coding settings. */
                if (0u != (state & USBUART_LINE_CODING_CHANGED))
                {      
                  uart_baudrate(USBUART_GetDTERate());
                                     
                    /* Get string to output. */
                   // sprintf(lineStr,"BR:%4ld %d%c%s", USBUART_GetDTERate(),\
                   //                (uint16) USBUART_GetDataBits(),\
                   //                parity[(uint16) USBUART_GetParityType()][0],\
                   //                stop[(uint16) USBUART_GetCharFormat()]);


                }

                /* Output on LCD Line Control settings. */
                if (0u != (state & USBUART_LINE_CONTROL_CHANGED))
                {                   
                    /* Get string to output. */
                    //state = USBUART_GetLineControl();
                    //sprintf(lineStr,"DTR:%s,RTS:%s",  (0u != (state & USBUART_LINE_CONTROL_DTR)) ? "ON" : "OFF",
                    //                                  (0u != (state & USBUART_LINE_CONTROL_RTS)) ? "ON" : "OFF");

                    /* Clear LCD line. */

                }
            }
        #endif /* (CY_PSOC3 || CY_PSOC5LP) */
        }
    }
}


/* [] END OF FILE */
