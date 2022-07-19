#include <iostream>
#include <vector>
#include <windows.h>
#include <fstream>
#include <limits>
#include <algorithm>
#include <map>
#include <sstream>

//Name of the window the batch file is running on
#define SERVERWND "MCServerCommands"

//Name of the batch file that starts the server
#define STARTBAT "start.bat"

//Location of the live log file
#define LOGFILE "logs/latest.log"

//The location of your C++ compiler
#define CC "E:\\compiling\\cygwin64\\bin\\g++.exe"

//For now only this player can mess with commands
//Eventually I'll make it so all operators can
#define OP "iwwenjoyer"

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
std::ifstream& goto_line(std::ifstream& file, long long num) {
    file.seekg(std::ios::beg);
    for(long long i = 0; i < num - 1; ++i)
        file.ignore(std::numeric_limits<std::streamsize>::max(),'\n');
    return file;
}

std::vector<std::string> files_in_dir(std::string dir) {
	HANDLE find;
    WIN32_FIND_DATAA ffd;

	find = FindFirstFileA((dir + "\\*").c_str(), &ffd);
	if (find == INVALID_HANDLE_VALUE)
		return { };

	std::vector<std::string> rv;

	do
		if(strcmp(ffd.cFileName, ".") && strcmp(ffd.cFileName, "..") && !(ffd.dwFileAttributes & FILE_ATTRIBUTE_DIRECTORY))
			rv.push_back(ffd.cFileName);
	while(FindNextFileA(find, &ffd));

	FindClose(find);
	return rv;
}
/*************************************************************************/

#include "commands/functions.h"

typedef void(*mccommand)(HWND, message);

std::map<std::string, mccommand> commands { };
std::map<std::string, HMODULE> dlls { };

std::vector<HANDLE> to_close;
HANDLE current_thread;

struct compile_params {
	compile_params(HWND hwnd, message msg) :
		hwnd { hwnd },
		msg { msg }
	{ }

	HWND hwnd;
	message msg;
};

DWORD WINAPI compile_command(LPVOID params) {
	HWND hwnd = ((compile_params*)params)->hwnd;
	message msg = ((compile_params*)params)->msg;
	std::ifstream raw { "commands\\raw\\" + msg[0] + ".txt" };
	if(!raw.is_open()) {
		delete (compile_params*)params;
		return 0;
	}
	std::stringstream buffer;
	buffer << raw.rdbuf() << '\n';
	raw.close();

	std::ofstream formatted { "commands\\raw\\" + msg[0] + ".cpp" };
	if(!formatted.is_open()) {
		delete (compile_params*)params;
		return 0;
	}
	formatted << "#include \"commands\\functions.h\"\n";
	formatted << "extern \"C\" void __declspec(dllexport) " + msg[0] + "(HWND hwnd, message msg) {\n";
	formatted << buffer.str();
	formatted << "}";
	formatted.close();

	SHELLEXECUTEINFOA compile { 0 };

	std::string exec = std::string("commands\\raw\\" + msg[0] + ".cpp -shared -o commands\\compiled\\" + msg[0] + ".dll commands\\functions.o");

	compile.cbSize			= sizeof(SHELLEXECUTEINFOA);
	compile.fMask			= SEE_MASK_NOCLOSEPROCESS;
	compile.lpVerb			= "open";
	compile.lpFile			= CC;
	compile.lpParameters	= exec.c_str();
	compile.lpDirectory		= NULL;
	compile.nShow			= SW_MINIMIZE;
	compile.hInstApp		= NULL;

	ShellExecuteExA(&compile);
	WaitForSingleObject(compile.hProcess, INFINITE);

	if(GetFileAttributesA(std::string("commands\\raw\\" + msg[0] + ".cpp").c_str()) != INVALID_FILE_ATTRIBUTES)
		DeleteFileA(std::string("commands\\raw\\" + msg[0] + ".cpp").c_str());
	if(GetFileAttributesA(std::string("commands\\compiled\\" + msg[0] + ".dll").c_str()) == INVALID_FILE_ATTRIBUTES) {
		delete (compile_params*)params;
		return 0;
	}

	HINSTANCE dll = LoadLibraryA(std::string("commands\\compiled\\" + msg[0] + ".dll").c_str());
	mccommand func = (mccommand)GetProcAddress(dll, msg[0].c_str());
	commands.emplace(msg[0], func);
	dlls.emplace(msg[0], dll);
	if(hwnd)
		(*func)(hwnd, msg);

	delete (compile_params*)params;
	return 0;
}

