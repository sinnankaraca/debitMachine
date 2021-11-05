/* USER CODE BEGIN Header */
/*******************************************************************************
 * File Name          : main.c Debit Machine
 * Description        : This is a real time demonstration of Debit Machine
 * 		Consisting of 7 different states - 7th state is initial state
 * 		Consisting of 5 different buttons
 * 		Consisting of a speaker
 * 		Consisting of 1 LCD screen
 * 		Consisting of 1 STM32 Nucleo
 *		Demonstration of real life debit machine.
 *
 *
 * Author:              Sinan KARACA
 * Date:                15.08.2021
 ******************************************************************************
 */
/* USER CODE END Header */
/* Includes ------------------------------------------------------------------*/
#include "main.h"

/* Private includes ----------------------------------------------------------*/
/* USER CODE BEGIN Includes */
#include <unistd.h>
#include <stdio.h>
#include "debounce.h"
#include "HD44780.h"

/* USER CODE END Includes */

/* Private typedef -----------------------------------------------------------*/
/* USER CODE BEGIN PTD */

/* USER CODE END PTD */

/* Private define ------------------------------------------------------------*/
/* USER CODE BEGIN PD */
/* USER CODE END PD */

/* Private macro -------------------------------------------------------------*/
/* USER CODE BEGIN PM */

/* USER CODE END PM */

/* Private variables ---------------------------------------------------------*/
TIM_HandleTypeDef htim1;

UART_HandleTypeDef huart2;

/* USER CODE BEGIN PV */
static const int16_t leftBtn = 0; //setting the pin assigned to each pb
static const int16_t rightBtn = 1;		//don't use pin 2 as it's connected
static const int16_t downBtn = 4;		//to VCP TX
static const int16_t upBtn = 3;
static const int16_t enterBtn = 7;
float bankCheck;

uint8_t passCode[4] = { '1', '2','3','4'};

uint16_t intAmount;
uint8_t indexTmp = 0;
uint8_t btnState;
//float amount = 1;
int multiplier = 1;
float amount = 1;             	//used to hold the transaction amount

uint8_t passCodesInt[4] = { 9,9,9,9};

uint8_t amountArray[5] = { 0,0,0,0,0};
char str[4] = { "****"} ;
uint8_t tempState;



enum pushButton {
	none, left, right, down, up, enter
};
//enumerated values for use with if
//(pbPressed == value) type conditional
//statements

/* USER CODE END PV */

/* Private function prototypes -----------------------------------------------*/
void SystemClock_Config(void);
static void MX_GPIO_Init(void);
static void MX_USART2_UART_Init(void);
static void MX_TIM1_Init(void);
/* USER CODE BEGIN PFP */

/* USER CODE END PFP */

/* Private user code ---------------------------------------------------------*/
/* USER CODE BEGIN 0 */

