
#include <Arduino.h>
#include <CAN.h>
#include "FS.h"
#include <LITTLEFS.h>
#include <WiFi.h>
#include <WiFiClient.h> 
#include "ESP32_FTPClient.h"

#define VERSION "Beta 0.3"

//..#ifdef ARDUINO_ARCH_ESP32
//..#error ARDUINO_ARCH_ESP32
//..#else
//..#error NO_ARDUINO_ARCH_ESP32
//..#endif

#define WIFI_SSID "XXXXXXX"         // Insert your WiFi SSID and password here
#define WIFI_PASS "XXXXXXX"

// Set your Static IP address
IPAddress local_IP(10, 0, 0, 0);    // Insert your correct IP addresses in the following
// Set your Gateway IP address
IPAddress gateway(10, 0, 0, 0);
IPAddress subnet(255, 255, 0, 0);
IPAddress primaryDNS(10, 0, 0, 0);
IPAddress secondaryDNS(10, 0, 0, 0);

char ftp_server[] = "10.0.0.XXX";   // Insert your ftp server IP here
char ftp_user[]   = "XXXXXXX";      // Insert your ftp server username here
char ftp_pass[]   = "XXXXX";        // Insert your ftp server password here

// you can pass a FTP timeout and debbug mode on the last 2 arguments
ESP32_FTPClient ftp (ftp_server,ftp_user,ftp_pass, 5000, 2);

#define FORMAT_LITTLEFS_IF_FAILED true

const char *ntpServer = "pool.ntp.org";
const long  gmtOffset_sec = 7200;
const int   daylightOffset_sec = 0;

// Set web server port number to 80
WiFiServer server(80);

// Variable to store the HTTP request
String header;
// Auxiliar variables to store the current output state
String LoggingState = "OFF";
String FtpTxState = "OFF";
String WiFiRSSIvalue = "0";
String ChargeLevelStr = "0"; 
String CurrentLevelStr = "0"; 
String VoltageLevelStr = "0"; 
String CountStr = "0"; 
int RSSIval = 1;
int TankLevel = 1;

// Current time
unsigned long currentTime = millis();
// Previous time
unsigned long previousTime = 0; 
// Define timeout time in milliseconds (example: 2000ms = 2s)
const long timeoutTime = 2000;

/*
DEV-Kit V1 Pins :
	3V3	
	GND
	15
	02
	04
	RX2
	TX2
	05
	18
	19
	21
	RDX
	TDX
	22
	23
*/	
#define TX_GPIO_NUM   19
#define RX_GPIO_NUM   18
//#define TX_GPIO_NUM   5
//#define RX_GPIO_NUM   4
//#define TX_GPIO_NUM   17
//#define RX_GPIO_NUM   16

char Unknowns[188] = "";
int LoggingOn = 0;
time_t LogStartTime;
File LogFile;
File ReadFile;
int LogFileSize;
char TxBuffer[1500];
int FtpSendDone = 1;
unsigned short BatteryVoltage_1     ;
short BatteryCurrent_1     ;
unsigned short BatteryVoltage_2     ;
short BatteryCurrent_2     ;
unsigned short BatteryTemperature_1 ;
unsigned short BatteryTemperature_2 ;
unsigned short SOC_1                ;
unsigned short SOH_1                ;
unsigned short SOC_2 ;
unsigned short SOH_2 ;
unsigned char  ChargingProcedure    ;
unsigned char  OperatingState       ;
unsigned short ActiveError          ;
unsigned short ChargeVoltageSetPoint;
unsigned short ChargeVoltage        ;
short MaxChargingCurrent    ;
short MaxDischargingCurrent ;
unsigned short DischargeVoltageLimit ;
unsigned short SOCHighDefinition ;
unsigned char  Alarm_1   ;
unsigned char  Alarm_2   ;
unsigned char  Alarm_3   ;
unsigned char  Alarm_4   ;
unsigned char  Warning_1 ;
unsigned char  Warning_2 ;
unsigned char  Warning_3 ;
unsigned char  Warning_4 ;
char ManufacturerDescription[9];
unsigned short CellChemistry    ;
unsigned short HardwareVersion  ;
unsigned short CapacityLow      ;
unsigned short SoftwareVersion  ;
int RxCanIds = 0;

