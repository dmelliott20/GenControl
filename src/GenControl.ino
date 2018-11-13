#include <DS3231.h>
#include <Wire.h>
#include <LiquidCrystal_I2C.h>
LiquidCrystal_I2C lcd(0x27, 20, 4); // set the LCD address to 0x27 for a 16 chars and 2 line display
DS3231 Clock;
bool Century = false;
bool h12;
bool PM;
byte errorlimit = 3;
byte errors = 0;
unsigned int ignition = 36;
unsigned int start = 38;
unsigned int stopgen = 40;
unsigned int output = 42;
unsigned int trigger = 44;
unsigned int reset = 52;
unsigned int button_select = 22;
unsigned int button_ok = 24;
int mode = 1;
//mode values
//0 = manual
//1 - Auto With Night mode
//2 - Auto Without Night mode
String modeline;
String genline;
String errline;
String testline;


void setup()
{
  Serial.begin(9600);
  pinMode(ignition, OUTPUT);
  digitalWrite(ignition, HIGH);
  pinMode(stopgen, OUTPUT);
  digitalWrite(stopgen, HIGH);
  pinMode(start, OUTPUT);
  digitalWrite(start, HIGH);
  pinMode(output, INPUT_PULLUP);
  pinMode(trigger, INPUT_PULLUP);
  pinMode(reset, INPUT_PULLUP);
  pinMode(button_select, INPUT_PULLUP);
  pinMode(button_ok, INPUT_PULLUP);
  Wire.begin();
  lcd.init();
  lcd.backlight();
  lcd.setCursor(0, 0);
  Serial.print("loading......");
  lcd.print("loading......");
  digitalWrite(ignition, HIGH);
  lcdupdateerrline("None");
  lcdupdategenline("Idle");
  lcdupdatetestline("N/A");
  //if (!TestGenerator(30)) halt("Test Fail");
}

void loop()
{
  ReadPinDB(output);
  if (errors < errorlimit)
  {
    switch (mode)
    {
      //manaul mode
      case 0:
        lcdupdatemodeline("Manual");
        break;
      //auto with night
      case 1:
        if (Clock.getHour(h12, PM) < 8 || Clock.getHour(h12, PM) > 19)
        {
         lcdupdatemodeline("Auto QT*");
          //if its QT then execute this code
        }
        else
        {
          lcdupdatemodeline("Auto QT");
          lcdupdategenline("Idle");
          if (ReadPinDB(trigger) == LOW)
          {
            lcdupdatemodeline("Running");
            if (StartGenerator(false))
            {
              seriallog("Generator Started");
              long startTime = millis();
              while (ReadPinDB(trigger) == LOW)
              {
                if (ReadPinDB(output) == HIGH)
                {
                  seriallog("Output Dropped");
                  errors++;
                  lcdupdateerrline("Output Drp");
                  break;
                }
                lcdupdategenline("Running  " + getelapsedtime(startTime));
                delay(500);
              }
              StopGenerator();
            }
            else
            {
              seriallog("Generator Failed to Start");
              lcdupdateerrline("Start Fail");
              StopGenerator();
            }
          }

        }
        break;

      //Auto Full Mode
      case 2:
        lcdupdatemodeline("AutoFull");
        lcdupdategenline("Idle");
        if (ReadPinDB(trigger) == LOW)
        {
          lcdupdatemodeline("Running");
          if (StartGenerator(false))
          {
            seriallog("Generator Started");
            long startTime = millis();
            while (ReadPinDB(trigger) == LOW)
            {
              if (ReadPinDB(output) == HIGH)
              {
                seriallog("Output Dropped");
                errors++;
                lcdupdateerrline("Output Drp");
                break;
              }
              lcdupdategenline("Running  " + getelapsedtime(startTime));
              delay(500);
            }
            StopGenerator();
          }
          else
          {
            seriallog("Generator Failed to Start");
            lcdupdateerrline("Start Fail");
            StopGenerator();
          }
        }
        break;
    }


    //if under the error threshold


  }
  else
  {
    //lockout

    halt("Lockout");
  }
  //auto testing of generator
  if (Clock.getDoW() == 7 && Clock.getHour(h12, PM) == 17 && Clock.getMinute() == 0)
  {
    if (!TestGenerator(300)) halt("Test Fail");
  }

  //check for entering of admin menu
  if (ReadPinDB(button_ok) == LOW)
  {
    adminmenu();
  }

  delay(200);
}

void lcdupdatemodeline(String mode)
{
  mode.remove(9);
  while (mode.length() < 9)
  {
    mode = mode + " ";
  }
  lcd.setCursor(0, 0);
  char buf[21];
  sprintf(buf, "%02d:%02d", Clock.getHour(h12, PM), Clock.getMinute());
  String text = "Mode: " + mode + String(buf);
  modeline = text;
  lcd.print(text);
}

