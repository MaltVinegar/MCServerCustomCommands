#include "functions.h"

//Sends text directly to the window handle hwnd; used for sending commands to the server
void send(HWND hwnd, std::string str) {
	for(char i : str)
		PostMessage(hwnd, WM_CHAR, i, 0);
//	Sends enter key
	PostMessage(hwnd, WM_CHAR, 13, 0);
//	When multiple `send` functions are called in a row it will improperly write for some reason
//	This seems to fix it
//	If consecutive commands are executing incorrectly it's probably because this is too short for you, try increasing it a little
	Sleep(10);
}

message::message() { }

//	Takes a string formatted like a line from the live log file
//	Splits it into time, thread info, name, and the body
//	Allows it to be more easily parsed
message::message(std::string str) {
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
void message::tokenize() {
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
std::string &message::operator[](size_t i) { return tokens.at(i); }
//	Gets the size of the token list
size_t message::size() { return tokens.size(); }