typedef struct 
{
  int Ofset;
  int Size;
  float Scale;
  const char *Unit;
  const char *Description;
  void *Address;
}CAN_ID ;

typedef struct 
{
  int Id;
  CAN_ID Entry[9];
  unsigned char Data[8];
}Can_Id ;

/*
Pylontech CAN Message ID & contents:- 
0x351: Charge and Discharge parameters = (max charge voltage, charge current, discharge current, discharge voltage (latter is not used by Solis)) 
0x355: SOC & SOH 
0x356: Current measurement of Battery Voltage, Current & Temp 
0x359: Protection & Alarm flags 
0x35C: Battery charge request flags (which I think are ignored by Solis; I don't use them) 
0x35E: Manufacturer name ("PYLON ") = ASCII "PYLON" followed by 3 spaces. 

   0x305;  //DEZ = 773 CNA_send_Network_alive_msg
   0X306,
   0X30F,
   0x351;  //DEZ = 849 Battery_limits
   0x355;  //DEZ = 853 Battery_SoC_SoH
   0x356;  //DEZ = 854 Battery_actual_values_UIt() 
   0x359;  //DEZ = 857 Battery_Error_Warnings, Bytes 0 and 2 have flags for error conditions 
   0X35A, 
   0x35C;  //DEZ = 860 Battery_Request
   0x35E;  //DEZ = 862 Battery_Manufacturer
   0X35F, 
*/

