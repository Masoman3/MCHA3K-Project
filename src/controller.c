#include <stddef.h>
#include <stdio.h>
#include <stdlib.h>
#include <math.h>
#include <avr/io.h>
#include <avr/pgmspace.h>
#include <kf.h>
#include <stdbool.h>
#include <util/atomic.h>

#include "motor.h"
#include "controller.h"
#include "encoders.h"
#include "pend_ctrl.h"
#include "task.h"

#include "ctrl_param.def"

static uint8_t CSVAL = 0x00;
static uint8_t OCR0_val = 0xFF;

static uint32_t ctrl_count = 0;

static CTRL_MODE ctrl_cur_mode = CTRL_HIL;

void ctrl_state_correct(void);

static task_s _state_update_task = {
		.interval = 10,
		.callback = ctrl_state_correct,
		.id = 255,
		.enabled = false
};

static states ctrl_states =
{
	.Theta 		= 0.0f,
	.Phi_ML		= 0.0f,
	.Phi_MR		= 0.0f,
	.dTheta 	= 0.0f,
	.dPhi_ML	= 0.0f,
	.dPhi_MR	= 0.0f,
	.Current_ML	= 0.0f,
	.Current_MR	= 0.0f,
	.Bias		= 0.0f,
	.Vref 		= 0.0f
};

void ctrl_set_freq(float interval);

void ctrl_init(void)
{
	/*
	 * Timer 0 Initialisation
	 * 	COM0 0:1 	=
	 *	CS 			= Defined by <CSVAL>
	 */
	TCCR0 |= _BV(WGM01);
	TIMSK |= _BV(OCIE0);
	ctrl_set_freq(T_main);


	// Parameter Initialisation
	ctrl_count = 0;
	ctrl_cur_mode = CTRL_OFF;

	motors_init();
	encoder_init();
	kf_init();

	ctrl_states.Theta = imu_get_atanTheta();///THETAGAIN;

	//Set up IMU task regularly update data
	//_state_update_task.interval = tasks_time_interval_to_task_interval(T_KF);

	//tasks_add(&_state_update_task);

	ctrl_set_mode(CTRL_NORMAL);
	//motors_set_pwm(MOTOR_LEFT, -MAX_PWM/2);
	//motors_set_pwm(MOTOR_RIGHT, -MAX_PWM/2);
}
static uint16_t last_kalman_run = 0;
void ctrl_run(void)
{
	uint32_t ctrl_count_tmp;
	ATOMIC_BLOCK(ATOMIC_FORCEON)
	{
		if (ctrl_count >= Main_loop_reset)
		{
			ctrl_count = 0;
		}
		ctrl_count_tmp = ctrl_count;
	}

	switch (ctrl_cur_mode)
	{
	case CTRL_NORMAL:
	{
		if (ctrl_count_tmp - last_kalman_run >= motor_interval)
		{
			if(bit_is_set(PORTA, PA6))
			{
				PORTA &= ~(_BV(PA6));
			}
			else
			{
				PORTA |= _BV(PA6);
			}

			kf_correct(&ctrl_states);
			kf_timestep(&ctrl_states);
			printf_P(PSTR("T:%g, dT:%g, B:%g, P: %g, dP: %g\n"), ctrl_states.Theta*RAD2DEG, ctrl_states.dTheta*RAD2DEG, ctrl_states.Bias*RAD2DEG, ctrl_states.dPhi_ML*RAD2DEG, ctrl_states.dPhi_MR*RAD2DEG);
			if (fabs(ctrl_states.Theta) <= M_PI/4)
			{
				PORTA |= _BV(PA5);
				float Torque = pend_ctrl_alloc(&ctrl_states);
				PORTA &= ~_BV(PA5);

				PORTA |= _BV(PA4);
				motor_set_torque(MOTOR_LEFT,Torque/2.0);
				motor_set_torque(MOTOR_RIGHT,Torque/2.0);
				motors_set_voltage(MOTOR_LEFT, motor_ctrl_alloc(MOTOR_LEFT, &ctrl_states));
				motors_set_voltage(MOTOR_RIGHT, motor_ctrl_alloc(MOTOR_RIGHT, &ctrl_states));
				PORTA &= ~_BV(PA4);
			}
			else
			{
				motors_set_voltage(MOTOR_LEFT, 0);
				motors_set_voltage(MOTOR_RIGHT, 0);
			}
			last_kalman_run = ctrl_count_tmp;
		}

	} break;
	case CTRL_HIL:
		break;
	case CTRL_IMUONLY:
		if (ctrl_count_tmp - last_kalman_run >= motor_interval)
		{
			kf_correct(&ctrl_states);
			kf_timestep(&ctrl_states);
			printf_P(PSTR("T:%g, dT:%g, B:%g, P: %g, dP: %g\n"), ctrl_states.Theta*RAD2DEG, ctrl_states.dTheta*RAD2DEG, ctrl_states.Bias*RAD2DEG, ctrl_states.dPhi_ML*RAD2DEG, ctrl_states.dPhi_MR*RAD2DEG);
			last_kalman_run = ctrl_count_tmp;
		}

		break;
	case CTRL_OFF:
	default:
		break;
	}
}

