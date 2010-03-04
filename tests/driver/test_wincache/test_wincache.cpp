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
   | Module: test_wincache.cpp                                                                    |
   +----------------------------------------------------------------------------------------------+
   | Author: Don Raman <donraman@microsoft.com>                                                   |
   +----------------------------------------------------------------------------------------------+
*/

#include "stdafx.h"
#include "test_wincache.h"
#include <time.h>
#include <Lmcons.h>

void TestWinCache::dumpHeader()
{
    struct tm  when;
    __time64_t now;
    char       buff[80];
    time( &now );
    _localtime64_s( &when, &now );
    asctime_s( buff, sizeof(buff), &when );
    string currentDate = "Time of Test Run: ";
    currentDate += buff;
    log(logPath, currentDate);

    string machineName = "Computer Name: ";
    if (getenv("COMPUTERNAME"))
        machineName += getenv("COMPUTERNAME");
    machineName += "\r\n";
    log(logPath, machineName);

    string userName = "User Name: ";
    if (getenv("USERNAME"))
        userName += getenv("USERNAME");
    userName += "\r\n";
    log(logPath, userName);


    string domainName = "Domain Name: ";
    if (getenv("USERDNSDOMAIN"))
        domainName += getenv("USERDNSDOMAIN");
    domainName += "\r\n";
    log(logPath, domainName);


    string procInfo = "Proccessor Info: ";
    if (getenv("PROCESSOR_IDENTIFIER"))
        procInfo += getenv("PROCESSOR_IDENTIFIER");
    procInfo += "\r\n";
    log(logPath, procInfo);

    DWORD dwVersion = 0; 
    DWORD dwMajorVersion = 0;
    DWORD dwMinorVersion = 0; 
    DWORD dwBuild = 0;

    dwVersion = GetVersion();

    // Get the Windows version.

    dwMajorVersion = (DWORD)(LOBYTE(LOWORD(dwVersion)));
    dwMinorVersion = (DWORD)(HIBYTE(LOWORD(dwVersion)));

    // Get the build number.

    if (dwVersion < 0x80000000)              
        dwBuild = (DWORD)(HIWORD(dwVersion));

    sprintf(buff, "%d.%d.%d", dwMajorVersion, dwMinorVersion, dwBuild);
    string os = "Windows Version: ";
    os += buff;
    os += "\r\n\r\n";
    log(logPath, os);
}

string TestWinCache::getLogFile(string physicalPath)
{
    if (logPath.empty())
    {
        string logFilePath = physicalPath;
        string::size_type pos = string::npos;

        string dir_seperator = "/\\";
        pos = logFilePath.find_last_of(dir_seperator);
        if (pos != string::npos)
        {
            logFilePath.erase(pos+1);
            logFilePath += "..\\temp\\result.txt";
        }
        else
        {
            logFilePath = "";
        }
        logPath = logFilePath;
        dumpHeader();
        return logFilePath;
    }
    else
    {
        return logPath;
    }
}

//Across PHP test case log file and this program there is a common log file.
//Yes, both of them log to same file named output.txt
//A PHP test case writes what functionality is getting tested
//and this program writes pass or fail status
//This file is in webroot folder and hence ensure that proper 
//permissions are there. If this fails I am exiting.
bool TestWinCache::log(string file, string str)
{
    //fopen is C, use iostream for new C++.
    //Below I am opening an output stream
    ofstream stream(file.c_str(), ios::app);
    if (stream.is_open())
    {
        if (!str.compare("PASSED\r\n") 
            || !str.compare("PASSED\r\n\r\n"))
        {
            ++passed;
        }
        stream << str;
        stream.close();
        return true;
    }
    else
    {
        return false;
    }
}