// FUNCTION      : setTone
// DESCRIPTION   : Calculates the PWM Period needed to obtain the freq
//				 : passed and the duty cycle of the PAM to
//				 : 50% (1/2 of the period)
// PARAMETERS    : int32 freq - frequency of the output
// RETURNS       : nothing
void setTone(int32_t freq) {
	int32_t pwmPeriod = 1000000000 / (freq * 250); //value can vary between 2 and 65535
	TIM_MasterConfigTypeDef sMasterConfig;
	TIM_OC_InitTypeDef sConfigOC;
	TIM_BreakDeadTimeConfigTypeDef sBreakDeadTimeConfig;

	htim1.Instance = TIM1;
	htim1.Init.Prescaler = 0;
	htim1.Init.CounterMode = TIM_COUNTERMODE_UP;
	htim1.Init.Period = pwmPeriod;
	htim1.Init.ClockDivision = TIM_CLOCKDIVISION_DIV1;
	htim1.Init.RepetitionCounter = 0;
	if (HAL_TIM_PWM_Init(&htim1) != HAL_OK) {
		Error_Handler();
	}

	sMasterConfig.MasterOutputTrigger = TIM_TRGO_RESET;
	sMasterConfig.MasterOutputTrigger2 = TIM_TRGO2_RESET;
	sMasterConfig.MasterSlaveMode = TIM_MASTERSLAVEMODE_DISABLE;
	if (HAL_TIMEx_MasterConfigSynchronization(&htim1, &sMasterConfig)
			!= HAL_OK) {
		Error_Handler();
	}

	sConfigOC.OCMode = TIM_OCMODE_PWM1;
	sConfigOC.Pulse = pwmPeriod / 2;
	sConfigOC.OCPolarity = TIM_OCPOLARITY_HIGH;
	sConfigOC.OCNPolarity = TIM_OCNPOLARITY_HIGH;
	sConfigOC.OCFastMode = TIM_OCFAST_DISABLE;
	sConfigOC.OCIdleState = TIM_OCIDLESTATE_RESET;
	sConfigOC.OCNIdleState = TIM_OCNIDLESTATE_RESET;
	/* adding this as ST Tech Support said PWM should be stopped before
	 * calling HAL_TIM_PWM_ConfigChannel and I've been getting flakey start-up
	 * i.e.: sometime PWM starts up, other times the line remains stuck high.
	 **************************************/
	HAL_TIM_PWM_Stop(&htim1, TIM_CHANNEL_1);
	/*************************************/
	if (HAL_TIM_PWM_ConfigChannel(&htim1, &sConfigOC, TIM_CHANNEL_1)
			!= HAL_OK) {
		Error_Handler();
	}

	sBreakDeadTimeConfig.OffStateRunMode = TIM_OSSR_DISABLE;
	sBreakDeadTimeConfig.OffStateIDLEMode = TIM_OSSI_DISABLE;
	sBreakDeadTimeConfig.LockLevel = TIM_LOCKLEVEL_OFF;
	sBreakDeadTimeConfig.DeadTime = 0;
	sBreakDeadTimeConfig.BreakState = TIM_BREAK_DISABLE;
	sBreakDeadTimeConfig.BreakPolarity = TIM_BREAKPOLARITY_HIGH;
	sBreakDeadTimeConfig.BreakFilter = 0;
	sBreakDeadTimeConfig.Break2State = TIM_BREAK2_DISABLE;
	sBreakDeadTimeConfig.Break2Polarity = TIM_BREAK2POLARITY_HIGH;
	sBreakDeadTimeConfig.Break2Filter = 0;
	sBreakDeadTimeConfig.AutomaticOutput = TIM_AUTOMATICOUTPUT_DISABLE;
	if (HAL_TIMEx_ConfigBreakDeadTime(&htim1, &sBreakDeadTimeConfig)
			!= HAL_OK) {
		Error_Handler();
	}
	HAL_TIM_MspPostInit(&htim1);
}

// FUNCTION      : waitForPBRelease
// DESCRIPTION   : Loops until the PB that is currently
//				 : pressed and at a logic low
//				 : is released. Release is debounced
// PARAMETERS    : pin - pin number
//                 port- port letter ie 'A'
// RETURNS       : nothing
void waitForPBRelease(const int16_t pin, const char port) {
	while (deBounceReadPin(pin, port, 10) == 0) {
		//do nothing wait for key press to be released
	}
}

// FUNCTION      : startUpLCDSplashScreen()
// DESCRIPTION   : displays Debit Demo for 2s
//                 on line 1 of the display and
//				 : Disappears
// PARAMETERS    : None
// RETURNS       : nothing
void startUpLCDSplashScreen(void) {
	char stringBuffer[16] = { 0 };
	HD44780_GotoXY(0, 0);
	snprintf(stringBuffer, 16, "   Debit Demo");
	HD44780_PutStr(stringBuffer);
	HAL_Delay(2000);
	HD44780_ClrScr();
}

//// FUNCTION      : pinSound()
//// DESCRIPTION   : This is the function makes the sound
////		When each pin pushed
////
//// PARAMETERS    : None
//// RETURNS       : nothing
//
//void pinSound(){
//	setTone(100);
//	HAL_Delay(50);
//	setTone(0);
//}

//// FUNCTION      : bankCheckSound()
//// DESCRIPTION   : This is the function when the transaction
////		is done succesfully.
////
//// PARAMETERS    : None
//// RETURNS       : nothing
//void bankCheckSound(){
//	setTone(2000);
//	HAL_Delay(100);
//	setTone(0);
//	setTone(2000);
//	HAL_Delay(100);
//	setTone(0);
//}

//// FUNCTION      : bankDeniedSound()
//// DESCRIPTION   : This is the function when the transaction
////		is denied by bank.
////
//// PARAMETERS    : None
//// RETURNS       : nothing
//
//void bankDeniedSound(){
//	setTone(10000);
//	HAL_Delay(500);
//}


// FUNCTION      : showChequingOrSavings()
// DESCRIPTION   : The function shows if the transaction will be done
//		from chequing or saving account.
//		or to be cancelled.
//
// PARAMETERS    : None
// RETURNS       : nothing