Can_Id CanId[] = 
{
///////// DATA ////////
  // ID 0X305 
  // 0 Battery voltage low byte Signed LSB = 0.01 V      total_voltage         
  // 1 Battery voltage high byte integer                                       
  // 2 Battery current low byte Signed LSB = 0.1 A       current               
  // 3 Battery current high byte integer                                       
  // 4 Battery temperature low byte Signed LSB = 0.1 °C  power_tube_temperature
  // 5 Battery temperature high byte integer
  // 6 SOC low byte                  Unsigned integer LSB = 1 %     capacity_remaining            
  // 7 SOC high byte                                                                              
  {0X305, 
   {
    {0, 2, 0.01, "V", "Battery voltage",       &BatteryVoltage_1     },
    {2, 2, 0.1,  "A", "Battery current",       &BatteryCurrent_1     },
    {4, 2, 0.1,  "V", "Battery temperature",   &BatteryTemperature_1 },
    {6, 2, 1,    "%", "SOC_1",                 &SOC_1                },
    {-1, 0, 0, NULL, NULL, NULL}
   }
  },
  // 
  // ID 0X306
  // 0 SOH low byte                  Unsigned integer LSB = 1 %     No mapping 100?               
  // 1 SOH high byte                                                                              
  // 2 Charging procedure
  // 3 Operating state
  // 4 Active error
  // 5
  // 6 Battery Charge Voltage set-point
  // 7
  {0X306,
   {
    {0, 2, 1, "%", "SOH_1",                     &SOH_1                },
    {2, 1, 1, "",  "Charging procedure",        &ChargingProcedure    },
    {3, 1, 1, "",  "Operating state",           &OperatingState       },
    {4, 2, 1, "",  "Active error",              &ActiveError          },
    {6, 2, 1, "V", "Charge Voltage set-point",  &ChargeVoltageSetPoint},
    {-1, 0, 0, NULL, NULL, NULL}
   }
  },
   
  // ID 0X30F ?
  {0X30F,
   {
    {-1, 0, 0, NULL, NULL, NULL}
   }
  },
  // 
  // ID 351
  // 0 Charge voltage          low byte   Unsigned integer LSB = 0.1 V No mapping?? Static charge_v          
  // 1 Charge voltage          high byte                                                                     
  // 2 Max charging current    low byte   Signed integer   LSB = 0.1 A charging_ overcurrent_protection      
  // 3 Max charging current    high byte                                                                     
  // 4 Max discharging current low byte   Signed integer   LSB = 0.1 A discharging_ overcurrent_protection   
  // 5 Max discharging current high byte                                                                     
  // 6 Discharge voltage limit low byte   Unsigned integer LSB = 0.1 V total_voltage_ undervoltage_protection
  // 7 Discharge voltage limit high byte
  // 
  {0X351,
   {
    {0, 2, 0.1, "V", "Charge voltage",            &ChargeVoltage        },
    {2, 2, 0.1, "A", "Max charging current",      &MaxChargingCurrent    },
    {4, 2, 0.1, "A", "Max discharging current",   &MaxDischargingCurrent },
    {6, 2, 0.1, "V", "Discharge voltage limit",   &DischargeVoltageLimit },
    {-1, 0, 0, NULL, NULL, NULL}
   }
  },
   
  // ID 355 
  // 0 SOC low byte                  Unsigned integer LSB = 1 %     capacity_remaining            
  // 1 SOC high byte                                                                              
  // 2 SOH low byte                  Unsigned integer LSB = 1 %     No mapping 100?               
  // 3 SOH high byte                                                                              
  // 4 SOC high definition low byte  Unsigned integer LSB = 0.01 %  Optional                      
  // 5 SOC high definition high byte                                raw_battery_remaining_capacity
  // 
  {0X355, 
   {
    {0, 2, 1,    "%", "SOC_2",                    &SOC_2 },
    {2, 2, 1,    "%", "SOH_2",                    &SOH_2 },
    {4, 3, 0.01, "%", "SOC high definition",      &SOCHighDefinition },
    {-1, 0, 0, NULL, NULL, NULL}
   }
  },
   
  // ID 356 
  // 0 Battery voltage low byte Signed LSB = 0.01 V      total_voltage         
  // 1 Battery voltage high byte integer                                       
  // 2 Battery current low byte Signed LSB = 0.1 A       current               
  // 3 Battery current high byte integer                                       
  // 4 Battery temperature low byte Signed LSB = 0.1 °C  power_tube_temperature
  // 5 Battery temperature high byte integer
  // 
  {0X356, 
   {
    {0, 2, 0.01, "V", "Battery voltage",         &BatteryVoltage_2     },
    {2, 2, 0.1,  "A", "Battery current",         &BatteryCurrent_2     },
    {4, 2, 0.1,  "C", "Battery temperature",     &BatteryTemperature_2 },
    {-1, 0, 0, NULL, NULL, NULL}
   }
  },
  // 
  // ID 0X359 
  //  Byte Number Name Description
  //  0 Protection Byte 1 See table 1 for bitfield settings
  //  1 Protection Byte 2 See table 2 for bitfield settings
  //  2 Alarm Byte 1 See table 3 for bitfield settings
  //  3 Alarm Byte 2 See table 4 for bitfield settings
  //  4 Module Number 8-bit integer representing quantity of parallel
  //  connected batteries.
  //  5 “P”, 0x50
  //  6 “N”, 0x4E
  //  7 Reserved Unused: byte should be “00”
  //Table 1 - Protection Byte 1 Bitfield: (If a bit is set, one of these caused batt self-protection mode)
  //  Bit 7 
  //    Discharge
  //    Over-Current
  //  Bit 6 
  //    N/A 
  //  Bit 5 
  //    N/A 
  //  Bit 4 
  //    Cell Under-
  //    Temp
  //  Bit 3 
  //    Cell Over-
  //    Temp
  //  Bit 2 
  //    Cell/Module
  //    Under-Voltage
  //  Bit 1 
  //    Cell/Module
  //    Over-Voltage
  //  Bit 0
  //    N/A
  //Table 2 – Protection Byte 2 Bitfield:
  //    Bit 7 
  //      N/A 
  //    Bit 6 
  //      N/A 
  //    Bit 5 
  //      N/A 
  //    Bit 4 
  //      N/A 
  //    Bit 3 
  //      System Error 
  //    Bit 2 
  //      N/A 
  //    Bit 1 
  //      N/A 
  //    Bit 0
  //      Charge Over-Current
  //Table 3 – Alarm Byte 1 Bitfield: (Real Time Status)
  //    Bit 7 
  //      Discharge
  //      High Current
  //    Bit 6 
  //      N/A 
  //    Bit 5 
  //      N/A 
  //    Bit 4 
  //      Cell Low
  //      Temp
  //    Bit 3 
  //      Cell High
  //      Temp
  //    Bit 2 
  //      Cell or Module
  //      Low Voltage
  //    Bit 1 
  //      Cell / Module
  //      High Voltage
  //    Bit 0
  //      N/A
  //Table 4 – Alarm Byte 2 Bitfield: (Real Time Status)
  //  Bit 7 
  //    N/A 
  //  Bit 6 
  //    N/A 
  //  Bit 5 
  //    N/A 
  //  Bit 4 
  //    N/A 
  //  Bit 3 
  //    Critical System Error 
  //  Bit 2 
  //    N/A 
  //  Bit 1 
  //    N/A 
  //  Bit 0
  //    Charge High Curre
  {0X359, 
   {
    {-1, 0, 0, NULL, NULL, NULL}
   }
  },
  // 
  // ID 35A 
  // 0 Alarm byte 1 char Unsigned
  // 1 Alarm byte 2 char              Bit orientated Alarm Unsigned structure
  // 2 Alarm byte 3 char Unsigned
  // 3 Alarm byte 4 char 
  // 4 1 char Warning byte Unsigned
  // 5 2 char Warning byte Unsigned  Bit orientated structure
  // 6 3 char Warning byte Unsigned
  // 7 4 char Warning byte Unsigned
  // 
  {0X35A, 
   {
    {0, 1, 1, "", "Alarm 1",    &Alarm_1   },
    {1, 1, 1, "", "Alarm 2",    &Alarm_2   },
    {2, 1, 1, "", "Alarm 3",    &Alarm_3   },
    {3, 1, 1, "", "Alarm 4",    &Alarm_4   },
    {4, 1, 1, "", "Warning 1",  &Warning_1 },
    {5, 1, 1, "", "Warning 2",  &Warning_2 },
    {6, 1, 1, "", "Warning 3",  &Warning_3 },
    {7, 1, 1, "", "Warning 4",  &Warning_4 },
    {-1, 0, 0, NULL, NULL, NULL}
   }
  },
  // 
  // ID35C 40 0B 00 00 00 00 00 00
  //
  // ID 0X35C  DEYE
  // Byte 0 =  BMS Request Flag Bitfield See table 5 below
  //  Bit Number Name Description
  //  0 Reserved Unused: bit should be “0”
  //  1 Reserved Unused: bit should be “0”
  //  2 Reserved Unused: bit should be “0”
  //  3 Full Charge Request
  //      1=charge; 0=normal
  //      Set if the battery has not been fully charged for a long
  //      time. Fully charging the battery allows the SOC
  //      calculation algorithm in the BMS to re-calibrate itself.
  //  4 Forced Charge Request 1
  //      1=charge; 0=normal
  //      Set when the battery reaches a low SoC threshold
  //      defined in the BMS itself.
  //      Immediately charge the battery system
  //      until these 2 flags = 0
  //  5 Forced Charge Request 2
  //      1=charge; 0=normal
  //  6 Discharge Enabled BMS sets when discharge from the 
  //      battery is allowed.
  //  7 Charge Enabled BMS sets when charging to the battery 
  //      is allowed.
  // Byte 1-7 reserved 
  {0X35C, 
   {
    {-1, 0, 0, NULL, NULL, NULL}
   }
  },
  // 
  // ID 0X35E
  // 0-8 string - Manufacturer description: GoodWe
  // 
  {0X35E,
   {
    {0, 0, 1, "", "Manufacturer",     &ManufacturerDescription},
    {-1, 0, 0, NULL, NULL, NULL}
   }
  },
  // 
  // ID 35F 
  // 0 Cell chemistry low byte     Unsigned integer Battey type  battery_type
  // 1 Cell chemistry high byte
  // 2 Hardware version low byte   Byte             HW Version:  maybe 1.0
  // 3 Hardware version high byte  Byte             “1.0”
  // 4 Capacity low byte           Unsigned integer LSB = 1 Ah   total_battery_capacity_setting
  // 5 Capacity high byte
  // 6 Software version low byte   Byte SW Version:              software_version
  // 7 Software version high byte  Byte “ 0.1”
  // 
  {0X35F, 
   {
    {0, 2, 1, "",   "Cell chemistry",           &CellChemistry    },
    {2, 2, 1, "",   "Hardware version",         &HardwareVersion  },
    {4, 2, 1, "Ah", "Capacity ",                &CapacityLow      },
    {6, 2, 1, "",   "Software version",         &SoftwareVersion  },
    {-1, 0, 0, NULL, NULL, NULL}
   }
  },

  // Terminator
  {-1,
   {
    {-1, 0, 0, NULL, NULL, NULL}
   }
  }
};   