//This function compares two files and return true if their contents
//are same otherwise false
bool TestWinCache::compare(string file1, string file2)
{
    //Now this is inout stream just for reading.
    ifstream stream1(file1.c_str());
    ifstream stream2(file2.c_str());
    string content1, content2;
    //Logic is pretty simple. I am reading both the files line by line
    //and comparing. getline function is coming from STL basic string.
    //getline reads the file line by line and stores the content in the
    //string specified.
    //If both the files passed to this function is invalid, this function
    //returns true. I think that is okay.
    while (getline(stream1, content1))
    {
        if (getline(stream2, content2))
        {
            if (content1.compare(content1) != 0)
                return false;
        }
        else
        {
            return false;
        }
    }
    if (getline(stream2, content2))
        return false;
    else
        return true;
}

//This file truncates the content of a file making it 0 bytes in size
//I am using it for truncating output.txt present in your webroot folder
//This call should never fail. Ensure that there are proper permissions
//for the adminitrator account to delete file in webroot folder.
bool TestWinCache::truncate(char* file)
{
    string file2 = file;
    string::size_type pos = string::npos;

    string dir_seperator = "/\\";
    pos = file2.find_last_of(dir_seperator);
    if (pos == string::npos)
        return false;
    else
    {
        file2.erase(pos+1);
        file2 += "..\\temp\\output.txt";
        if (!_access(file2.c_str(), 06))
        {
            //cout << "Truncating file..." << endl;
            ofstream stream(file2.c_str(), ios::out | ios::trunc);
            stream.close();
            return true;
        }
        else
        {
            return false;
        }
    }
}

//This builds up the file for comparison. Suppoase I am running
//http://localhost/wincachetest/wincache1.php
//I expect:
//1. output.txt to be in the same physical folder where wincache1.php is there
//2. wincache1.out.txt to be in the same physical folder where wincache1.php is there
//This function builds the full path to above two files
//and sends it to compare function.
bool TestWinCache::verify(string prefix)
{
    string file1(prefix);
    file1 += ".out.txt";
    string file2 = prefix;
    string::size_type pos = string::npos;

    string dir_seperator = "/\\";
    pos = file2.find_last_of(dir_seperator);
    if (pos == string::npos)
        return false;
    else
    {
        file2.erase(pos+1);
        file2 += "..\\temp\\output.txt";
        return compare(file1, file2);
    }
}

DWORD TestWinCache::cleanup(char* src)
{
    WIN32_FIND_DATA ffd;
    char szDir[MAX_PATH];
    size_t length_of_arg;
    HANDLE hFind = INVALID_HANDLE_VALUE;
    DWORD dwError=0;
    char fullPath[MAX_PATH];


    // Check that the input path plus 3 is not longer than MAX_PATH.
    // Three characters are for the "\*" plus NULL appended below.

    length_of_arg = strlen(src);

    if (length_of_arg > (MAX_PATH - 3))
    {
        cout << "Input directory too long.\n";
        return (-1);
    }

    // Prepare string for use with FindFile functions.  First, copy the
    // string to a buffer, then append '\*' to the directory name.

    strcpy(szDir, src);
    strcat(szDir, "\\*");

    // Find the first file in the directory.

    hFind = FindFirstFile(szDir, &ffd);

    if (INVALID_HANDLE_VALUE == hFind) 
    {
        //DisplayErrorBox(TEXT("FindFirstFile"));
        return dwError;
    } 

    // List all the files in the directory with some info about them.

    do
    {
        if (ffd.dwFileAttributes & FILE_ATTRIBUTE_DIRECTORY)
        {
            continue;
        }
        else
        {
            int fullPathLen = strlen(src) + strlen(ffd.cFileName) + 2;
            if (fullPathLen > MAX_PATH)
            {
                cout << "Input directory too long.\n";
                return (-1);
            }
            strcpy(fullPath, src);
            strcat(fullPath, "\\");
            strcat(fullPath, ffd.cFileName);
            cout << fullPath << endl;

            if (DeleteFile(fullPath) !=0)
            {
                cout << "Deleted file " << fullPath << ":SUCCESS" << endl;
            }
            else
            {
                cout << "Deleted file " << fullPath << ":FAILURE" << endl;
                cout << GetLastError();
                return -1;
            }
        }
    }
    while (FindNextFile(hFind, &ffd) != 0);

    FindClose(hFind);
    return dwError;
}

