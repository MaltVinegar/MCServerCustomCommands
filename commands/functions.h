#pragma once

#include <string>
#include <vector>
#include <windows.h>
#include <fstream>
#include <limits>

void send(HWND hwnd, std::string str);

struct message {
	message();
	message(std::string str);
	void tokenize();
	std::string &operator[](size_t i);
	size_t size();

//			Example:
//	[00:14:59] [Server thread/INFO]: <IWWenjoyer> #Hello World!
	std::string time { };					//	"00:14:59"
	std::string info { };					//	"Server thread/INFO"
	std::string name { };					//	"IWWenjoyer"
	std::string said { };					//	"#Hello World!"
	std::vector<std::string> tokens { };	//	"Hello", "World!"

};
