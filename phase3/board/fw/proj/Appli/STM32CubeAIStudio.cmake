# Edited for the v6 Zn-ion streaming app (phase3/board/fw): three ST C-models (enc fp32, fade INT8, head fp32)
# + zn app, no validation stack (pattern of v5_auto/phase3/board/fw/proj_patch).
set(ZN ${CMAKE_CURRENT_LIST_DIR}/../..)
target_link_directories(${CMAKE_PROJECT_NAME} PUBLIC ${CMAKE_CURRENT_LIST_DIR}/../Middlewares/ST/AI/Lib)
target_link_libraries(${CMAKE_PROJECT_NAME} :NetworkRuntime1210_CM7_GCC.a m)
target_sources(${CMAKE_PROJECT_NAME} PRIVATE
    ${CMAKE_CURRENT_LIST_DIR}/../Middlewares/ST/AI/Misc/Src/syscalls.c
    ${CMAKE_CURRENT_LIST_DIR}/../Middlewares/ST/AI/Misc/Src/aiTestUtility.c
    ${CMAKE_CURRENT_LIST_DIR}/AI/App/user_init.c
    ${ZN}/net_enc/enc.c ${ZN}/net_enc/enc_data.c
    ${ZN}/net_fade/fade.c ${ZN}/net_fade/fade_data.c
    ${ZN}/net_head/head.c ${ZN}/net_head/head_data.c
    ${ZN}/zn_app.c ${ZN}/zn_data.c ${ZN}/app_zn.c)
target_include_directories(${CMAKE_PROJECT_NAME} PUBLIC
    ${CMAKE_CURRENT_LIST_DIR}/../Middlewares/ST/AI/Misc/Inc ${CMAKE_CURRENT_LIST_DIR}/../Middlewares/ST/AI/Inc
    ${CMAKE_CURRENT_LIST_DIR}/AI/App ${ZN} ${ZN}/net_enc ${ZN}/net_fade ${ZN}/net_head)
target_sources(STM32_Drivers PRIVATE
    ${CMAKE_CURRENT_LIST_DIR}/../Drivers/STM32H7RSxx_HAL_Driver/Src/stm32h7rsxx_hal_xspi.c
    ${CMAKE_CURRENT_LIST_DIR}/../Drivers/BSP/STM32H7RSxx_Nucleo/stm32h7rsxx_nucleo.c
    ${CMAKE_CURRENT_LIST_DIR}/../Drivers/BSP/Components/mx25uw25645g/mx25uw25645g.c)
target_include_directories(STM32_Drivers PUBLIC
    ${CMAKE_CURRENT_LIST_DIR}/../Drivers/STM32H7RSxx_HAL_Driver/Inc
    ${CMAKE_CURRENT_LIST_DIR}/../Drivers/BSP/Components/mx25uw25645g
    ${CMAKE_CURRENT_LIST_DIR}/../Drivers/BSP/STM32H7RSxx_Nucleo)
target_compile_definitions(${CMAKE_PROJECT_NAME} PRIVATE $<$<BOOL:${ZN_WEIGHTS_IN_SRAM}>:ZN_WEIGHTS_IN_SRAM>)