void ctrl_set_mode(CTRL_MODE state)
{
	ctrl_cur_mode = state;
	switch (state)
	{
	case CTRL_OFF:
		TCCR0 &= ~CSVAL;
		_state_update_task.enabled = false;
		motors_set_pwm(MOTOR_LEFT, 0);
		motors_set_pwm(MOTOR_RIGHT, 0);
		break;
	case CTRL_IMUONLY:
		motors_set_pwm(MOTOR_LEFT, 0);
		motors_set_pwm(MOTOR_RIGHT, 0);
		//No break deliberate
	case CTRL_NORMAL:
		TCCR0 |= CSVAL;
		kf_init();
		ctrl_states.Theta = imu_get_atanTheta();
		//_state_update_task.enabled = true;
		ctrl_count = 0;
		break;
	case CTRL_HIL:
		TCCR0 &= ~CSVAL;
		_state_update_task.enabled = false;
		motors_set_pwm(MOTOR_LEFT, 0);
		motors_set_pwm(MOTOR_RIGHT, 0);

		ctrl_states.Theta 		= 0.0f;
		ctrl_states.Phi_ML		= 0.0f;
		ctrl_states.Phi_MR		= 0.0f;
		ctrl_states.dTheta 		= 0.0f;
		ctrl_states.dPhi_ML		= 0.0f;
		ctrl_states.dPhi_MR		= 0.0f;
		ctrl_states.Current_ML	= 0.0f;
		ctrl_states.Current_MR	= 0.0f;
		ctrl_states.Bias		= 0.0f;
		ctrl_states.Vref 		= 0.0f;
		break;
	default:
		break;
	}
}

void ctrl_set_freq(float interval)
{
	const uint16_t prescaler_values[] = {1, 8, 64, 256, 1024};
	const uint8_t clock_select_values[] = {(_BV(CS00)), (_BV(CS01)), (_BV(CS01) | _BV(CS00)), (_BV(CS02)), (_BV(CS02) | _BV(CS00))};

	for (uint8_t i = 0; i < 5; i++)
	{
		uint16_t tmp_val;
		tmp_val = (uint16_t)round(F_CPU*interval/(prescaler_values[i]))-1;
		if (tmp_val < 255)
		{
			OCR0 = OCR0_val = tmp_val & 0x00FF;
			CSVAL = clock_select_values[i];
			return;
		}
	}
}

void ctrl_state_correct(void)
{
	kf_correct(&ctrl_states);
}

void ctrl_isr(void)
{
	ctrl_count++;
}

states* ctrl_get_states(void)
{
	return &ctrl_states;
}

void ctrl_set_states(states val)
{
	ctrl_states = val;
}

