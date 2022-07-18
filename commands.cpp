#include <iostream>
#include <vector>
#include <windows.h>
#include <fstream>
#include <limits>
#include <algorithm>

//Name of the window the batch file is running on
#define SERVERWND "MCServerCommands"

//Name of the batch file that starts the server
#define STARTBAT "start.bat"

//Location of the live log file
#define LOGFILE "logs/latest.log"

/******************** unimportant for making commands ********************/
struct ProcessWindowsInfo {
	DWORD ProcessID;
	std::vector<HWND> Windows;

	ProcessWindowsInfo(DWORD const AProcessID) :
		ProcessID(AProcessID)
	{ }
};
BOOL __stdcall EnumProcessWindowsProc(HWND hwnd, LPARAM lParam) {
	ProcessWindowsInfo *Info = reinterpret_cast<ProcessWindowsInfo*>(lParam);
	DWORD WindowProcessID;

	GetWindowThreadProcessId(hwnd, &WindowProcessID);

	if(WindowProcessID == Info->ProcessID)
		Info->Windows.push_back(hwnd);

	return true;
}
std::ifstream& goto_line(std::ifstream& file, long long num){
    file.seekg(std::ios::beg);
    for(long long i = 0; i < num - 1; ++i)
        file.ignore(std::numeric_limits<std::streamsize>::max(),'\n');
    return file;
}
/*************************************************************************/

//Sends text directly to the window handle hwnd; used for sending commands to the server
void send(HWND hwnd, std::string str) {
	for(char i : str)
		PostMessage(hwnd, WM_CHAR, i, 0);
//	Sends enter key
	PostMessage(hwnd, WM_CHAR, 13, 0);
//	When multiple `send` functions are called in a row it will improperly write for some reason
//	This seems to fix it
//	If consecutive commands are executing incorrectly it's probably because this is too short for you, try increasing it a little
	Sleep(4);
}


struct message {
	message() { }

//	Takes a string formatted like a line from the live log file
//	Splits it into time, thread info, name, and the body
//	Allows it to be more easily parsed
	message(std::string str) {
		auto at = str.begin();
		auto end = str.end();
		while(*++at != ']')
			time += *at;
		at += 2;
		while(*++at != ']')
			info += *at;
		at += 2;
		if(*++at == '<' || *at == '[')
			while(*++at != '>' && *at != ']')
				name += *at;
		++at;
		while(++at < end)
			said += *at;
	}
//	Splits the body of the line at each space
//	A message like `#com ab c and 100` would be split into
//	"com", "ab", "c", "and", "100"
	void tokenize() {
		auto at = said.begin() + 1;
		auto end = said.end();
		while(at < end) {
			tokens.emplace_back();
			while(*at != ' ' && at < end)
				tokens.back() += *at++;
			++at;
		}
	}
//	Gets an item in the list of tokens by index
	std::string &operator[](size_t i) { return tokens.at(i); }
//	Gets the size of the token list
	size_t size() { return tokens.size(); }

//			Example:
//	[00:14:59] [Server thread/INFO]: <IWWenjoyer> Hello World!
	std::string time { };					//	"00:14:59"
	std::string info { };					//	"Server thread/INFO"
	std::string name { };					//	"IWWenjoyer"
	std::string said { };					//	"Hello World!"
	std::vector<std::string> tokens { };	//	"Hello", "World!"
};

//	This is where all the commands are
//	Some example commands are provided
//	TODO: make a system to store commands in plain text and load live
void execute(HWND hwnd, message msg) {
//	Is it said by a player? Did they say anything?
	if(!msg.name.length() || !msg.said.length()) return;
//	Is it a command?
	if(msg.said.at(0) != '#') return;

//	Makes everything lowercase
	transform(msg.name.begin(), msg.name.end(), msg.name.begin(), ::tolower);
	transform(msg.said.begin(), msg.said.end(), msg.said.begin(), ::tolower);
//	Splits up the message into more usable bits
	msg.tokenize();

/******************** put commands here / go wild! :D ********************/
//	Don't forget to add your commands to the help command
	if(msg[0] == "help") {
		send(hwnd, "/tellraw " + msg.name + " {\"text\":\"List of commands:\"}");
		send(hwnd, "/tellraw " + msg.name + " {\"text\":\"    Use #tp [player name] to teleport to another player\"}");
		return;
	}
	if(msg[0] == "tp" && msg.size() >= 2) {
		if(msg[1] == msg.name) return;
		send(hwnd, "/teleport " + msg.name + " " + msg[1]);
		return;
	}
/*************************************************************************/
}

int main(int argc, char** argv) {
//	This will be a handle to the command promt window running the server
	HWND hwnd = nullptr;

//	Checks if the server's window exists and gets a handle to it if it does
	hwnd = FindWindowA(NULL, SERVERWND);

//	If the server could not be found, start it
	if(!hwnd) {
		SHELLEXECUTEINFOA sei = {0};

		sei.cbSize		= sizeof(SHELLEXECUTEINFOA);
		sei.fMask		= SEE_MASK_NOCLOSEPROCESS;
		sei.lpVerb		= "open";
//		The batch file that the server will be ran from
		sei.lpFile		= STARTBAT;
		sei.lpDirectory	= NULL;
		sei.nShow		= SW_SHOW;
		sei.hInstApp	= NULL;

//		Starts the batch file that starts the server and saves info to `sei`
		ShellExecuteExA(&sei);
//		If the server is unable to open the log file, comment this out and uncommend the next line
//		That probably means that this program opened the log file before the server was able to and is denying it access
		Sleep(5000);
	//	system("pause");

//		Gets all of the window handles associated with the process started by the batch file
		ProcessWindowsInfo Info(GetProcessId(sei.hProcess));
		EnumWindows((WNDENUMPROC)EnumProcessWindowsProc, reinterpret_cast<LPARAM>(&Info));

//		Sets hwnd to the command prompt window the server is being ran from
		hwnd = Info.Windows.at(0);
//		Changes the window title to the decided server window title
		SetWindowTextA(hwnd, SERVERWND);
	}

//	Opens the log file
	std::ifstream file { LOGFILE };
//	This is the current line in the log file
	long long at = 0;
	if(!file.is_open())
		std::cout << "Couldn't open log file!!\n";
	else {
//		If the server was started when this process was started the log file will have text in it already
//		This will move `at` to the end of that file so we aren't re-reading lines
		at = std::count(std::istreambuf_iterator<char>(file), std::istreambuf_iterator<char>(), '\n') + 1;
		file.close();
		if(hwnd) std::cout << "Connection to server successful!\n";
	}
	std::string str;
	message msg;
	while(true) {
//		There's probably be a better way to do this but if it isn't closed then opened before reading it won't properly read it
		file.open(LOGFILE);
		if(!file.is_open()) {
			std::cout << "Couldn't open log file!!\n";
			continue;
		}
//		Go to the last read line
		goto_line(file, at);
//		Is there anything new?
		if(std::getline(file, str)) {
//			Parse the new line
			msg = message(str);
//			Execute if it's a command
			execute(hwnd, msg);
//			Increments the index of the last read line
			at++;
		}
//		Closes the file
		file.close();
	}
}
