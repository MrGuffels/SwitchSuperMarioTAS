#include "Controller.h"

typedef enum {
	L_UP,
	L_DOWN,
	L_LEFT,
	L_RIGHT,
	R_UP,
	R_DOWN,
	R_LEFT,
	R_RIGHT,
	A,
	B,
	Y,
	X,
	L,
	R,
	ZL,
	ZR,
	MINUS,
    PLUS,
	SPRINT,
	SPRINT_JUMP,
	SUSPEND,
	SYNC,
    NOTHING
} Buttons_t;

typedef struct {
	Buttons_t button;
	uint16_t duration;
} command; 

//Instructions for controller goes here
//Plug in on sync screen
static const command step[] = {
	
	{SYNC,5},
	{NOTHING,25},
	{SYNC,5},
	{NOTHING,25},
	{A,5},
	{NOTHING,100},
	//Game Start
	{A,5},
	{NOTHING,115},
	{SPRINT,47},
	{SPRINT_JUMP,16},	//On Block
	{SPRINT,8},
	{SPRINT_JUMP,3},	//On Platform
	{SPRINT,16},
	{SPRINT_JUMP,3},	//Coin Block
	{SPRINT,13},
	{SPRINT_JUMP,3},	//Off Platform/Pipe1
	{SPRINT,36},
	{SPRINT_JUMP,7},	//Pipe2
	{SPRINT,11},
	{SPRINT_JUMP,16},	//Pipe3
	{SPRINT,15},
	{SPRINT_JUMP,3},	//Off Pipe3
	{SPRINT,26},
	{SPRINT_JUMP,10},	//On Pipe4
	{L_DOWN,10},		//In Pipe4
	{NOTHING,70},
	{SPRINT,15},
	{SPRINT_JUMP,8},	//SecretUp
	{SPRINT,23},
	{SPRINT_JUMP,3},	//SecretDown
	{L_LEFT,7},
	{NOTHING,17},
	{SPRINT,5},
	{NOTHING,195},
	{SPRINT,39},
	{SPRINT_JUMP,3},	//2nd Goomba
	{SPRINT,23},
	{SPRINT_JUMP,3},	//Last Pipe
	{SPRINT,14},
	{SPRINT_JUMP,17},
	{SPRINT,8},
	{SPRINT_JUMP,20},
	{SPRINT,30},

	{NOTHING,460},

	//Reset
	{SUSPEND,5},
	{NOTHING,25},
	{L_DOWN,5},
	{NOTHING,15},
	{L_RIGHT,5},

	{NOTHING,10000},


	
};

// Main entry point.
int main(void) {
	// We'll start by performing hardware and peripheral setup.
	SetupHardware();
	// We'll then enable global interrupts for our use.
	GlobalInterruptEnable();
	// Once that's done, we'll enter an infinite loop.
	for (;;)
	{
		// We need to run our task to process and deliver data for our IN and OUT endpoints.
		HID_Task();
		// We also need to run the main USB management task.
		USB_USBTask();
	}
}

// Configures hardware and peripherals, such as the USB peripherals.
void SetupHardware(void) {
	// We need to disable watchdog if enabled by bootloader/fuses.
	MCUSR &= ~(1 << WDRF);
	wdt_disable();

	// We need to disable clock division before initializing the USB hardware.
	clock_prescale_set(clock_div_1);
	// We can then initialize our hardware and peripherals, including the USB stack.

	// The USB stack should be initialized last.
	USB_Init();
}

// Fired to indicate that the device is enumerating.
void EVENT_USB_Device_Connect(void) {
	// We can indicate that we're enumerating here (via status LEDs, sound, etc.).
}

// Fired to indicate that the device is no longer connected to a host.
void EVENT_USB_Device_Disconnect(void) {
	// We can indicate that our device is not ready (via status LEDs, sound, etc.).
}