CMD_STATUS ctrl_cmd(int argc, const char *argv[])
{
	if (!strcmp_P(argv[0], PSTR("mode")) && argc == 2)
	{
		if (!strcmp_P(argv[1], PSTR("OFF")))
		{
			ctrl_set_mode(CTRL_OFF);
			return CMD_OK;
		}
		else if (!strcmp_P(argv[1], PSTR("NORMAL")))
		{
			ctrl_set_mode(CTRL_NORMAL);
			return CMD_OK;
		}
		else if (!strcmp_P(argv[1], PSTR("HIL")))
		{
			ctrl_set_mode(CTRL_HIL);
			return CMD_OK;
		}
		else if (!strcmp_P(argv[1], PSTR("IMUONLY")))
		{
			ctrl_set_mode(CTRL_IMUONLY);
			return CMD_OK;
		}
	}
#ifdef FIXEDPOINT_STATES
	else if (!strcmp_P(argv[0], PSTR("run")) && argc == 2)
	{
		if (!strcmp_P(argv[1], PSTR("ML")))
		{
			int32_t tmp = motor_ctrl_alloc(MOTOR_LEFT, &ctrl_states);
			printf_P(PSTR("%g\n"), (float)tmp*MAX_VOLTAGE/MAX_PWM);
			return CMD_OK;
		}
		else if (!strcmp_P(argv[1], PSTR("MR")))
		{
			int32_t tmp = motor_ctrl_alloc(MOTOR_RIGHT, &ctrl_states);
			printf_P(PSTR("%g\n"), (float)tmp*MAX_VOLTAGE/MAX_PWM);
			return CMD_OK;
		}
		else if (!strcmp_P(argv[1], PSTR("PEND")))
		{
			kf_timestep(&ctrl_states);
			float tmp = pend_ctrl_alloc(&ctrl_states);
			printf_P(PSTR("%g\n"), (float)tmp);
			return CMD_OK;
		}
	}
	else if (!strcmp_P(argv[0], PSTR("correct")) && argc == 3)
	{
		if (!strcmp_P(argv[1], PSTR("theta")))
		{
			float tmp = atof(argv[2]);
			kf_acccorrection(tmp, &ctrl_states);
			return CMD_OK;
		}
		else if (!strcmp_P(argv[1], PSTR("dtheta")))
		{
			float tmp = atof(argv[2]);
			kf_gyrocorrection(tmp, &ctrl_states);
			return CMD_OK;
		}
		else if (!strcmp_P(argv[1], PSTR("phi")))
		{
			float tmp = atof(argv[2]);
			kf_enccorrection(MOTOR_LEFT, tmp, &ctrl_states);
			kf_enccorrection(MOTOR_RIGHT, tmp, &ctrl_states);
			return CMD_OK;
		}
	}
	else if (!strcmp_P(argv[0], PSTR("set")) && argc == 3)
	{
		if (!strcmp_P(argv[1], PSTR("vref")))
		{
			float tmp = atof(argv[2]);
			ctrl_states.Vref = (int16_t)(tmp * K_FP_GAIN);
			return CMD_OK;
		}
		else if (!strcmp_P(argv[1], PSTR("theta")))
		{
			float tmp = atof(argv[2]);
			ctrl_states.Theta = (int32_t)(tmp *K_FP_GAIN);
			return CMD_OK;
		}
		else if (!strcmp_P(argv[1], PSTR("dtheta")))
		{
			float tmp = atof(argv[2]);
			ctrl_states.dTheta = (int32_t)(tmp *K_FP_GAIN);
			return CMD_OK;
		}
		else if (!strcmp_P(argv[1], PSTR("phi")))
		{
			float tmp = atof(argv[2]);
			ctrl_states.Phi_ML = ctrl_states.Phi_MR = (int32_t)(tmp *K_FP_GAIN);
			return CMD_OK;
		}
		else if (!strcmp_P(argv[1], PSTR("dphi")))
		{
			float tmp = atof(argv[2]);
			ctrl_states.dPhi_ML = ctrl_states.dPhi_MR = (int32_t)(tmp *K_FP_GAIN);
			return CMD_OK;
		}
	}
	else if (!strcmp_P(argv[0], PSTR("get")) && argc == 2)
	{
		if (!strcmp_P(argv[1], PSTR("vref")))
		{
			printf_P(PSTR("vref: %g\n"), (float)ctrl_states.Vref/(float)K_FP_GAIN);
			return CMD_OK;
		}
		else if (!strcmp_P(argv[1], PSTR("theta")))
		{
			printf_P(PSTR("theta: %g\n"), (float)ctrl_states.dTheta/(float)K_FP_GAIN);
			return CMD_OK;
		}
		else if (!strcmp_P(argv[1], PSTR("dtheta")))
		{
			printf_P(PSTR("dtheta: %g\n"), (float)ctrl_states.dTheta/(float)K_FP_GAIN);
			return CMD_OK;
		}
		else if (!strcmp_P(argv[1], PSTR("phi")))
		{
			printf_P(PSTR("phi: %g\n"), (float)(ctrl_states.Phi_ML + ctrl_states.Phi_MR)/2.0/(float)K_FP_GAIN);
			return CMD_OK;
		}
		else if (!strcmp_P(argv[1], PSTR("dphi")))
		{
			printf_P(PSTR("dphi: %g\n"), (float)(ctrl_states.dPhi_ML + ctrl_states.dPhi_MR)/2.0/(float)K_FP_GAIN);
			return CMD_OK;
		}
		else if (!strcmp_P(argv[1], PSTR("current")))
		{
			printf_P(PSTR("Current: %g\n"), (float)(ctrl_states.Current_ML + ctrl_states.Current_MR)/2.0/(float)K_FP_GAIN);
			return CMD_OK;
		}
		else if (!strcmp_P(argv[1], PSTR("bias")))
		{
			printf_P(PSTR("bias: %g\n"), (float)ctrl_states.Bias/(float)K_FP_GAIN);
			return CMD_OK;
		}
		else if (!strcmp_P(argv[1], PSTR("states")))
		{
			printf_P(PSTR("t:%g, dt:%g, p:%g, dp:%g, v:%g\n"), ctrl_states.Theta, ctrl_states.dTheta, (ctrl_states.Phi_ML + ctrl_states.Phi_MR)/2.0, (ctrl_states.dPhi_ML + ctrl_states.dPhi_MR)/2.0, ctrl_states.Vref);
			return CMD_OK;
		}
	}
#else
	else if (!strcmp_P(argv[0], PSTR("run")) && argc == 2)
	{
		if (!strcmp_P(argv[1], PSTR("ML")))
		{
			float tmp = motor_ctrl_alloc(MOTOR_LEFT, &ctrl_states);
			printf_P(PSTR("%g\n"), tmp);
			motors_set_voltage(MOTOR_LEFT, tmp);
			return CMD_OK;
		}
		else if (!strcmp_P(argv[1], PSTR("MR")))
		{
			float tmp = motor_ctrl_alloc(MOTOR_RIGHT, &ctrl_states);
			printf_P(PSTR("%g\n"), tmp);
			motors_set_voltage(MOTOR_RIGHT, tmp);
			return CMD_OK;
		}
		else if (!strcmp_P(argv[1], PSTR("PEND")))
		{
			float tmp = pend_ctrl_alloc(&ctrl_states);
			printf_P(PSTR("%g\n"), (float)tmp);
#ifdef FIXEDPOINT_MOTORCTRL
			motor_set_torque(MOTOR_LEFT, tmp/2 * K_FP_GAIN);
			motor_set_torque(MOTOR_RIGHT, tmp/2 * K_FP_GAIN);
#else
			motor_set_torque(MOTOR_LEFT, tmp);
			motor_set_torque(MOTOR_RIGHT, tmp);
#endif
			return CMD_OK;
		}
		else if (!strcmp_P(argv[1], PSTR("KFTIME")))
		{
			kf_timestep(&ctrl_states);
			printf_P(PSTR("%g\n"), ctrl_states.Theta);
			return CMD_OK;
		}
	}
	else if (!strcmp_P(argv[0], PSTR("correct")) && argc == 3)
	{
		if (!strcmp_P(argv[1], PSTR ("theta")))
		{
			float tmp = atof(argv[2]);
#ifndef FIXEDPOINT_KALMAN
			kf_acccorrection(tmp, &ctrl_states);
#else
			kf_acccorrection((int32_t)tmp*K_FP_GAIN, &ctrl_states);
#endif
			return CMD_OK;
		}
		else if (!strcmp_P(argv[1], PSTR("dtheta")))
		{
			float tmp = atof(argv[2]);
#ifndef FIXEDPOINT_KALMAN
			kf_gyrocorrection(tmp, &ctrl_states);
#else
			kf_gyrocorrection((int32_t)tmp*K_FP_GAIN, &ctrl_states);
#endif
			return CMD_OK;
		}
		else if (!strcmp_P(argv[1], PSTR("phi")))
		{
			float tmp = atof(argv[2]);
#ifndef FIXEDPOINT_KALMAN
			kf_enccorrection(MOTOR_LEFT, tmp, &ctrl_states);
			kf_enccorrection(MOTOR_RIGHT, tmp, &ctrl_states);
#else
			kf_enccorrection(MOTOR_LEFT, (int32_t)tmp*K_FP_GAIN, &ctrl_states);
			kf_enccorrection(MOTOR_RIGHT, (int32_t)tmp*K_FP_GAIN, &ctrl_states);
#endif
			return CMD_OK;
		}
	}
	else if (!strcmp_P(argv[0], PSTR("set")) && argc == 3)
	{
		if (!strcmp_P(argv[1], PSTR("vref")))
		{
			float tmp = atof(argv[2]);
			ctrl_states.Vref = tmp;
			return CMD_OK;
		}
		else if (!strcmp_P(argv[1], PSTR("theta")))
		{
			float tmp = atof(argv[2]);
			ctrl_states.Theta = tmp;
			return CMD_OK;
		}
		else if (!strcmp_P(argv[1], PSTR("dtheta")))
		{
			float tmp = atof(argv[2]);
			ctrl_states.dTheta = tmp;
			return CMD_OK;
		}
		else if (!strcmp_P(argv[1], PSTR("phi")))
		{
			float tmp = atof(argv[2]);
			ctrl_states.Phi_ML = ctrl_states.Phi_MR = tmp;
			return CMD_OK;
		}
		else if (!strcmp_P(argv[1], PSTR("dphi")))
		{
			float tmp = atof(argv[2]);
			ctrl_states.dPhi_ML = ctrl_states.dPhi_MR = tmp;
			return CMD_OK;
		}
		else if (!strcmp_P(argv[1], PSTR("cur_ML")))
		{
			float tmp = atof(argv[2]);
			ctrl_states.Current_ML = tmp;
			return CMD_OK;
		}
		else if (!strcmp_P(argv[1], PSTR("cur_MR")))
		{
			float tmp = atof(argv[2]);
			ctrl_states.Current_MR = tmp;
			return CMD_OK;
		}
	}
	else if (!strcmp_P(argv[0], PSTR("get")) && argc == 2)
	{
		if (!strcmp_P(argv[1], PSTR("vref")))
		{
			printf_P(PSTR("vref: %g\n"), ctrl_states.Vref);
			return CMD_OK;
		}
		else if (!strcmp_P(argv[1], PSTR("theta")))
		{
			printf_P(PSTR("theta: %g\n"), ctrl_states.Theta);
			return CMD_OK;
		}
		else if (!strcmp_P(argv[1], PSTR("dtheta")))
		{
			printf_P(PSTR("dtheta: %g\n"), ctrl_states.dTheta);
			return CMD_OK;
		}
		else if (!strcmp_P(argv[1], PSTR("phi")))
		{
			printf_P(PSTR("phi: %g\n"), (ctrl_states.Phi_ML + ctrl_states.Phi_MR)/2.0);
			return CMD_OK;
		}
		else if (!strcmp_P(argv[1], PSTR("dphi")))
		{
			printf_P(PSTR("dphi: %g\n"), (ctrl_states.dPhi_ML + ctrl_states.dPhi_MR)/2.0);
			return CMD_OK;
		}
		else if (!strcmp_P(argv[1], PSTR("current")))
		{
			printf_P(PSTR("current: %g\n"), (ctrl_states.Current_ML + ctrl_states.Current_MR)/2.0);
			return CMD_OK;
		}
		else if (!strcmp_P(argv[1], PSTR("bias")))
		{
			printf_P(PSTR("bias: %g\n"), ctrl_states.Bias);
			return CMD_OK;
		}
		else if (!strcmp_P(argv[1], PSTR("states")))
		{
			printf_P(PSTR("t:%g, dt:%g, p:%g, dp:%g, v:%g\n"), ctrl_states.Theta, ctrl_states.dTheta, (ctrl_states.Phi_ML + ctrl_states.Phi_MR)/2.0, (ctrl_states.dPhi_ML + ctrl_states.dPhi_MR)/2.0, ctrl_states.Vref);
			return CMD_OK;
		}
	}
#endif
	return CMD_INVALIDPARAM;
}