void showChequingOrSavings (int input){

	char stringBuffer[16] = { 0 };


	if (input == 1) {
		//then up was pressed.
		HD44780_ClrScr();
		snprintf(stringBuffer, 16, "Chequing...");
		HD44780_GotoXY(1, 0);
		HD44780_PutStr(stringBuffer);
	} else if (input == 2) {
		//then down pressed
		HD44780_ClrScr();
		HD44780_GotoXY(1, 0);
		snprintf(stringBuffer, 16, "Savings...");
		HD44780_PutStr(stringBuffer);

	}else if (input == 3) {
		//then right pressed
		HD44780_ClrScr();
		snprintf(stringBuffer, 16, "Cancel...");
		HD44780_GotoXY(1, 0);
		HD44780_PutStr(stringBuffer);

	}

}

// FUNCTION      : pulsePWM
// DESCRIPTION   : Turns on the PWM for the pulseTime in ms
//                 provided and then turns off PWM
// PARAMETERS    : address of Timer Handle var (e.g.: &htim1)
//                 pulseTime in ms
// RETURNS       : nothing
void pulsePWM(TIM_HandleTypeDef *htim1, int32_t pulseTime) {
	HAL_TIMEx_PWMN_Start(htim1, TIM_CHANNEL_1);
	HAL_Delay(pulseTime);
	HAL_TIMEx_PWMN_Stop(htim1, TIM_CHANNEL_1);
}

//  FUNCTION      : pushButtonInit
//   DESCRIPTION   : Calls deBounceInit to initialize ports that
//                   will have pushbutton on them to be inputs.
//			         Initializing PA0,PA1,PA4 and PA3
//                   Switches are assigned as follows
//                   PA0			PA1			PA4			PA3 	PA3
//                   left		right		down			up		enter
//
//                   Note: Don't use PA2 as it is connected to VCP TX and you'll
//                   lose printf output ability.
//   PARAMETERS    : None
//   RETURNS       : nothing
void pushButtonInit(void) {
	deBounceInit(leftBtn, 'A', 1); 		//1 = pullup resistor enabled
	deBounceInit(rightBtn, 'A', 1); 		//1 = pullup resistor enabled
	deBounceInit(downBtn, 'A', 1); 			//1 = pullup resistor enabled
	deBounceInit(upBtn, 'A', 1); 		//1 = pullup resistor enabled
	deBounceInit(enterBtn, 'A', 1);		//1 = pullup resistor enabled
}

// FUNCTION      : displayWelcome()
// DESCRIPTION   : clears the LCD display and displays
//                 Welcome on line 1 of the display
// PARAMETERS    : None
// RETURNS       : nothing
void displayWelcome(void) {
	char stringBuffer[16] = { 0 };
	HD44780_ClrScr();
	snprintf(stringBuffer, 16, "Debit Machine");
	HD44780_PutStr(stringBuffer);
	HD44780_GotoXY(0, 1);
	HD44780_PutStr("Sinan KARACA");
}

// FUNCTION      : wrongPinScreen()
// DESCRIPTION   : Shows if the pin is not correct
//
// PARAMETERS    : None
// RETURNS       : nothing

void wrongPinScreen(){
	HD44780_ClrScr();
	HD44780_GotoXY(0, 0);
	HD44780_PutStr("Wrong Pin!");
	HD44780_GotoXY(0, 1);
	HD44780_PutStr("Try Again !!");
}

// FUNCTION      : succesfullScreen()
// DESCRIPTION   : Shows if the pin is correct
//
// PARAMETERS    : None
// RETURNS       : nothing

void succesfullScreen(void){
	HD44780_ClrScr();
	HD44780_GotoXY(0, 0);
	HD44780_PutStr("Pin Correct!");
	HD44780_GotoXY(0, 1);
	HD44780_PutStr("Wait for bank!");

}

// FUNCTION      : choosePin()
// DESCRIPTION   : Shows how to choose pin
// 		with up and down buttons
//
// PARAMETERS    : None
// RETURNS       : nothing

void choosePin(void){
	HD44780_ClrScr();
	HD44780_GotoXY(0, 0);
	HD44780_PutStr("Choose Pin!");
	HD44780_GotoXY(0, 1);
	HD44780_PutStr("With up&down");
}

// FUNCTION      : displayAmount()
// DESCRIPTION   : clears the LCD display and displays
//                 the $amount received on line 1 of the display
// PARAMETERS    : float - amount to display
// RETURNS       : nothing
void displayAmount(float amount) {
	char stringBuffer[16] = { 0 };
	HD44780_ClrScr();
	snprintf(stringBuffer, 16, "$%.2f", amount);
	HD44780_PutStr(stringBuffer);
}


