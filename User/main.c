/*
*********************************************************************************************************
*
*	模块名称 : 主程序模块。
*	文件名称 : main.c
*	版    本 : V1.0
*	说    明 : 本实验主要学习FreeRTOS的串口调试方法（打印任务执行情况）
*              实验目的：
*                1. 学习FreeRTOS的串口调试方法（打印任务执行情况）。
*                2. 为了获取FreeRTOS任务的执行情况，需要执行如下三个操作
*					a. 在FreeRTOSConfig.h文件中使能如下宏
*                	    #define configUSE_TRACE_FACILITY	                1
*                		#define configGENERATE_RUN_TIME_STATS               1
*                		#define configUSE_STATS_FORMATTING_FUNCTIONS        1
*                		#define portCONFIGURE_TIMER_FOR_RUN_TIME_STATS()    (ulHighFrequencyTimerTicks = 0ul)
*                		#define portGET_RUN_TIME_COUNTER_VALUE()            ulHighFrequencyTimerTicks
*					b. 创建一个定时器，这个定时器的时间基准精度要高于系统时钟节拍，这样得到的任务信息才准确。
*                   c. 调用函数vTaskList和vTaskGetRunTimeStats即可获取任务任务的执行情况。
*              实验内容：
*                1. 按下按键K1可以通过串口打印任务执行情况（波特率115200，数据位8，奇偶校验位无，停止位1）
*                   =================================================
*                   任务名      任务状态 优先级   剩余栈 任务序号
*                   vTaskUserIF     R       1       318     1
*                	IDLE            R       0       118     5
*                	vTaskLED        B       2       490     2
*                	vTaskMsgPro     B       3       490     3
*               	vTaskStart      B       4       490     4
*
*                	任务名       运行计数         使用率
*                	vTaskUserIF     467             <1%
*                	IDLE            126495          99%
*                	vTaskMsgPro     1               <1%
*                	vTaskStart      639             <1%
*                	vTaskLED        0               <1%
*                  串口软件建议使用SecureCRT（V5光盘里面有此软件）查看打印信息。
*                  各个任务实现的功能如下：
*                   vTaskTaskUserIF 任务: 接口消息处理	
*                   vTaskLED        任务: LED闪烁
*                   vTaskMsgPro     任务: 消息处理，这里是用作LED闪烁
*                   vTaskStart      任务: 启动任务，也就是最高优先级任务，这里实现按键扫描
*                2. 任务运行状态的定义如下，跟上面串口打印字母B, R, D, S对应：
*                    #define tskBLOCKED_CHAR		( 'B' )  阻塞
*                    #define tskREADY_CHAR		    ( 'R' )  就绪
*                    #define tskDELETED_CHAR		( 'D' )  删除
*                    #define tskSUSPENDED_CHAR	    ( 'S' )  挂起
*              注意事项：
*                 1. 本实验推荐使用串口软件SecureCRT，要不串口打印效果不整齐。此软件在
*                    V5开发板光盘里面有。
*                 2. 务必将编辑器的缩进参数和TAB设置为4来阅读本文件，要不代码显示不整齐。
*
*	修改记录 :
*		版本号    日期         作者            说明
*       V1.0    2016-03-15   Eric2013    1. ST固件库到V1.5.0版本
*                                        2. BSP驱动包V1.2
*                                        3. FreeRTOS版本V8.2.3
*
*	Copyright (C), 2016-2020, 安富莱电子 www.armfly.com
*
*********************************************************************************************************
*/
#include "includes.h"
#include "camera.h"
#include "following.h"

#if defined(FREE_RTOS_STUDY) && FREE_RTOS_STUDY
#include "a_print_task_info.h"
#include "b_timer_test.h"
#endif
/*
**********************************************************************************************************
											函数声明
**********************************************************************************************************
*/
static void vTaskTaskUserIF(void *pvParameters);
static void vTaskFPScalcu(void *pvParameters);
static void vTaskKeyCapture(void *pvParameters);
static void AppTaskCreate (void);
static void AppObjCreate (void);

