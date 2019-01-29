#pragma once

#include <stdlib.h>
#include <iostream>
#include <list>
#include <string>

using namespace std;

//-------------------------------------------
// File operations
//-------------------------------------------
namespace FileOps{
	const static string m_svTopFileName = "./output/pipearch_top.sv";
	const static string m_instructionsHeaderFileName = "../sw/Instruction.h";

	static void FileCopy(string sourceFile, string destinationFile) {
		string command = "cp " + sourceFile + " " + destinationFile;
		int res = system(command.c_str());
		if (res != 0) {
			cout << "Could not copy " << sourceFile << " to " << destinationFile << endl;
			exit(1);
		}
	}

	static void FindAndInstert(string fileName, string keyword, string newPhrase) {
		ifstream orgFile(fileName);
		ofstream newFile(fileName + "_temp");

		unsigned BufferLength = keyword.length();
		char buffer[BufferLength+1];
		buffer[BufferLength] = '\0';
		char c;
		while(orgFile.get(c)) {
			for (unsigned i = 0; i < BufferLength-1; i++) {
				buffer[i] = buffer[i+1];
			}
			buffer[BufferLength-1] = c;

			if (strcmp(buffer, keyword.c_str()) == 0) {
				cout << "Found: " << buffer << endl;
				long pos = newFile.tellp();
				newFile.seekp(pos-BufferLength);
				newFile << endl << newPhrase << endl;
				newFile << keyword;
				break;
			}
			else{
				newFile.put(c);
			}
		}
		while(orgFile.get(c)) {
			newFile.put(c);
		}

		orgFile.close();
		newFile.close();

		string temp = "cp " + fileName + "_temp " + fileName;
		system(temp.c_str());
		temp = "rm " + fileName + "_temp";
		system(temp.c_str());
	}
};