// FUNCTION      : checkIfBankOk()
// DESCRIPTION   :
// PARAMETERS    : none
// RETURNS       : integer
float checkIfBankOk() {

	float bankCheckTemp ;
	printf("Waiting for the approval of the transaction \r\n");
	printf("Please type 1 for approval \r\n");
	printf("Please type 2 for denial \r\n");
	float result = 0;
	result = scanf("%f", &bankCheckTemp);
	if (result == 0)		//then somehow non-float chars were entered
	{						//and nothing was assigned to %f
		fpurge(STDIN_FILENO); //clear the last erroneous char(s) from the input stream
	}
	return result;
}

// FUNCTION      : checkOkOrCancel()
// DESCRIPTION   : Checks whether the OK or Cancel
//                 button has been pressed.
// PARAMETERS    : none
// RETURNS       : int8_t, 3 if cancel pressed, 4 if ok
//                 ok pressed. 0 returned if neither
//                 has pressed.
enum pushButton checkOkOrCancel(void) {
	if (deBounceReadPin(upBtn, 'A', 10) == 0) {
		//then the cancel pushbutton has been pressed
		return up;
	} else if (deBounceReadPin(downBtn, 'A', 10) == 0) {
		//then ok pressed
		return down;
	}else if (deBounceReadPin(rightBtn, 'A', 10) == 0) {
		//then ok pressed
		return right;
	}else if (deBounceReadPin(leftBtn, 'A', 10) == 0) {
		//then ok pressed
		return left;
	}else if (deBounceReadPin(enterBtn, 'A', 10) == 0) {
		//then ok pressed
		return enter;
	}
	return none; //as ok or cancel was not pressed.
}

// FUNCTION      : displayOkOrCancel()
// DESCRIPTION   : displays "OK or Cancel?" on line 2 of LCD
// PARAMETERS    : none
// RETURNS       : nothing.
void displayOkCancel(float amount) {
	HD44780_ClrScr();
	char stringBuffer[16] = { 0 };
	HD44780_GotoXY(0, 1); //move to second line first position
	snprintf(stringBuffer, 16, "OK or Cancel?");
	HD44780_PutStr(stringBuffer);
	HD44780_GotoXY(9, 0);
	snprintf(stringBuffer, 16, "$%.2f", amount);
	HD44780_PutStr(stringBuffer);
}

// FUNCTION      : receipt()
// DESCRIPTION   : successfull transaction receipt
// PARAMETERS    : none
// RETURNS       : nothing.

void receipt() {
	printf("The transaction was successfull \r\n");

	HD44780_ClrScr();
	HD44780_GotoXY(0, 0);
	HD44780_PutStr("The receipt:");
	HD44780_GotoXY(0, 1);

	if(tempState == 1){
		HD44780_PutStr("Chequing Acc.");
	} else if (tempState == 2) {
		HD44780_PutStr("Savings Acc.");
	}

	HAL_Delay(5000);

	displayAmount(amount);
	HD44780_GotoXY(0, 1);
	HD44780_PutStr("Thank you!");
	HAL_Delay(5000);
}



/* USER CODE END 0 */

/**
  * @brief  The application entry point.
  * @retval int
  */