void lcdupdategenline(String text)
{
  text.remove(14);
  while (text.length() < 14)
  {
    text = text + " ";
  }
  lcd.setCursor(0, 1);
  text = "Gen : " + text;
  genline = text;
  lcd.print(text);
}

void lcdupdatetestline(String text)
{
  text.remove(14);
  while (text.length() < 14)
  {
    text = text + " ";
  }
  lcd.setCursor(0, 3);
  text = "Test: " + text;
  testline = text;
  lcd.print(text);
}

void lcdupdateerrline(String text)
{
  text.remove(11);
  while (text.length() < 11)
  {
    text = text + " ";
  }
  lcd.setCursor(0, 2);
  text = "Err : " + text + " E" + String(errors);
  errline = text;
  lcd.print(text);
}

void lcdupdateline(int line, String text)
{
  text.remove(20);
  while (text.length() < 20)
  {
    text = text + " ";
  }
  switch (line)
  {
    case 0:
      lcd.setCursor(0, 0);
      lcd.print(text);
      break;
    case 1:
      lcd.setCursor(0, 1);
      lcd.print(text);
      break;
    case 2:
      lcd.setCursor(0, 2);
      lcd.print(text);
      break;
    case 3:
      lcd.setCursor(0, 3);
      lcd.print(text);
      break;
  }

}

bool StartGenerator(bool isTest)
{
  seriallog("StartGenerator() Called");
  if (errors < errorlimit)
  {
    seriallog("Error Counter @ " + String(errors) + "\\" + String(errorlimit));
    lcdupdategenline("Starting...");
    seriallog("Switching on ignition");
    digitalWrite(ignition, LOW);
    delay(2000);
    for (int x = 1; x < 4; x++)
    {
      if (isTest == false && ReadPinDB(trigger) == HIGH) break;
      long crankstarttime = millis();
      seriallog("Crank Start Time " + String(millis()));
      long crankendtime = crankstarttime + 15000;
      long crankmidtime = crankstarttime + 10000;
      seriallog("Crank Attempt " + String(x));
      lcdupdategenline("Crank Att " + String(x));
      if (x != 1)
      {
        seriallog("Waiting 5 seconds");
        delay(5000);
      }
      seriallog("Cranking...");
      digitalWrite(start, LOW);
      while (ReadPinDB(output) == HIGH && crankendtime > millis())
      {
        if (crankmidtime < millis())
        {
          lcdupdategenline("Wait for Power");
          digitalWrite(start, HIGH);
        }
      }
      digitalWrite(start, HIGH);
      delay(200);
      if (ReadPinDB(output) == LOW)
      {
        return true;
      }
    }
    lcdupdategenline("Idle");
    errors++;
    lcdupdateerrline("Start Fail");
    return false;
  }
  else
  {
    //lockout
  }
}

bool StopGenerator()
{
  seriallog("StopGenerator() Called");
  lcdupdategenline("Stopping...");
  digitalWrite(ignition, HIGH);
  if (ReadPinDB(output) == LOW)
  {
    digitalWrite(stopgen, LOW);
    do {
      lcdupdategenline("Wait 4 Pwr Drop");
    } while (ReadPinDB(output) == LOW);
    lcdupdategenline("Pwr Dropped");
    delay(2000);
    digitalWrite(stopgen, HIGH);

  }
  lcdupdategenline("Stopped");
}

bool TestGenerator(long runtime) {
  seriallog("TestGenerator() Called");
  long testlenght = runtime * 1000;
  seriallog("Test Lenght:" + String(testlenght));
  lcdupdatemodeline("Test Run");
  if (StartGenerator(true))
  {
    long startTime = millis();
    long endTime = startTime + testlenght;
    while (endTime > millis())
    {
      delay(1000);
      seriallog("Testing  " + getelapsedsecs(startTime));
      lcdupdategenline("Testing  " + getelapsedsecs(startTime));
      if (ReadPinDB(output) == HIGH)
      {
        StopGenerator();
        lcdupdatetestline("Fail " + String(Clock.getDate()) + "/"  + String(Clock.getMonth(Century)));
        return false;
      }
    }
    StopGenerator();
    lcdupdatetestline("Success " + String(Clock.getDate()) + "/"  + String(Clock.getMonth(Century)));
    return true;
  }
  else
  {
    StopGenerator();
    lcdupdatetestline("Fail " + String(Clock.getDate()) + "/"  + String(Clock.getMonth(Century)));
    return false;
  }
}