void GetNetworkTime(void)
{  
struct tm new_ts;

  // Init and get the time
  configTime(gmtOffset_sec, daylightOffset_sec, ntpServer);
    //Get RTC time
  getLocalTime(&new_ts);
  Serial.print("Current time obtained from RTC after NTP config is: ");
  Serial.println(&new_ts, "%A, %B %d %Y %H:%M:%S");

}

void readFile(fs::FS &fs, const char * path)
{
    Serial.printf("Reading file: %s\n", path);

    File file = fs.open(path);
    if(!file || file.isDirectory()){
        Serial.println("- failed to open file for reading");
        return;
    }

    Serial.println("- read from file:");
    while(file.available()){
        Serial.write(file.read());
    }
    file.close();
}

void writeFile(fs::FS &fs, const char * path, const char * message)
{
    Serial.printf("Writing file: %s\n", path);

    File file = fs.open(path, FILE_WRITE);
    if(!file){
        Serial.println("- failed to open file for writing");
        return;
    }
    if(file.print(message)){
        Serial.println("- file written");
    } else {
        Serial.println("- write failed");
    }
    file.close();
}

void appendFile(fs::FS &fs, const char * path, const char * message)
{
    Serial.printf("Appending to file: %s\n", path);

    File file = fs.open(path, FILE_APPEND);
    if(!file){
        Serial.println("- failed to open file for appending");
        return;
    }
    if(file.print(message)){
        Serial.println("- message appended");
    } else {
        Serial.println("- append failed");
    }
    file.close();
}

