/*
   +----------------------------------------------------------------------------------------------+
   | Windows Cache for PHP                                                                        |
   +----------------------------------------------------------------------------------------------+
   | Copyright (c) 2009, Microsoft Corporation. All rights reserved.                              |
   |                                                                                              |
   | Redistribution and use in source and binary forms, with or without modification, are         |
   | permitted provided that the following conditions are met:                                    |
   | - Redistributions of source code must retain the above copyright notice, this list of        |
   | conditions and the following disclaimer.                                                     |
   | - Redistributions in binary form must reproduce the above copyright notice, this list of     |
   | conditions and the following disclaimer in the documentation and/or other materials provided |
   | with the distribution.                                                                       |
   | - Neither the name of the Microsoft Corporation nor the names of its contributors may be     |
   | used to endorse or promote products derived from this software without specific prior written|
   | permission.                                                                                  |
   |                                                                                              |
   | THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS "AS IS" AND ANY EXPRESS  |
   | OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED WARRANTIES OF              |
   | MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE   |
   | COPYRIGHT HOLDER OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL,    |
   | EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE|
   | GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED   |
   | AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING    |
   | NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED |
   | OF THE POSSIBILITY OF SUCH DAMAGE.                                                           |
   +----------------------------------------------------------------------------------------------+
   | Module: test_wincache.h                                                                      |
   +----------------------------------------------------------------------------------------------+
   | Author: Don Raman <donraman@microsoft.com>                                                   |
   +----------------------------------------------------------------------------------------------+
*/
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
        logEntry += "\r\n\r\n";
        string file = getLogFile(logPath);
        log(file, logEntry);
        cout << endl << logEntry << endl;;
        cout << "Please see " << logPath << " for details." << endl;
    }
    DWORD runtest(char* src, char* url);
};