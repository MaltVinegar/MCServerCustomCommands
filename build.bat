@echo off
pushd commands
if not exist functions.a g++ functions.cpp -c
popd
g++ -std=c++20 commands.cpp -o commands.exe commands\functions.o