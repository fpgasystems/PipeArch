#pragma once

#include "Operation.h"

class Load : public Operation{
public:
	Load(string name) : Operation(1, 5, name, t_vector, t_remote) {}

	void Instantiate(string& fileName) {
		if (!m_instantiated) {
			cout << "Instantiate " << m_name << endl;
			unsigned loadChannelNumber = 0;
			for (Unit* output: m_outputs) {
				string channel = WriteChannel(output, loadChannelNumber);
				loadChannelNumber++;
				FindAndInstert(fileName, "//?LOADCH", channel);
				output->Instantiate(fileName);
			}
			m_instantiated = true;
		}
	}

	virtual void PrintInfo(string tab) {
		if (!m_printed) {
			m_printed = true;
			cout << tab << "--------------------Name: " << m_name << endl;

			cout << tab << "OutputWidth: " << GetOutputWidthAsString();
			cout << ", UnitType: " << GetUnitTypeAsString();
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
};

class WriteBack : public Operation{
public:
	WriteBack(string name) : Operation(2, 6, name, t_vector, t_remote) {}

	void Instantiate(string& fileName) {
		if (!m_instantiated) {
			cout << "Instantiate " << m_name << endl;
			unsigned storeChannelNumber = 0;
			for (Unit* input: m_inputs) {
				string channel = ReadChannel(input, storeChannelNumber);
				storeChannelNumber++;
				FindAndInstert(fileName, "//?STORECH", channel);
			}
			FindAndInstert(fileName, "//?STORECH", ReadChannelSelection(m_inputs.size()));
			m_instantiated = true;
		}
	}

	virtual void PrintInfo(string tab) {
		if (!m_printed) {
			m_printed = true;
			cout << tab << "--------------------Name: " << m_name << endl;

			cout << tab << "OutputWidth: " << GetOutputWidthAsString();
			cout << ", UnitType: " << GetUnitTypeAsString();
			cout << endl;

			cout << tab << "Inputs (" << m_inputs.size() << "): ";
			for (Unit* input: m_inputs) {
				cout << input->GetName() << " ";
			}
			cout << endl;
		}
	}
};

class Compute : public Operation {
private:
	list<Unit*> m_leftInputs;
	list<Unit*> m_rightInputs;

public:
	Compute(unsigned opId, unsigned numDefaultRegs, string name, OutputWidth outputWidth) : Operation(opId, numDefaultRegs, name, outputWidth, t_compute) {}

	void AddLeftInput(Unit* input) {
		m_inputs.push_back(input);
		m_leftInputs.push_back(input);
	}

	void AddRightInput(Unit* input) {
		m_inputs.push_back(input);
		m_rightInputs.push_back(input);
	}

	void Instantiate(string& fileName) {
		if (!m_instantiated) {
			cout << "Instantiate " << m_name << endl;

			unsigned readChannelNumber = 0;
			for (Unit* input: m_inputs) {
				string channel = ReadChannel(input, readChannelNumber);
				readChannelNumber++;
				FindAndInstert(fileName, "//?LOCALC", channel);
			}

			unsigned writeChannelNumber = 0;
			for (Unit* output: m_outputs) {
				string channel = WriteChannel(output, writeChannelNumber);
				writeChannelNumber++;
				FindAndInstert(fileName, "//?LOCALC", channel);
				output->Instantiate(fileName);
			}

			m_instantiated = true;
		}
	}

	void PrintInfo(string tab){
		if (!m_printed) {
			m_printed = true;
			cout << tab << "--------------------Name: " << m_name << endl;

			cout << tab << "OutputWidth: " << GetOutputWidthAsString();
			cout << ", UnitType: " << GetUnitTypeAsString();
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
};