String getelapsedtime(long startTime)
{
  unsigned long elapsedTime = millis() - startTime;
  unsigned long allSeconds = elapsedTime / 1000;
  int runHours = allSeconds / 3600;
  int secsRemaining = allSeconds % 3600;
  int runMinutes = secsRemaining / 60;
  int runSeconds = secsRemaining % 60;
  char buf[21];
  sprintf(buf, "%02d:%02d", runHours, runMinutes);
  return String(buf);
}
String getelapsedsecs(long startTime)
{
  unsigned long elapsedTime = millis() - startTime;
  unsigned long allSeconds = elapsedTime / 1000;
  int runHours = allSeconds / 3600;
  int secsRemaining = allSeconds % 3600;
  int runMinutes = secsRemaining / 60;
  int runSeconds = secsRemaining % 60;
  char buf[21];
  sprintf(buf, "%02d:%02d", runMinutes, runSeconds);
  return String(buf);
}

void seriallog(String text)
{
  Serial.println(text);
}

void halt(String text)
{
  StopGenerator();
  lcdupdateerrline(text);
  lcdupdatemodeline("Halt!");
  while (1) {
    lcdupdatemodeline("Halt!");
    if (ReadPinDB(button_ok) == LOW)
    {
      errors = 0;
      lcdupdateerrline("Errors Reset");
      break;
    }
    delay(1000);
  }
}

void adminmenu()
{
  int menupos = 1;
  lcd.clear();
  lcdupdateline(0, "Admin Menu");
  delay(500);
  while (1) {
    if (menupos == 1) {
      lcdupdateline(1, ">Set Mode");
    } else {
      lcdupdateline(1, " Set Mode");
    }
    if (menupos == 2) {
      lcdupdateline(2, ">Run Gen Test");
    } else {
      lcdupdateline(2, " Run Gen Test");
    }
    if (menupos == 3) {
      lcdupdateline(3, ">Exit");
    } else {
      lcdupdateline(3, " Exit");
    }
    if (ReadPinDB(button_select) == LOW)
    {
      if (menupos == 3)
      {
        menupos = 1;
      }
      else
      {
        menupos++;
      }
      delay(500);
    }
    if (ReadPinDB(button_ok) == LOW)
    {
      break;
    }
  }
  switch (menupos)
  {
    case 1:
      modemenu();
      break;
    case 2:
      testmenu();      
      break;
    case 3:
      break;
  }
  returnlcd();
}

void modemenu()
{
  int menupos = 1;
  lcd.clear();
  lcdupdateline(0, "Admin Menu");
  delay(500);
  while (1) {
    if (menupos == 1) {
      lcdupdateline(1, ">Auto With QT");
    } else {
      lcdupdateline(1, " Auto With QT");
    }
    if (menupos == 2) {
      lcdupdateline(2, ">Auto All");
    } else {
      lcdupdateline(2, " Auto All");
    }
    if (menupos == 3) {
      lcdupdateline(3, ">Manual");
    } else {
      lcdupdateline(3, " Manual");
    }
    if (ReadPinDB(button_select) == LOW)
    {
      if (menupos == 3)
      {
        menupos = 1;
      }
      else
      {
        menupos++;
      }
      delay(500);
    }
    if (ReadPinDB(button_ok) == LOW)
    {
      break;
    }
  }
  switch (menupos)
  {
    case 1:
      mode = 1;
      break;
    case 2:
      mode = 2;
      break;
    case 3:
      mode = 0;
      break;
  }
}

void testmenu()
{
  int menupos = 1;
  lcd.clear();
  lcdupdateline(0, "Test Menu");
  delay(500);
  while (1) {
    if (menupos == 1) {
      lcdupdateline(1, ">Test - 15secs");
    } else {
      lcdupdateline(1, " Test - 15secs");
    }
    if (menupos == 2) {
      lcdupdateline(2, ">Test - 3 Mins");
    } else {
      lcdupdateline(2, " Test - 3 Mins");
    }
    if (menupos == 3) {
      lcdupdateline(3, ">Test - 15 Mins");
    } else {
      lcdupdateline(3, " Test - 15 Mins");
    }
    if (ReadPinDB(button_select) == LOW)
    {
      if (menupos == 3)
      {
        menupos = 1;
      }
      else
      {
        menupos++;
      }
      delay(500);
    }
    if (ReadPinDB(button_ok) == LOW)
    {
      break;
    }
  }
  switch (menupos)
  {
    case 1:
      lcd.clear();
      if (!TestGenerator(15)) halt("Test Fail");
      break;
    case 2:
      lcd.clear();
      if (!TestGenerator(180)) halt("Test Fail");
      break;
    case 3:
      lcd.clear();
      if (!TestGenerator(900)) halt("Test Fail");
      break;
  }
}

void returnlcd()
{
  lcdupdateline(1, genline);
  lcdupdateline(2, errline);
  lcdupdateline(3, testline);
}

bool ReadPinDB(int inputpin)
{
  while(1)
  {
    int readvalues =0;
    for (int i = 0; i < 5; i++) 
    {
      delay(2);
      readvalues += digitalRead(inputpin);
    }
    if (readvalues==0)
    {
      return LOW;
    }
    if (readvalues==5)
    {
      return HIGH;
    }
    Serial.println("Bounce Detected - re-reading");
  }
}
