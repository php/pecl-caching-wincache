#include "Exdisp.h"
#include <comutil.h>
#include <iostream>
#include <fstream>
#include <string>
#include <io.h>
using namespace std;

/*I just encapsulated everything in a class. I don't need any design pattern here
and I don't want to apply it just for the sake of it. But for the sake of C++
I encapsulated it in a class :)*/
/*But this will help me in putting a final summary. I would escape using global
variable.*/
class TestWinCache
{
    unsigned int total;
    unsigned int passed;
    string logPath;
    static const int realtotal = 10;

    void dumpHeader();
    string getLogFile(string physicalPath);
    bool log(string file, string str);
    bool compare(string file1, string file2);
    bool truncate(char* file);
    bool verify(string prefix);
    DWORD cleanup(char* src);
    DWORD copy(char* src, char* dest);
    bool runinIE(char* fullURL, char* physicalPath);
    
public:
    TestWinCache():total(0), passed(0)
    {
    }
    ~TestWinCache()
    {
        string logEntry;
        logEntry = "\r\nSummary:\r\n";
        logEntry += "Total test cases ran: ";
        char buf[256];
        sprintf(buf, "%d", total);
        logEntry += buf;
        logEntry += "\r\n";
        logEntry += "Total test cases passed: ";
        sprintf(buf, "%d", passed);
        logEntry += buf;
        logEntry += "\r\n";
        logEntry += "Expected Test to run: ";
        sprintf(buf, "%d", realtotal);
        logEntry += buf;
        logEntry += "\r\n\r\n";
        string file = getLogFile(logPath);
        log(file, logEntry);
        cout << endl << logEntry << endl;;
        cout << "Please see " << logPath << " for details." << endl;
    }
    DWORD runtest(char* src, char* url);
};