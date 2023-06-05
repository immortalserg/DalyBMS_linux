// smart-bms  to  mqtt
// espirans.msk@gmail.com
#include <unistd.h>
#include <stdio.h>
#include <stdlib.h>
#include <thread>
#include <fcntl.h>
#include <string.h>
#include <termios.h>
#include <pthread.h>
#include <iostream>
#include <string>
#include <fstream>

#include "main.h"
#include "tools.h"
#include "inputparser.h"

bool debugFlag = false;
bool runOnce = false;
time_t start_time;
volatile bool BMS_conect = false;

string devicename = "/dev/ttyUSB0";
string MqttServer;
string MQTTtopik;
string out_to;
int runinterval;
int fd;

    #define len_buf 96
    unsigned char buffer0[len_buf];
    //unsigned char bufn[6][16];
    #define len_zapros  13
    unsigned char zapros_bms[6][len_zapros]={
    {0xA5,0x40,0x90,0x08,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x7D},  //A5 40 90 08 00 00 00 00 00 00 00 00 7D
	{0xA5,0x40,0x93,0x08,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x80},
    {0xA5,0x40,0x94,0x08,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x81},
    {0xA5,0x40,0x97,0x08,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x84}, // balansing
    {0xA5,0x40,0x92,0x08,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x7F},  // temperatura 
    {0xA5,0x40,0x95,0x08,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x82}  // u cells  
	};

    
    uint8_t cels_count  = 4;
    uint16_t V_cels[16] = {0};
    uint8_t b_cels[16] = {0};//Balance status
    int16_t NTC[8] = {0};
    uint16_t Vbat = 0;
    int16_t Abat = 0;
    uint32_t RemCapacity = 0;
    uint32_t TypCapacity = 0;
    uint16_t ProtectStatus = 0;
    uint16_t RSOC= 100;
    uint8_t NTC_numb = 2;
    uint8_t Chg_FET_stat = 1;
    uint8_t DisChg_FET_stat = 1;




// ---------------------------------------

void attemptAddSetting(int *addTo, string addFrom) {
    try {
        *addTo = stof(addFrom);
    } catch (exception e) {
        cout << e.what() << '\n';
        cout << "There's probably a string in the settings file where an int should be.\n";
    }
}


void getSettingsFile(string filename) {

    try {
        string fileline, linepart1, linepart2;
        ifstream infile;
        infile.open(filename);

        while(!infile.eof()) {
            getline(infile, fileline);
            size_t firstpos = fileline.find("#");

            if(firstpos != 0 && fileline.length() != 0) {    // Ignore lines starting with # (comment lines)
                size_t delimiter = fileline.find("=");
                linepart1 = fileline.substr(0, delimiter);
                linepart2 = fileline.substr(delimiter+1, string::npos - delimiter);

                if(linepart1 == "device"){
                    devicename = linepart2;
                    lprintf("device: <%s>\n",devicename.c_str());
                }else if(linepart1 == "run_interval"){
                    attemptAddSetting(&runinterval, linepart2);
                    lprintf("run_interval: %d\n",runinterval);
                }else if(linepart1 == "MQTTserver"){
                    MqttServer = linepart2;
                    lprintf("MQTTserver: <%s>\n",MqttServer.c_str());
                }else if(linepart1 == "MQTTtopik"){
                    MQTTtopik = linepart2;
                    lprintf("MQTTtopik: <%s>\n",MQTTtopik.c_str());
                }else if(linepart1 == "out_to"){
                    out_to = linepart2;
                    lprintf("out_to: <%s>\n",out_to.c_str());
                }else{
                    continue;
                }
                    
            }
        }
        infile.close();
    } catch (...) {
        cout << "Settings could not be read properly...\n";
    }
}


