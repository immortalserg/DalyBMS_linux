// #ifndef ___MAIN_H
// #define ___MAIN_H

#include <atomic>

  using namespace std;

bool uart_open(void);
void rashet(void);
void posol(void);
string ProtectStat(void);
bool CheckCRC(uint8_t n);
bool query(uint8_t num_zap);
void poll(void);
void attemptAddSetting(int *addTo, string addFrom);
void getSettingsFile(string filename);

//#endif // ___MAIN_H