int main(void)
{
  /* USER CODE BEGIN 1 */

  /* USER CODE END 1 */

  /* MCU Configuration--------------------------------------------------------*/

  /* Reset of all peripherals, Initializes the Flash interface and the Systick. */
  HAL_Init();

  /* USER CODE BEGIN Init */

  /* USER CODE END Init */

  /* Configure the system clock */
  SystemClock_Config();

  /* USER CODE BEGIN SysInit */

  /* USER CODE END SysInit */

  /* Initialize all configured peripherals */
  MX_GPIO_Init();
  MX_USART2_UART_Init();
  MX_TIM1_Init();
  /* USER CODE BEGIN 2 */

	printf("Debit Card State Machine\r\n");
	HD44780_Init();
	/* setup Port A bits 0,1,2 and 3, i.e.: PA0-PA3 for input */
	pushButtonInit();
	displayWelcome();

  /* USER CODE END 2 */

  /* Infinite loop */
  /* USER CODE BEGIN WHILE */
	while (1) {
    /* USER CODE END WHILE */

    /* USER CODE BEGIN 3 */

		static int8_t transactionState = 7;
		enum pushButton pbPressed = none;  //will hold pushbutton defined above depending on
							   	   	   	   //the pushbutton pressed
		/*states:
		 1   For choosing amount from pins
		 2   For approve user typed amount if it is correct
		 3   For choosing Chequing or Saving account
		 4   For compare the pin with the initial pin of 2564 integer value
		 5   Pin Correct, send transaction data to bank. Waiting
		 for OK back from Bank If OK from Bank received. Print
		 Receipt, Record transaction. Move back to State 7.
		 6   Cancel Pressed. Display "Transaction Cancelled" back to state 7
		 7   This is the inital state, it is only sliding welcome string
		 */

		switch (transactionState) {
		case 1: 					//checking for the amout of money which is typed by buttons

			pbPressed = checkOkOrCancel();

			if (pbPressed == none) {
				btnState = 0;
			}

			if (pbPressed != none && btnState == 0) {
				btnState = 1;
				if (pbPressed == down) {
					//Creating pin sound through speaker
//					pinSound();
					//up button means increment the correspounding value
					if (intAmount <= 0) {
						intAmount = 9;
					}else {
						intAmount =  intAmount - 1 ;
					}

				} else if (pbPressed == up) {
					//Creating pin sound through speaker
//					pinSound();
					//up button means increment the correspounding value
					if (intAmount >= 9) {
						intAmount = 0;
					}else {
						intAmount = intAmount + 1;
					}

				} else if (pbPressed == right){
					//Creating pin sound through speaker
//					pinSound();
					//right button means go to next number

					btnState = 1;
					if(multiplier == 4){
						multiplier = 4;
					}else{
						multiplier = multiplier  + 1;
					}
					intAmount = amountArray[multiplier];

				} else if (pbPressed == left){
					//Creating pin sound through speaker
//					pinSound();
					//left button means go to previous number

					btnState = 1;


					if(multiplier == 0){
						multiplier = 0;
					}else{
						multiplier = multiplier -1 ;
					}
					intAmount = amountArray[multiplier];

				} else if (pbPressed == enter){
					//Creating pin sound through speaker
//					pinSound();
					//enter  button work like enter in keyboard
					btnState = 1;
					displayOkCancel(amount);

					transactionState = 2;
					break;

				}
				//amount array is 5 digit 1 dimensional
				// Max decimal number is 999
				// Max float number is 999,99
				// to turn to float the code line below do the job
				amountArray[multiplier] = intAmount;
				amount = 100 * amountArray[0]  + 10 * amountArray[1] + amountArray[2] + amountArray[3] * 0.10 + amountArray[4] * 0.01  ;
				displayAmount(amount);
			}

				transactionState = 1;


			break;
		case 2: 						//amount has been received waiting for

			pbPressed = checkOkOrCancel();

			if (pbPressed != none) {
				if (pbPressed == down) {
//					pinSound();
					//then cancel was pressed.
					displayAmount(amount);
					transactionState = 7;
				} else if (pbPressed == up) {
//					pinSound();
					//then ok pressed
					transactionState = 3;
				}
			}
			break;

		case 3:
			pbPressed = checkOkOrCancel();
			//btnState check is necessary every push of a button
			//every push means 1 time increment, decrement, right or left move.

			if (pbPressed == none) {
				btnState = 0;
			}

			if (pbPressed != none && btnState == 0) {
				btnState = 1;
				if (pbPressed == up) {
//					pinSound();
					tempState = 1;
				} else if (pbPressed == down) {
//					pinSound();
					tempState = 2;
				}else if (pbPressed == right) {
//					pinSound();
					tempState = 3;
				}else if (pbPressed == enter) {
//					pinSound();
					if (tempState == 3){
						transactionState = 1;
						break;
					}else{
						transactionState = 4;
						choosePin();
						break;
					}
				}

				//TempState is showing if the transcation is occured on Chequing or Saving
				//TemState = 1 , Chequing go to state 4
				//TemState = 2 , Savings go to state 4
				//TemState = 3 , Cancel go to state 1
				showChequingOrSavings(tempState);
			}

			break;
		case 4:
			pbPressed = checkOkOrCancel();

			//This is the state that checks if the typed pin is correct
			if (pbPressed == none) {
				btnState = 0;
			}


			if (pbPressed != none && btnState == 0) {
				btnState = 1;
				if (pbPressed == down) {
//					pinSound();

					// decrement the correspounding value
					if (intAmount <= 0) {
						intAmount = 9;
					}else {
						intAmount =  intAmount - 1 ;
					}

				} else if (pbPressed == up) {
//					pinSound();

					// increment the correspounding value
					if (intAmount >= 9) {
						intAmount = 0;
					}else {
						intAmount = intAmount + 1;
					}
					//more code needed to get to state 3 goes here

				} else if (pbPressed == right){
//					pinSound();

					// go to next number
					btnState = 1;
					if(indexTmp == 3){
						indexTmp = 3;
					}else{
						indexTmp = indexTmp + 1;
					}
					intAmount = passCodesInt[indexTmp];

				} else if (pbPressed == left){
//					pinSound();

					// go to previous number
					btnState = 1;
					if(indexTmp == 0){
						indexTmp = 0;
					}else{
						indexTmp = indexTmp - 1;
					}
					intAmount = passCodesInt[indexTmp];

				} else if (pbPressed == enter){
//					pinSound();
					btnState = 1;

					//passCodeTemp is created to get temporary decimal value of pincode
					//each digit stored in array
					int passCodeTemp;
					passCodeTemp = passCodesInt[0] * 1000 + passCodesInt[1] * 100 + passCodesInt[2] * 10 + passCodesInt[3] ;

					//check if password correct with the initial passcode
					if(passCodeTemp == 2564){
						transactionState = 5;
						//Show user if the pincode is correct
						succesfullScreen();
						break;

					}else{

						HD44780_ClrScr();
						HD44780_GotoXY(0, 0);
						HD44780_PutStr("Pin Not Correct");
						HD44780_GotoXY(0, 1);
						HD44780_PutStr("Try Again");
						HAL_Delay(2000);

						transactionState = 1;
						break;

					}
				}
				passCodesInt[indexTmp] = intAmount;


				//The line of codes below
				//Get the value from passCodeInt array and make it string to show on the lcd.
				int i=0;
				int k = 0;
				for (i=0; i<4; i++){
					k += sprintf(&str[k], "%d", passCodesInt[i]);
				}

				HD44780_ClrScr();
				HD44780_GotoXY(0, 0);
				HD44780_PutStr("Pass Code:");
				HD44780_GotoXY(0, 1);
				HD44780_PutStr(str);

			}
			transactionState = 4;
			break;
		case 5:
			//code needed here

			//The function check if the transaction approved from bank.
			bankCheck = checkIfBankOk();

			if (bankCheck == 1){

				HD44780_ClrScr();
				HD44780_GotoXY(0, 0);
				HD44780_PutStr("Succesfull");
				HAL_Delay(2000);
//				bankCheckSound();

				transactionState = 6;
				break;
			} else if (bankCheck != 1){

				//Shows the user if the pin is not correct
				wrongPinScreen();

				//Bank denied sound
//				bankDeniedSound();
				transactionState = 2;
				break;
			}

			break;
		case 6:
			//This is the stage that transcation has been done!
			//Show the receipt to user.
			receipt();
			//Go to initial state.
			transactionState = 7;
			break;
		case 7:
			// The line of code has been created for sliding string on the lcd.
			// This state is the initial state , program start with this stage.
			// it consist of 3 ms delay in for loop
			// The string is suppose to shift right and left in 150 ms.
			for (int i = 0; i < 7; i++) {
				for (int k = 1; k < 51; k++){
					HAL_Delay(3);
					pbPressed = checkOkOrCancel();
					if (pbPressed != none) {
						transactionState = 1;
						break;
					}
				}
				if (pbPressed != none) {
					transactionState = 1;
					break;
				}

				HD44780_ClrScr();
				HD44780_GotoXY(i, 0);
				HD44780_PutStr("*Welcome*");
			}
			for (int i = 6; i > 0; i--) {
				for (int k = 1; k < 51; k++){
					HAL_Delay(3);
					pbPressed = checkOkOrCancel();
					if (pbPressed != none) {
						transactionState = 1;
						break;

					}
				}
				if (pbPressed != none) {
					transactionState = 1;
					break;
				}
				HD44780_ClrScr();
				HD44780_GotoXY(i, 0);
				HD44780_PutStr("*Welcome*");
			}


		default:
			break;
		} //closing brace for switch

	}
  /* USER CODE END 3 */
}