DWORD TestWinCache::copy(char* src, char* dest)
{
    WIN32_FIND_DATA ffd;
    char szDir[MAX_PATH];
    size_t length_of_arg;
    HANDLE hFind = INVALID_HANDLE_VALUE;
    DWORD dwError=0;
    char fullPath[MAX_PATH];
    char destPath[MAX_PATH];
    DWORD dwAttrs;   

    // Check that the input path plus 3 is not longer than MAX_PATH.
    // Three characters are for the "\*" plus NULL appended below.

    length_of_arg = strlen(src);

    if (length_of_arg > (MAX_PATH - 3))
    {
        cout << "Input directory too long.\n";
        return (-1);
    }

    // Prepare string for use with FindFile functions.  First, copy the
    // string to a buffer, then append '\*' to the directory name.

    strcpy(szDir, src);
    strcat(szDir, "\\*");

    // Find the first file in the directory.

    hFind = FindFirstFile(szDir, &ffd);

    if (INVALID_HANDLE_VALUE == hFind) 
    {
        //DisplayErrorBox(TEXT("FindFirstFile"));
        return dwError;
    } 

    // List all the files in the directory with some info about them.

    do
    {
        if (ffd.dwFileAttributes & FILE_ATTRIBUTE_DIRECTORY)
        {
            continue;
        }
        else
        {
            int fullPathLen = strlen(src) + strlen(ffd.cFileName) + 2;
            if (fullPathLen > MAX_PATH)
            {
                cout << "Input directory too long.\n";
                return (-1);
            }
            strcpy(fullPath, src);
            strcat(fullPath, "\\");
            strcat(fullPath, ffd.cFileName);
            cout << fullPath << endl;
            strcpy(destPath, dest);
            strcat(destPath, "\\");
            strcat(destPath, ffd.cFileName);

            if (CopyFile(fullPath, destPath, FALSE) !=0)
            {
                cout << "Copied file " << fullPath << " to folder " << destPath << ":SUCCESS" << endl;
                dwAttrs = GetFileAttributes(destPath); 
                if (dwAttrs==INVALID_FILE_ATTRIBUTES) return -1; 

                if ((dwAttrs & FILE_ATTRIBUTE_READONLY)) 
                { 
                    SetFileAttributes(destPath, 
                        dwAttrs | ~FILE_ATTRIBUTE_READONLY); 
                }
            }
            else
            {
                cout << "Copied file " << fullPath << " to folder " << destPath << ":FAILURE" << endl;
                cout << GetLastError();
                return -1;
            }
        }
    }
    while (FindNextFile(hFind, &ffd) != 0);

    FindClose(hFind);
    return dwError;
}

