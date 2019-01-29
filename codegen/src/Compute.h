#pragma once

#include "Operation.h"
#include "Memory.h"

class Load : public Operation{
public:
	Load(string name) : Operation(1, name, t_vector, t_remote) {}

	void Instantiate() {
		if (!m_instantiated) {
			cout << "Instantiate " << m_name << endl;
			unsigned loadChannelNumber = 0;
			for (Unit* output: m_outputs) {
				string channel = WriteChannel(output, 5+loadChannelNumber);
				loadChannelNumber++;
				FileOps::FindAndInstert(FileOps::m_svTopFileName, "//?LOADCH", channel);
				output->Instantiate();
			}
			FileOps::FindAndInstert(FileOps::m_svTopFileName, "//?LOADCH", WriteChannelSelection(m_outputs));
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
	WriteBack(string name) : Operation(2, name, t_vector, t_remote) {}

	void Instantiate() {
		if (!m_instantiated) {
			cout << "Instantiate " << m_name << endl;
			unsigned storeChannelNumber = 0;
			for (Unit* input: m_inputs) {
				string channel = ReadChannel(input, 6+storeChannelNumber);
				storeChannelNumber++;
				FileOps::FindAndInstert(FileOps::m_svTopFileName, "//?STORECH", channel);
			}
			FileOps::FindAndInstert(FileOps::m_svTopFileName, "//?STORECH", ReadChannelSelection("to_" + m_name, m_inputs, 5, 0));
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
protected:
	string m_opName;
	list<Unit*> m_leftInputs;
	list<Unit*> m_rightInputs;
	unsigned m_readConfigReg;
	unsigned m_leftReadConfigReg;
	unsigned m_rightReadConfigReg;
	unsigned m_readSelectionReg;
	unsigned m_writeConfigReg;

public:
	Compute(
		unsigned opId,
		string name,
		OutputWidth outputWidth,
		string opName,
		unsigned readConfigReg,
		unsigned leftReadConfigReg,
		unsigned rightReadConfigReg,
		unsigned readSelectionReg,
		unsigned writeConfigReg)
	: Operation(opId, name, outputWidth, t_compute)
	{
		m_opName = opName;
		m_readConfigReg = readConfigReg;
		m_leftReadConfigReg = leftReadConfigReg;
		m_rightReadConfigReg = rightReadConfigReg;
		m_readSelectionReg = readSelectionReg;
		m_writeConfigReg = writeConfigReg;
	}

	void AddLeftInput(Unit* input) {
		m_inputs.push_back(input);
		m_leftInputs.push_back(input);
	}

	void AddRightInput(Unit* input) {
		m_inputs.push_back(input);
		m_rightInputs.push_back(input);
	}

	virtual void Instantiate() {
		if (!m_instantiated) {
			cout << "Instantiate " << m_name << endl;

			string fromName("from_" + m_name);
			string toName("to_" + m_name);
			string instantiation("");
			if (m_outputWidth == t_scalar) {
				instantiation += "internal_interface #(.WIDTH(32)) " + fromName + "();\n";
			}
			else {
				instantiation += "internal_interface #(.WIDTH(512)) " + fromName + "();\n";
			}

			if (m_leftInputs.front()->GetOutputWidth() == t_scalar) {
				instantiation += "internal_interface #(.WIDTH(32)) " + toName + "_left();\n";
			}
			else {
				instantiation += "internal_interface #(.WIDTH(512)) " + toName + "_left();\n";
			}
			
			if (m_rightInputs.front()->GetOutputWidth() == t_scalar) {
				instantiation += "internal_interface #(.WIDTH(32)) " + toName + "_right();\n";
			}
			else {
				instantiation += "internal_interface #(.WIDTH(512)) " + toName + "_right();\n";
			}
			
			instantiation += m_opName + "\n";
			instantiation += m_opName + "_" + m_name + "_inst(\n";
			instantiation += ".clk, .reset,\n";
			instantiation += ".op_start(op_start[" + to_string(m_opId) + "]),\n";
			instantiation += ".op_done(op_done[" + to_string(m_opId) + "]),\n";
			instantiation += ".regs0(regs[" + to_string(m_readConfigReg) + "]),\n";
			instantiation += ".left_input(" + toName + "_left.from_commonread),\n";
			instantiation += ".right_input(" + toName + "_right.from_commonread),\n";
			instantiation += ".result(" + fromName + ")\n";
			instantiation += ");\n";

			FileOps::FindAndInstert(FileOps::m_svTopFileName, "//?LOCALC", instantiation);

			unsigned leftReadChannelNumber = 0;
			for (Unit* input: m_leftInputs) {
				string channel = ReadChannel(input, m_leftReadConfigReg+leftReadChannelNumber);
				leftReadChannelNumber++;
				FileOps::FindAndInstert(FileOps::m_svTopFileName, "//?LOCALC", channel);
			}
			unsigned rightReadChannelNumber = 0;
			for (Unit* input: m_rightInputs) {
				string channel = ReadChannel(input, m_rightReadConfigReg+rightReadChannelNumber);
				rightReadChannelNumber++;
				FileOps::FindAndInstert(FileOps::m_svTopFileName, "//?LOCALC", channel);
			}
			unsigned writeChannelNumber = 0;
			for (Unit* output: m_outputs) {
				string channel = WriteChannel(output, m_writeConfigReg);
				writeChannelNumber++;
				FileOps::FindAndInstert(FileOps::m_svTopFileName, "//?LOCALC", channel);
			}

			FileOps::FindAndInstert(FileOps::m_svTopFileName, "//?LOCALC", ReadChannelSelection("to_" + m_name + "_left", m_leftInputs, m_readSelectionReg, 0));
			FileOps::FindAndInstert(FileOps::m_svTopFileName, "//?LOCALC", ReadChannelSelection("to_" + m_name + "_right", m_rightInputs, m_readSelectionReg, 4));
			FileOps::FindAndInstert(FileOps::m_svTopFileName, "//?LOCALC", WriteChannelSelection(m_outputs));


			string opcode("");
			opcode += "8'd" + to_string(10+m_opId) + ":\n";
			opcode += "begin\n";
			opcode += "\top_start[" + to_string(m_opId) + "] <= 1'b1;\n";
			opcode += "\tregs[" + to_string(m_readConfigReg) + "] <= instruction[" + to_string(m_readConfigReg) + "];\n";
			opcode += "\tregs[" + to_string(m_readSelectionReg) + "] <= instruction[" + to_string(m_readSelectionReg) + "];\n";
			leftReadChannelNumber = 0;
			for (Unit* input: m_leftInputs) {
				if (m_leftReadConfigReg+leftReadChannelNumber > m_readConfigReg) {
					if (input->GetUnitType() == t_local) {
						int offsetRegister = ((LocalMemory*)input)->GetOffsetRegister();
						if (offsetRegister != -1) {
							opcode += "\tregs[" + to_string(m_leftReadConfigReg+leftReadChannelNumber) + 
								"] <= instruction[" + to_string(m_leftReadConfigReg+leftReadChannelNumber) + "] + regs[" + to_string(offsetRegister) + "];\n";
						}
						else {
							opcode += "\tregs[" + to_string(m_leftReadConfigReg+leftReadChannelNumber) + 
								"] <= instruction[" + to_string(m_leftReadConfigReg+leftReadChannelNumber) + "];\n";
						}
					}
					else {
						opcode += "\tregs[" + to_string(m_leftReadConfigReg+leftReadChannelNumber) + 
							"] <= instruction[" + to_string(m_leftReadConfigReg+leftReadChannelNumber) + "];\n";
					}
				}
			}
			rightReadChannelNumber = 0;
			for (Unit* input: m_rightInputs) {
				if (m_rightReadConfigReg+rightReadChannelNumber > m_readConfigReg) {
					if (input->GetUnitType() == t_local) {
						int offsetRegister = ((LocalMemory*)input)->GetOffsetRegister();
						if (offsetRegister != -1) {
							opcode += "\tregs[" + to_string(m_rightReadConfigReg+rightReadChannelNumber) + 
								"] <= instruction[" + to_string(m_rightReadConfigReg+rightReadChannelNumber) + "] + regs[" + to_string(offsetRegister) + "];\n";
						}
						else {
							opcode += "\tregs[" + to_string(m_rightReadConfigReg+rightReadChannelNumber) + 
								"] <= instruction[" + to_string(m_rightReadConfigReg+rightReadChannelNumber) + "];\n";
						}
					}
					else {
						opcode += "\tregs[" + to_string(m_rightReadConfigReg+rightReadChannelNumber) + 
							"] <= instruction[" + to_string(m_rightReadConfigReg+rightReadChannelNumber) + "];\n";
					}
				}
			}
			writeChannelNumber = 0;
			for (Unit* output: m_outputs) {
				if (m_writeConfigReg+writeChannelNumber > m_readConfigReg) {
					opcode += "\tregs[" + to_string(m_writeConfigReg+writeChannelNumber) + "] <= instruction[" + to_string(m_writeConfigReg+writeChannelNumber) + "];\n";
				}
			}
			opcode += "\tprogram_counter <= program_counter + 1;\n";
			opcode += "end\n";
			FileOps::FindAndInstert(FileOps::m_svTopFileName, "//?OPCODES", opcode);


			string instruction("");
			instruction += "void " + m_name + "(\n";
			leftReadChannelNumber = 0;
			for (Unit* input: m_leftInputs) {
				if (m_leftReadConfigReg+leftReadChannelNumber > m_readConfigReg) {
					instruction += "\taccess_t accessPropertiesIn_" + input->GetName() + ",\n";
				}
				leftReadChannelNumber++;
			}
			rightReadChannelNumber = 0;
			for (Unit* input: m_rightInputs) {
				if (m_rightReadConfigReg+rightReadChannelNumber > m_readConfigReg) {
					instruction += "\taccess_t accessPropertiesIn_" + input->GetName() + ",\n";
				}
				rightReadChannelNumber++;
			}
			writeChannelNumber = 0;
			for (Unit* output: m_outputs) {
				if (m_writeConfigReg+writeChannelNumber > m_readConfigReg) {
					instruction += "\taccess_t accessPropertiesOut_" + output->GetName() + ",\n";
				}
				writeChannelNumber++;
			}
			if (m_leftInputs.size() > 1 || m_rightInputs.size() > 1) {
				instruction += "\tuint32_t leftChannelSelect,\n";
				instruction += "\tuint32_t rightChannelSelect)\n";
			}
			instruction += "\tuint32_t numLinesToProcess)\n";

			instruction += "{\n";
			instruction += "\tm_data[15] = " + to_string(10+m_opId) + ";\n";
			instruction += "\tm_data[" + to_string(m_readConfigReg) + "] = numLinesToProcess << 16;\n";
			if (m_leftInputs.size() > 1 || m_rightInputs.size() > 1) {
				instruction += "\tm_data[" + to_string(m_readSelectionReg) + "] = ((rightChannelSelect & 0xF) << 4) | (leftChannelSelect & 0xF);\n";
			}
			else {
				instruction += "\tm_data[" + to_string(m_readSelectionReg) + "] = 0;\n";
			}
			leftReadChannelNumber = 0;
			for (Unit* input: m_leftInputs) {
				if (m_leftReadConfigReg+leftReadChannelNumber > m_readConfigReg) {
					instruction += "\tm_data[" + to_string(m_leftReadConfigReg+leftReadChannelNumber) + "] = " +
						"(accessPropertiesIn_" + input->GetName() + ".m_lengthInCL << 16) | accessPropertiesIn_" + input->GetName() + ".m_offsetInCL;\n";
				}
				leftReadChannelNumber++;
			}
			rightReadChannelNumber = 0;
			for (Unit* input: m_rightInputs) {
				if (m_rightReadConfigReg+rightReadChannelNumber > m_readConfigReg) {
					instruction += "\tm_data[" + to_string(m_rightReadConfigReg+rightReadChannelNumber) + "] = " +
						"(accessPropertiesIn_" + input->GetName() + ".m_lengthInCL << 16) | accessPropertiesIn_" + input->GetName() + ".m_offsetInCL;\n";
				}
				rightReadChannelNumber++;
			}
			writeChannelNumber = 0;
			for (Unit* output: m_outputs) {
				if (m_writeConfigReg+writeChannelNumber > m_readConfigReg) {
					instruction += "\tm_data[" + to_string(m_writeConfigReg+writeChannelNumber) + "] = " +
						"(accessPropertiesOut_" + output->GetName() + ".m_lengthInCL << 16) | accessPropertiesOut_" + output->GetName() + ".m_offsetInCL;\n";
				}
				writeChannelNumber++;
			}

			instruction += "}\n";

			FileOps::FindAndInstert(FileOps::m_instructionsHeaderFileName, "//?INST", instruction);

			m_instantiated = true;

			for (Unit* output: m_outputs) {
				output->Instantiate();
			}
		}
	}

	virtual void PrintInfo(string tab){
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