/*
**********************************************************************************************************
											变量声明
**********************************************************************************************************
*/
static TaskHandle_t xHandleTaskUserIF = NULL;
static TaskHandle_t xHandleTaskLED = NULL;
static TaskHandle_t xHandleTaskMsgPro = NULL;
static TaskHandle_t xHandleTaskStart = NULL;
static TaskHandle_t xHandleTaskFollowing = NULL;

QueueHandle_t xQueueLineProcess = NULL;
QueueHandle_t xQueueCameraReady = NULL;

/*
*********************************************************************************************************
*	函 数 名: main
*	功能说明: 标准c程序入口。
*	形    参：无
*	返 回 值: 无
*********************************************************************************************************
*/
int main(void)
{
	/* 
	  在启动调度前，为了防止初始化STM32外设时有中断服务程序执行，这里禁止全局中断(除了NMI和HardFault)。
	  这样做的好处是：
	  1. 防止执行的中断服务程序中有FreeRTOS的API函数。
	  2. 保证系统正常启动，不受别的中断影响。
	  3. 关于是否关闭全局中断，大家根据自己的实际情况设置即可。
	  在移植文件port.c中的函数prvStartFirstTask中会重新开启全局中断。通过指令cpsie i开启，__set_PRIMASK(1)
	  和cpsie i是等效的。
     */
	__set_PRIMASK(1);  
	
	/* 硬件初始化 */
	bsp_Init(); 
	
	/* 1. 初始化一个定时器中断，精度高于滴答定时器中断，这样才可以获得准确的系统信息 仅供调试目的，实际项
		  目中不要使用，因为这个功能比较影响系统实时性。
	   2. 为了正确获取FreeRTOS的调试信息，可以考虑将上面的关闭中断指令__set_PRIMASK(1); 注释掉。 
	*/
	vSetupSysInfoTest();

#if defined(FREE_RTOS_STUDY) && FREE_RTOS_STUDY	
	create_timer_task_func();
#else
	/* 创建任务 */
	AppTaskCreate();

	AppObjCreate();
#endif
	
    /* 启动调度，开始执行任务 */
    vTaskStartScheduler();

	/* 
	  如果系统正常启动是不会运行到这里的，运行到这里极有可能是用于定时器任务或者空闲任务的
	  heap空间不足造成创建失败，此要加大FreeRTOSConfig.h文件中定义的heap大小：
	  #define configTOTAL_HEAP_SIZE	      ( ( size_t ) ( 17 * 1024 ) )
	*/
	while(1);
}

/*
*********************************************************************************************************
*	函 数 名: vTaskTaskUserIF
*	功能说明: 接口消息处理。
*	形    参: pvParameters 是在创建该任务时传递的形参
*	返 回 值: 无
*   优 先 级: 1  (数值越小优先级越低，这个跟uCOS相反)
*********************************************************************************************************
*/
static void vTaskTaskUserIF(void *pvParameters)
{
	uint8_t ucKeyCode;
	uint8_t pcWriteBuffer[500];

    while(1)
    {
		ucKeyCode = bsp_GetKey();
		
		if (ucKeyCode != KEY_NONE)
		{
			switch (ucKeyCode)
			{
				/* K1键按下 打印任务执行情况 */
				case KEY_DOWN_K1:			 
					printf("=================================================\r\n");
					printf("任务名      任务状态 优先级   剩余栈 任务序号\r\n");
					vTaskList((char *)&pcWriteBuffer);
					printf("%s\r\n", pcWriteBuffer);
				
					printf("\r\n任务名       运行计数         使用率\r\n");
					vTaskGetRunTimeStats((char *)&pcWriteBuffer);
					printf("%s\r\n", pcWriteBuffer);
					break;
				
				/* 其他的键值不处理 */
				default:                     
					break;
			}
		}
		
		vTaskDelay(20);
	}
}

/*
*********************************************************************************************************
*	函 数 名: vTaskFPScalcu
*	功能说明: LED闪烁	
*	形    参: pvParameters 是在创建该任务时传递的形参
*	返 回 值: 无
*   优 先 级: 2  
*********************************************************************************************************
*/
extern uint16_t fps_recording;
static void vTaskFPScalcu(void *pvParameters)
{
	uint8_t num = 0;
	pwm_func_test();
	bsp_LedOn(2);
	bsp_LedOff(3);
    while(1)
    {
		bsp_LedToggle(2);
		bsp_LedToggle(3);
        vTaskDelay(1000);
		printf("------------------------------- fps:%d\r\n",fps_recording);
		fps_recording = 0;
		num++;
    }
}