/**
  * @brief System Clock Configuration
  * @retval None
  */
void SystemClock_Config(void)
{
  RCC_OscInitTypeDef RCC_OscInitStruct = {0};
  RCC_ClkInitTypeDef RCC_ClkInitStruct = {0};
  RCC_PeriphCLKInitTypeDef PeriphClkInit = {0};

  /** Configure LSE Drive Capability
  */
  HAL_PWR_EnableBkUpAccess();
  __HAL_RCC_LSEDRIVE_CONFIG(RCC_LSEDRIVE_LOW);
  /** Initializes the RCC Oscillators according to the specified parameters
  * in the RCC_OscInitTypeDef structure.
  */
  RCC_OscInitStruct.OscillatorType = RCC_OSCILLATORTYPE_LSE|RCC_OSCILLATORTYPE_MSI;
  RCC_OscInitStruct.LSEState = RCC_LSE_ON;
  RCC_OscInitStruct.MSIState = RCC_MSI_ON;
  RCC_OscInitStruct.MSICalibrationValue = 0;
  RCC_OscInitStruct.MSIClockRange = RCC_MSIRANGE_6;
  RCC_OscInitStruct.PLL.PLLState = RCC_PLL_ON;
  RCC_OscInitStruct.PLL.PLLSource = RCC_PLLSOURCE_MSI;
  RCC_OscInitStruct.PLL.PLLM = 1;
  RCC_OscInitStruct.PLL.PLLN = 16;
  RCC_OscInitStruct.PLL.PLLP = RCC_PLLP_DIV7;
  RCC_OscInitStruct.PLL.PLLQ = RCC_PLLQ_DIV2;
  RCC_OscInitStruct.PLL.PLLR = RCC_PLLR_DIV2;
  if (HAL_RCC_OscConfig(&RCC_OscInitStruct) != HAL_OK)
  {
    Error_Handler();
  }
  /** Initializes the CPU, AHB and APB buses clocks
  */
  RCC_ClkInitStruct.ClockType = RCC_CLOCKTYPE_HCLK|RCC_CLOCKTYPE_SYSCLK
                              |RCC_CLOCKTYPE_PCLK1|RCC_CLOCKTYPE_PCLK2;
  RCC_ClkInitStruct.SYSCLKSource = RCC_SYSCLKSOURCE_PLLCLK;
  RCC_ClkInitStruct.AHBCLKDivider = RCC_SYSCLK_DIV1;
  RCC_ClkInitStruct.APB1CLKDivider = RCC_HCLK_DIV1;
  RCC_ClkInitStruct.APB2CLKDivider = RCC_HCLK_DIV1;

  if (HAL_RCC_ClockConfig(&RCC_ClkInitStruct, FLASH_LATENCY_1) != HAL_OK)
  {
    Error_Handler();
  }
  PeriphClkInit.PeriphClockSelection = RCC_PERIPHCLK_USART2;
  PeriphClkInit.Usart2ClockSelection = RCC_USART2CLKSOURCE_PCLK1;
  if (HAL_RCCEx_PeriphCLKConfig(&PeriphClkInit) != HAL_OK)
  {
    Error_Handler();
  }
  /** Configure the main internal regulator output voltage
  */
  if (HAL_PWREx_ControlVoltageScaling(PWR_REGULATOR_VOLTAGE_SCALE1) != HAL_OK)
  {
    Error_Handler();
  }
  /** Enable MSI Auto calibration
  */
  HAL_RCCEx_EnableMSIPLLMode();
}