bool uart_open(){
    //   int fd;
    //fd = open( "/dev/ttyUSB0", O_RDWR| O_NONBLOCK | O_NDELAY );
    fd = open(devicename.c_str(), O_RDWR | O_NONBLOCK | O_NDELAY );
    if (!isatty (fd)) { 
        close(fd); 
        //return -1; }
     //if (fd == -1) {
        printf("BMS: Unable to open device %s \n",devicename.c_str());
        sleep(5);
        return false;
    }
    
    speed_t baud = B9600;

    struct termios settings;
    tcgetattr(fd, &settings);

    cfsetospeed(&settings, baud);      // baud rate
    settings.c_cflag &= ~PARENB;       // no parity
    settings.c_cflag &= ~CSTOPB;       // 1 stop bit
    settings.c_cflag &= ~CSIZE;
    settings.c_cflag |= CS8 | CLOCAL;  // 8 bits
    settings.c_oflag &= ~OPOST;        // raw output
    settings.c_iflag &= ~ICRNL; // не преобразовывать перевод каретки в конец строки при вводе 
         
    settings.c_lflag &= ~(ICANON | ECHO | ECHOE | ISIG);// not  canonical mode
    settings.c_cc[VTIME] = 1;
    settings.c_cc[VMIN] = 0;

    tcsetattr(fd, TCSANOW, &settings); // apply the settings
    tcflush(fd, TCOFLUSH);

 return true;
}

bool query(uint8_t num_zap) {
    time_t started;
    bool res = false;
    int n = 0;

    if (!uart_open()) { 
        close(fd); 
        return false;
    }

    unsigned char c='0';
    while (read(fd,&c,1)>0){
                usleep(10000);
    }   
 
    // ---------------------------------------------------------------

    //send a command ------------------------------------
    write(fd, zapros_bms[num_zap], len_zapros);

    for (int a=0; a < len_buf; a++) {
       buffer0[a] = 0;
    }
      
    for (int a=0; a < len_zapros; a++) {
       lprintf("%X ", zapros_bms[num_zap][a]);
    }
    lprintf("\n");

    
    
    // recive data ------------------------------------
  time(&started);
  
  //unsigned char c='0';
  while (time(NULL) - started < 2)  // таймаут 2 сек
  {
    if (read(fd,&c,1)>0){
        buffer0[n++] = c;
       usleep(5000);
       if (zapros_bms[num_zap][2] == 0x95){
            while (read(fd,&buffer0[n],1)>0){
            usleep(10000);
            n++;
            }
        }else{
            while (read(fd,&buffer0[n],1)>0){
                usleep(5000);
                n++;
            }           
        }

        lprintf("OTVET: ");
        for (int a=0; a < n; a++) {
         lprintf("%X ", buffer0[a]);
        }
        lprintf("\n");
        
      if (CheckCRC(n)  && (buffer0[0] == 0xA5) && (buffer0[1] == 0x01) ){
        res = true;   
      }
      break; 
    } 
  }

    // ------------------------------------

   
   close(fd);

    lprintf("   n = %d", n);

   return res;
}


void poll() {

         BMS_conect = true;
         lprintf("\n\n");
        for (uint8_t z = 0; z < 6; z++) {  //  только 2 запроса из трех
            lprintf("ZAPROS %d: ", z);
            if (query(z)) {
                rashet();
                lprintf("  tRUE\n\n", z); 
            } else {
                BMS_conect = false;
                lprintf("  fALSE\n\n", z);
                return;
            } 
              usleep(1000);  
        }
        posol();
}


bool CheckCRC(uint8_t n) {
  bool cr = false;
  uint8_t l = buffer0[3] + 3;
  uint8_t crc=0;
  n = n / 13;  

    for (uint8_t k = 0; k < n; k++){
        l = buffer0[k*13 + 3] + 3;
        crc = 0 ;
    
        for (uint8_t i=0; i<=l;i++){
        crc += buffer0[k*13 + i];
        }
        lprintf("CRC: %x", crc);
        if (crc == buffer0[k*13 + l +1]) {
            lprintf(" => OK");  
            cr = true;
        } else {
            lprintf(" => NOT ok !!!!");  
            BMS_conect = 0; 
            return cr;   
        }
        lprintf("\n");
    }
  return cr;
}