void listDir(fs::FS &fs, const char * dirname, uint8_t levels)
{
    Serial.printf("Listing directory: %s\r\n", dirname);

    File root = fs.open(dirname);
    if(!root){
        Serial.println("- failed to open directory");
        return;
    }
    if(!root.isDirectory()){
        Serial.println(" - not a directory");
        return;
    }

    File file = root.openNextFile();
    while(file){
        if(file.isDirectory()){
            Serial.print("  DIR : ");
            Serial.println(file.name());
            if(levels){
                listDir(fs, file.name(), levels -1);
            }
        } else {
            Serial.print("  FILE: ");
            Serial.print(file.name());
            Serial.print("\tSIZE: ");
            Serial.println(file.size());
        }
        file = root.openNextFile();
    }
}

void setup() 
{
  Serial.begin (115200);
  while (!Serial);
  delay (1000);

  Serial.println ("CAN Receiver");

  if(!LITTLEFS.begin(FORMAT_LITTLEFS_IF_FAILED))
  {
      Serial.println("LITTLEFS Mount Failed");
      return;
  }

  // Setup the CAN 
  CAN.setPins (RX_GPIO_NUM, TX_GPIO_NUM);

  // start the CAN bus at 500 kbps
  if (!CAN.begin(500E3)) 
  {
    Serial.println("Starting CAN failed!");
    while (1);
  }
//  sleep(1);
//  CAN.filter(0X356, 0XFFF);
//  sleep(1);
  // Configures static IP address
  if (!WiFi.config(local_IP, gateway, subnet, primaryDNS, secondaryDNS)) {
    Serial.println("STA Failed to configure");
  }
  //WiFi.mode(WIFI_STA);
  // Connect to Wi-Fi network with SSID and password
  Serial.print("Connecting to ");
  Serial.println(WIFI_SSID);
  WiFi.begin( WIFI_SSID, WIFI_PASS );
  while (WiFi.status() != WL_CONNECTED) 
  {
    Serial.print('.');
    delay(1000);
  }
  Serial.printf("\nIP = ");
  Serial.println(WiFi.localIP());
  Serial.printf("RSSI = ");
  Serial.println(WiFi.RSSI());  // Print WiFi signal strength
  RSSIval = WiFi.RSSI();

  server.begin();
  
}