/**
  * @brief TIM1 Initialization Function
  * @param None
  * @retval None
  */
static void MX_TIM1_Init(void)
{

  /* USER CODE BEGIN TIM1_Init 0 */

  /* USER CODE END TIM1_Init 0 */

  TIM_MasterConfigTypeDef sMasterConfig = {0};
  TIM_OC_InitTypeDef sConfigOC = {0};
  TIM_BreakDeadTimeConfigTypeDef sBreakDeadTimeConfig = {0};

  /* USER CODE BEGIN TIM1_Init 1 */

  /* USER CODE END TIM1_Init 1 */
  htim1.Instance = TIM1;
  htim1.Init.Prescaler = 0;
  htim1.Init.CounterMode = TIM_COUNTERMODE_UP;
  htim1.Init.Period = 65535;
  htim1.Init.ClockDivision = TIM_CLOCKDIVISION_DIV1;
  htim1.Init.RepetitionCounter = 0;
  htim1.Init.AutoReloadPreload = TIM_AUTORELOAD_PRELOAD_DISABLE;
  if (HAL_TIM_PWM_Init(&htim1) != HAL_OK)
  {
    Error_Handler();
  }
  sMasterConfig.MasterOutputTrigger = TIM_TRGO_RESET;
  sMasterConfig.MasterOutputTrigger2 = TIM_TRGO2_RESET;
  sMasterConfig.MasterSlaveMode = TIM_MASTERSLAVEMODE_DISABLE;
  if (HAL_TIMEx_MasterConfigSynchronization(&htim1, &sMasterConfig) != HAL_OK)
  {
    Error_Handler();
  }
  sConfigOC.OCMode = TIM_OCMODE_PWM1;
  sConfigOC.Pulse = 0;
  sConfigOC.OCPolarity = TIM_OCPOLARITY_HIGH;
  sConfigOC.OCNPolarity = TIM_OCNPOLARITY_HIGH;
  sConfigOC.OCFastMode = TIM_OCFAST_DISABLE;
  sConfigOC.OCIdleState = TIM_OCIDLESTATE_RESET;
  sConfigOC.OCNIdleState = TIM_OCNIDLESTATE_RESET;
  if (HAL_TIM_PWM_ConfigChannel(&htim1, &sConfigOC, TIM_CHANNEL_1) != HAL_OK)
  {
    Error_Handler();
  }
  sBreakDeadTimeConfig.OffStateRunMode = TIM_OSSR_DISABLE;
  sBreakDeadTimeConfig.OffStateIDLEMode = TIM_OSSI_DISABLE;
  sBreakDeadTimeConfig.LockLevel = TIM_LOCKLEVEL_OFF;
  sBreakDeadTimeConfig.DeadTime = 0;
  sBreakDeadTimeConfig.BreakState = TIM_BREAK_DISABLE;
  sBreakDeadTimeConfig.BreakPolarity = TIM_BREAKPOLARITY_HIGH;
  sBreakDeadTimeConfig.BreakFilter = 0;
  sBreakDeadTimeConfig.Break2State = TIM_BREAK2_DISABLE;
  sBreakDeadTimeConfig.Break2Polarity = TIM_BREAK2POLARITY_HIGH;
  sBreakDeadTimeConfig.Break2Filter = 0;
  sBreakDeadTimeConfig.AutomaticOutput = TIM_AUTOMATICOUTPUT_DISABLE;
  if (HAL_TIMEx_ConfigBreakDeadTime(&htim1, &sBreakDeadTimeConfig) != HAL_OK)
  {
    Error_Handler();
  }
  /* USER CODE BEGIN TIM1_Init 2 */

  /* USER CODE END TIM1_Init 2 */
  HAL_TIM_MspPostInit(&htim1);

}