// Fired when the host set the current configuration of the USB device after enumeration.
void EVENT_USB_Device_ConfigurationChanged(void) {
	bool ConfigSuccess = true;

	// We setup the HID report endpoints.
	ConfigSuccess &= Endpoint_ConfigureEndpoint(JOYSTICK_OUT_EPADDR, EP_TYPE_INTERRUPT, JOYSTICK_EPSIZE, 1);
	ConfigSuccess &= Endpoint_ConfigureEndpoint(JOYSTICK_IN_EPADDR, EP_TYPE_INTERRUPT, JOYSTICK_EPSIZE, 1);

	// We can read ConfigSuccess to indicate a success or failure at this point.
}

// Process control requests sent to the device from the USB host.
void EVENT_USB_Device_ControlRequest(void) {
	// We can handle two control requests: a GetReport and a SetReport.

	// Not used here, it looks like we don't receive control request from the Switch.
}

// Process and deliver data from IN and OUT endpoints.
void HID_Task(void) {
	// If the device isn't connected and properly configured, we can't do anything here.
	if (USB_DeviceState != DEVICE_STATE_Configured)
		return;

	// We'll start with the OUT endpoint.
	Endpoint_SelectEndpoint(JOYSTICK_OUT_EPADDR);
	// We'll check to see if we received something on the OUT endpoint.
	if (Endpoint_IsOUTReceived())
	{
		// If we did, and the packet has data, we'll react to it.
		if (Endpoint_IsReadWriteAllowed())
		{
			// We'll create a place to store our data received from the host.
			USB_JoystickReport_Output_t JoystickOutputData;
			// We'll then take in that data, setting it up in our storage.
			while(Endpoint_Read_Stream_LE(&JoystickOutputData, sizeof(JoystickOutputData), NULL) != ENDPOINT_RWSTREAM_NoError);
			// At this point, we can react to this data.

			// However, since we're not doing anything with this data, we abandon it.
		}
		// Regardless of whether we reacted to the data, we acknowledge an OUT packet on this endpoint.
		Endpoint_ClearOUT();
	}

	// We'll then move on to the IN endpoint.
	Endpoint_SelectEndpoint(JOYSTICK_IN_EPADDR);
	// We first check to see if the host is ready to accept data.
	if (Endpoint_IsINReady())
	{
		// We'll create an empty report.
		USB_JoystickReport_Input_t JoystickInputData;
		// We'll then populate this report with what we want to send to the host.
		GetNextReport(&JoystickInputData);
		// Once populated, we can output this data to the host. We do this by first writing the data to the control stream.
		while(Endpoint_Write_Stream_LE(&JoystickInputData, sizeof(JoystickInputData), NULL) != ENDPOINT_RWSTREAM_NoError);
		// We then send an IN packet on this endpoint.
		Endpoint_ClearIN();
	}
}

typedef enum {
	SYNC_CONTROLLER,
	SYNC_POSITION,
	BREATHE,
	PROCESS,
	CLEANUP,
	DONE
} State_t;
State_t state = SYNC_CONTROLLER;

#define ECHOES 2
int echoes = 0;
USB_JoystickReport_Input_t last_report;

int report_count = 0;
int xpos = 0;
int ypos = 0;
int bufindex = 0;
int duration_count = 0;
int portsval = 0;

