/* Replaces the AI Studio validation app: main.c calls these two hooks (as v5 app_naion.c). */
#include <stdio.h>
void zn_app_run(void);
void STM32CubeAI_Studio_AI_Init(void) { }
void STM32CubeAI_Studio_AI_Process(void) { static int done = 0; if (!done) { zn_app_run(); printf("D,done\r\n"); done = 1; } }