bool TestWinCache::runinIE(char* fullURL, char* physicalPath)
{
    if (SUCCEEDED(OleInitialize(NULL)))
    {
        IWebBrowser2*    pBrowser2;
        //Create an instance of Internet Expplorer
        CoCreateInstance(CLSID_InternetExplorer, NULL, CLSCTX_LOCAL_SERVER, 
            IID_IWebBrowser2, (void**)&pBrowser2);
        if (pBrowser2)
        {
            VARIANT vEmpty;
            VariantInit(&vEmpty);
            //Convert the URL represented as array of char to BSTR
            BSTR bstrURL = _com_util::ConvertStringToBSTR(fullURL);

            //Before navigating see if file output.txt exists
            //if exist truncate it
            truncate(physicalPath);
            string logFilePath = getLogFile(physicalPath);

            //navigate to the URL in Internet Explorer
            cout << "Navigating to URL: " << fullURL << endl;
            string logMsg = "Navigating to URL: ";
            logMsg += fullURL;
            logMsg += "\r\n";
            log(logFilePath, logMsg);
            HRESULT hr = pBrowser2->Navigate(bstrURL, &vEmpty, &vEmpty, &vEmpty, &vEmpty);
            if (SUCCEEDED(hr))
            {
                ++total;
                pBrowser2->put_Visible(VARIANT_TRUE);

                //Wait until the PHP script has finished running.
                READYSTATE state = READYSTATE_UNINITIALIZED;
                pBrowser2->get_ReadyState(&state);
                while (state != READYSTATE_COMPLETE)
                {
                    pBrowser2->get_ReadyState(&state);
                }

                //Let's do some verification.
                if (logFilePath.empty())
                {
                    cout << "Cannot figure out log file path from " << physicalPath << endl;
                    exit(1);
                }
                if (verify(physicalPath))
                {
                    if (!log(logFilePath, "PASSED\r\n"))
                    {
                        cout << "Cannot write to log file at " << logFilePath << endl;
                    }
                }
                else
                {
                    if (!log(logFilePath, "FAILED\r\n"))
                    {
                        cout << "Cannot write to log file at " << logFilePath << endl;
                    }
                }
                //Before navigating see if file output.txt exists
                //if exist truncate it
                truncate(physicalPath);

                //Refresh Internet Explorer
                VARIANT v1;
                VariantInit(&v1);
                v1.vt = VT_I4;
                //This is equivalent of Cntl+F5 and this is what we want
                v1.lVal = REFRESH_COMPLETELY;
                cout << "Refreshing (Cntl+F5) URL: " << fullURL << endl;
                string logMsg = "Refreshing (Cntl+F5) URL: ";
                logMsg += fullURL;
                logMsg += "\r\n";
                log(logFilePath, logMsg);
                pBrowser2->Refresh2(&v1);
                ++total;

                //Wait again till PHP has finihsed running.
                state = READYSTATE_UNINITIALIZED;
                pBrowser2->get_ReadyState(&state);
                while (state != READYSTATE_COMPLETE)
                {
                    pBrowser2->get_ReadyState(&state);
                }
                //Let's do some verification.
                //Let's do some verification.
                if (verify(physicalPath))
                {
                    if (!log(logFilePath, "PASSED\r\n\r\n"))
                    {
                        cout << "Cannot write to log file at " << logFilePath << endl;
                    }
                }
                else
                {
                    if (!log(logFilePath, "FAILED\r\n\r\n"))
                    {
                        cout << "Cannot write to log file at " << logFilePath << endl;
                    }
                }
                //Close the browser.
                pBrowser2->Quit();
            }
            else
            {
                pBrowser2->Quit();
            }

            //Free all the resources
            SysFreeString(bstrURL);
            pBrowser2->Release();
        }

        OleUninitialize();
        return true;
    }
}