void rashet(){
    lprintf("  rashet: %d", buffer0[1]);
    uint8_t x = 0;
    uint8_t y = 0;
    uint8_t z = 0;
    switch (buffer0[2]){
        case 0x90:
          Vbat = buffer0[4]<<8 | buffer0[5];
          Abat = buffer0[8]<<8 | buffer0[9];
          Abat = 30000 - Abat;
          RSOC = buffer0[10]<<8 | buffer0[11];
        break;

        case 0x92:
          for (uint8_t k = 0; k < NTC_numb; k++){
              NTC[k]  =  buffer0[(4 + k*2)];
              NTC[k] = NTC[k]-40;
            //NTC[k]  =  buffer0[(27 + k*2)]<<8 | buffer0[(28 + k*2)];
            //NTC[k] = NTC[k]- 2731;
          }  
 
        break;

        case 0x93:
         RemCapacity = buffer0[9]<<16 | buffer0[10]<<8 | buffer0[11];
         Chg_FET_stat = buffer0[5];
         DisChg_FET_stat = buffer0[6];
        break;

        case 0x94:
          cels_count = buffer0[4];
          if (cels_count > 16){
              cels_count = 16;
          }

          NTC_numb = buffer0[5];
          if (NTC_numb > 3){
            NTC_numb = 3;
          }
        //   for (uint8_t k = 0; k < cels_count; k++){
        //     V_cels[k]  =  buffer0[(4 + k*2)]<<8 | buffer0[(5 + k*2)];
        //   } 
        break;

        case 0x95:
          for (uint8_t k = 0; k < cels_count; k++){  
            x = (k/3) * 13;
            y = k - (3*(k/3));
            z = x + 5 + y*2;
            V_cels[k]  =  buffer0[z]<<8 | buffer0[z+1];
          } 
        break;

        case 0x97:
          for (uint8_t k = 0; k < 8; k++){
           b_cels[k] = buffer0[4]  >> k;  //сдвигаем на k 
           b_cels[k] = b_cels[k] & 0b00000001; //  берем только D0 
          }
          for (uint8_t k = 0; k < 8; k++){
           b_cels[8+k] = buffer0[5]  >> k;  //сдвигаем на k 
           b_cels[8+k] = b_cels[8+k] & 0b00000001; //  берем только D0 
          } 
        break;

        //   TypCapacity = buffer0[10]<<8 | buffer0[11];
        //   ProtectStatus = buffer0[20]<<8 | buffer0[21];
  
        default:
        break;
      } 
}


string ProtectStat(){
  // bit0	 Over-charge protection with single cell 
	// bit1	 Over-discharge protection voltage with single cell 
	// bit2	 Over-voltage protection with whole battery pack
	// bit3	Low-voltage protection with battery pack
	// bit4	High temperature protection of charge
	// bit5	Low temperature protection of charge 
	// bit6	High temperature protection of discharge 
	// bit7	Low temperature protection of discharge 
	// bit8	Over-current protection of charge 
	// bit9	 Over-current protection of discharge
	// bit10	 Short protection 
	// bit11	 IC inspection erros
	// bit12	 MOS lock by software
  uint16_t stat=0;
  string b_status = "-no-";
  if (ProtectStatus > 0){
    b_status.clear();
    for (uint8_t k = 0; k < 13; k++){
      stat = ProtectStatus  >> k;  //сдвигаем на k 
      stat = stat & 0b0000000000000001; //  берем только D0 
        if (stat){
            switch (k){       
                case 0:
                b_status += "Over-charge cell";
                break;
                
                case 1:
                b_status += "Over-discharge cell";
                break;

                case 2:
                b_status += "Over-voltage battery";
                break;

                case 3:
                b_status += "Low-voltage battery";
                break;

                case 4:
                b_status += "High temp of charge";
                break;

                case 5:
                b_status += "Low temp of charge";
                break;

                case 6:
                b_status += "High temp of discharge";
                break;

                case 7:
                b_status += "Low temp of discharge";
                break;

                case 8:
                b_status += "Over-current of charge";
                break;

                case 9:
                b_status += "Over-current of discharge";
                break;

                case 10:
                b_status += "Short protection";
                break;

                case 11:
                b_status += "IC inspection erros";
                break;

                case 12:
                b_status += "MOS lock by software";
                break;
            }

          b_status += "; ";
          
        } 
    }
  }
  return b_status;
}