void PackData(int packetId, unsigned char data[])
{
Can_Id *Id;
int i = 0, j;
char unknown[8];

  Id = &CanId[0];
  while(Id->Id != -1)
  {
    if(packetId == Id->Id)
    {
      while(Id->Entry[i].Ofset != -1)
      {
        switch(Id->Entry[i].Size)
        {
          case 1:  // One byte value
            *((unsigned char *)Id->Entry[i].Address) = (unsigned char)(
          data[Id->Entry[i].Ofset]); 
            break;
  
          case 2:  // Two byte value
          case 3:  // High Definition short
            *((unsigned short *)Id->Entry[i].Address) = (unsigned short)( 
                 (unsigned short)(data[Id->Entry[i].Ofset + 1]) * 256 + data[Id->Entry[i].Ofset]); 
            break;
  
          case 0:  // NULL terminated string
            memcpy((unsigned char *)Id->Entry[i].Address, &data[0], 8); 
            break;
        }
        i++;
      }
      for(j = 0; j < 8; j++)    
        Id->Data[j] = data[j]; 
      return;
    }
    Id++;
  }
  sprintf(unknown, "0X%03X ", packetId);
  strcat(Unknowns, unknown);
}

void PrintAll(void)
{
Can_Id *Id;
int i = 0, j;
int cnt = 0;

  Id = &CanId[0];
  while(Id->Id != -1)
  {
//    if(Id->Entry[0].Ofset == -1)
//    {
      Serial.printf("ID%03X ", Id->Id);
      for(j = 0; j < 8; j++)
        Serial.printf("%02X ", Id->Data[j]); 
      Serial.printf("\n"); 
//      Id++;
//      continue;
//    }
    while(Id->Entry[i].Ofset != -1)
    {
      Serial.printf("%24s ", Id->Entry[i].Description);
      switch(Id->Entry[i].Size)
      {
      case 1:  // One byte value
        Serial.printf("%4.0f %-3s   ", 
          *((unsigned char *)Id->Entry[i].Address) * 
                             Id->Entry[i].Scale, 
                             Id->Entry[i].Unit) ; 
        break;

      case 2:  // Two byte value
        Serial.printf("%4.2f %-3s   ", 
          *((short *)Id->Entry[i].Address) * 
                              Id->Entry[i].Scale, 
                              Id->Entry[i].Unit) ; 
        break;

      case 3:  // High Definition short
        Serial.printf("%4.3f %-3s   ",  
          *((unsigned short *)Id->Entry[i].Address) * 
                              Id->Entry[i].Scale * 3, 
                              Id->Entry[i].Unit) ; 
        break;

      case 0:  // NULL terminated string
        Serial.printf("%s   ", (char *)Id->Entry[i].Address); 
        break;
      }
      if((cnt++ % 2) == 1)
        Serial.printf("\n"); 
      i++;
    }
    if((cnt % 2) == 1)
      Serial.printf("\n"); 
    i = 0;
    Id++;
  }
  if(strlen(Unknowns))
    Serial.printf("Unknown ID's : %s\n", Unknowns);
  Serial.printf("\n"); 
}

int ReadFileUpCall(unsigned char * Buffer, int Len)
{
int count;

  count = ReadFile.read(Buffer, Len);
  if(count < Len)
    FtpSendDone = 1;
  return(count);
}

void TransferLogFile(fs::FS &fs)
{
int len = 0;

  ReadFile = fs.open("/log.csv");
  if(!ReadFile)
  {
    Serial.println("- failed to open file for reading");
    return;
  }
  ftp.OpenConnection();
  ftp.InitFile("Type A");
  ftp.NewFile("log.csv");
  FtpSendDone = 0;
  ftp.WriteClientUpCall();
  if(FtpSendDone == 0)
  {
    Serial.println("Failed to ftp file");
    FtpSendDone = -1;
  }
  FtpTxState = "ON";
  ftp.CloseFile();
  ftp.CloseConnection();
}

void StartLogToFile(fs::FS &fs)
{
  Serial.printf("Writing file: log.csv\n");

  LogFile = fs.open("/log.csv", FILE_WRITE);
  if(!LogFile)
  {
    Serial.println("- failed to open file for writing");
    sleep(1);
    return;
  }
  LogFileSize = strlen("Time(S),Cur(A),Vol(V), SOC(%%)\n");
  if(LogFile.print(    "Time(S),Cur(A),Vol(V), SOC(%%)\n"))
  {
    Serial.printf("File written headder\n");
  } 
  else 
  {
    Serial.println("Log write failed");
  }
  LoggingOn = 1;
  LogStartTime = time(&LogStartTime);
}

