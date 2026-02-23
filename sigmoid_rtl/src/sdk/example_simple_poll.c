
/***************************** Include Files *********************************/
#include "xaxidma.h"
#include "xdebug.h"
#include "xiltimer.h"
#include "xparameters.h"

#include <sleep.h>
#include <stdio.h>

#include <xaxidma_hw.h>

/******************** Constant Definitions **********************************/

#ifndef SDT
    #define DMA_DEV_ID XPAR_AXIDMA_0_DEVICE_ID
#endif

#define MAX_PKT_LEN (64UL * 1024UL * 1024UL - 1)

u8 tx_buffer[MAX_PKT_LEN] __attribute__((aligned(8)));
u8 rx_buffer[MAX_PKT_LEN] __attribute__((aligned(8)));

#define TX_BUFFER_BASE ((void *)&tx_buffer[0])
#define RX_BUFFER_BASE ((void *)&rx_buffer[0])

/**************************** Type Definitions *******************************/

/***************** Macros (Inline Functions) Definitions *********************/

/************************** Function Prototypes ******************************/

#ifndef SDT
int SigmoidDma_SimplePollExample(u16 DeviceId);
#else
int SigmoidDma_SimplePollExample(UINTPTR BaseAddress);
#endif

/************************** Variable Function *****************************/

XAxiDma AxiDma;

/************************** Function Definitions **************************/
#include <math.h>
float sigmoidf(float n)
{
    return (1 / (1 + powf(2.71828182846, -n)));
}

int main()
{
    int Status;

    xil_printf("\r\n--- Entering main() --- \r\n");

#ifndef SDT
    Status = SigmoidDma_SimplePollExample(DMA_DEV_ID);
#else
    Status = SigmoidDma_SimplePollExample(XPAR_XAXIDMA_0_BASEADDR);
#endif

    if (Status != XST_SUCCESS) {
        xil_printf("Sigmoid DMA Failed\r\n");
        return XST_FAILURE;
    }

    xil_printf("Successfully ran Sigmoid DMA Example\r\n");

    xil_printf("--- Exiting main() --- \r\n");

    return XST_SUCCESS;
}

#ifndef SDT
int SigmoidDma_SimplePollExample(u16 DeviceId)
#else
int SigmoidDma_SimplePollExample(UINTPTR BaseAddress)
#endif
{
    XAxiDma_Config *CfgPtr;
    int             Status;
    unsigned        Index;
    u8             *TxBufferPtr;
    u8             *RxBufferPtr;

    /* Metrics variables */
    XTime  tStart, tEnd;
    double time_sec;
    double mbs;
    double ops;
    double total_bytes;

    TxBufferPtr = (u8 *)TX_BUFFER_BASE;
    RxBufferPtr = (u8 *)RX_BUFFER_BASE;

    /* Initialize the XAxiDma device.
     */
#ifndef SDT
    CfgPtr = XAxiDma_LookupConfig(DeviceId);
    if (!CfgPtr) {
        xil_printf("No config found for %d\r\n", DeviceId);
        return XST_FAILURE;
    }
#else
    CfgPtr = XAxiDma_LookupConfig(BaseAddress);
    if (!CfgPtr) {
        xil_printf("No config found for %d\r\n", BaseAddress);
        return XST_FAILURE;
    }
#endif

    Status = XAxiDma_CfgInitialize(&AxiDma, CfgPtr);
    if (Status != XST_SUCCESS) {
        xil_printf("Initialization failed %d\r\n", Status);
        return XST_FAILURE;
    }

    if (XAxiDma_HasSg(&AxiDma)) {
        xil_printf("Device configured as SG mode \r\n");
        return XST_FAILURE;
    }

    /* Disable interrupts, we use polling mode
     */
    XAxiDma_IntrDisable(&AxiDma, XAXIDMA_IRQ_ALL_MASK, XAXIDMA_DEVICE_TO_DMA);
    XAxiDma_IntrDisable(&AxiDma, XAXIDMA_IRQ_ALL_MASK, XAXIDMA_DMA_TO_DEVICE);

    // TODO: Use something meaningful
    for (Index = 0; Index < MAX_PKT_LEN; Index++) {
        TxBufferPtr[Index] = 0x00;
    }
    /* Flush the buffers before the DMA transfer, in case the Data Cache
     * is enabled
     */
    Xil_DCacheFlushRange((UINTPTR)TxBufferPtr, MAX_PKT_LEN);
    Xil_DCacheFlushRange((UINTPTR)RxBufferPtr, MAX_PKT_LEN);

    /* ------------------------------------------------ */
    /* START METRICS MEASUREMENT */
    /* ------------------------------------------------ */
    sleep(0);
    XTime_GetTime(&tStart);

    Status = XAxiDma_SimpleTransfer(&AxiDma, (UINTPTR)RxBufferPtr, MAX_PKT_LEN, XAXIDMA_DEVICE_TO_DMA);

    if (Status != XST_SUCCESS) {
        return XST_FAILURE;
    }

    Status = XAxiDma_SimpleTransfer(&AxiDma, (UINTPTR)TxBufferPtr, MAX_PKT_LEN, XAXIDMA_DMA_TO_DEVICE);

    if (Status != XST_SUCCESS) {
        return XST_FAILURE;
    }

    /*Wait till tranfer is done or 1usec * 10^6 iterations of timeout occurs*/
    while (XAxiDma_Busy(&AxiDma, XAXIDMA_DEVICE_TO_DMA) && XAxiDma_Busy(&AxiDma, XAXIDMA_DMA_TO_DEVICE)) {
    }

    /* ------------------------------------------------ */
    /* END METRICS MEASUREMENT */
    /* ------------------------------------------------ */
    XTime_GetTime(&tEnd);

    /* Calculate Metrics */
    /* Convert timer counts to seconds */
    time_sec = (double)(tEnd - tStart) / (double)(COUNTS_PER_SECOND);
    total_bytes = (double)MAX_PKT_LEN;
    mbs = 2 * (total_bytes / 1024.0 / 1024.0) / time_sec;
    ops = ((double)MAX_PKT_LEN / 1024.0 / 1024.0) / time_sec;

    printf("\r\nPerformance Results:\r\n");
    printf("  Transfer Size: %lu bytes\r\n", MAX_PKT_LEN);
    printf("  Timer Ticks:   %llu\r\n", tEnd - tStart);
    printf("  Total Time:    %.3f s\r\n", time_sec);
    printf("  Total Time:    %.1f us\r\n", time_sec * 1000000);
    printf("  Throughput:    %.3f MB/s\r\n", mbs);
    printf("  Operations:    %.1f MOps/s\r\n", ops);

    /* Test finishes successfully */
    return XST_SUCCESS;
}