void posol(){
    //lprintf("\n");
   if ((out_to == "consol") && BMS_conect)
   //if (out_to == "consol")
   {
        printf("{\"BMS_status\":%d,", BMS_conect);
        printf("\"Vbat\":%.2f,", (float)Vbat/10);
        printf("\"Abat\":%.2f,", (float)Abat/10);
        printf("\"AhBat\":%.1f,", (float)RemCapacity/1000);
        //printf("\"AhTyp\":%.1f,", (float)TypCapacity/1000);
        printf("\"Chg_FET\":%d,", Chg_FET_stat);
        printf("\"DisChg_FET\":%d,", DisChg_FET_stat);
        printf("\"RSOC\":%.1f,", (float)RSOC/10);
        for (uint8_t k = 0; k < cels_count; k++){
            printf("\"Vbank%d\":%.3f,",k, (float)V_cels[k]/1000);
            printf("\"Balans%d\":%d,",k, b_cels[k]);
        } 
        for (uint8_t k = 0; k < NTC_numb; k++){
            //printf("\"Temp%d\":%.2f,",k, (float)NTC[k]/10);
            printf("\"Temp%d\":%d,",k, NTC[k]);
        }
        printf("\"Protect\":\"%s\"", ProtectStat().c_str());
        printf("}");
        printf("\n");
        lprintf("\n");
   }else if (out_to == "mqtt"){
        char sys_run[1024];
        string sys_mqtt = "mosquitto_pub -h " + MqttServer +" -t '/" + MQTTtopik + "' -m ";
        sprintf(sys_run,
        " %s'{\"BMS_status\":%d,\"Vbat\":%.2f,\"Abat\":%.2f,\"AhBat\":%.2f,\"AhTyp\":%.1f,\"Chg_FET\":%d,\"DisChg_FET\":%d,\"RSOC\":%.1f}'"
        ,sys_mqtt.c_str(),BMS_conect,(float)Vbat/10,(float)Abat/10,(float)RemCapacity/1000,(float)TypCapacity/100,Chg_FET_stat,DisChg_FET_stat,(float)RSOC/10);
        system(sys_run);

        for (uint8_t k = 0; k < NTC_numb; k++){
            sprintf(sys_run," %s'{\"Temp%d\":%d}'",sys_mqtt.c_str(),k,NTC[k]);
            system(sys_run);
        }

        for (uint8_t k = 0; k < cels_count; k++){
            sprintf(sys_run," %s'{\"Vbank%d\":%.3f,\"Balans%d\":%d}'",sys_mqtt.c_str(),k,(float)V_cels[k]/1000,k, b_cels[k]);
            system(sys_run);
        }
   }

}


int main(int argc, char* argv[]) {

    // Get command flag settings from the arguments (if any)
    InputParser cmdArgs(argc, argv);
    const string &rawcmd = cmdArgs.getCmdOption("-r");

    if(cmdArgs.cmdOptionExists("-h") || cmdArgs.cmdOptionExists("--help")) {
        return print_help();
    }
    if(cmdArgs.cmdOptionExists("-d") || cmdArgs.cmdOptionExists("--debug")) {
        debugFlag = true;
    }
    if(cmdArgs.cmdOptionExists("-1") || cmdArgs.cmdOptionExists("--once")) {
        runOnce = true;
    }
    lprintf("BMS: Debug set\n\n");

    // Get the rest of the settings from the conf file
    if( access( "./bms.conf", F_OK ) != -1 ) { // file exists
        getSettingsFile("./bms.conf");
    } else if( access( "/etc/BMS/bms.conf", F_OK ) != -1 ) {  // file doesn't exist
        getSettingsFile("/etc/BMS/bms.conf");// file exists
    } else {
        lprintf("config file not found\n\n");
        exit(0);
    }

        time(&start_time);

    while (!uart_open()) { 
        close(fd); 
        //printf("BMS: Unable to open fd \n");
        // sleep(5);
        // return false;
    }

    poll();

    while (true) { 

    
        if(runOnce && BMS_conect) {
            lprintf("BMS: queries complete, exiting loop.\n");
            printf("\n");
            exit(0);
        }
            
        if (time(NULL) - start_time >= runinterval){

            time(&start_time);
            poll();

        }

        usleep(1000);        
    }

    return 0;
}
