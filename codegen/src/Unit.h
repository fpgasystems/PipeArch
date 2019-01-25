#pragma once

#include <stdlib.h>
#include <iostream>
#include <list>
#include <string>

#include "Compute.h"

using namespace std;

//-------------------------------------------
// File operations
//-------------------------------------------
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

//-------------------------------------------
// Unit
//-------------------------------------------
enum OutputWidth {t_scalar, t_vector};
enum Location {t_remote, t_local, t_temporary};
class Unit {
protected:
	list<Unit*> m_leftInputs;
	list<Unit*> m_rightInputs;
	list<Unit*> m_outputs;

	string m_name;
	string m_interfaceName;
	OutputWidth m_outputWidth;
	Location m_location;
	Compute* m_compute;
	bool m_printed;
	bool m_instantiated;

public:
	Unit(string name, OutputWidth outputWidth, Location location) {
		m_name = name;
		m_interfaceName = name + "_interface";
		m_outputWidth = outputWidth;
		m_location = location;
		m_compute = nullptr;
		m_printed = false;
		m_instantiated = false;
	}

	void AddInput(Unit* input) {
		m_leftInputs.push_back(input);
	}

	void AddLeftInput(Unit* input) {
		m_leftInputs.push_back(input);
	}

	void AddRightInput(Unit* input) {
		m_rightInputs.push_back(input);
	}

	void AddOutput(Unit* output) {
		m_outputs.push_back(output);
	}

	list<Unit*> GetOutputs() {
		return m_outputs;
	}

	list<Unit*> GetInputs() {
		return m_leftInputs;
	}

	string GetName() {
		return m_name;
	}

	string GetInterfaceName() {
		return m_interfaceName;
	}

	OutputWidth GetOutputWidth() {
		return m_outputWidth;
	}

	string GetOutputWidthAsString() {
		switch(m_outputWidth) {
			case t_scalar:
				return "t_scalar";
			case t_vector:
				return "t_vector";
		}
	}

	string GetLocationAsString() {
		switch(m_location) {
			case t_remote:
				return "t_remote";
			case t_local:
				return "t_local";
			case t_temporary:
				return "t_temporary";
		}
	}

	Location GetLocation() {
		return m_location;
	}

	void AddCompute(Compute* compute) {
		m_compute = compute;
	}

	Compute* GetCompute() {
		return m_compute;
	}

	void PrintInfo(string tab) {
		if (!m_printed) {
			m_printed = true;
			cout << tab << "--------------------Name: " << m_name << endl;

			cout << tab << "OutputWidth: " << GetOutputWidthAsString();
			cout << ", Location: " << GetLocationAsString();
			if (m_compute != nullptr) {
				cout << ", Compute: " << m_compute->GetName();
			}
			cout << endl;

			cout << tab << "Left Inputs (" << m_leftInputs.size() << "): ";
			for (Unit* input: m_leftInputs) {
				cout << input->GetName() << " ";
			}
			cout << endl;
			cout << tab << "Right Inputs (" << m_rightInputs.size() << "): ";
			for (Unit* input: m_rightInputs) {
				cout << input->GetName() << " ";
			}
			cout << endl;
			cout << tab << "Outputs (" << m_outputs.size() << "): ";
			for (Unit* output: m_outputs) {
				cout << output->GetName() << " ";
			}
			cout << endl;
			for (Unit* output: m_outputs) {
				if (output->GetName().compare("writeback") != 0) {
					output->PrintInfo(tab + "\t");
				}
			}
		}
	}

	virtual string Instantiate(string fileName) {
		cout << "Unit object cannot be instantiated" << endl;
		exit(1);
	}
};