#include "robot.h"

#include "robot_tasks.h"
#include "chassis_task.h"
#include "gimbal_task.h"
#include "launch_task.h"
#include "remote.h"
#include "gimbal_task.h"
#include "imu_task.h"
#include "referee_system.h"
#include "buzzer.h"
#include "supercap.h"
#include "rate_limiter.h"

Robot_State_t g_robot_state = {0};
extern Remote_t g_remote;
extern Supercap_t g_supercap;

rate_limiter_t rate_limiter_x;
rate_limiter_t rate_limiter_y;
rate_limiter_t rate_limiter_omega;

/**
 * @brief This function initializes the robot.
 * This means setting the state to STARTING_UP,
 * initializing the buzzer, and calling the
 * Robot_Task_Start() for the task scheduling
 */
void Robot_Init()
{
    g_robot_state.state = STARTING_UP;

    Buzzer_Init();
    Melody_t system_init_melody = {
        .notes = SYSTEM_INITIALIZING,
        .loudness = 0.5f,
        .note_num = SYSTEM_INITIALIZING_NOTE_NUM,
    };
    Buzzer_Play_Melody(system_init_melody); // TODO: Change to non-blocking

    // Initialize all tasks
    Robot_Tasks_Start();
}

/**
 * @brief This function handles the starting up state of the robot, initializing all hardware.
 */
void Handle_Starting_Up_State()
{
    // Initialize all hardware
    Chassis_Task_Init();
    Gimbal_Task_Init();
    Launch_Task_Init();
    Remote_Init(&huart3);
    CAN_Service_Init();
    Referee_System_Init(&huart1);
    Supercap_Init(&g_supercap);

    rate_limiter_init(&rate_limiter_x, 0.002f);
    rate_limiter_init(&rate_limiter_y, 0.002f);
    rate_limiter_init(&rate_limiter_omega, 0.002f);

    // Set robot state to disabled
    g_robot_state.state = DISABLED;
}

/**
 * @brief This function handles the enabled state of the robot.
 * This means processing remote input, and subsystem control.
 */
void Handle_Enabled_State()
{
    if (g_remote.online_flag == REMOTE_OFFLINE || g_remote.controller.right_switch == DOWN)
    {
        g_robot_state.state = DISABLED;
    }
    else
    {
        // Process movement and components in enabled state
        Referee_Set_Robot_State();
        Process_Remote_Input();
        Process_Chassis_Control();
        Process_Gimbal_Control();
        Process_Launch_Control();
    }
}

/**
 * @brief This function handles the disabled state of the robot.
 * This means disabling all motors and components
 */
void Handle_Disabled_State()
{
    DJI_Motor_Disable_All();
    //  Disable all major components
    g_robot_state.launch.IS_FLYWHEEL_ENABLED = 0;
    g_robot_state.chassis.x_speed = 0;
    g_robot_state.chassis.y_speed = 0;

    if (g_remote.online_flag == REMOTE_ONLINE && g_remote.controller.right_switch != DOWN)
    {
        g_robot_state.state = ENABLED;
        DJI_Motor_Enable_All();
    }
}

void Process_Remote_Input()
{    
    // Process remote input
    // g_robot_state.input.vx = g_remote.controller.left_stick.x;
    // g_robot_state.input.vy = g_remote.controller.left_stick.y;
    // g_robot_state.input.vomega = g_remote.controller.right_stick.x;

    g_robot_state.input.vx = rate_limiter(&rate_limiter_x, g_remote.controller.left_stick.x);
    g_robot_state.input.vy = rate_limiter(&rate_limiter_y, g_remote.controller.left_stick.y);
    g_robot_state.input.vomega = rate_limiter(&rate_limiter_omega, g_remote.controller.right_stick.x);
}

void Process_Chassis_Control()
{
    Chassis_Ctrl_Loop();
}

void Process_Gimbal_Control()
{
}

void Process_Launch_Control()
{
}

/**
 *  This function is called periodically by the Robot Task.
 *  It serves as the top level state machine for the robot based on the current state.
 *  Appropriate functions are called.
 */
void Robot_Command_Loop()
{
    switch (g_robot_state.state)
    {
    case STARTING_UP:
        Handle_Starting_Up_State();
        break;
    case DISABLED:
        Handle_Disabled_State();
        break;
    case ENABLED:
        Handle_Enabled_State();
        break;
    default:
        Error_Handler();
        break;
    }
}