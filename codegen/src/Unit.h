#pragma once

#include "FileOps.h"

//-------------------------------------------
// Unit
//-------------------------------------------
enum OutputWidth {t_scalar, t_vector};
enum UnitType {t_remote, t_local, t_temporary, t_compute};
class Unit {
protected:
	list<Unit*> m_inputs;
	list<Unit*> m_outputs;

	string m_name;
	string m_interfaceName;
	OutputWidth m_outputWidth;
	UnitType m_unitType;
	bool m_printed;
	bool m_instantiated;

public:
	static bool CheckVector(Unit& unit, string opName) {
		if (unit.GetOutputWidth() != t_vector) {
			cout << opName << ", " << unit.GetName() << " is not a vector" << endl;
			exit(1);
		}
	}

	static bool CheckScalar(Unit& unit, string opName) {
		if (unit.GetOutputWidth() != t_scalar) {
			cout << opName << ", " << unit.GetName() << " is not a scalar" << endl;
			exit(1);
		}
	}

	Unit(string name, OutputWidth outputWidth, UnitType unitType) {
		m_name = name;
		m_interfaceName = name + "_interface";
		m_outputWidth = outputWidth;
		m_unitType = unitType;
		m_printed = false;
		m_instantiated = false;
	}

	void AddInput(Unit* input) {
		m_inputs.push_back(input);
	}

	void AddOutput(Unit* output) {
		m_outputs.push_back(output);
	}

	list<Unit*> GetInputs() {
		return m_inputs;
	}

	list<Unit*> GetOutputs() {
		return m_outputs;
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

	UnitType GetUnitType() {
		return m_unitType;
	}

	string GetUnitTypeAsString() {
		switch(m_unitType) {
			case t_remote:
				return "t_remote";
			case t_local:
				return "t_local";
			case t_temporary:
				return "t_temporary";
			case t_compute:
				return "t_compute";
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

	virtual void Instantiate(string& fileName) {
		cout << "Unit object cannot be instantiated" << endl;
		exit(1);
	}
};