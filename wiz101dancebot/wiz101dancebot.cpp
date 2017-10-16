// Christopher R. Fischer
// 10/15/2017
// VCC / FHS
// Wizard101 Gaming Bot.
// Copyright 2017. Please do not redistrute or use outside of written authorization.

#include "stdafx.h"
#include <windows.h>
#include <tchar.h>
#include <stdio.h>
#include <psapi.h>
#include <iostream>
#include <time.h>
#include <stdlib.h>



using namespace std;

const int offsets[30] = { (int)(0x00003964), ((int)0x0000266C), 0, (int)0x1F14, (int)0x1EF8, (int)0x9200,};
// Base Address referes to the base address of the DLL we are referencing.
int baseAddress = 0;
// CurrentRow, CurrentColumn, CurrentTime, CurrentScore, CurrentLevel, BaseRowColumn
int speed = 200;
bool errors = true;
int getRandNum(int lowbound, int highbound) {
	srand(time(NULL));
	return lowbound + (rand() % highbound);
}		
int readNoPointer(HANDLE w101, int offset) {
	// Access pointer stored at arbitrary location, retrieve current memory address, and retrieve value.
	// This function is used when we don't need to access a pointer. This is unusual for the current setup.
	// Such applications include calculating the addresses by hand.
	UINT_PTR PointerAddress = (UINT_PTR)(offset + baseAddress);
	DWORD CorrectAddress;
	DWORD Value;
	//printf("0x%x\n", PointerAddress);
	ReadProcessMemory(w101, (void*)PointerAddress, &Value, sizeof(DWORD), 0);
	return Value;
}
int readAddress(HANDLE w101, int offset) {
	// Access pointer stored at arbitrary location, retrieve current memory address, and retrieve value.
	UINT_PTR PointerAddress = (UINT_PTR)(offset + baseAddress);
	DWORD CorrectAddress;
	DWORD Value;
	//printf("0x%x\n", PointerAddress);
	ReadProcessMemory(w101, (void*)PointerAddress, &CorrectAddress, sizeof(DWORD), 0);
	ReadProcessMemory(w101, (void*)CorrectAddress, &Value, sizeof(DWORD), 0);
	return Value;
}
int writeAddress(HANDLE w101, int offset, DWORD value) {
	//Access Pointer and write at arbritray memory.
	UINT_PTR adr = (UINT_PTR)(offset + baseAddress);
	DWORD CorrectAddress;
	DWORD ValueInMemory;
	ReadProcessMemory(w101, (void*)adr, &CorrectAddress, sizeof(DWORD), 0);
	ReadProcessMemory(w101, (void*)CorrectAddress, &ValueInMemory, sizeof(DWORD), 0);
	WriteProcessMemory(w101, (void*)CorrectAddress, &value, sizeof(DWORD), 0);
	return ValueInMemory;
}
// Adapted from Microsoft's Devolper Network Open Library, thanks for the function!
int GetBaseAddress(DWORD processID)
{
	HMODULE hMods[1024];
	HANDLE hProcess;
	DWORD cbNeeded;
	unsigned int i;

	hProcess = OpenProcess(PROCESS_QUERY_INFORMATION |
		PROCESS_VM_READ,
		FALSE, processID);
	if (EnumProcessModules(hProcess, hMods, sizeof(hMods), &cbNeeded))
	{
		for (i = 0; i < (cbNeeded / sizeof(HMODULE)); i++)
		{
			TCHAR szModName[MAX_PATH];

			if (GetModuleFileNameEx(hProcess, hMods[i], szModName,
				sizeof(szModName) / sizeof(TCHAR)))
			{				
				if (_tcscmp(szModName, _T("C:\\ProgramData\\KingsIsle Entertainment\\Wizard101\\Bin\\MG_concentration.dll")) == 0) {
					//_tprintf(TEXT("\t%s (0x%08X)\n"), szModName, hMods[i]);
					return (int)hMods[i];
				}
			}
		}
	}

	// Release the handle to the process.

	CloseHandle(hProcess);
	// We know it wasn't found!
	printf("\n\nFATAL ERROR: Concentration Game not loaded in! To retry, type anything and press enter.");
	printf("\nPress any key to continue.\n");
	char arr;
	cin >> arr;
	return GetBaseAddress(processID);
}
void mouse_move(int x, int y) {
	INPUT Inputs[3] = { 0 };

	Inputs[0].type = INPUT_MOUSE;
	Inputs[0].mi.dx = x; // desired X coordinate
	Inputs[0].mi.dy = y; // desired Y coordinate
	Inputs[0].mi.dwFlags = MOUSEEVENTF_ABSOLUTE | MOUSEEVENTF_MOVE;

	Inputs[1].type = INPUT_MOUSE;
	Inputs[1].mi.dwFlags = MOUSEEVENTF_LEFTDOWN;

	Inputs[2].type = INPUT_MOUSE;
	Inputs[2].mi.dwFlags = MOUSEEVENTF_LEFTUP;

	SendInput(3, Inputs, sizeof(INPUT));
}
void click(int x, int y) {
	// This function takes in positions on the board and sends hardware events ("clicks");
	// Find location
	
	//printf("click card %d %d \t", x, y);
	int basex = 789;
	int basey = 349;
	int bx = basex + (y * 64) + getRandNum(10, 44);
	int by = basey + (x * 64) + getRandNum(10, 44);
	bx= bx * (65535 / 1920 );
	by= by * (65535 / 1080 );
	//printf("click cord %d %d \n", bx, by);
	mouse_move(bx, by);
}
int retrieveValue(int row, int column, HANDLE w101) {
	// For every ROW past 0, we add 28 hex (40 dec).
	// For every COLUMN past 0, we add F0 hex, 240 dec
	int off = offsets[5] + (row*40)+(column*240);
	//printf("Calculated Offset - %d\n", off);
	return readNoPointer(w101, off);
}
void winRound(HANDLE w101) {
	// This is cheating because we trick the game into thinking there is only one column/row, and with it's poorly written logic it determains that the round is over after the last click (probably a edge case trigger).
	// In the real world, this functions use exists primarily because in memory the array is stored in relative co-ordinates.
	// So, in the first few rounds, we can't accurately predict where to click because (annoyingly) the cards, if not taking up the whole board, aren't in the same place every time.
	// The solution is to use this function, set the round to win, and send clicks to each and every box until we blindly hit the right one.
	// Again, I'm not a fan of this either, but we can email KI later with our suggestions on how they write their proprietary code.
	writeAddress(w101, 0, (DWORD)1);
	writeAddress(w101, 1, (DWORD)1);
}
void findPairs(HANDLE w101) {
	// Let's find a bunch of pairs. First, we should set up our int array.
	int numRow = readAddress(w101, offsets[0]);
	int numCol = readAddress(w101, offsets[1]);
	// C requires constant numbers, and the board has a maximum of 7 x 7.
	// 9 is used as a buffer to make my life easier as a programmer.
	int board[9][9];

	// Cool, now that we have a board, let's fill it with values.
	for (int currentRow = 0; currentRow < numRow; currentRow++) {
		for (int currentColumn = 0; currentColumn < numCol; currentColumn++) {
			board[currentRow][currentColumn] = retrieveValue(currentRow, currentColumn, w101);
			// Leaving these prints in would print the board (duh!)
			printf("%4d ", board[currentRow][currentColumn]);
		}
		printf("\n");
	}
	int offrow = 0;
	int offcol = 0;
	if (numCol != 7 || numRow != 6) {
		// Not a full board drawn.
		printf("WARNING: Incomplete board detected, please input 0,0 location offsets.\n");
		scanf("%d %d", &offrow, &offcol);
		printf("\n");
	}
	// Now that we have the board, let's locate our cards (in range)
	bool found;
	
	for (int i = 0; i <= 40; i++) {
		found = false;
		// Set this card as not found, because we don't want to print out or try to click non-existant cards.	
		for (int currentRow = 0; currentRow < numRow; currentRow++) {
			for (int currentColumn = 0; currentColumn < numCol; currentColumn++) {
				//click(currentRow, currentColumn);
				if (board[currentRow][currentColumn] == i) {
					// Cool, so this value is on there, we've found one!
					// For right now, we're going to print the co-ordinates of where on the board our match is. 
					// Later, we can make a clicking function.
					if (!found) {
						printf("Card %2d: ", i);
						found = true;
					}
					click(currentRow+offrow, currentColumn+offcol);
					Sleep(speed+getRandNum(5,10));
				}
				if (getRandNum(0, 5) == 1 && errors) {
					// 8.4% chance of a misclick or random click 
					click(currentColumn, currentRow);
				}
			}
		}
		if (found) {
			printf("\n");
		}
		if (errors)
		{
			for (int currentRow = 0; currentRow < numRow; currentRow++) {
				for (int currentColumn = 0; currentColumn < numCol; currentColumn++) {
					click(currentRow+offrow, currentColumn+offcol);
				}
			}
		}
		Sleep(20);
	}
	Sleep(1500);
}
int main() {
	HWND wizard101 = FindWindow(NULL, "Wizard101");
	DWORD PID;
	GetWindowThreadProcessId(wizard101, &PID);
	printf("Please enter the speed multiplier (delay): ");
	scanf("%d", &speed);
	printf("\n");
	HANDLE w101 = OpenProcess(PROCESS_VM_OPERATION | PROCESS_VM_READ | PROCESS_VM_WRITE, false, PID);
	while (true) {
		// Is the module still loaded?
		baseAddress = GetBaseAddress(PID);
		printf("Time: d // Row Count: %d // Column Count: %d // Current Level: %d // Current Score: %d\n", readAddress(w101, offsets[0]), readAddress(w101, offsets[1]), readAddress(w101, offsets[4]), readAddress(w101, offsets[3]));
		findPairs(w101);
		Sleep(500);
	}
	system("pause");
    return 0;
}