//	Some example commands are provided
//	TODO: add a way to query in-game values
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

	if(msg.name == OP) {
		if(msg[0] == "unl" && msg.size() >= 2 && dlls.contains(msg[1])) {
			FreeLibrary(dlls[msg[1]]);
			dlls.erase(dlls.find(msg[1]));
			commands.erase(commands.find(msg[1]));
			DeleteFileA(std::string("commands\\compiled\\" + msg[1] + ".dll").c_str());
		}
		if(msg[0] == "new" && msg.size() >= 2 && GetFileAttributesA(std::string("commands\\raw\\" + msg[1] + ".txt").c_str()) == INVALID_FILE_ATTRIBUTES) {
			std::ofstream create { "commands\\raw\\" + msg[1] + ".txt", std::ios::out};
			create.close();
		}
		if(msg[0] == "app" && msg.size() >= 2 && GetFileAttributesA(std::string("commands\\raw\\" + msg[1] + ".txt").c_str()) != INVALID_FILE_ATTRIBUTES) {
			std::ofstream app { "commands\\raw\\" + msg[1] + ".txt", std::ios::app};
			app << '\n';
			for(size_t i {2}; i < msg.size();)
				app << msg[i] << ((++i==msg.size())?"":" ");
			app.close();
		}
		if(msg[0] == "pop" && msg.size() >= 2 && GetFileAttributesA(std::string("commands\\raw\\" + msg[1] + ".txt").c_str()) != INVALID_FILE_ATTRIBUTES) {
			std::ifstream in("commands\\raw\\" + msg[1] + ".txt");
			std::ofstream out("commands\\raw\\" + msg[1] + ".tmp");
			std::string line;
			std::getline(in, line);
			for(std::string tmp; std::getline(in, tmp); line.swap(tmp))
				if(line.size())
					out << '\n' << line;
			in.close();
			out.close();
			DeleteFileA(std::string("commands\\raw\\" + msg[1] + ".txt").c_str());
			std::rename(("commands\\raw\\" + msg[1] + ".tmp").c_str(), ("commands\\raw\\" + msg[1] + ".txt").c_str());
		}
		if(
			msg[0] == "rem" &&
			msg.size() >= 3 &&
			!msg[2].empty() && std::find_if(msg[2].begin(), msg[2].end(),
				[](unsigned char c) { return !std::isdigit(c); }) == msg[2].end() &&
			GetFileAttributesA(std::string("commands\\raw\\" + msg[1] + ".txt").c_str()) != INVALID_FILE_ATTRIBUTES
		) {
			std::ifstream in { "commands\\raw\\" + msg[1] + ".txt" };
			std::ofstream out { "commands\\raw\\" + msg[1] + ".tmp" };
			std::string line;
			size_t rem = std::stoi(msg[2].c_str());
			std::string tmp;
			size_t at { };
			do {
				line.swap(tmp);
				if(at != rem && line.size())
					out << ((at)?"\n":"") << line;
				at++;
			} while(std::getline(in, tmp));
			in.close();
			out.close();
			DeleteFileA(std::string("commands\\raw\\" + msg[1] + ".txt").c_str());
			std::rename(("commands\\raw\\" + msg[1] + ".tmp").c_str(), ("commands\\raw\\" + msg[1] + ".txt").c_str());
		}
		if(msg[0] == "raw" && msg.size() >= 2 && GetFileAttributesA(std::string("commands\\raw\\" + msg[1] + ".txt").c_str()) != INVALID_FILE_ATTRIBUTES) {
			std::ifstream file { "commands\\raw\\" + msg[1] + ".txt" };
			std::string str;
			for(size_t at { }; std::getline(file, str);)
				send(hwnd, "/tell " + msg.name + " " + std::to_string(++at) + ": " + str);
			file.close();
		}
		if(msg[0] == "tellall" && msg.size() >= 2 && GetFileAttributesA(std::string("commands\\raw\\" + msg[1] + ".txt").c_str()) != INVALID_FILE_ATTRIBUTES) {
			std::ifstream file { "commands\\raw\\" + msg[1] + ".txt" };
			std::string str;
			for(size_t at { }; std::getline(file, str);)
				send(hwnd, "/say " + std::to_string(++at) + ": " + str);
			file.close();
		}
		if(msg[0] == "del" && msg.size() >= 2 && GetFileAttributesA(std::string("commands\\raw\\" + msg[1] + ".txt").c_str()) != INVALID_FILE_ATTRIBUTES)
			DeleteFileA(std::string("commands\\raw\\" + msg[1] + ".txt").c_str());
	}
	if(GetFileAttributesA(std::string("commands\\compiled\\" + msg[0] + ".dll").c_str()) != INVALID_FILE_ATTRIBUTES) {
		HINSTANCE dll = LoadLibraryA(std::string("commands\\compiled\\" + msg[0] + ".dll").c_str());
		mccommand func = (mccommand)GetProcAddress(dll, msg[0].c_str());
		commands.emplace(msg[0], func);
		dlls.emplace(msg[0], dll);
	}
	if(commands.contains(msg[0])) {
		(*commands[msg[0]])(hwnd, msg);
		return;
	}
	if(GetFileAttributesA(std::string("commands\\raw\\" + msg[0] + ".txt").c_str()) != INVALID_FILE_ATTRIBUTES) {
		HANDLE thread = CreateThread(NULL, 0, &compile_command, new compile_params(hwnd, msg), 0, NULL);
		if(thread) to_close.push_back(thread);
		return;
	}
}

int main(int argc, char** argv) {
//	This will be a handle to the command promt window running the server
	HWND hwnd = nullptr;

//	Checks if the server's window exists and gets a handle to it if it does
	hwnd = FindWindowA(NULL, SERVERWND);

//	If the server could not be found, start it
	if(!hwnd) {
		SHELLEXECUTEINFOA sei { 0 };

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

	std::vector<std::string> commands = files_in_dir("commands\\raw");
	for(std::string &i : commands) {
		message msg { };
		msg.tokens.push_back(i.substr(0, i.size() - 4));
		if(GetFileAttributesA(std::string("commands\\compiled\\" + msg[0] + ".dll").c_str()) == INVALID_FILE_ATTRIBUTES) {
			HANDLE thread = CreateThread(NULL, 0, &compile_command, new compile_params(nullptr, msg), 0, NULL);
			if(thread) to_close.push_back(thread);
		}
	}

//	Opens the log file
	std::ifstream file { LOGFILE, std::ios::in };
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
//	TODO - HIGH PRIORITY: make this not take an entire thread
	while(true) {
//		There's probably be a better way to do this but if it isn't closed then opened before reading it won't properly read it
		file.open(LOGFILE, std::ios::in);
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
		if(WaitForSingleObject(current_thread, 0) == WAIT_OBJECT_0) {
			CloseHandle(current_thread);
			current_thread = NULL;
		}
		if(!current_thread && to_close.size()) {
			current_thread = to_close.front();
			to_close.erase(to_close.begin());
		}
	}
}