/*
*********************************************************************************************************
*	函 数 名: vTaskKeyCapture
*	功能说明: 信息处理，这里是用作LED闪烁	
*	形    参: pvParameters 是在创建该任务时传递的形参
*	返 回 值: 无
*   优 先 级: 3  
*********************************************************************************************************
*/
static void vTaskKeyCapture(void *pvParameters)
{
	uint8_t ucKeyCode;
	printf("key task init\r\n");
    while(1)
    {
		ucKeyCode = bsp_GetKey();
		if (ucKeyCode > 0)
		{
			switch (ucKeyCode)
			{
				case KEY_DOWN_K1:
					printf("get key1\r\n");
					break;

				case KEY_DOWN_K2:
					printf("get key2\r\n");
					break;

				default:
					break;
			}
			update_key_for_camera(ucKeyCode);
		}
        vTaskDelay(5);
    }
}

/*
*********************************************************************************************************
*	函 数 名: AppTaskCreate
*	功能说明: 创建应用任务
*	形    参：无
*	返 回 值: 无
*********************************************************************************************************
*/
static void AppTaskCreate (void)
{
    // xTaskCreate( vTaskTaskUserIF,   	/* 任务函数  */
    //              "vTaskUserIF",     	/* 任务名    */
    //              512,               	/* 任务栈大小，单位word，也就是4字节 */
    //              NULL,              	/* 任务参数  */
    //              1,                 	/* 任务优先级*/
    //              &xHandleTaskUserIF );  /* 任务句柄  */
	
	
	xTaskCreate( vTaskFPScalcu,    		/* 任务函数  */
                 "vTaskFPScalcu",  		/* 任务名    */
                 512,         		/* 任务栈大小，单位word，也就是4字节 */
                 NULL,        		/* 任务参数  */
                 4,           		/* 任务优先级*/
                 &xHandleTaskLED ); /* 任务句柄  */
	
	xTaskCreate( vTaskKeyCapture,     		/* 任务函数  */
                 "vTaskKeyCapture",   		/* 任务名    */
                 512,             		/* 任务栈大小，单位word，也就是4字节 */
                 NULL,           		/* 任务参数  */
                 4,               		/* 任务优先级*/
                 &xHandleTaskMsgPro );  /* 任务句柄  */
	
	
	xTaskCreate( vTaskCameraCapture,     		/* 任务函数  */
                 "vTaskCameraCapture",   		/* 任务名    */
                 1024,            		/* 任务栈大小，单位word，也就是4字节 */
                 NULL,           		/* 任务参数  */
                 3,              		/* 任务优先级*/
                 &xHandleTaskStart );   /* 任务句柄  */

	xTaskCreate( vTaskLineProcess,     		/* 任务函数  */
                 "vTaskLineProcess",   		/* 任务名    */
                 512,            		/* 任务栈大小，单位word，也就是4字节 */
                 NULL,           		/* 任务参数  */
                 3,              		/* 任务优先级*/
                 &xHandleTaskFollowing );   /* 任务句柄  */
}

static void AppObjCreate (void)
{
	/* 创建10个uint8_t型消息队列 */
	xQueueLineProcess = xQueueCreate(5, sizeof(uint8_t));
    if( xQueueLineProcess == 0 )
    {
		printf("[%s] create queu fail\r\n",__func__);
        /* 没有创建成功，用户可以在这里加入创建失败的处理机制 */
    }
	
	/* 创建10个存储指针变量的消息队列，由于CM3/CM4内核是32位机，一个指针变量占用4个字节 */
	xQueueCameraReady = xQueueCreate(5, sizeof(uint8_t));
    if( xQueueCameraReady == 0 )
    {
		printf("[%s] create queu fail\r\n",__func__);
        /* 没有创建成功，用户可以在这里加入创建失败的处理机制 */
    }
}

/***************************** 安富莱电子 www.armfly.com (END OF FILE) *********************************/