DWORD TestWinCache::runtest(char* src, char* url)
{
    WIN32_FIND_DATA ffd;
    char szDir[MAX_PATH];
    size_t length_of_arg;
    HANDLE hFind = INVALID_HANDLE_VALUE;
    DWORD dwError=0;
    char fullURL[MAX_PATH];
    char physicalPath[MAX_PATH];


    // Check that the input path plus 3 is not longer than MAX_PATH.
    // Three characters are for the "\*" plus NULL appended below.

    length_of_arg = strlen(src);

    if (length_of_arg > (MAX_PATH - 3))
    {
        cout << "Input directory too long.\n";
        return (-1);
    }

    // Prepare string for use with FindFile functions.  First, copy the
    // string to a buffer, then append '\*' to the directory name.

    strcpy(szDir, src);
    strcat(szDir, "\\*");

    // Find the first file in the directory.

    hFind = FindFirstFile(szDir, &ffd);

    if (INVALID_HANDLE_VALUE == hFind) 
    {
        return dwError;
    } 

    // Traverse the directory. Equivalent of opendir/scandir in UNIX.

    do
    {
        //I expect only files of these types to be present in the folder.
        //Ignore ones below.
        if ((ffd.dwFileAttributes & FILE_ATTRIBUTE_DIRECTORY)
            ||(strstr(ffd.cFileName, ".out.txt"))
            ||(strstr(ffd.cFileName, "_base"))
            ||(!stricmp(ffd.cFileName, "common.inc"))
            ||(!stricmp(ffd.cFileName, "output.txt"))
            ||(!stricmp(ffd.cFileName, "result.txt")))
        {
            continue;
        }
        else
        {
            int fullPathLen = strlen(src) + strlen(ffd.cFileName) + 2;
            if (fullPathLen > MAX_PATH)
            {
                cout << "Input directory too long.\n";
                return (-1);
            }
            strcpy(fullURL, url);
            strcat(fullURL, "/");
            strcat(fullURL, ffd.cFileName);
            //cout << fullURL << endl;

            strcpy(physicalPath, src);
            strcat(physicalPath, "\\");
            strcat(physicalPath, ffd.cFileName);
            physicalPath[strlen(physicalPath) - 4] = '\0';
            runinIE(fullURL, physicalPath);
        }
    }
    while (FindNextFile(hFind, &ffd) != 0);

    FindClose(hFind);
    return dwError;
}

int main(int argc, char* argv[])
{
    if (argc == 2)
    {
        if (stricmp(argv[1], "--detailed"))
        {
            cout << "Usage: test_wincache folder_path local_url" << endl;
            cout << "       folder_path: path to a webroot containing the PHP files" << endl;
            cout << "       local_url: valid url to the folder which can be browsed" << endl << endl;
            cout << "Example: test_wincache C:\\inetpub\\wwwroot\\testwincache http://localhost/testwincache" << endl;
            cout << "Please run the command from an elevated prompt." << endl;
            cout << endl << "Here are the steps required to run this:" << endl;
            cout << "\t1. Writing PHP Test Cases:" << endl;
            cout << "\t\t--Create the main PHP file with name wincache<number>.php" << endl;
            cout << "\t\t--If the above file includes any file make sure to name it wincache_base<number>.php" << endl;
            cout << "\t\t--Run you file and capture the ouput in a file named wincache<number>.out.txt" << endl;
            cout << "\t\t--Ensure that <number> postfix for main as well as included and out files are same" << endl;
            cout << "\t2. Once you have the PHP files ready create a folder under your webroot" << endl;
            cout << "\t3. You can name the folder anything like wincachetest or so" << endl;
            cout << "\t4. Copy all the files created in Step 1 to newly created folder" << endl;
            cout << "\t5. Create a folder named TEMP within this folder and ensure that a PHP application has" << endl;
            cout << "\t   specific right to write to it. This is very important" << endl;
            cout << "\t6. Use the MYECHO to write to file in the TEMP folder" << endl;
            cout << "\t7. MYECHO function can be found in the file named common.inc" << endl;
            cout << "\t8. Use LOGTOFILE function to print at the beginning of your PHP file" << endl;
            cout << "\t   to give a brief description of what your test does. LOGTOFILE function" << endl;
            cout << "\t   definition can also be found in the file common.inc" << endl;
            cout << "\t9. You are done. Use the coomand above to test your PHP files" << endl;
            exit(1);
        }
    }
    if (argc != 3)
    {
        cout << "Usage: test_wincache folder_path local_url" << endl;
        cout << "       folder_path: path to a webroot containing the PHP files" << endl;
        cout << "       local_url: valid url to the folder which can be browsed" << endl << endl;
        cout << "Example: test_wincache C:\\inetpub\\wwwroot\\testwincache http://localhost/testwincache" << endl;
        cout << "For step by step help pass --detailed as an argument" << endl;
        exit(1);
    }
    char* src = argv[1];
    char* url = argv[2];
    TestWinCache test;
    //copy(src, dest);
    test.runtest(src, url);
    //cleanup(dest);
    return 0;
}