void StopLogToFile(void)
{
  LoggingOn = 0;
  LogFile.close();
  Serial.printf("File closed, size = %d\n", LogFileSize);
}

void LogToFile(void)
{
char buffer[40];
time_t Now;

  // Time(S),Cur(A),Vol(V), SOC(%%),Cur_1(A),Vol_1(V), SOC_1(%%)\n
  sprintf(&buffer[0], "%d, %2.2f, %2.1f, %d, %2.2f, %2.1f, %d\n",
      time(&Now) - LogStartTime,
      BatteryCurrent_2  * 0.1,
      BatteryVoltage_2  * 0.01,
      SOC_2,
      BatteryCurrent_1  * 0.1,
      BatteryVoltage_1  * 0.01,
      SOC_1                  );
  LogFileSize += strlen(buffer);
  if(LogFile.print(buffer))
  {
    Serial.printf("File written, size = %d\n", LogFileSize);
  } 
  else 
  {
    Serial.println("Log write failed");
  }
}

void loop() 
{
int i, packetId, packetDlc ;
unsigned char data[8];
int packetSize;
static int lastLog   = millis();
static int lastPrint = millis();
 
WiFiClient client = server.available();   // Listen for incoming clients

  // try to parse packet
  packetSize = CAN.parsePacket();
  if (packetSize) 
  {
    packetId = CAN.packetId();
    if(packetId == 0X355)
      RxCanIds |= 0x01;
    if(packetId == 0X356)
      RxCanIds |= 0x02;
    //if(packetId == 0X351)
    //  RxCanIds |= 0x04;
    //if(packetId == 0X35F)
    //  RxCanIds |= 0x08;

    if (CAN.packetRtr()) 
    {
      Serial.print ("RTR Length ");
      packetDlc = CAN.packetDlc();
      Serial.println (packetDlc);
    } 
    else 
    {
      //Serial.printf("%X,", packetId);
      //Serial.print (" Length ");
      //Serial.println (packetSize);

      // only print packet data for non-RTR packets
      i = 0;
      while (CAN.available()) 
      {
        data[i] = CAN.read();
        //Serial.printf(" 0X%02X", data[i]);
        i++;
      }
      PackData(packetId, &data[0]);
    }
  }

  if (client) 
  {                             // If a new client connects,
    currentTime = millis();
    previousTime = currentTime;
    Serial.println("New Client.");          // print a message out in the serial port
    String currentLine = "";                // make a String to hold incoming data from the client
    while (client.connected() && currentTime - previousTime <= timeoutTime) 
    {  // loop while the client's connected
      currentTime = millis();
      if (client.available()) 
      {             // if there's bytes to read from the client,
        char c = client.read();             // read a byte, then
        Serial.write(c);                    // print it out the serial monitor
        header += c;
        if (c == '\n') 
        {                    // if the byte is a newline character
          // if the current line is blank, you got two newline characters in a row.
          // that's the end of the client HTTP request, so send a response:
          if (currentLine.length() == 0)
          {
            // HTTP headers always start with a response code (e.g. HTTP/1.1 200 OK)
            // and a content-type so the client knows what's coming, then a blank line:
            client.println("HTTP/1.1 200 OK");
            client.println("Content-type:text/html");
            client.println("Connection: close");
            client.println();
            
            // turns the logging on and off
            if (header.indexOf("GET /log/on") >= 0) 
            {
              if(!LogFile)
              {
                Serial.println("Logging switch on");
                LoggingState = "ON";
                StartLogToFile(LITTLEFS);
              } 
              else
              {
                Serial.println("SKIP Logging switch on");
                LoggingState = "OFF";
              }
            }
            else if (header.indexOf("GET /log/off") >= 0) 
            {
              Serial.println("Logging switch off");
              LoggingState = "OFF";
              StopLogToFile();
            } 

            // Turn FTP transfer on/off
            if (header.indexOf("GET /ftp/on") >= 0) 
            {
              if(FtpSendDone != 0)
              {
                Serial.println("FTP Tx switch on");
                FtpTxState = "BUSY";
                TransferLogFile(LITTLEFS);
              } 
              else
              {
                Serial.println("SKIP FTP Tx switch on");
                FtpTxState = "OFF";
              }
            }
            else if (header.indexOf("GET /ftp/off") >= 0) 
            {
              Serial.println("FTP Tx switch off");
              FtpTxState = "OFF";
            } 
            
            // Display the HTML web page
            client.println("<!DOCTYPE html><html>");
            client.println("<head><meta name=\"viewport\" content=\"width=device-width, initial-scale=1\">");
            client.println("<link rel=\"icon\" href=\"data:,\">");
            // CSS to style the on/off buttons 
            // Feel free to change the background-color and font-size attributes to fit your preferences
            client.println("<style>html { font-family: Helvetica; display: inline-block; margin: 0px auto; text-align: center;}");
            client.println(".button { background-color: #4CAF50; border: none; color: white; padding: 16px 40px;");
            client.println("text-decoration: none; font-size: 30px; margin: 2px; cursor: pointer;}");
            client.println(".button2 {background-color: #555555;}</style></head>");
            
            // Web Page Heading
            client.printf("<body><h1>ESP BMS Monitor version %s %d.%d.%d.%d</h1>\n", 
                      VERSION, local_IP[0], local_IP[1], local_IP[2], local_IP[3]);
            
            // Display current state, and ON/OFF buttons for Logging switch  
            WiFiRSSIvalue = String(RSSIval);
            client.println("<p>WiFi RSSI db = " + WiFiRSSIvalue + "</p>");

            VoltageLevelStr = String(BatteryVoltage_2 * 0.01);
            CurrentLevelStr = String(BatteryCurrent_2 * 0.1);
            ChargeLevelStr  = String(SOC_2);
            client.println("<p>Voltage = " + VoltageLevelStr + "V Current = " + CurrentLevelStr + "A SOC = " + ChargeLevelStr + " %" + "</p>");

            CountStr  = String(LogFileSize); 
            if (LoggingState == "ON") 
              client.println("<p>Logging State " + LoggingState + " " + CountStr + " bytes" + "</p>");
            else
              client.println("<p>Logging State " + LoggingState + "</p>");
            // If the LoggingState is off, it displays the ON button       
            if (LoggingState == "OFF") 
              client.println("<p><a href=\"/log/on\"><button class=\"button\">Turn logging ON now</button></a></p>");
            else 
              client.println("<p><a href=\"/log/off\"><button class=\"button button2\">Turn logging OFF now</button></a></p>");
            if (FtpTxState == "OFF") 
              client.println("<p><a href=\"/ftp/on\"><button class=\"button button3\">Turn FTP Tx ON now</button></a></p>");
            else if(FtpTxState == "BUSY")
              client.println("<p><a href=\"/ftp/off\"><button class=\"button button4\">FTP Tx BUSY</button></a></p>");
            else
              client.println("<p><a href=\"/ftp/off\"><button class=\"button button5\">Turn FTP Tx OFF now</button></a></p>");
             
            client.println("<p><a href=\"/log/refrech\"><button class=\"button button6\">Refresh</button></a></p>");
               
            client.println("</body></html>");
            
            // The HTTP response ends with another blank line
            client.println();
            // Break out of the while loop
            break;
          } 
          else 
          { // if you got a newline, then clear currentLine
            currentLine = "";
          }
        } 
        else if (c != '\r') 
        {  // if you got anything else but a carriage return character,
          currentLine += c;      // add it to the end of the currentLine
        }
      }
    }
    // Clear the header variable
    header = "";
    currentLine = ""; 
    // Close the connection
    client.stop();
    Serial.println("Client disconnected.");
    Serial.println("");
  }
  //if(!LogFile)
  //  StartLogToFile(LittleFS);
  //if(LogFileSize >= 200)
  //{
  //  StopLogToFile();
  //  TransferLogFile(LittleFS);
  //}
  if(lastLog > millis()) // when millis() wrap
  {
    lastLog = millis();
  }
  if(lastPrint > millis()) // when millis() wrap
  {
    lastPrint = millis();
  }
  if(LoggingOn && ((lastLog + 10000) <= millis()))
  {
    lastLog = millis();
    LogToFile();
  }
  if((lastPrint + 9000) <= millis())
  {
    lastPrint = millis();
//    if((RxCanIds & 0x03) == 0x03)
//    {
      PrintAll();
      RxCanIds = 0;
//    }
  }
}