// Prepare the next report for the host.
void GetNextReport(USB_JoystickReport_Input_t* const ReportData) {

	// Prepare an empty report
	memset(ReportData, 0, sizeof(USB_JoystickReport_Input_t));
	ReportData->LX = STICK_CENTER;
	ReportData->LY = STICK_CENTER;
	ReportData->RX = STICK_CENTER;
	ReportData->RY = STICK_CENTER;
	ReportData->HAT = HAT_CENTER;

	// Repeat ECHOES times the last report
	if (echoes > 0)
	{
		memcpy(ReportData, &last_report, sizeof(USB_JoystickReport_Input_t));
		echoes--;
		return;
	}

	// States and moves management
	switch (state)
	{

		case SYNC_CONTROLLER:
			state = BREATHE;
			break;		
            
        case SYNC_POSITION:
			bufindex = 0;


			ReportData->Button = 0;
			ReportData->LX = STICK_CENTER;
			ReportData->LY = STICK_CENTER;
			ReportData->RX = STICK_CENTER;
			ReportData->RY = STICK_CENTER;
			ReportData->HAT = HAT_CENTER;


			state = BREATHE;
			break;

		case BREATHE:
			state = PROCESS;
			break;

		case PROCESS:

			switch (step[bufindex].button)
			{

				case L_UP:
					ReportData->LY = STICK_MIN;				
					break;

				case L_DOWN:
					ReportData->LY = STICK_MAX;				
					break;

				case L_LEFT:
					ReportData->LX = STICK_MIN;				
					break;

				case L_RIGHT:
					ReportData->LX = STICK_MAX;				
					break;

				case R_UP:
					ReportData->RY = STICK_MIN;				
					break;

				case R_DOWN:
					ReportData->RY = STICK_MAX;				
					break;

				case R_LEFT:
					ReportData->RX = STICK_MIN;				
					break;

				case R_RIGHT:
					ReportData->RX = STICK_MAX;				
					break;
					
				case A:
					ReportData->Button |= SWITCH_A;
					break;

				case B:
					ReportData->Button |= SWITCH_B;
					break;

				case Y:
					ReportData->Button |= SWITCH_Y;
					break;

				case X:
					ReportData->Button |= SWITCH_X;
					break;

				case L:
					ReportData->Button |= SWITCH_L;
					break;

				case R:
					ReportData->Button |= SWITCH_R;
					break;
				
				case ZL:
					ReportData->Button |= SWITCH_ZL;
					break;

				case ZR:
					ReportData->Button |= SWITCH_ZR;
					break;

				case MINUS:
					ReportData->Button |= SWITCH_MINUS;

                case PLUS:
                    ReportData->Button |= SWITCH_PLUS;

				case SPRINT:
					ReportData->Button |= SWITCH_B;
					ReportData->LX = STICK_MAX;
					break;

				case SPRINT_JUMP:
					ReportData->Button |= SWITCH_B | SWITCH_A;
					ReportData->LX = STICK_MAX;
					break;

				case SUSPEND:
					ReportData->Button |= SWITCH_ZL | SWITCH_ZR;
					break;

				case SYNC:
					ReportData->Button |= SWITCH_L | SWITCH_R;
					break;

				default:
					ReportData->LX = STICK_CENTER;
					ReportData->LY = STICK_CENTER;
					ReportData->RX = STICK_CENTER;
					ReportData->RY = STICK_CENTER;
					ReportData->HAT = HAT_CENTER;
					break;
			}

			duration_count++;

			if (duration_count > step[bufindex].duration)
			{
				bufindex++;
				duration_count = 0;				
			}


			if (bufindex > (int)( sizeof(step) / sizeof(step[0])) - 1)
			{

				// state = CLEANUP;

				bufindex = 7;
				duration_count = 0;

				state = BREATHE;

				ReportData->LX = STICK_CENTER;
				ReportData->LY = STICK_CENTER;
				ReportData->RX = STICK_CENTER;
				ReportData->RY = STICK_CENTER;
				ReportData->HAT = HAT_CENTER;


				// state = DONE;
//				state = BREATHE;

			}

			break;

		case CLEANUP:
			state = DONE;
			break;

		case DONE:
			#ifdef ALERT_WHEN_DONE
			portsval = ~portsval;
			PORTD = portsval; //flash LED(s) and sound buzzer if attached
			PORTB = portsval;
			_delay_ms(250);
			#endif
			return;
	}

	// // Inking
	// if (state != SYNC_CONTROLLER && state != SYNC_POSITION)
	// 	if (pgm_read_byte(&(image_data[(xpos / 8) + (ypos * 40)])) & 1 << (xpos % 8))
	// 		ReportData->Button |= SWITCH_A;

	// Prepare to echo this report
	memcpy(&last_report, ReportData, sizeof(USB_JoystickReport_Input_t));
	echoes = ECHOES;

}