/**
  * @brief USART2 Initialization Function
  * @param None
  * @retval None
  */
static void MX_USART2_UART_Init(void)
{

  /* USER CODE BEGIN USART2_Init 0 */

  /* USER CODE END USART2_Init 0 */

  /* USER CODE BEGIN USART2_Init 1 */

  /* USER CODE END USART2_Init 1 */
  huart2.Instance = USART2;
  huart2.Init.BaudRate = 115200;
  huart2.Init.WordLength = UART_WORDLENGTH_8B;
  huart2.Init.StopBits = UART_STOPBITS_1;
  huart2.Init.Parity = UART_PARITY_NONE;
  huart2.Init.Mode = UART_MODE_TX_RX;
  huart2.Init.HwFlowCtl = UART_HWCONTROL_NONE;
  huart2.Init.OverSampling = UART_OVERSAMPLING_16;
  huart2.Init.OneBitSampling = UART_ONE_BIT_SAMPLE_DISABLE;
  huart2.AdvancedInit.AdvFeatureInit = UART_ADVFEATURE_NO_INIT;
  if (HAL_UART_Init(&huart2) != HAL_OK)
  {
    Error_Handler();
  }
  /* USER CODE BEGIN USART2_Init 2 */

  /* USER CODE END USART2_Init 2 */

}

/**
  * @brief GPIO Initialization Function
  * @param None
  * @retval None
  */
static void MX_GPIO_Init(void)
{
  GPIO_InitTypeDef GPIO_InitStruct = {0};

  /* GPIO Ports Clock Enable */
  __HAL_RCC_GPIOC_CLK_ENABLE();
  __HAL_RCC_GPIOA_CLK_ENABLE();
  __HAL_RCC_GPIOB_CLK_ENABLE();

  /*Configure GPIO pin Output Level */
  HAL_GPIO_WritePin(LD3_GPIO_Port, LD3_Pin, GPIO_PIN_RESET);

  /*Configure GPIO pin : LD3_Pin */
  GPIO_InitStruct.Pin = LD3_Pin;
  GPIO_InitStruct.Mode = GPIO_MODE_OUTPUT_PP;
  GPIO_InitStruct.Pull = GPIO_NOPULL;
  GPIO_InitStruct.Speed = GPIO_SPEED_FREQ_LOW;
  HAL_GPIO_Init(LD3_GPIO_Port, &GPIO_InitStruct);

}

/* USER CODE BEGIN 4 */


/* USER CODE END 4 */

/**
  * @brief  This function is executed in case of error occurrence.
  * @retval None
  */
void Error_Handler(void)
{
  /* USER CODE BEGIN Error_Handler_Debug */
	/* User can add his own implementation to report the HAL error return state */

  /* USER CODE END Error_Handler_Debug */
}

#ifdef  USE_FULL_ASSERT
/**
  * @brief  Reports the name of the source file and the source line number
  *         where the assert_param error has occurred.
  * @param  file: pointer to the source file name
  * @param  line: assert_param error line source number
  * @retval None
  */
void assert_failed(uint8_t *file, uint32_t line)
{
  /* USER CODE BEGIN 6 */
  /* User can add his own implementation to report the file name and line number,
     tex: printf("Wrong parameters value: file %s on line %d\r\n", file, line) */
  /* USER CODE END 6 */
}
#endif /* USE_FULL_ASSERT */

/************************ (C) COPYRIGHT STMicroelectronics *****END OF FILE****/
